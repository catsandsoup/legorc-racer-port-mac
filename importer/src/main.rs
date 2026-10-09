// ssrc-import: converts the player's own copy of LEGO Supersonic RC
// (game.dcr, a Shockwave/Director 8.5 movie) into open formats that the
// native port loads: glTF scenes, PNG images, MP3 audio and JSON data.
//
// Parsing is done by DirPlayer's Director/W3D/HKE readers (GPL-3.0).

mod audio;
mod gltf;
mod lingo_literal;
mod movie;
mod physics;
mod scene;
mod textures;

use std::path::PathBuf;

fn main() {
    let args: Vec<String> = std::env::args().collect();
    if args.len() < 3 {
        eprintln!("usage: ssrc-import <game.dcr> <output-dir> [--verbose]");
        std::process::exit(2);
    }
    let input = PathBuf::from(&args[1]);
    let out = PathBuf::from(&args[2]);
    let verbose = args.iter().any(|a| a == "--verbose");
    match movie::run(&input, &out, verbose) {
        Ok(()) => println!("ssrc-import: done -> {}", out.display()),
        Err(e) => {
            eprintln!("ssrc-import: error: {e}");
            std::process::exit(1);
        }
    }
}
