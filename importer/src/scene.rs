// W3D scene -> glTF document.
//
// The scene graph is kept exactly as authored (Director units, Z up) under a
// single root node whose matrix converts to glTF/Godot space: Y up, metres
// (1 Director unit = 1 inch = 0.0254 m, the world scale stored in the game's
// Havok file). Node names are preserved so game logic can find
// "Checkpoint_N" / "prizes_N" groups and individual models by name.

use std::collections::{BTreeMap, HashMap};
use std::path::Path;

use serde_json::json;
use vm_rust::director::chunks::w3d::types::{W3dNodeType, W3dScene, W3dShader};

use crate::gltf::{Document, Material, Node, Primitive};
use crate::textures::decode_w3d_texture;

pub const WORLD_SCALE: f32 = 0.0254;

/// Director (x, y, z) Z-up inches  ->  Godot (x, z, -y) Y-up metres, as a
/// column-major 4x4 matrix.
pub fn director_to_godot_matrix() -> [f32; 16] {
    let s = WORLD_SCALE;
    [
        s, 0.0, 0.0, 0.0, //
        0.0, 0.0, -s, 0.0, //
        0.0, s, 0.0, 0.0, //
        0.0, 0.0, 0.0, 1.0,
    ]
}

/// Lightmap data for one model: lightmap image name and per-mesh UVs.
pub struct ModelLightmap {
    pub lightmap: String,
    pub uvs: Vec<Vec<[f32; 2]>>,
}

pub struct SceneOptions<'a> {
    pub lightmaps: &'a HashMap<String, ModelLightmap>,
    pub texture_dir_name: String,
}

pub struct SceneReport {
    pub models: usize,
    pub textures: usize,
    pub lightmapped_meshes: usize,
    pub lightmap_mismatches: Vec<String>,
}

fn flip_v(uv: &[f32; 2]) -> [f32; 2] {
    [uv[0], 1.0 - uv[1]]
}

fn shader_extras(sh: &W3dShader, scene: &W3dScene) -> serde_json::Value {
    let mat = scene.materials.iter().find(|m| m.name == sh.material_name);
    let layers: Vec<serde_json::Value> = sh
        .texture_layers
        .iter()
        .map(|l| {
            json!({
                "texture": l.name.to_string(),
                "blend_func": l.blend_func, "blend_src": l.blend_src, "blend_const": l.blend_const,
                "tex_mode": l.tex_mode, "intensity": l.intensity,
                "repeat_s": l.repeat_s, "repeat_t": l.repeat_t,
                "tex_transform": l.tex_transform.to_vec(),
            })
        })
        .collect();
    json!({
        "w3d_shader": sh.name.to_string(),
        "w3d_material": sh.material_name.to_string(),
        "use_diffuse_with_texture": sh.use_diffuse_with_texture,
        "ambient": mat.map(|m| m.ambient.to_vec()),
        "diffuse": mat.map(|m| m.diffuse.to_vec()),
        "specular": mat.map(|m| m.specular.to_vec()),
        "emissive": mat.map(|m| m.emissive.to_vec()),
        "shininess": mat.map(|m| m.shininess),
        "opacity": mat.map(|m| m.opacity),
        "layers": layers,
    })
}

/// Pick the shader list a model uses for its meshes. A model resource carries
/// named shader bindings; Director uses the one named after the model (or its
/// resource), falling back to the resource's default binding.
fn mesh_shaders(scene: &W3dScene, node_name: &str, res_name: &str, mesh_count: usize, node_shader: &str) -> Vec<String> {
    let res = scene.model_resources.iter().find(|(k, _)| k.to_string() == res_name).map(|(_, v)| v);
    let mut out = vec![node_shader.to_string(); mesh_count];
    if let Some(res) = res {
        let pick = res
            .shader_bindings
            .iter()
            .find(|b| b.name.to_string().eq_ignore_ascii_case(node_name))
            .or_else(|| res.shader_bindings.iter().find(|b| b.name.to_string().eq_ignore_ascii_case(res_name)))
            .or_else(|| res.shader_bindings.iter().find(|b| !b.name.to_string().eq_ignore_ascii_case("default")))
            .or_else(|| res.shader_bindings.first());
        if let Some(b) = pick {
            for (i, s) in b.mesh_bindings.iter().enumerate().take(mesh_count) {
                out[i] = s.to_string();
            }
        }
    }
    out
}

pub fn convert(scene: &W3dScene, out_dir: &Path, glb_name: &str, opts: &SceneOptions) -> Result<SceneReport, String> {
    let mut doc = Document::default();
    let mut report = SceneReport { models: 0, textures: 0, lightmapped_meshes: 0, lightmap_mismatches: vec![] };

    // Textures -> PNG files next to the .glb, referenced by relative URI.
    let tex_dir = out_dir.join(&opts.texture_dir_name);
    let mut image_index: HashMap<String, usize> = HashMap::new();
    let mut image_alpha: HashMap<String, bool> = HashMap::new();
    let mut names: Vec<_> = scene.texture_images.keys().map(|k| k.to_string()).collect();
    names.sort();
    for name in names {
        let data = scene.texture_images.iter().find(|(k, _)| k.to_string() == name).map(|(_, v)| v).unwrap();
        if let Some(img) = decode_w3d_texture(data) {
            let file = format!("{}.png", sanitize(&name));
            img.save_png(&tex_dir.join(&file))?;
            image_alpha.insert(name.clone(), img.has_alpha());
            image_index.insert(name.clone(), doc.images.len());
            doc.images.push(format!("{}/{}", opts.texture_dir_name, file));
            report.textures += 1;
        }
    }

    // Materials: one per W3D shader actually used by a mesh.
    let mut material_index: HashMap<String, usize> = HashMap::new();
    let mut get_material = |doc: &mut Document, shader_name: &str, double_sided: bool| -> Option<usize> {
        if let Some(&i) = material_index.get(shader_name) {
            if double_sided {
                doc.materials[i].double_sided = true;
            }
            return Some(i);
        }
        let sh = scene.shaders.iter().find(|s| s.name.to_string() == shader_name)?;
        let mat = scene.materials.iter().find(|m| m.name == sh.material_name);
        let base_tex = sh.texture_layers.first().map(|l| l.name.to_string()).filter(|n| n != "DefaultTexture");
        let base_texture = base_tex.as_ref().and_then(|n| image_index.get(n).copied());
        let diffuse = mat.map(|m| m.diffuse).unwrap_or([1.0; 4]);
        let opacity = mat.map(|m| m.opacity).unwrap_or(1.0);
        let base_color = if base_texture.is_some() && !sh.use_diffuse_with_texture {
            [1.0, 1.0, 1.0, opacity]
        } else {
            [diffuse[0], diffuse[1], diffuse[2], opacity]
        };
        let tex_alpha = base_tex.as_ref().and_then(|n| image_alpha.get(n)).copied().unwrap_or(false);
        let emissive = mat.map(|m| [m.emissive[0], m.emissive[1], m.emissive[2]]).unwrap_or([0.0; 3]);
        doc.materials.push(Material {
            name: shader_name.to_string(),
            base_color,
            base_texture,
            emissive,
            alpha_blend: opacity < 0.999 || tex_alpha,
            alpha_mask: false,
            double_sided,
            unlit: false,
            extras: Some(shader_extras(sh, scene)),
        });
        let i = doc.materials.len() - 1;
        material_index.insert(shader_name.to_string(), i);
        Some(i)
    };

    // Nodes. W3D lists nodes flat with parent names; "world" is the root.
    let root = doc.nodes.len();
    doc.nodes.push(Node {
        name: "DirectorWorld".into(),
        matrix: Some(director_to_godot_matrix()),
        mesh: None,
        children: vec![],
        extras: Some(json!({"note": "Director Z-up inches -> Y-up metres"})),
    });
    doc.roots.push(root);

    let mut node_index: BTreeMap<String, usize> = BTreeMap::new();
    for n in &scene.nodes {
        let name = n.name.to_string();
        if node_index.contains_key(&name) {
            // Duplicate names (e.g. DefaultView) are irrelevant to the game.
            continue;
        }
        let kind = match n.node_type {
            W3dNodeType::Model => "model",
            W3dNodeType::Group => "group",
            W3dNodeType::Light => "light",
            W3dNodeType::View => "camera",
        };
        let mut extras = json!({"w3d_type": kind, "visibility": n.visibility});
        let mut mesh = None;
        if n.node_type == W3dNodeType::Model {
            let res_name = if !n.model_resource_name.is_empty() { n.model_resource_name.to_string() } else { n.resource_name.to_string() };
            let clod = scene.clod_meshes.iter().find(|(k, _)| k.to_string() == res_name).map(|(_, v)| v);
            if let Some(meshes) = clod {
                let shaders = mesh_shaders(scene, &name, &res_name, meshes.len(), &n.shader_name.to_string());
                let lm = opts.lightmaps.get(&name.to_lowercase());
                let mut prims = vec![];
                for (i, m) in meshes.iter().enumerate() {
                    if m.faces.is_empty() || m.positions.is_empty() {
                        continue;
                    }
                    let uv0 = m.tex_coords.first().filter(|t| t.len() == m.positions.len()).map(|t| t.iter().map(flip_v).collect());
                    let uv1 = lm.and_then(|lm| lm.uvs.get(i)).and_then(|uvs| {
                        if uvs.len() == m.positions.len() {
                            Some(uvs.iter().map(flip_v).collect::<Vec<_>>())
                        } else {
                            report.lightmap_mismatches.push(format!("{name} mesh {i}: {} uvs vs {} verts", uvs.len(), m.positions.len()));
                            None
                        }
                    });
                    if uv1.is_some() {
                        report.lightmapped_meshes += 1;
                    }
                    let normals = if m.normals.len() == m.positions.len() { m.normals.clone() } else { vec![] };
                    let material = get_material(&mut doc, &shaders[i], n.visibility == 3);
                    prims.push(Primitive {
                        positions: m.positions.clone(),
                        normals,
                        uv0,
                        uv1,
                        indices: m.faces.iter().flat_map(|f| [f[0], f[1], f[2]]).collect(),
                        material,
                    });
                }
                if !prims.is_empty() {
                    doc.meshes.push((name.clone(), prims));
                    mesh = Some(doc.meshes.len() - 1);
                    report.models += 1;
                }
                extras["resource"] = json!(res_name);
                extras["shaders"] = json!(shaders);
                if let Some(lm) = lm {
                    extras["lightmap"] = json!(lm.lightmap);
                }
            }
        }
        let idx = doc.nodes.len();
        doc.nodes.push(Node { name: name.clone(), matrix: Some(n.transform), mesh, children: vec![], extras: Some(extras) });
        node_index.insert(name, idx);
    }
    for n in &scene.nodes {
        let name = n.name.to_string();
        let Some(&idx) = node_index.get(&name) else { continue };
        let parent = n.parent_name.to_string();
        let parent_idx = if parent.is_empty() || parent.eq_ignore_ascii_case("world") { Some(root) } else { node_index.get(&parent).copied() };
        let p = parent_idx.unwrap_or(root);
        if p != idx && !doc.nodes[p].children.contains(&idx) {
            doc.nodes[p].children.push(idx);
        }
    }

    // Lights as data (the port rebuilds lighting itself): resource colours
    // plus each light node's world matrix (Director space). Directional and
    // spot lights shine down their node's -Z axis.
    let worlds = vm_rust::director::chunks::w3d::skeleton::build_node_world_matrices(&scene.nodes);
    let mut lights = vec![];
    for n in scene.nodes.iter().filter(|n| n.node_type == W3dNodeType::Light) {
        let res = if !n.resource_name.is_empty() { n.resource_name.to_string() } else { n.name.to_string() };
        let Some(l) = scene.lights.iter().find(|l| l.name.to_string() == res || l.name == n.name) else { continue };
        let m = worlds.iter().find(|(k, _)| **k == n.name).map(|(_, m)| m.to_vec());
        lights.push(json!({
            "node": n.name.to_string(), "name": l.name.to_string(),
            "type": format!("{:?}", l.light_type).to_lowercase(), "color": l.color.to_vec(),
            "attenuation": l.attenuation.to_vec(), "spot_angle": l.spot_angle, "enabled": l.enabled,
            "specular": l.specular, "world_matrix_director": m,
        }));
    }
    doc.extras = Some(json!({"lights": lights, "world_scale": WORLD_SCALE}));
    let side = json!({"lights": lights});
    std::fs::create_dir_all(out_dir).map_err(|e| e.to_string())?;
    std::fs::write(out_dir.join(glb_name.replace(".glb", "_lights.json")), serde_json::to_vec_pretty(&side).unwrap()).map_err(|e| e.to_string())?;

    std::fs::create_dir_all(out_dir).map_err(|e| e.to_string())?;
    std::fs::write(out_dir.join(glb_name), doc.to_glb()).map_err(|e| e.to_string())?;
    Ok(report)
}

pub fn sanitize(s: &str) -> String {
    s.chars().map(|c| if c.is_ascii_alphanumeric() || c == '-' || c == '_' { c } else { '_' }).collect()
}
