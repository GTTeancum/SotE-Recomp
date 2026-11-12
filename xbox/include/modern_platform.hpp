#pragma once
#include <windows.h>
#include "modern_controls.hpp"
struct XboxInputMutex {
    CRITICAL_SECTION mutex;
    XboxInputMutex(){InitializeCriticalSection(&mutex);}
};
struct XboxInputGuard {
    XboxInputMutex& mutex;
    explicit XboxInputGuard(XboxInputMutex& value):mutex(value){EnterCriticalSection(&mutex.mutex);}
    ~XboxInputGuard(){LeaveCriticalSection(&mutex.mutex);}
};
namespace sote::controls_menu {
enum class ControlScheme : int { Classic, Modern };
enum class SchemeSlot : int { OnFoot, Bike, Count };
struct ModernControlsTuning { modern_controls::Tuning on_foot{}; };
ControlScheme current_scheme(SchemeSlot slot);
ModernControlsTuning modern_controls_tuning();
}
