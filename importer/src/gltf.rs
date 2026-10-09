// Minimal glTF 2.0 (.glb) writer: nodes, meshes, materials, external
// PNG textures. Enough for the converted Supersonic RC scenes, and plain
// enough that any DCC tool (Blender etc.) can open and re-export the files.

use serde_json::{json, Value};

pub struct Primitive {
    pub positions: Vec<[f32; 3]>,
    pub normals: Vec<[f32; 3]>,
    pub uv0: Option<Vec<[f32; 2]>>,
    pub uv1: Option<Vec<[f32; 2]>>,
    pub indices: Vec<u32>,
    pub material: Option<usize>,
}

pub struct Node {
    pub name: String,
    pub matrix: Option<[f32; 16]>,
    pub mesh: Option<usize>,
    pub children: Vec<usize>,
    pub extras: Option<Value>,
}

#[derive(Default)]
pub struct Material {
    pub name: String,
    pub base_color: [f32; 4],
    pub base_texture: Option<usize>,
    pub emissive: [f32; 3],
    pub alpha_blend: bool,
    pub alpha_mask: bool,
    pub double_sided: bool,
    pub unlit: bool,
    pub extras: Option<Value>,
}

#[derive(Default)]
pub struct Document {
    pub nodes: Vec<Node>,
    pub roots: Vec<usize>,
    pub meshes: Vec<(String, Vec<Primitive>)>,
    pub materials: Vec<Material>,
    /// Image URIs relative to the .glb file.
    pub images: Vec<String>,
    pub extras: Option<Value>,
}

struct Bin {
    data: Vec<u8>,
    views: Vec<Value>,
    accessors: Vec<Value>,
}

impl Bin {
    fn align(&mut self) {
        while self.data.len() % 4 != 0 {
            self.data.push(0);
        }
    }

    fn view(&mut self, bytes: &[u8], target: Option<u32>) -> usize {
        self.align();
        let offset = self.data.len();
        self.data.extend_from_slice(bytes);
        let mut v = json!({"buffer": 0, "byteOffset": offset, "byteLength": bytes.len()});
        if let Some(t) = target {
            v["target"] = json!(t);
        }
        self.views.push(v);
        self.views.len() - 1
    }

    fn vec_f32<const N: usize>(&mut self, items: &[[f32; N]], ty: &str, bounds: bool) -> usize {
        let mut bytes = Vec::with_capacity(items.len() * N * 4);
        for it in items {
            for c in it {
                bytes.extend_from_slice(&c.to_le_bytes());
            }
        }
        let view = self.view(&bytes, Some(34962));
        let mut acc = json!({"bufferView": view, "componentType": 5126, "count": items.len(), "type": ty});
        if bounds && !items.is_empty() {
            let mut mn = [f32::MAX; N];
            let mut mx = [f32::MIN; N];
            for it in items {
                for k in 0..N {
                    mn[k] = mn[k].min(it[k]);
                    mx[k] = mx[k].max(it[k]);
                }
            }
            acc["min"] = json!(mn.to_vec());
            acc["max"] = json!(mx.to_vec());
        }
        self.accessors.push(acc);
        self.accessors.len() - 1
    }

    fn indices(&mut self, idx: &[u32]) -> usize {
        let mut bytes = Vec::with_capacity(idx.len() * 4);
        for i in idx {
            bytes.extend_from_slice(&i.to_le_bytes());
        }
        let view = self.view(&bytes, Some(34963));
        self.accessors.push(json!({"bufferView": view, "componentType": 5125, "count": idx.len(), "type": "SCALAR"}));
        self.accessors.len() - 1
    }
}

impl Document {
    pub fn to_glb(&self) -> Vec<u8> {
        let mut bin = Bin { data: vec![], views: vec![], accessors: vec![] };

        let meshes: Vec<Value> = self
            .meshes
            .iter()
            .map(|(name, prims)| {
                let prims: Vec<Value> = prims
                    .iter()
                    .map(|p| {
                        let mut attrs = serde_json::Map::new();
                        attrs.insert("POSITION".into(), json!(bin.vec_f32(&p.positions, "VEC3", true)));
                        if p.normals.len() == p.positions.len() {
                            attrs.insert("NORMAL".into(), json!(bin.vec_f32(&p.normals, "VEC3", false)));
                        }
                        if let Some(uv) = &p.uv0 {
                            attrs.insert("TEXCOORD_0".into(), json!(bin.vec_f32(uv, "VEC2", false)));
                        }
                        if let Some(uv) = &p.uv1 {
                            attrs.insert("TEXCOORD_1".into(), json!(bin.vec_f32(uv, "VEC2", false)));
                        }
                        let mut v = json!({"attributes": attrs, "indices": bin.indices(&p.indices), "mode": 4});
                        if let Some(m) = p.material {
                            v["material"] = json!(m);
                        }
                        v
                    })
                    .collect();
                json!({"name": name, "primitives": prims})
            })
            .collect();

        let nodes: Vec<Value> = self
            .nodes
            .iter()
            .map(|n| {
                let mut v = json!({"name": n.name});
                if let Some(m) = n.matrix {
                    v["matrix"] = json!(m.to_vec());
                }
                if let Some(mesh) = n.mesh {
                    v["mesh"] = json!(mesh);
                }
                if !n.children.is_empty() {
                    v["children"] = json!(n.children);
                }
                if let Some(e) = &n.extras {
                    v["extras"] = e.clone();
                }
                v
            })
            .collect();

        let mut uses_unlit = false;
        let materials: Vec<Value> = self
            .materials
            .iter()
            .map(|m| {
                let mut pbr = json!({"baseColorFactor": m.base_color.to_vec(), "metallicFactor": 0.0, "roughnessFactor": 1.0});
                if let Some(t) = m.base_texture {
                    pbr["baseColorTexture"] = json!({"index": t, "texCoord": 0});
                }
                let mut v = json!({"name": m.name, "pbrMetallicRoughness": pbr, "emissiveFactor": m.emissive.to_vec(), "doubleSided": m.double_sided});
                if m.alpha_blend {
                    v["alphaMode"] = json!("BLEND");
                } else if m.alpha_mask {
                    v["alphaMode"] = json!("MASK");
                    v["alphaCutoff"] = json!(0.5);
                }
                if m.unlit {
                    uses_unlit = true;
                    v["extensions"] = json!({"KHR_materials_unlit": {}});
                }
                if let Some(e) = &m.extras {
                    v["extras"] = e.clone();
                }
                v
            })
            .collect();

        let images: Vec<Value> = self.images.iter().map(|u| json!({"uri": u})).collect();
        let textures: Vec<Value> = (0..self.images.len()).map(|i| json!({"source": i, "sampler": 0})).collect();

        bin.align();
        let mut root = json!({
            "asset": {"version": "2.0", "generator": "ssrc-import (Supersonic RC native port)"},
            "scene": 0,
            "scenes": [{"nodes": self.roots}],
            "nodes": nodes,
            "meshes": meshes,
            "materials": materials,
            "accessors": bin.accessors,
            "bufferViews": bin.views,
            "buffers": [{"byteLength": bin.data.len()}],
        });
        if !images.is_empty() {
            root["images"] = json!(images);
            root["textures"] = json!(textures);
            root["samplers"] = json!([{"magFilter": 9729, "minFilter": 9987, "wrapS": 10497, "wrapT": 10497}]);
        }
        if uses_unlit {
            root["extensionsUsed"] = json!(["KHR_materials_unlit"]);
        }
        if let Some(e) = &self.extras {
            root["extras"] = e.clone();
        }

        let mut js = serde_json::to_vec(&root).unwrap();
        while js.len() % 4 != 0 {
            js.push(b' ');
        }
        let total = 12 + 8 + js.len() + 8 + bin.data.len();
        let mut out = Vec::with_capacity(total);
        out.extend_from_slice(b"glTF");
        out.extend_from_slice(&2u32.to_le_bytes());
        out.extend_from_slice(&(total as u32).to_le_bytes());
        out.extend_from_slice(&(js.len() as u32).to_le_bytes());
        out.extend_from_slice(b"JSON");
        out.extend_from_slice(&js);
        out.extend_from_slice(&(bin.data.len() as u32).to_le_bytes());
        out.extend_from_slice(b"BIN\0");
        out.extend_from_slice(&bin.data);
        out
    }
}
