#pragma once

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace sote::menu_skin { struct Snapshot; }

namespace sote::control_bindings {
// The native window uses Win32 virtual-key identities. Controller identities
// match SDL_GameControllerButton/Axis; no SDL object escapes the input thread.
enum class Kind : uint8_t { None, Key, Button, Axis };
enum class Device : uint8_t { Keyboard, Controller };
enum class Context : int { OnFoot, Snowspeeder, Turret, Bike, Outrider, Count };
struct Token {
    Kind kind = Kind::None;
    int code = 0;
    int sign = 1;
    auto operator<=>(const Token&) const = default;
};
Token key(int vk);
Token button(int sdl_button);
Token axis(int sdl_axis, int direction = 1);
std::string token_name(Token token);
std::string token_text(Token token);
bool parse_token(const std::string& text, Token& token);

struct PhysicalInput {
    std::array<uint8_t, 256> keys{};
    std::array<uint8_t, 32> buttons{};
    std::array<int16_t, 6> axes{};
    bool connected = false;
    int32_t instance = -1;
};
float value(const PhysicalInput& input, Token token);
bool neutral(const PhysicalInput& input, Device device);

struct NativeTable {
    int preset = 0;
    std::array<uint16_t, 48> masks{};
    std::array<std::string, 48> labels{};
    bool modern_foot = false;
    bool modern_bike = false;
};
struct Action {
    std::string id, label;
    Context context = Context::OnFoot;
    std::array<std::vector<Token>, 2> targets;
};
using Layout = std::array<std::vector<Action>, 5>;
// Each row contains only routes with identical native effects. Shared native
// actions (e.g. Jump / Thrust) are shown together rather than falsely separated.
Layout make_layout(const NativeTable& table);
std::string storage_key(const Action& action, Device device);

class Assignments {
public:
    std::map<std::string, Token> overrides;
    bool load(const std::filesystem::path& path, std::string& error);
    bool save(const std::filesystem::path& path, std::string& error) const;
    std::vector<Token> sources(const Action& action, Device device) const;
    std::vector<std::string> conflicts(const std::vector<Action>& context,
        const Action& action, Device device, Token proposed) const;
    PhysicalInput apply(const PhysicalInput& raw,
        const std::vector<Action>& context) const;
};

enum class Phase : int { Browse, Release, Capture, ConflictRelease, Conflict,
    DefaultsRelease, Defaults, Settle };
struct EditorView {
    bool visible = false, editing = false;
    int group = 0, row = 0, column = 0, scroll = 0;
    Phase phase = Phase::Browse;
    std::string title, detail, hint, status;
};
// All public runtime functions synchronize their internal state. Rendering uses
// immutable menu snapshots and never reads live guest memory.
void initialize(const std::filesystem::path& runtime_directory);
void observe_menu(const uint8_t* rdram, size_t size);
void observe_context(const uint8_t* rdram, size_t size, Context context);
void decorate(sote::menu_skin::Snapshot& snapshot);
// 0=passthrough, 1=consume, 2=one native Back pulse, 3=one native Pause pulse.
int handle_menu(const PhysicalInput& raw, bool visible, uint64_t now_ms,
    bool native_menu_visible = false);
void focus_lost();
void scroll(int rows);
bool capturing();
EditorView editor_view();
PhysicalInput remap(const PhysicalInput& input, bool native_menu_visible);
uint16_t bike_button(bool accelerate);
// Diagnostics/tests read the same live model used by the menu and remapper.
Layout current_layout();
std::map<std::string, Token> saved_assignments();
}
extern "C" void sote_bindings_context(uint8_t* rdram, int context);
