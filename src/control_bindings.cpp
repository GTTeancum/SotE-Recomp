#include "control_bindings.hpp"
#include "controls_menu.hpp"
#include "menu_skin.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <set>
#include <sstream>
#include <system_error>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace sote::control_bindings {
namespace {
constexpr int enter = 13, escape = 27, up = 38, down = 40, left = 37, right = 39;
constexpr int pad_a = 0, pad_b = 1, pad_x = 2, pad_y = 3, pad_back = 4;
constexpr int pad_du = 11, pad_dd = 12, pad_dl = 13, pad_dr = 14;
constexpr std::array<int, 5> display_order{0, 1, 4, 3, 2};
const char* context_ids[] = {"foot", "snowspeeder", "turret", "bike", "outrider"};
const char* context_names[] = {"On Foot", "Snowspeeder", "Turret", "Speeder Bike", "Outrider"};
uint64_t clock_ms() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
std::string trim(std::string s) {
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    return s.substr(b, s.find_last_not_of(" \t\r\n") - b + 1);
}
bool valid(Token t) {
    if (t.kind == Kind::None) return t.code == 0;
    if (t.kind == Kind::Key) return t.code > 0 && t.code < 256;
    if (t.kind == Kind::Button) return t.code >= 0 && t.code < 32;
    return t.kind == Kind::Axis && t.code >= 0 && t.code < 6 &&
        (t.sign == 1 || (t.sign == -1 && t.code < 4));
}
bool allowed(Token t, Device d) {
    if (t.kind == Kind::None) return true;
    if (!valid(t)) return false;
    if (d == Device::Controller)
        return (t.kind == Kind::Button && t.code != pad_back && t.code != 5) || t.kind == Kind::Axis;
    // Escape remains a recovery route. System combinations are never bindings.
    return t.kind == Kind::Key && t.code != escape && t.code != 122 &&
        t.code != 91 && t.code != 92 && t.code != 18 && t.code != 164 && t.code != 165;
}
void write_value(PhysicalInput& input, Token t, float v) {
    v = std::clamp(v, 0.0f, 1.0f);
    if (t.kind == Kind::Key && valid(t)) input.keys[t.code] = uint8_t(v > .5f);
    else if (t.kind == Kind::Button && valid(t)) input.buttons[t.code] = uint8_t(v > .5f);
    else if (t.kind == Kind::Axis && valid(t)) {
        auto& a = input.axes[t.code];
        const int magnitude = static_cast<int>(std::lround(v * (t.sign < 0 ? 32768.0f : 32767.0f)));
        const int other = t.sign > 0 ? std::min(0, int(a)) : std::max(0, int(a));
        a = static_cast<int16_t>(std::clamp(other + t.sign * magnitude, -32768, 32767));
    }
}
void clear_value(PhysicalInput& input, Token t) {
    if (t.kind == Kind::Axis && valid(t)) {
        if ((t.sign > 0 && input.axes[t.code] > 0) || (t.sign < 0 && input.axes[t.code] < 0)) input.axes[t.code] = 0;
    } else write_value(input, t, 0);
}
uint16_t primary(uint16_t mask, uint16_t preferred) {
    if (mask & preferred) return preferred;
    return static_cast<uint16_t>(mask & uint16_t(~mask + 1U));
}
std::string join_names(const std::vector<Token>& sources) {
    if (sources.empty()) return "Unbound";
    std::string s;
    for (auto t : sources) { if (!s.empty()) s += " / "; s += token_name(t); }
    return s;
}
}
Token key(int k) { return {Kind::Key, k, 1}; }
Token button(int b) { return {Kind::Button, b, 1}; }
Token axis(int a, int sign) { return {Kind::Axis, a, sign}; }
float value(const PhysicalInput& i, Token t) {
    if (!valid(t)) return 0;
    if (t.kind == Kind::Key) return i.keys[t.code] ? 1.0f : 0.0f;
    if (!i.connected) return 0;
    if (t.kind == Kind::Button) return i.buttons[t.code] ? 1.0f : 0.0f;
    if (t.kind == Kind::Axis) return std::max(0.0f,
        i.axes[t.code] * float(t.sign) / (t.sign < 0 ? 32768.0f : 32767.0f));
    return 0;
}
bool neutral(const PhysicalInput& i, Device d) {
    if (d == Device::Keyboard)
        return std::none_of(i.keys.begin(), i.keys.end(), [](uint8_t b) { return b != 0; });
    if (!i.connected) return true;
    return std::none_of(i.buttons.begin(), i.buttons.end(), [](uint8_t b) { return b != 0; }) &&
        std::none_of(i.axes.begin(), i.axes.end(), [](int16_t a) { return std::abs(int(a)) > 8192; });
}
std::string token_text(Token t) {
    switch (t.kind) {
        case Kind::None: return "none";
        case Kind::Key: return "key:" + std::to_string(t.code);
        case Kind::Button: return "pad:" + std::to_string(t.code);
        case Kind::Axis: return "axis:" + std::to_string(t.code) + (t.sign < 0 ? ":-" : ":+");
    }
    return "none";
}
bool parse_token(const std::string& s, Token& t) {
    if (s == "none") { t = {}; return true; }
    try {
        const auto pos = s.find(':');
        if (pos == std::string::npos) return false;
        size_t n = 0;
        const int code = std::stoi(s.substr(pos + 1), &n);
        const std::string rest = s.substr(pos + 1 + n);
        if (s.substr(0, pos) == "key" && rest.empty()) t = key(code);
        else if (s.substr(0, pos) == "pad" && rest.empty()) t = button(code);
        else if (s.substr(0, pos) == "axis" && (rest == ":+" || rest == ":-")) t = axis(code, rest == ":+" ? 1 : -1);
        else return false;
        return valid(t);
    } catch (...) { return false; }
}
std::string token_name(Token t) {
    if (t.kind == Kind::None) return "Unbound";
    if (t.kind == Kind::Button) {
        static constexpr const char* names[] = {"A", "B", "X", "Y", "View", "Guide", "Start", "LS Click", "RS Click", "LB", "RB", "D-Up", "D-Down", "D-Left", "D-Right", "Misc", "Paddle 1", "Paddle 2", "Paddle 3", "Paddle 4", "Touchpad"};
        if (t.code >= 0 && t.code < int(std::size(names))) return names[t.code];
        return "Button " + std::to_string(t.code);
    }
    if (t.kind == Kind::Axis) {
        const char* names[] = {"LS", "LS", "RS", "RS", "LT", "RT"};
        if (t.code < 0 || t.code >= 6) return "Unknown";
        if (t.code >= 4) return names[t.code];
        return std::string(names[t.code]) + (t.code % 2 ? (t.sign < 0 ? " Up" : " Down") : (t.sign < 0 ? " Left" : " Right"));
    }
    if (t.kind == Kind::Key) {
        if ((t.code >= 'A' && t.code <= 'Z') || (t.code >= '0' && t.code <= '9')) return std::string(1, char(t.code));
        if (t.code >= 112 && t.code <= 135) return "F" + std::to_string(t.code - 111);
        if (t.code >= 96 && t.code <= 105) return "Num " + std::to_string(t.code - 96);
        switch (t.code) {
            case 1: return "Mouse 1"; case 2: return "Mouse 2"; case 4: return "Mouse 3";
            case 5: return "Mouse 4"; case 6: return "Mouse 5";
            case 8: return "Backspace"; case 9: return "Tab"; case 13: return "Enter";
            case 16: return "Shift"; case 17: return "Ctrl"; case 20: return "Caps Lock";
            case 27: return "Escape"; case 32: return "Space";
            case 33: return "Page Up"; case 34: return "Page Down"; case 35: return "End"; case 36: return "Home";
            case 37: return "Left"; case 38: return "Up"; case 39: return "Right"; case 40: return "Down";
            case 45: return "Insert"; case 46: return "Delete";
            case 160: return "Left Shift"; case 161: return "Right Shift"; case 162: return "Left Ctrl"; case 163: return "Right Ctrl";
            case 186: return ";"; case 187: return "="; case 188: return ","; case 189: return "-";
            case 190: return "."; case 191: return "/"; case 192: return "`"; case 219: return "["; case 220: return "\\"; case 221: return "]"; case 222: return "'";
            default: return "Key " + std::to_string(t.code);
        }
    }
    return "Unknown";
}

Layout make_layout(const NativeTable& t) {
    Layout result;
    constexpr int ranges[5][2] = {{12,24},{24,31},{36,39},{31,36},{39,48}};
    struct Route { Token input; uint16_t mask; };
    for (int g = 0; g < 5; ++g) {
        const bool mf = g == 0 && t.modern_foot, mb = g == 3 && t.modern_bike;
        const std::string prefix = "p" + std::to_string(t.preset) + "." +
            (mf || mb ? "modern." : "classic.") + context_ids[g] + ".";
        auto add_fixed = [&](const std::string& id, const std::string& label, Token k, Token p) {
            Action a{prefix + id, label, static_cast<Context>(g), {}};
            if (k.kind != Kind::None) a.targets[0].push_back(k);
            if (p.kind != Kind::None) a.targets[1].push_back(p);
            result[g].push_back(std::move(a));
        };
        add_fixed("move_forward", "Move Forward", key('W'), mb ? Token{} : axis(1,-1));
        add_fixed("move_back", "Move Backward", key('S'), mb ? Token{} : axis(1,1));
        add_fixed("move_left", "Move Left", key('A'), axis(0,-1));
        add_fixed("move_right", "Move Right", key('D'), axis(0,1));
        if (mf) {
            add_fixed("look_up", "Look Up", {}, axis(3,-1));
            add_fixed("look_down", "Look Down", {}, axis(3,1));
            add_fixed("look_left", "Look Left", {}, axis(2,-1));
            add_fixed("look_right", "Look Right", {}, axis(2,1));
        }
        std::array<std::vector<Route>, 2> routes;
        routes[0] = {{key('Z'),0x8000},{key(32),0x8000},{key('X'),0x4000},{key('C'),0x2000},{key(13),0x1000},
            {key(38),0x0800},{key(40),0x0400},{key(37),0x0200},{key(39),0x0100},{key('Q'),0x20},{key('E'),0x10},
            {key('I'),8},{key('K'),4},{key('J'),2},{key('L'),1}};
        routes[1] = {{button(0),0x8000},{button(2),0x4000},{button(1),0x4000},{button(3),2},
            {button(6),0x1000},{button(9),0x20},{button(10),0x10},
            {button(11),0x0800},{button(12),0x0400},{button(13),0x0200},{button(14),0x0100},
            {axis(4),0x2000},{axis(3,-1),8},{axis(3,1),4},{axis(2,-1),2},{axis(2,1),1}};
        if (mf) {
            routes[1] = {{button(0),t.masks[13]},{axis(5),t.masks[12]},
                {button(1),t.masks[19]},{button(2),t.masks[18]},{button(3),t.masks[20]},
                {button(9),t.masks[22]},{button(10),t.masks[22]},
                {button(11),uint16_t(0x0800|t.masks[23])},{button(12),0x0400},
                {button(13),0x0200},{button(14),0x0100},{button(6),0x1000}};
        } else if (mb) {
            // Clear the three trigger/button bits without narrowing a
            // complemented 32-bit constant (MSVC C4310).
            for (auto& r : routes[1]) r.mask &= uint16_t{0x1FFF};
            // Prefer trigger targets for an equivalent native action. A
            // button alias has the same digital effect but would throw away
            // analog depth before the bike's duty-cycle accelerator/brake.
            routes[1].insert(routes[1].begin(), {
                {axis(5),primary(t.masks[32],0x8000)},
                {axis(4),primary(t.masks[31],0x4000)}});
        }
        // Group by the complete native effect signature, not merely by a
        // translated label. Alternative routes in a row are genuinely equal.
        std::map<std::vector<int>, size_t> indices;
        for (int d = 0; d < 2; ++d) for (const auto& r : routes[d]) {
            std::vector<int> effects;
            for (int a = 3; a < 48; ++a)
                if ((a < 12 || (a >= ranges[g][0] && a < ranges[g][1])) && (t.masks[a] & r.mask)) effects.push_back(a);
            if (effects.empty()) continue;
            auto it = indices.find(effects);
            if (it == indices.end()) {
                std::string id = "action", label;
                std::set<std::string> used;
                for (int a : effects) {
                    id += "_" + std::to_string(a);
                    std::string name = t.labels[a];
                    if (a >= 4 && a <= 7) name = std::string("D-Pad Move ") + std::array<const char*,4>{"Forward","Back","Left","Right"}[a-4];
                    if (a >= 8 && a <= 11) name = std::string("Camera ") + std::array<const char*,4>{"Up","Down","Left","Right"}[a-8];
                    if (name.empty()) name = "Action " + std::to_string(a);
                    if (used.insert(name).second) { if (!label.empty()) label += " / "; label += name; }
                }
                const size_t index = result[g].size();
                result[g].push_back({prefix + id, label, static_cast<Context>(g), {}});
                it = indices.emplace(effects, index).first;
            }
            auto& targets = result[g][it->second].targets[d];
            if (std::find(targets.begin(),targets.end(),r.input) == targets.end()) targets.push_back(r.input);
        }
    }
    return result;
}
std::string storage_key(const Action& a, Device d) { return a.id + (d == Device::Keyboard ? ".key" : ".pad"); }
std::vector<Token> Assignments::sources(const Action& a, Device d) const {
    const auto it = overrides.find(storage_key(a,d));
    if (it == overrides.end()) return a.targets[int(d)];
    if (it->second.kind == Kind::None) return {};
    return {it->second};
}
std::vector<std::string> Assignments::conflicts(const std::vector<Action>& rows, const Action& a, Device d, Token p) const {
    std::vector<std::string> result;
    if (p.kind == Kind::None) return result;
    for (const auto& row : rows) {
        if (row.id == a.id || row.targets[int(d)].empty()) continue;
        const auto list = sources(row,d);
        if (std::find(list.begin(),list.end(),p) != list.end()) result.push_back(row.id);
    }
    return result;
}
PhysicalInput Assignments::apply(const PhysicalInput& raw, const std::vector<Action>& rows) const {
    PhysicalInput out = raw;
    // Read ONLY raw throughout; sequential/chained rebinds must not feed back.
    for (const auto& row : rows) for (int d=0; d<2; ++d) {
        for (auto t : row.targets[d]) clear_value(out,t);
        const auto it=overrides.find(storage_key(row,static_cast<Device>(d)));
        if (it!=overrides.end()) clear_value(out,it->second);
    }
    for (const auto& row : rows) for (int d=0; d<2; ++d) {
        if (row.targets[d].empty()) continue;
        const auto it=overrides.find(storage_key(row,static_cast<Device>(d)));
        if (it==overrides.end()) {
            for (auto t : row.targets[d]) write_value(out,t,value(raw,t));
        } else {
            // Every target in this row has the same native effect signature.
            // Emit one representative, never every alias at once.
            write_value(out,row.targets[d].front(),value(raw,it->second));
        }
    }
    return out;
}
bool Assignments::load(const std::filesystem::path& path, std::string& error) {
    overrides.clear(); error.clear();
    std::error_code ec;
    if (!std::filesystem::exists(path,ec)) {
        if (ec) error = "Cannot inspect saved bindings: " + ec.message();
        return !ec;
    }
    std::ifstream in(path);
    if (!in) { error="Cannot read saved bindings"; return false; }
    std::string line;
    while (std::getline(in,line)) {
        if (line.size()>1024) continue;
        if (line.starts_with("\xEF\xBB\xBF")) line.erase(0,3);
        line=trim(line.substr(0,line.find_first_of("#;")));
        if (line.empty() || line.front()=='[') continue;
        const auto p=line.find('='); if(p==std::string::npos) continue;
        const std::string k=trim(line.substr(0,p)),v=trim(line.substr(p+1));
        if(k.size()>240 || k.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_.")!=std::string::npos) continue;
        const bool kb=k.ends_with(".key"),pad=k.ends_with(".pad"); Token token;
        if((kb||pad) && parse_token(v,token) && allowed(token,kb?Device::Keyboard:Device::Controller)) overrides[k]=token;
    }
    if (in.bad()) { error="Error reading bindings"; return false; }
    return true;
}
bool Assignments::save(const std::filesystem::path& path, std::string& error) const {
    error.clear();
    try {
        std::filesystem::create_directories(path.parent_path());
        auto temporary=path; temporary += ".tmp";
        {
            std::ofstream out(temporary,std::ios::trunc|std::ios::binary);
            if(!out) { error="Cannot write bindings file"; return false; }
            out<<"; SotE runtime control bindings. Generated only after a confirmed edit.\n"
                "; Defaults are computed from the active native preset and control scheme.\n"
                "version = 1\n[bindings]\n";
            for(const auto& [k,v]:overrides) out<<k<<" = "<<token_text(v)<<"\n";
            out.flush(); if(!out) { error="Cannot flush bindings file"; return false; }
            out.close(); if(!out) { error="Cannot close bindings file"; return false; }
        }
#ifdef _WIN32
        if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
            error="Cannot replace bindings file (Windows error "+std::to_string(GetLastError())+")"; return false;
        }
#else
        std::filesystem::rename(temporary,path);
#endif
        return true;
    } catch(const std::exception& e) { error=std::string("Cannot save bindings: ")+e.what(); return false; }
}

namespace {
struct Runtime {
    std::mutex mutex;
    Assignments assignments;
    std::filesystem::path path;
    Layout menu_layout, play_layout;
    NativeTable menu_table, play_table;
    bool menu_valid=false, play_valid=false;
    int context=0;
    uint64_t context_time=0;
    EditorView editor;
    PhysicalInput previous;
    bool entrance_release=true, exit_pending=false, external_block=false;
    int capture_instance=-1, repeat_direction=0;
    uint64_t repeat_at=0;
    Token proposed;
    std::vector<std::string> conflicts;
} state;
std::vector<std::pair<int,int>> positions(const Layout& layout) {
    std::vector<std::pair<int,int>> out;
    for(int g:display_order) for(int r=0;r<int(layout[g].size());++r) out.emplace_back(g,r);
    return out;
}
void ensure_selection() {
    auto& e=state.editor;
    e.group=std::clamp(e.group,0,4);
    e.row=std::clamp(e.row,0,std::max(0,int(state.menu_layout[e.group].size())-1));
    int line=0, selected=0,total=0;
    for(int g:display_order) { ++line; for(int r=0;r<int(state.menu_layout[g].size());++r) {if(g==e.group&&r==e.row)selected=line; ++line;} ++line; }
    total=line;
    if(selected<e.scroll)e.scroll=selected;
    if(selected>e.scroll+9)e.scroll=selected-9;
    e.scroll=std::clamp(e.scroll,0,std::max(0,total-10));
}
void move_selection(int n) {
    const auto list=positions(state.menu_layout); if(list.empty())return;
    const auto p=std::find(list.begin(),list.end(),std::pair{state.editor.group,state.editor.row});
    const int index=p==list.end()?0:int(p-list.begin());
    const auto [g,r]=list[std::clamp(index+n,0,int(list.size())-1)];
    state.editor.group=g; state.editor.row=r; state.editor.status.clear(); ensure_selection();
}
const Action* selected() {
    const auto& e=state.editor;
    if(e.group<0||e.group>=5||e.row<0||e.row>=int(state.menu_layout[e.group].size()))return nullptr;
    return &state.menu_layout[e.group][e.row];
}
bool edge(const PhysicalInput& raw, Token t, float threshold=.5f) {
    return value(raw,t)>threshold && value(state.previous,t)<=threshold;
}
bool all_neutral(const PhysicalInput& p) {return neutral(p,Device::Keyboard)&&neutral(p,Device::Controller);}
void settle(const std::string& status) {
    state.editor.phase=Phase::Settle; state.editor.status=status;
    state.editor.title.clear();state.editor.detail.clear();state.editor.hint.clear();state.conflicts.clear();
}
void commit(bool unbind_conflicts) {
    const auto* a=selected(); if(!a)return;
    const auto d=static_cast<Device>(state.editor.column);
    auto updated=state.assignments;
    if(unbind_conflicts) for(const auto& id:state.conflicts)
        updated.overrides[id+(d==Device::Keyboard?".key":".pad")]={};
    updated.overrides[storage_key(*a,d)]=state.proposed;
    std::string error;
    if(updated.save(state.path,error)) {
        state.assignments=std::move(updated);
        std::printf("[sote][bindings] %s = %s\n",storage_key(*a,d).c_str(),token_text(state.proposed).c_str());
        settle("Saved: "+a->label+" = "+token_name(state.proposed));
    } else { std::fprintf(stderr,"[sote][bindings] %s\n",error.c_str());settle("Not saved; previous binding retained. "+error); }
}
void set_dialog(Phase phase, std::string title,std::string detail,std::string hint) {
    state.editor.phase=phase;state.editor.title=std::move(title);state.editor.detail=std::move(detail);state.editor.hint=std::move(hint);
}
bool same_table(const NativeTable& a,const NativeTable& b) {
    return a.preset==b.preset&&a.masks==b.masks&&a.labels==b.labels&&a.modern_foot==b.modern_foot&&a.modern_bike==b.modern_bike;
}
bool read_table(const uint8_t* ram,size_t size,bool menu,NativeTable& table) {
    if(!ram||size<0x190000||(size&3)!=0)return false;
    auto byte=[&](uint32_t a)->uint8_t{return ram[(a&0x7fffff)^3];};
    auto half=[&](uint32_t a)->uint16_t{return uint16_t((unsigned(byte(a))<<8)|byte(a+1));};
    auto word=[&](uint32_t a)->uint32_t{return (uint32_t(half(a))<<16)|half(a+2);};
    int preset=int16_t(half(0x800d252c));
    if(menu) {
        const int profile=byte(0x8018bbfd);if(profile>=4)return false;
        preset=byte(0x8018bbf8+profile*0x7a+0x1d)&15;
    }
    if(preset<0||preset>=8)return false;
    table.preset=preset;
    for(int a=0;a<48;++a) {
        table.masks[a]=half(0x800e6808+preset*96+a*2);
        const uint32_t ptr=word(0x800da70c+a*4);
        if(ptr<0x80000000U||ptr>=0x80000000U+size)return false;
        std::string label;
        for(uint32_t k=0;k<120&&(ptr&0x7fffff)+k<size;++k) {
            const auto ch=byte(ptr+k); if(!ch)break; label+=char(ch);
        }
        table.labels[a]=menu_skin::plain_text(label);
    }
    if(table.masks[3]!=0x1000)return false; // loaded native control-table guard
    table.modern_foot=controls_menu::current_scheme(controls_menu::SchemeSlot::OnFoot)==controls_menu::ControlScheme::Modern;
    table.modern_bike=controls_menu::current_scheme(controls_menu::SchemeSlot::Bike)==controls_menu::ControlScheme::Modern;
    return true;
}
}
void initialize(const std::filesystem::path& root) {
    std::lock_guard lock(state.mutex);
    state.path=root/"Sdata"/"controls_bindings.ini";
    std::string error;state.assignments.load(state.path,error);
    state.menu_valid=false;state.play_valid=false;state.editor={};state.previous={};
    state.entrance_release=true;state.exit_pending=false;state.external_block=false;state.context_time=0;
    if(!error.empty())std::fprintf(stderr,"[sote][bindings] %s\n",error.c_str());
    std::printf("[sote][bindings] Loaded %zu overrides; fixed menu recovery controls.\n",state.assignments.overrides.size());
}
void observe_menu(const uint8_t* ram,size_t size) {
    NativeTable table;if(!read_table(ram,size,true,table))return;
    std::lock_guard lock(state.mutex);
    if(!state.menu_valid||!same_table(state.menu_table,table)) {
        if(state.menu_valid)settle("Preset or scheme changed; using its saved bindings.");
        state.menu_table=table;state.menu_layout=make_layout(table);state.menu_valid=true;ensure_selection();
    }
}
void observe_context(const uint8_t* ram,size_t size,Context context) {
    if(int(context)<0||int(context)>=5)return;
    NativeTable table;if(!read_table(ram,size,false,table))return;
    std::lock_guard lock(state.mutex);
    if(!state.play_valid||!same_table(state.play_table,table)) {
        state.play_table=table;state.play_layout=make_layout(table);state.play_valid=true;
    }
    state.context=int(context);state.context_time=clock_ms();
}
void decorate(menu_skin::Snapshot& s) {
    if(s.screen!=menu_skin::Screen::Controls)return;
    std::lock_guard lock(state.mutex);if(!state.menu_valid)return;
    for(int g=0;g<5;++g) {
        s.native_controls[g].clear();
        for(const auto& a:state.menu_layout[g]) {
            auto text=[&](int d){return a.targets[d].empty()?std::string("--"):join_names(state.assignments.sources(a,static_cast<Device>(d)));};
            s.native_controls[g].push_back({a.label,text(1),text(0)});
        }
    }
    ensure_selection();
    s.rebinding=true;s.binding_editing=state.editor.editing;s.binding_group=state.editor.group;s.binding_row=state.editor.row;s.binding_column=state.editor.column;
    s.controls_scroll=state.editor.scroll;s.binding_phase=int(state.editor.phase);
    s.binding_title=state.editor.title;s.binding_detail=state.editor.detail;s.binding_hint=state.editor.hint;s.binding_status=state.editor.status;
}
int handle_menu(const PhysicalInput& raw,bool visible,uint64_t now,bool native_menu_visible) {
    std::lock_guard lock(state.mutex);auto& e=state.editor;
    auto done=[&](int result){state.previous=raw;return result;};
    if(!visible||!state.menu_valid) {
        if(e.visible) {e.visible=false;e.editing=false;state.external_block=true;e.phase=Phase::Browse;state.exit_pending=false;}
        if(state.external_block) {if(all_neutral(raw))state.external_block=false;return done(1);}
        // A reserved, edge-triggered recovery key: even clearing every Pause
        // binding must not strand a keyboard-only player outside the menus.
        // Holding Escape across the menu transition must not immediately
        // trigger Back and close the menu it has just opened.
        if(edge(raw,key(escape)) && (native_menu_visible ||
            (state.play_valid && clock_ms()-state.context_time <= 200)))
            return done(native_menu_visible ? 2 : 3);
        return done(0);
    }
    if(!e.visible) {e.visible=true;e.editing=false;e.phase=Phase::Browse;state.entrance_release=true;state.exit_pending=false;state.repeat_direction=0;ensure_selection();}
    if(state.exit_pending)return done(1);
    if(state.entrance_release) {if(all_neutral(raw))state.entrance_release=false;return done(1);}
    const bool cancel=edge(raw,key(escape))||edge(raw,button(pad_back));
    // Native Controls zooms automatically when the Options row is focused.
    // Keep this reference/preset mode native until Enter/A explicitly opens
    // the editor. Otherwise Up/Down can no longer reach Graphics/Schemes and
    // Left/Right can no longer change the original game's preset.
    if(!e.editing) {
        if(cancel) {state.exit_pending=true;return done(2);}
        if(edge(raw,key(enter))||edge(raw,button(pad_a))) {
            e.editing=true;e.phase=Phase::Browse;e.status.clear();
            state.entrance_release=true;ensure_selection();return done(1);
        }
        return done(0);
    }
    if(e.phase==Phase::Browse) {
        if(cancel||edge(raw,button(pad_b))) {
            e.editing=false;e.status.clear();state.entrance_release=true;
            return done(1);
        }
        const auto nav=[&](int vk,int pad,int ax,int sign){return value(raw,key(vk))>.5f||value(raw,button(pad))>.5f||value(raw,axis(ax,sign))>.65f;};
        const int direction=nav(down,pad_dd,1,1)?1:nav(up,pad_du,1,-1)?-1:0;
        if(direction && (direction!=state.repeat_direction||now>=state.repeat_at)) {
            move_selection(direction);state.repeat_at=now+(direction!=state.repeat_direction?350:100);
        }
        state.repeat_direction=direction;
        if(edge(raw,key(left))||edge(raw,button(pad_dl))||edge(raw,axis(0,-1),.65f))e.column=0;
        if(edge(raw,key(right))||edge(raw,button(pad_dr))||edge(raw,axis(0,1),.65f))e.column=1;
        if(edge(raw,key(33)))move_selection(-8);
        if(edge(raw,key(34)))move_selection(8);
        if(edge(raw,key(36)))move_selection(-1000);
        if(edge(raw,key(35)))move_selection(1000);
        if(edge(raw,key(120))||edge(raw,button(pad_y))) {
            set_dialog(Phase::DefaultsRelease,"Restore Defaults?",std::string(context_names[e.group])+": keyboard and controller.\nOther contexts and presets will not change.","Release buttons, then Enter / A to restore; Esc / B to cancel.");
            return done(1);
        }
        const auto* a=selected();if(!a)return done(1);
        if(edge(raw,key(46))||edge(raw,button(pad_x))) {
            if(a->targets[e.column].empty()){e.status="This input belongs to another listed/shared control.";return done(1);}
            state.proposed={};state.conflicts.clear();commit(false);return done(1);
        }
        if(edge(raw,key(enter))||edge(raw,button(pad_a))) {
            if(a->targets[e.column].empty()){e.status="No independent input in this column; use its linked row.";return done(1);}
            if(e.column==1&&!raw.connected){e.status="Connect a controller before assigning a button.";return done(1);}
            state.capture_instance=raw.instance;
            set_dialog(Phase::Release,"Change: "+a->label,"Release the opening key/button and center the sticks.","Esc / View cancels. B can be assigned during capture.");
        }
        return done(1);
    }
    if(e.phase==Phase::Settle) {if(all_neutral(raw))e.phase=Phase::Browse;return done(1);}
    if(cancel) {settle("Cancelled; binding unchanged.");return done(1);}
    if((e.phase==Phase::Release||e.phase==Phase::Capture||e.phase==Phase::ConflictRelease||e.phase==Phase::Conflict)&&e.column==1&&(!raw.connected||raw.instance!=state.capture_instance)) {
        settle("Controller disconnected; binding unchanged.");return done(1);
    }
    if(e.phase==Phase::Release) {
        if(all_neutral(raw)) {
            const auto* a=selected();
            set_dialog(Phase::Capture,"Change: "+(a?a->label:std::string("control")),
                e.column==0?"Press a key or mouse button.":"Press a controller button, trigger, or stick direction.",
                "Esc / View cancels. B can be assigned during capture.");
        }
        return done(1);
    }
    if(e.phase==Phase::Capture) {
        Token candidate;const auto d=static_cast<Device>(e.column);
        if(d==Device::Keyboard) {
            if(raw.keys[18]||raw.keys[164]||raw.keys[165]||raw.keys[91]||raw.keys[92])return done(1);
            for(int k=1;k<256;++k)if(edge(raw,key(k))&&allowed(key(k),d)) {
                // Prefer side-specific modifiers over their generic VK aliases.
                if((k==16&&(raw.keys[160]||raw.keys[161]))||(k==17&&(raw.keys[162]||raw.keys[163])))continue;
                candidate=key(k);break;
            }
        } else {
            for(int b=0;b<32;++b)if(edge(raw,button(b))&&allowed(button(b),d)){candidate=button(b);break;}
            if(candidate.kind==Kind::None)for(int a=0;a<6;++a)for(int sign:{-1,1})
                if(valid(axis(a,sign))&&edge(raw,axis(a,sign),.65f)) {candidate=axis(a,sign);break;}
        }
        if(candidate.kind==Kind::None)return done(1);
        const auto* a=selected();if(!a){settle("No active control.");return done(1);}
        state.proposed=candidate;state.conflicts=state.assignments.conflicts(state.menu_layout[e.group],*a,d,candidate);
        if(state.conflicts.empty())commit(false);
        else {
            std::string names;
            for(const auto& row:state.menu_layout[e.group])if(std::find(state.conflicts.begin(),state.conflicts.end(),row.id)!=state.conflicts.end()) {
                if(!names.empty())names+=" / ";
                names+=row.label;
            }
            set_dialog(Phase::ConflictRelease,"Binding Already Used",token_name(candidate)+" is assigned to:\n"+names+"\nMove it here and unbind the conflicting control?",
                "Release first. Enter / A moves it; Esc / B cancels.");
        }
        return done(1);
    }
    if(e.phase==Phase::ConflictRelease||e.phase==Phase::DefaultsRelease) {
        if(all_neutral(raw))e.phase=e.phase==Phase::ConflictRelease?Phase::Conflict:Phase::Defaults;
        return done(1);
    }
    if(edge(raw,button(pad_b))) {settle("Cancelled; bindings unchanged.");return done(1);}
    if(edge(raw,key(enter))||edge(raw,button(pad_a))) {
        if(e.phase==Phase::Conflict)commit(true);
        else if(e.phase==Phase::Defaults) {
            auto updated=state.assignments;
            for(const auto& a:state.menu_layout[e.group])for(auto d:{Device::Keyboard,Device::Controller})updated.overrides.erase(storage_key(a,d));
            std::string error;
            if(updated.save(state.path,error)){state.assignments=std::move(updated);settle("Defaults restored for "+std::string(context_names[e.group])+".");}
            else settle("Not saved; previous bindings retained. "+error);
        }
    }
    return done(1);
}
void focus_lost() {
    std::lock_guard lock(state.mutex);
    if(state.editor.editing&&state.editor.phase!=Phase::Browse)settle("Focus lost; pending change cancelled.");
    state.entrance_release=true;state.external_block=true;state.previous={};state.repeat_direction=0;
}
void scroll(int rows) {std::lock_guard lock(state.mutex);if(state.editor.phase==Phase::Browse&&state.menu_valid)move_selection(rows);}
bool capturing() {std::lock_guard lock(state.mutex);return state.editor.editing&&state.editor.phase!=Phase::Browse&&state.editor.phase!=Phase::Settle;}
EditorView editor_view(){std::lock_guard lock(state.mutex);return state.editor;}
PhysicalInput remap(const PhysicalInput& raw,bool native_menu_visible) {
    std::lock_guard lock(state.mutex);
    if(native_menu_visible||!state.play_valid||clock_ms()-state.context_time>200)return raw;
    return state.assignments.apply(raw,state.play_layout[state.context]);
}
uint16_t bike_button(bool accelerate) {
    std::lock_guard lock(state.mutex);
    if(!state.play_valid)return accelerate?0x8000:0x4000;
    return primary(state.play_table.masks[accelerate?32:31],accelerate?0x8000:0x4000);
}
Layout current_layout(){std::lock_guard lock(state.mutex);return state.menu_layout;}
std::map<std::string,Token> saved_assignments(){std::lock_guard lock(state.mutex);return state.assignments.overrides;}
}
extern "C" void sote_bindings_context(uint8_t* rdram,int context) {
    try {sote::control_bindings::observe_context(rdram,0x800000,static_cast<sote::control_bindings::Context>(context));}
    catch(const std::exception& e){static bool reported=false;if(!reported)std::fprintf(stderr,"[sote][bindings] Native capture failed: %s\n",e.what());reported=true;}
}
