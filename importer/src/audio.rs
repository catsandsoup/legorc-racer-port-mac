// Sounds. Every sound in Supersonic RC is Shockwave Audio, i.e. MPEG-2
// Layer III at 16 kHz mono, sometimes behind a 4-byte header. We locate the
// first valid frame sync and write a plain .mp3; anything else is written as
// PCM WAV.

use std::path::Path;

fn mp3_start(data: &[u8]) -> Option<usize> {
    (0..data.len().min(64).saturating_sub(1)).find(|&i| {
        data[i] == 0xFF && (data[i + 1] & 0xE0) == 0xE0 && (data[i + 1] & 0x06) != 0 && i + 2 < data.len() && (data[i + 2] & 0xF0) != 0xF0
    })
}

pub fn write_sound(raw: &[u8], wav: impl FnOnce() -> Vec<u8>, out_base: &Path) -> Result<String, String> {
    if let Some(dir) = out_base.parent() {
        std::fs::create_dir_all(dir).map_err(|e| e.to_string())?;
    }
    if let Some(start) = mp3_start(raw) {
        let p = out_base.with_extension("mp3");
        std::fs::write(&p, &raw[start..]).map_err(|e| e.to_string())?;
        Ok("mp3".into())
    } else {
        let p = out_base.with_extension("wav");
        std::fs::write(&p, wav()).map_err(|e| e.to_string())?;
        Ok("wav".into())
    }
}
