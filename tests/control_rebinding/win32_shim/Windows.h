#pragma once
// Test-only window/keyboard boundary. SDL and production frontend.cpp are real.
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
using HWND = void*;
using DWORD = uint32_t;
using ULONGLONG = uint64_t;
inline std::array<uint8_t,256> test_win_keys{};
inline bool test_window_focused = true;
inline short GetAsyncKeyState(int key) {return key>=0&&key<256&&test_win_keys[key]?short(-32768):0;}
inline HWND GetForegroundWindow() {return test_window_focused?reinterpret_cast<HWND>(uintptr_t(1)):nullptr;}
inline DWORD GetCurrentProcessId() {return 1;}
inline DWORD GetWindowThreadProcessId(HWND,DWORD* pid) {*pid=1;return 1;}
inline ULONGLONG GetTickCount64() {return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
inline int fopen_s(FILE** out,const char* path,const char* mode) {*out=std::fopen(path,mode);return *out?0:1;}
#define VK_SPACE 32
#define VK_RETURN 13
#define VK_UP 38
#define VK_DOWN 40
#define VK_LEFT 37
#define VK_RIGHT 39
#define VK_PRIOR 33
#define VK_NEXT 34
