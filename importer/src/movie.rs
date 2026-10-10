// Loads game.dcr (and its external cast GUILibrary.cct, found next to it)
// with DirPlayer's Director reader and writes the converted asset tree:
//
//   manifest.json            what was converted, counts, format version
//   scenes/world.glb         the playfield (models, lightmaps on UV2)
//   scenes/car.glb           the RC car ("holypolycrap"), body + 4 wheels
//   scenes/{prize,star,arrow}.glb
//   scenes/<scene>_textures/ W3D textures as PNG
//   images/<cast>/<name>.png 2D art (menus, help, car shadow)
//   lightmaps/<name>.png     baked lightmaps for the world
//   images.json              registration points and sizes for 2D art
//   audio/<name>.mp3         sound effects and music
//   sounds.json              sound metadata
//   physics/collision.json   Havok collision world + car hull
//   text.json                text members

use std::collections::HashMap;
use std::path::Path;

use serde_json::json;
use vm_rust::director::file::get_variable_multiplier;
use vm_rust::director::lingo::decompiler::decompile_handler;
use vm_rust::player::cast_lib::CastMemberRef;
use vm_rust::player::cast_member::CastMemberType;
use vm_rust::player::testing::{run_test, TestPlayer};
use vm_rust::player::testing_shared::TestHarness;

use crate::lingo_literal::{self, Value};
use crate::scene::{self, sanitize, ModelLightmap, SceneOptions};
use crate::{audio, physics};

pub const FORMAT_VERSION: u32 = 1;

fn find_lightmap_uvs(player: &vm_rust::player::DirPlayer) -> Result<HashMap<String, ModelLightmap>, String> {
    for cast in &player.movie.cast_manager.casts {
        for (&num, m) in &cast.members {
            let CastMemberType::Script(_) = &m.member_type else { continue };
            if !m.name.eq_ignore_ascii_case("UV_Lightmap_UVs") {
                continue;
            }
            let mref = CastMemberRef { cast_lib: cast.number as i32, cast_member: num as i32 };
            let script = player.movie.cast_manager.get_script_by_ref(&mref).ok_or("lightmap script not loaded")?;
            let lctx = cast.lctx.as_ref().ok_or("no script context")?;
            let mult = get_variable_multiplier(cast.capital_x, cast.dir_version);
            for h in &script.chunk.handlers {
                let d = decompile_handler(h, &script.chunk, lctx, cast.dir_version, mult);
                if !d.name.eq_ignore_ascii_case("getUVs_UV_Lightmap") {
                    continue;
                }
                let line = d.lines.iter().find(|l| l.text.trim_start().starts_with("return ")).ok_or("no return in getUVs")?;
                let literal = line.text.trim_start().trim_start_matches("return ").trim();
                let Value::Props(models) = lingo_literal::parse(literal)? else {
                    return Err("lightmap data is not a property list".into());
                };
                let mut out = HashMap::new();
                for (k, v) in models {
                    let Some(name) = k.key_name() else { continue };
                    let lightmap = match v.get("lightmap") {
                        Some(Value::Str(s)) => s.clone(),
                        Some(Value::Num(n)) => format!("{:03}", *n as i64),
                        _ => continue,
                    };
                    let mut uvs = vec![];
                    if let Some(meshes) = v.get("uvs").and_then(|u| u.as_list()) {
                        for mesh in meshes {
                            let list = mesh.as_list().cloned().unwrap_or_default();
                            uvs.push(
                                list.iter()
                                    .map(|p| match p.as_list().map(|l| l.as_slice()) {
                                        Some([u, v, ..]) => [u.as_num().unwrap_or(0.0) as f32, v.as_num().unwrap_or(0.0) as f32],
                                        _ => [0.0, 0.0],
                                    })
                                    .collect(),
                            );
                        }
                    }
                    out.insert(name.to_lowercase(), ModelLightmap { lightmap, uvs });
                }
                return Ok(out);
            }
        }
    }
    Err("UV_Lightmap_UVs script not found".into())
}

pub fn run(input: &Path, out: &Path, verbose: bool) -> Result<(), String> {
    let movie = std::fs::canonicalize(input).map_err(|e| format!("{}: {e}", input.display()))?;
    let movie = movie.to_string_lossy().to_string();
    std::fs::create_dir_all(out).map_err(|e| e.to_string())?;
    let out = out.to_path_buf();
    let mut result: Result<(), String> = Err("not run".into());
    run_test(async {
        let mut p = TestPlayer::new();
        p.load_movie(&movie).await;
        let player = unsafe { vm_rust::player::player_mut() };
        result = convert_all(player, &out, verbose);
        std::mem::forget(p);
    });
    result
}

fn convert_all(player: &vm_rust::player::DirPlayer, out: &Path, verbose: bool) -> Result<(), String> {
    let mut manifest = json!({"format_version": FORMAT_VERSION, "game": "LEGO Supersonic RC", "source": "game.dcr"});
    let lightmaps = find_lightmap_uvs(player)?;
    let no_lightmaps = HashMap::new();
    let palettes = player.movie.cast_manager.palettes();

    let mut images = serde_json::Map::new();
    let mut sounds = serde_json::Map::new();
    let mut texts = serde_json::Map::new();
    let mut scenes = serde_json::Map::new();
    let mut world_scene = None;
    let mut hke = None;

    for cast in &player.movie.cast_manager.casts {
        let cast_dir = sanitize(&cast.name).to_lowercase();
        let mut nums: Vec<_> = cast.members.keys().copied().collect();
        nums.sort();
        for n in nums {
            let m = &cast.members[&n];
            match &m.member_type {
                CastMemberType::Bitmap(b) => {
                    let Some(bmp) = player.bitmap_manager.get_bitmap(b.image_ref) else { continue };
                    let (w, h) = (bmp.width as u32, bmp.height as u32);
                    if w == 0 || h == 0 || m.name.is_empty() {
                        continue;
                    }
                    let mut px = vec![0u8; (w * h * 4) as usize];
                    for y in 0..h {
                        for x in 0..w {
                            let i = ((y * w + x) * 4) as usize;
                            let (r, g, bb) = bmp.get_pixel_color(&palettes, x as u16, y as u16);
                            px[i] = r;
                            px[i + 1] = g;
                            px[i + 2] = bb;
                            px[i + 3] = if bmp.bit_depth == 32 && bmp.use_alpha { bmp.data[i + 3] } else { 255 };
                        }
                    }
                    let sub = if m.name.ends_with("LightingMap") { "lightmaps".to_string() } else { format!("images/{cast_dir}") };
                    let rel = format!("{sub}/{}.png", sanitize(&m.name));
                    let path = out.join(&rel);
                    std::fs::create_dir_all(path.parent().unwrap()).map_err(|e| e.to_string())?;
                    image::save_buffer(&path, &px, w, h, image::ColorType::Rgba8).map_err(|e| e.to_string())?;
                    images.insert(
                        m.name.clone(),
                        json!({"file": rel, "cast": cast.name, "member": n, "width": w, "height": h, "reg": [b.reg_point.0, b.reg_point.1], "alpha": bmp.use_alpha}),
                    );
                }
                CastMemberType::Sound(s) => {
                    let base = out.join("audio").join(sanitize(&m.name));
                    let kind = audio::write_sound(&s.sound.data(), || s.sound.to_wav(), &base)?;
                    sounds.insert(
                        m.name.clone(),
                        json!({"file": format!("audio/{}.{kind}", sanitize(&m.name)), "loop": s.info.loop_enabled, "rate": s.sound.sample_rate()}),
                    );
                }
                CastMemberType::Text(t) => {
                    texts.insert(m.name.clone(), json!({"text": t.text, "font": t.font, "size": t.font_size}));
                }
                CastMemberType::Field(f) => {
                    texts.insert(m.name.clone(), json!({"text": f.text, "font": f.font, "size": f.font_size}));
                }
                CastMemberType::Shockwave3d(w) => {
                    let Some(sc) = &w.parsed_scene else { continue };
                    let file = match m.name.to_lowercase().as_str() {
                        "3d" => "world".to_string(),
                        "holypolycrap" => "car".to_string(),
                        "arrow2" => "arrow".to_string(),
                        other => sanitize(other),
                    };
                    let lm = if file == "world" { &lightmaps } else { &no_lightmaps };
                    let flip_v = std::env::var("SSRC_IMPORT_NOFLIP").is_err();
                    let opts = SceneOptions { lightmaps: lm, texture_dir_name: format!("{file}_textures"), flip_v };
                    let rep = scene::convert(sc, &out.join("scenes"), &format!("{file}.glb"), &opts)?;
                    for mm in &rep.lightmap_mismatches {
                        eprintln!("warning: lightmap uv mismatch: {mm}");
                    }
                    scenes.insert(
                        file.clone(),
                        json!({"file": format!("scenes/{file}.glb"), "member": m.name, "models": rep.models, "textures": rep.textures, "lightmapped_meshes": rep.lightmapped_meshes}),
                    );
                    if file == "world" {
                        world_scene = Some(sc.clone());
                    }
                }
                CastMemberType::HavokPhysics(h) => {
                    hke = Some(h.state.hke_data.clone());
                }
                _ => {}
            }
        }
    }

    let world = world_scene.ok_or("3D world member '3d' not found")?;
    let hke = hke.ok_or("Havok member not found")?;
    let phys = physics::convert(&hke, &world, &out.join("physics"))?;

    manifest["scenes"] = json!(scenes);
    manifest["sounds"] = json!(sounds.len());
    manifest["images"] = json!(images.len());
    manifest["lightmapped_models"] = json!(lightmaps.len());
    manifest["physics"] = json!({"file": "physics/collision.json", "bodies": phys.bodies, "triangles": phys.triangles});
    manifest["stage"] = json!({"width": player.movie.rect.width(), "height": player.movie.rect.height()});
    // The movie's frame rate and the score's tempo channel: the game's
    // physics step depends on the frame rate (drivingManager.update).
    let tempos: Vec<serde_json::Value> = player.movie.score.tempo_channel_data.iter()
        .map(|(frame_idx, td)| json!({"frame": frame_idx + 1, "tempo": td.tempo, "fps_or_delay": td.tempo_cue_point}))
        .collect();
    manifest["tempo"] = json!({"movie_frame_rate": player.movie.frame_rate, "score_tempo_changes": tempos});
    write_json(&out.join("images.json"), &json!(images))?;
    write_json(&out.join("sounds.json"), &json!(sounds))?;
    write_json(&out.join("text.json"), &json!(texts))?;
    write_json(&out.join("manifest.json"), &manifest)?;
    if verbose {
        println!("{}", serde_json::to_string_pretty(&manifest).unwrap());
    }
    Ok(())
}

fn write_json(path: &Path, v: &serde_json::Value) -> Result<(), String> {
    std::fs::write(path, serde_json::to_vec_pretty(v).unwrap()).map_err(|e| format!("{}: {e}", path.display()))
}
