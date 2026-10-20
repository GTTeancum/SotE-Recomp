#pragma once
namespace sote::menu_renderer {
// Called before RT64::Application::setup. Resources belong to its render hooks.
void install();
void game_display_list_ready();
}
