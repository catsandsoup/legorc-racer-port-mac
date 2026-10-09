#!/usr/bin/env python
# Builds the game library (GDExtension) for the current platform:
#   scons platform=macos arch=arm64 target=template_release
import os

env = SConscript("third_party/godot-cpp/SConstruct")
env.Append(CPPPATH=["src/"])
sources = Glob("src/*.cpp")

if env["platform"] == "macos":
    library = env.SharedLibrary(
        "game/bin/libsupersonic.{}.{}.framework/libsupersonic.{}.{}".format(
            env["platform"], env["target"], env["platform"], env["target"]
        ),
        source=sources,
    )
else:
    library = env.SharedLibrary(
        "game/bin/libsupersonic{}{}".format(env["suffix"], env["SHLIBSUFFIX"]),
        source=sources,
    )

Default(library)
