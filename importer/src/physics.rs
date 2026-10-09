// Havok (.hke) collision world -> JSON triangle soups in Godot space.
//
// Supersonic RC's Havok file holds 84 fixed bodies, each a triangle mesh in
// metres local to its body. Like the original Xtra (and DirPlayer), each mesh
// is placed with the matching W3D model's world transform. The player car is
// not in the HKE: the game builds it at runtime from the W3D model "Car"
// (makeMovableRigidBody("car", mass 3, convex)), so we export that hull too.

use std::path::Path;

use serde_json::json;
use vm_rust::director::chunks::w3d::skeleton::build_node_world_matrices;
use vm_rust::director::chunks::w3d::types::W3dScene;
use vm_rust::player::handlers::datum_handlers::cast_member::hke_parser::{parse_hke, HkePrimitiveKind};

use crate::scene::WORLD_SCALE;

fn to_godot(p: [f64; 3]) -> [f32; 3] {
    let s = WORLD_SCALE as f64;
    [(p[0] * s) as f32, (p[2] * s) as f32, (-p[1] * s) as f32]
}

fn xform(m: &[f32; 16], v: [f64; 3], normalize_axes: bool) -> [f64; 3] {
    let mut c = [[m[0] as f64, m[1] as f64, m[2] as f64], [m[4] as f64, m[5] as f64, m[6] as f64], [m[8] as f64, m[9] as f64, m[10] as f64]];
    if normalize_axes {
        for col in c.iter_mut() {
            let n = (col[0] * col[0] + col[1] * col[1] + col[2] * col[2]).sqrt();
            if n > 1e-9 {
                for k in col.iter_mut() {
                    *k /= n;
                }
            }
        }
    }
    [
        c[0][0] * v[0] + c[1][0] * v[1] + c[2][0] * v[2] + m[12] as f64,
        c[0][1] * v[0] + c[1][1] * v[1] + c[2][1] * v[2] + m[13] as f64,
        c[0][2] * v[0] + c[1][2] * v[1] + c[2][2] * v[2] + m[14] as f64,
    ]
}

pub struct PhysicsReport {
    pub bodies: usize,
    pub triangles: usize,
}

pub fn convert(hke_data: &[u8], world: &W3dScene, out_dir: &Path) -> Result<PhysicsReport, String> {
    let hke = parse_hke(hke_data);
    let inv_scale = if hke.world_scale.abs() > 1e-9 { 1.0 / hke.world_scale as f64 } else { 1.0 / WORLD_SCALE as f64 };
    let worlds = build_node_world_matrices(&world.nodes);
    let mut bodies = vec![];
    let mut tri_total = 0;
    for mesh in &hke.meshes {
        if mesh.vertices.is_empty() || mesh.triangles.is_empty() {
            continue;
        }
        let name = mesh.name.to_string();
        let props = hke.bodies.iter().find(|b| b.name.to_string().eq_ignore_ascii_case(&name));
        let offset = props
            .and_then(|p| {
                p.primitives.iter().find_map(|pr| match &pr.kind {
                    HkePrimitiveKind::Mesh { mesh_name } if mesh_name.eq_ignore_ascii_case(&name) => Some(pr.local_translation),
                    _ => None,
                })
            })
            .map(|t| [t[0] as f64 * inv_scale, t[1] as f64 * inv_scale, t[2] as f64 * inv_scale])
            .unwrap_or([0.0; 3]);
        let model = worlds.iter().find(|(k, _)| k.to_string().eq_ignore_ascii_case(&name)).map(|(_, m)| *m);
        let mut verts = Vec::with_capacity(mesh.vertices.len() * 3);
        for v in &mesh.vertices {
            let local = [v[0] as f64 * inv_scale + offset[0], v[1] as f64 * inv_scale + offset[1], v[2] as f64 * inv_scale + offset[2]];
            let w = match &model {
                Some(m) => xform(m, local, true),
                None => local,
            };
            verts.extend_from_slice(&to_godot(w));
        }
        // Godot's ConcavePolygonShape3D wants a flat triangle list.
        let mut faces = Vec::with_capacity(mesh.triangles.len() * 9);
        for t in &mesh.triangles {
            for &i in t {
                let i = i as usize * 3;
                faces.extend_from_slice(&verts[i..i + 3]);
            }
        }
        tri_total += mesh.triangles.len();
        bodies.push(json!({
            "name": name,
            "placed_by_model": model.is_some(),
            "friction": props.and_then(|p| p.dynamic_friction).unwrap_or(0.3),
            "restitution": props.and_then(|p| p.restitution).unwrap_or(0.3),
            "faces": faces,
        }));
    }

    // Car hull from the W3D "Car" box model, in the model's local frame.
    let car = world.nodes.iter().find(|n| n.name.to_string().eq_ignore_ascii_case("car"));
    let mut car_json = json!(null);
    if let Some(car) = car {
        let res = if !car.model_resource_name.is_empty() { car.model_resource_name } else { car.resource_name };
        if let Some((_, meshes)) = world.clod_meshes.iter().find(|(k, _)| **k == res) {
            let pts: Vec<[f32; 3]> = meshes.iter().flat_map(|m| m.positions.iter().copied()).collect();
            car_json = json!({
                "model_world_matrix_director": worlds.iter().find(|(k, _)| **k == car.name).map(|(_, m)| m.to_vec()),
                "hull_points_local_director": pts.iter().map(|p| p.to_vec()).collect::<Vec<_>>(),
            });
        }
    }

    let out = json!({
        "source": "havok.hke",
        "world_name": hke.world_name,
        "hke_world_scale": hke.world_scale,
        "hke_gravity": hke.gravity.map(|g| g.to_vec()),
        "hke_tolerance": hke.tolerance,
        "bodies": bodies,
        "car": car_json,
    });
    std::fs::create_dir_all(out_dir).map_err(|e| e.to_string())?;
    std::fs::write(out_dir.join("collision.json"), serde_json::to_vec(&out).unwrap()).map_err(|e| e.to_string())?;
    Ok(PhysicsReport { bodies: hke.meshes.len(), triangles: tri_total })
}
