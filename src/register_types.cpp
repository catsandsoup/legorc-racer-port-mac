#include "driving.h"
#include "game.h"
#include "sound.h"

#include <gdextension_interface.h>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

using namespace godot;

static void initialize_supersonic(ModuleInitializationLevel level) {
	if (level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	GDREGISTER_CLASS(ssrc::SoundSystem);
	GDREGISTER_CLASS(ssrc::DrivingSession);
	GDREGISTER_CLASS(ssrc::SupersonicGame);
}

static void uninitialize_supersonic(ModuleInitializationLevel level) {
	(void)level;
}

extern "C" {
GDExtensionBool GDE_EXPORT supersonic_library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address,
		const GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization) {
	GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);
	init_obj.register_initializer(initialize_supersonic);
	init_obj.register_terminator(uninitialize_supersonic);
	init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
	return init_obj.init();
}
}
