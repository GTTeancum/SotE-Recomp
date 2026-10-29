#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>

#ifdef _WIN32
#include <Windows.h>
#else
#include <SDL.h>
// Binding files use stable virtual-key numbers on every host platform.
constexpr int VK_LBUTTON=1, VK_RBUTTON=2, VK_MBUTTON=4;
constexpr int VK_TAB=9, VK_RETURN=13, VK_ESCAPE=27, VK_SPACE=32;
constexpr int VK_LEFT=37, VK_UP=38, VK_RIGHT=39, VK_DOWN=40;
constexpr int VK_NUMPAD2=98, VK_NUMPAD4=100, VK_NUMPAD6=102, VK_NUMPAD8=104;
constexpr int VK_F1=112;
#endif

namespace sote::host {
inline uint64_t clock_ms() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
inline std::filesystem::path executable_directory() {
#ifdef _WIN32
    wchar_t path[32768]{};
    const auto length=GetModuleFileNameW(nullptr,path,32768);
    if(length && length<32768) return std::filesystem::path(path).parent_path();
#else
    std::error_code error;
    const auto path=std::filesystem::read_symlink("/proc/self/exe",error);
    if(!error) return path.parent_path();
#endif
    return std::filesystem::current_path();
}

inline bool key_down(int key) {
#ifdef _WIN32
    return (GetAsyncKeyState(key)&0x8000)!=0;
#else
    const auto mouse=SDL_GetMouseState(nullptr,nullptr);
    if(key==1) return (mouse&SDL_BUTTON_LMASK)!=0;
    if(key==2) return (mouse&SDL_BUTTON_RMASK)!=0;
    if(key==4) return (mouse&SDL_BUTTON_MMASK)!=0;
    if(key==5) return (mouse&SDL_BUTTON_X1MASK)!=0;
    if(key==6) return (mouse&SDL_BUTTON_X2MASK)!=0;
    SDL_Scancode scan=SDL_SCANCODE_UNKNOWN;
    if(key>='A' && key<='Z') scan=static_cast<SDL_Scancode>(SDL_SCANCODE_A+key-'A');
    else if(key>='1' && key<='9') scan=static_cast<SDL_Scancode>(SDL_SCANCODE_1+key-'1');
    else if(key=='0') scan=SDL_SCANCODE_0;
    else if(key>=112 && key<=123) scan=static_cast<SDL_Scancode>(SDL_SCANCODE_F1+key-112);
    else if(key>=97 && key<=105) scan=static_cast<SDL_Scancode>(SDL_SCANCODE_KP_1+key-97);
    else switch(key) {
        case 8: scan=SDL_SCANCODE_BACKSPACE; break;
        case 9: scan=SDL_SCANCODE_TAB; break;
        case 13: scan=SDL_SCANCODE_RETURN; break;
        case 16: return key_down(160)||key_down(161);
        case 17: return key_down(162)||key_down(163);
        case 18: return key_down(164)||key_down(165);
        case 19: scan=SDL_SCANCODE_PAUSE; break;
        case 20: scan=SDL_SCANCODE_CAPSLOCK; break;
        case 27: scan=SDL_SCANCODE_ESCAPE; break;
        case 32: scan=SDL_SCANCODE_SPACE; break;
        case 33: scan=SDL_SCANCODE_PAGEUP; break;
        case 34: scan=SDL_SCANCODE_PAGEDOWN; break;
        case 35: scan=SDL_SCANCODE_END; break;
        case 36: scan=SDL_SCANCODE_HOME; break;
        case 37: scan=SDL_SCANCODE_LEFT; break;
        case 38: scan=SDL_SCANCODE_UP; break;
        case 39: scan=SDL_SCANCODE_RIGHT; break;
        case 40: scan=SDL_SCANCODE_DOWN; break;
        case 45: scan=SDL_SCANCODE_INSERT; break;
        case 46: scan=SDL_SCANCODE_DELETE; break;
        case 96: scan=SDL_SCANCODE_KP_0; break;
        case 106: scan=SDL_SCANCODE_KP_MULTIPLY; break;
        case 107: scan=SDL_SCANCODE_KP_PLUS; break;
        case 109: scan=SDL_SCANCODE_KP_MINUS; break;
        case 110: scan=SDL_SCANCODE_KP_PERIOD; break;
        case 111: scan=SDL_SCANCODE_KP_DIVIDE; break;
        case 160: scan=SDL_SCANCODE_LSHIFT; break;
        case 161: scan=SDL_SCANCODE_RSHIFT; break;
        case 162: scan=SDL_SCANCODE_LCTRL; break;
        case 163: scan=SDL_SCANCODE_RCTRL; break;
        case 164: scan=SDL_SCANCODE_LALT; break;
        case 165: scan=SDL_SCANCODE_RALT; break;
        case 186: scan=SDL_SCANCODE_SEMICOLON; break;
        case 187: scan=SDL_SCANCODE_EQUALS; break;
        case 188: scan=SDL_SCANCODE_COMMA; break;
        case 189: scan=SDL_SCANCODE_MINUS; break;
        case 190: scan=SDL_SCANCODE_PERIOD; break;
        case 191: scan=SDL_SCANCODE_SLASH; break;
        case 192: scan=SDL_SCANCODE_GRAVE; break;
        case 219: scan=SDL_SCANCODE_LEFTBRACKET; break;
        case 220: scan=SDL_SCANCODE_BACKSLASH; break;
        case 221: scan=SDL_SCANCODE_RIGHTBRACKET; break;
        case 222: scan=SDL_SCANCODE_APOSTROPHE; break;
    }
    int count=0;
    const auto* keys=SDL_GetKeyboardState(&count);
    return scan!=SDL_SCANCODE_UNKNOWN && scan<count && keys[scan];
#endif
}
} // namespace sote::host
