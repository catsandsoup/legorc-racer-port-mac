// Texture decoding. W3D textures are JPEG or PNG; textures authored with an
// alpha channel are stored as an RGB JPEG followed by a block
//   [width u32][height u32][zlibLen u32][zlib 8-bit alpha, bottom-up rows]
// (format notes from DirPlayer's WebGL2 renderer, GPL-3.0).

use std::io::Read;
use std::path::Path;

pub struct Rgba {
    pub w: u32,
    pub h: u32,
    pub px: Vec<u8>,
}

impl Rgba {
    pub fn has_alpha(&self) -> bool {
        self.px.chunks_exact(4).any(|p| p[3] < 250)
    }

    pub fn save_png(&self, path: &Path) -> Result<(), String> {
        if let Some(dir) = path.parent() {
            std::fs::create_dir_all(dir).map_err(|e| e.to_string())?;
        }
        image::save_buffer(path, &self.px, self.w, self.h, image::ColorType::Rgba8)
            .map_err(|e| format!("{}: {e}", path.display()))
    }
}

pub fn decode_w3d_texture(data: &[u8]) -> Option<Rgba> {
    if data.len() < 4 {
        return None;
    }
    let is_jpeg = data[0] == 0xFF && data[1] == 0xD8;
    let is_png = data[0] == 0x89 && data[1] == 0x50;
    if !is_jpeg && !is_png {
        return None;
    }
    let img = image::load_from_memory(data).ok()?.to_rgba8();
    let (w, h) = (img.width(), img.height());
    let mut px = img.into_raw();
    if is_jpeg {
        if let Some(eoi) = data.windows(2).position(|b| b[0] == 0xFF && b[1] == 0xD9) {
            let tail = &data[eoi + 2..];
            if tail.len() >= 14 {
                let aw = u32::from_le_bytes([tail[0], tail[1], tail[2], tail[3]]);
                let ah = u32::from_le_bytes([tail[4], tail[5], tail[6], tail[7]]);
                let alen = u32::from_le_bytes([tail[8], tail[9], tail[10], tail[11]]) as usize;
                if aw == w && ah == h && tail[12] == 0x78 && tail.len() >= 12 + alen {
                    let mut alpha = Vec::new();
                    let mut dec = flate2::read::ZlibDecoder::new(&tail[12..12 + alen]);
                    if dec.read_to_end(&mut alpha).is_ok() && alpha.len() >= (w * h) as usize {
                        let (wu, hu) = (w as usize, h as usize);
                        for y in 0..hu {
                            let src = (hu - 1 - y) * wu;
                            for x in 0..wu {
                                px[(y * wu + x) * 4 + 3] = alpha[src + x];
                            }
                        }
                    }
                }
            }
        }
    }
    Some(Rgba { w, h, px })
}
