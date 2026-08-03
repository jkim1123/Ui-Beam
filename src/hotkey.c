#include <obs-module.h>
#include <obs-frontend-api.h>

#include "output.h"

static obs_hotkey_id dump_hotkey;
static void dump_pressed(void* data, obs_hotkey_id id, obs_hotkey_t* hotkey, bool pressed) {
	if (!pressed)
		return;
	blog(LOG_INFO, "Dumping buffer data");
	flush_buffer();
}

void register_hotkeys(void)
{
	dump_hotkey = obs_hotkey_register_frontend("ui_beam.dump", "UI-Beam Dump Button", dump_pressed, NULL);
}