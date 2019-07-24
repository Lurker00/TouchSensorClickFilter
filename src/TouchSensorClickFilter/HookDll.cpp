// HookDll.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "HookDll.h"

#include <stdio.h>
#include <mutex>

class LoggerImpl {
public:
    LoggerImpl()
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

    ~LoggerImpl()
    {
#if _DEBUG
        if (fLog) fclose(fLog);
#endif
    }

    template<typename ...Args>
    void Log(Args ... args)
    {
#if _DEBUG
        if (!fLog) return;
        fprintf(fLog, "[%10u] ", ::GetTickCount());
        fprintf(fLog, args...);
        fflush(fLog);
#endif
    }

private:
#if _DEBUG
    FILE* fLog = NULL;
#endif
};

class HookDllImpl {
public:
    HookDllImpl();
    ~HookDllImpl();

    bool LowLevelMouseProc(WPARAM wParam, const MSLLHOOKSTRUCT& msllhs);
    bool Disabled() const { return Disable; }
    void Enable(bool enable) { Disable = !enable; }
    HHOOK hHook() const { return Hook; }

private:
    template<typename ...Args>
    void Log(Args ... args) { Logger.Log(args...); }

    HHOOK Hook = 0;
    bool  Disable = false;
    MSLLHOOKSTRUCT SavedEvent = { 0, };
    unsigned LButtonDownCounter = 0;
    bool LButtonDown = false;
    std::mutex Mutex;
    LoggerImpl Logger;
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

    if (Instance->LowLevelMouseProc(wParam, msllhs))
        return 1; // Stop further processing

    return ::CallNextHookEx(hHook, nCode, wParam, lParam);
}

HookDllImpl::HookDllImpl() {
    HINSTANCE hSelf = NULL;
    ::GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (wchar_t *)::LowLevelMouseProc, &hSelf);
    Hook = ::SetWindowsHookEx(WH_MOUSE_LL, ::LowLevelMouseProc, hSelf, 0);
}

HookDllImpl::~HookDllImpl() {
    if (Hook)
        ::UnhookWindowsHookEx(Hook);
}

bool HookDllImpl::LowLevelMouseProc(WPARAM wParam, const MSLLHOOKSTRUCT& msllhs)
{
    std::unique_lock<std::mutex> lock(Mutex, std::try_to_lock);
    if (!lock.owns_lock())
    {
        Log("Can't lock\n");
        return false;
    }

    if (wParam == WM_LBUTTONUP || wParam == WM_LBUTTONDOWN)
    {
        Log("wParam=0x%016llX flags=0x%08X dwExtraInfo=%016llX (%4d,%4d) time=%u\n"
            , wParam, msllhs.flags, msllhs.dwExtraInfo, msllhs.pt.x, msllhs.pt.y, msllhs.time);
    }

    if (wParam == WM_LBUTTONDOWN)
    {
        if (LButtonDownCounter == 0) SavedEvent = msllhs;
        else                         SavedEvent.time = msllhs.time;
        LButtonDownCounter++;
        return true;
    }

    if (wParam == WM_LBUTTONUP && LButtonDownCounter == 0)
    {
        // Sticky drag mode?
        Log("Excess button up\n");
        return true;
    }

    if (wParam == WM_LBUTTONUP && LButtonDownCounter > 1)
    {
        Log("Nested button up\n");
        LButtonDownCounter--;
        return true;
    }

    if ((wParam == WM_LBUTTONUP || wParam == WM_MOUSEMOVE) && SavedEvent.time != 0)
    {
        // Disable too fast clicks!
        if (SavedEvent.time <= msllhs.time && msllhs.time - SavedEvent.time < 50)
        {
            if (wParam == WM_LBUTTONUP)
            {
                SavedEvent.time = 0;
                LButtonDownCounter = 0;
                LButtonDown = false;
                Log("Skip too short click\n");
            }
            else
            {
                Log("Skip too short move\n");
            }
        }
        else
        {
            switch (wParam)
            {
            case WM_LBUTTONUP:
                if (!LButtonDown)
                    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
                mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                Log("Button up simulated\n");
                LButtonDownCounter = 0;
                SavedEvent.time = 0;
                LButtonDown = false;
                break;
            case WM_MOUSEMOVE:
                if (LButtonDown) return false;

                mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
                mouse_event(MOUSEEVENTF_MOVE, msllhs.pt.x - SavedEvent.pt.x, msllhs.pt.y - SavedEvent.pt.y, 0, 0);
                LButtonDown = true;
                // Sticky button: drag mode
                LButtonDownCounter = 0;
                break;
            }
        }

        return true;
    }

    if (wParam == WM_LBUTTONUP)
    {
        Log("Button up processed\n");
        return true;
    }

    return false;
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
