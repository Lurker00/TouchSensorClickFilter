// HookDll.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "HookDll.h"

#include <stdio.h>
#include <mutex>

class Logger {
public:
    Logger()
    {
#if _DEBUG
        TCHAR szPath[MAX_PATH + 4];
        if (GetModuleFileName(NULL, szPath, MAX_PATH) > 0)
        {
            _tcscat(szPath, _T(".log"));
            fLog = _tfopen(szPath, _T("wt"));
        }
#endif
    }

    ~Logger()
    {
#if _DEBUG
        if (fLog)
        {
            Log("Stopped");
            fclose(fLog);
        }
#endif
    }

    template<typename ...Args>
    void Log(Args ... args)
    {
#if _DEBUG
        if (!fLog) return;
        fprintf(fLog, "[%10u] ", ::GetTickCount());
        fprintf(fLog, args...);
        fprintf(fLog, "\n");
        fflush(fLog);
#endif
    }

private:
#if _DEBUG
    FILE* fLog = NULL;
#endif
};

class HookDllImpl {
    static const unsigned TOO_FAST_CLICK = 50; // ms
public:
    HookDllImpl();
    ~HookDllImpl();

    bool LowLevelMouseProc(WPARAM wParam, const MSLLHOOKSTRUCT& msllhs);
    void TimerProc();
    bool Disabled() const { return Disable; }
    void Enable(bool enable) { Disable = !enable; }
    HHOOK hHook() const { return Hook; }

    Logger Log;

private:
    void StopTimer()
    {
        if (Timer)
        {
            ::DeleteTimerQueueTimer(NULL, Timer, NULL);
            Timer = NULL;
            Log.Log("Timer removed");
        }
    }

    HHOOK  Hook    = 0;
    HANDLE Timer   = 0;
    bool   Disable = false;
    MSLLHOOKSTRUCT SavedEvent = { 0, };
    unsigned LButtonDownCounter = 0;
    bool LButtonDown = false;
    std::mutex Mutex;
};

static HookDllImpl* Instance = nullptr;

static bool MyEvent(WPARAM wParam)
{
    static WPARAM events[] = { WM_MOUSEMOVE, WM_LBUTTONDOWN, WM_LBUTTONUP/*, WM_RBUTTONDOWN, WM_RBUTTONUP*/ };
    for (auto e : events)
        if (e == wParam) return true;
    return false;
}

static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (Instance == nullptr)
        return 0; // Should never happen!

    const HHOOK hHook = Instance->hHook();

    if (nCode != HC_ACTION || !MyEvent(wParam) || Instance->Disabled())
        return ::CallNextHookEx(hHook, nCode, wParam, lParam);

    const MSLLHOOKSTRUCT& msllhs = *(MSLLHOOKSTRUCT*)lParam;

    const ULONG_PTR MOUSEEVENTF_FROMTOUCH = 0xFF515700;
    if ((msllhs.dwExtraInfo & MOUSEEVENTF_FROMTOUCH) == MOUSEEVENTF_FROMTOUCH)
        return ::CallNextHookEx(hHook, nCode, wParam, lParam);

    if (msllhs.flags != 0) // Injected event, don't process
        return ::CallNextHookEx(hHook, nCode, wParam, lParam);

    try {
        if (Instance->LowLevelMouseProc(wParam, msllhs))
            return 1; // Stop further processing
    }
    catch (const std::exception& err) {
        // Lock from the same thread? It happens when several WM_MOUSEMOVE come while processing WM_LBUTTONUP
        Instance->Log.Log("wParam=0x%016llX flags=0x%08X dwExtraInfo=%016llX (%4d,%4d) time=%u -> %s"
            , wParam, msllhs.flags, msllhs.dwExtraInfo, msllhs.pt.x, msllhs.pt.y, msllhs.time, err.what());
    }

    return ::CallNextHookEx(hHook, nCode, wParam, lParam);
}

static void CALLBACK TimerProc(void* /*lpParametar*/, BOOLEAN /*TimerOrWaitFired*/)
{
    if (Instance == nullptr)
        return; // Should never happen!

    try {
        Instance->TimerProc();
    }
    catch (const std::exception& err) {
        // Lock from the same thread?
        Instance->Log.Log("%s", err.what());
    }
}

HookDllImpl::HookDllImpl() {
    HINSTANCE hSelf = NULL;
    ::GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (wchar_t *)::LowLevelMouseProc, &hSelf);
    Hook = ::SetWindowsHookEx(WH_MOUSE_LL, ::LowLevelMouseProc, hSelf, 0);
}

HookDllImpl::~HookDllImpl() {
    if (Hook)
        ::UnhookWindowsHookEx(Hook);
    StopTimer();
}

template <typename T> inline
T abs(const T& v) { return v < 0 ? -v : v; }

static bool TooClose(const POINT& p1, const POINT& p2)
{
    const int MIN_DISTANCE = 2;
    return abs(p1.x - p2.x) < MIN_DISTANCE && abs(p1.y - p2.y) < MIN_DISTANCE;
}

bool HookDllImpl::LowLevelMouseProc(WPARAM wParam, const MSLLHOOKSTRUCT& msllhs)
{
    std::lock_guard<std::mutex> lock(Mutex);
    StopTimer();

    if (wParam == WM_LBUTTONUP || wParam == WM_LBUTTONDOWN)
    {
        Log.Log("wParam=0x%016llX flags=0x%08X dwExtraInfo=%016llX (%4d,%4d) time=%u"
            , wParam, msllhs.flags, msllhs.dwExtraInfo, msllhs.pt.x, msllhs.pt.y, msllhs.time);
    }

    if (wParam == WM_LBUTTONDOWN)
    {
        if (LButtonDownCounter == 0)
        {
            SavedEvent = msllhs;
            ::CreateTimerQueueTimer(&Timer, NULL, ::TimerProc, this, TOO_FAST_CLICK, 0, WT_EXECUTEINTIMERTHREAD);
            Log.Log("Timer started");
        }
        else
            SavedEvent.time = msllhs.time;
        LButtonDownCounter++;
        return true;
    }

    if (wParam == WM_LBUTTONUP && LButtonDownCounter == 0)
    {
        // Sticky drag mode?
        Log.Log("Excess button up");
        return true;
    }

    if (wParam == WM_LBUTTONUP && LButtonDownCounter > 1)
    {
        Log.Log("Nested button up: %u", LButtonDownCounter);
        LButtonDownCounter--;
        return true;
    }

    if ((wParam == WM_LBUTTONUP || wParam == WM_MOUSEMOVE) && SavedEvent.time != 0)
    {
        // Disable too fast clicks!
        if (SavedEvent.time <= msllhs.time && msllhs.time - SavedEvent.time < TOO_FAST_CLICK)
        {
            if (wParam == WM_LBUTTONUP)
            {
                SavedEvent.time = 0;
                LButtonDownCounter = 0;
                LButtonDown = false;
                Log.Log("Skip too fast click");
                return true;
            }
            else
            {
                Log.Log("Skip too fast move");
                return true;
            }
        }
        else
        {
            switch (wParam)
            {
            case WM_LBUTTONUP:
                LButtonDownCounter = 0;
                SavedEvent.time    = 0;

                if (!LButtonDown)
                {
                    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
                    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                    LButtonDown = false;
                    Log.Log("Button up simulated");
                    return true;
                }
                else
                {
                    LButtonDown = false;
                    Log.Log("Button up pass through");
                    return false;
                }
            case WM_MOUSEMOVE:
                if (LButtonDownCounter > 0 && TooClose(msllhs.pt, SavedEvent.pt))
                {
                    Log.Log("Skip too short move");
                    return true;
                }

                // Sticky button: drag mode
                LButtonDownCounter = 0;

                if (LButtonDown)
                {
                    Log.Log("wParam=0x%016llX flags=0x%08X dwExtraInfo=%016llX (%4d,%4d) time=%u"
                        , wParam, msllhs.flags, msllhs.dwExtraInfo, msllhs.pt.x, msllhs.pt.y, msllhs.time);
                    return false;
                }
                else
                {
                    // After Timer implementation, this code should be unreachable:
                    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
                    mouse_event(MOUSEEVENTF_MOVE, msllhs.pt.x - SavedEvent.pt.x, msllhs.pt.y - SavedEvent.pt.y, 0, 0);
                    LButtonDown = true;
                    return true;
                }
            }
        }

        // This line is unreachable, actually!
        return true;
    }

    if (wParam == WM_LBUTTONUP)
    {
        Log.Log("Button up processed");
        return true;
    }

    return false;
}

void HookDllImpl::TimerProc()
{
    std::lock_guard<std::mutex> lock(Mutex);
    if (!Timer) return; // Too late!

    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
    LButtonDown = true;
    Log.Log("Button down by timer");
}

HookDll::HookDll()
{
    Instance = new HookDllImpl();
}

HookDll::~HookDll()
{
    HookDllImpl* impl = Instance;
    if (impl)
    {
        Instance = nullptr;
        delete impl;
    }
}

bool HookDll::Disabled()
{
    return Instance == nullptr || Instance->Disabled();
}

void HookDll::Enable(bool enable)
{
    if (Instance) Instance->Enable(enable);
}
