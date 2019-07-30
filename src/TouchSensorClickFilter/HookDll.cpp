// HookDll.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "HookDll.h"

#include "helpers/Logger.h"
#include "helpers/RDPDetector.h"
#include "helpers/CriticalSection.h"

class HookDllImpl {
    /********************************************************************
        In no a test I was able to click faster than in 50 ms.
        In all the tests, false clicks by touch sensor were under 16 ms.
    *********************************************************************/
    static const unsigned TOO_FAST_CLICK = 50; // ms
public:
    HookDllImpl();
    ~HookDllImpl();

    bool LowLevelMouseProc(WPARAM wParam, const MSLLHOOKSTRUCT& msllhs);
    void TimerProc();

    bool Disabled() { return Disable || Remote.Detected(); }
    void Enable(bool enable) { Disable = !enable; }

    HHOOK hHook() const { return Hook; }

    unsigned Clicks() const { return ClicksEliminated; }
    unsigned Moves()  const { return MovesEliminated;  }

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

    void SimulateLeftDown()
    {
        StopTimer();
        mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
        LButtonDown = true;
    }

    HHOOK  Hook    = 0;
    HANDLE Timer   = 0;
    bool   Disable = false;
    MSLLHOOKSTRUCT SavedEvent = { 0, };
    unsigned LButtonDownCounter = 0;
    bool LButtonDown = false;
    CriticalSection Mutex;

    RDPDetector Remote;
    unsigned ClicksEliminated = 0;
    unsigned MovesEliminated  = 0;
};

static HookDllImpl* Instance = nullptr;

static bool MyEvent(WPARAM wParam)
{
    // Only the events that HookDllImplHookDllImpl::LowLevelMouseProc expects
    static const WPARAM events[] = { WM_MOUSEMOVE, WM_LBUTTONDOWN, WM_LBUTTONUP };
    for (auto e : events)
        if (e == wParam) return true;
    return false;
}

// Windows API callback
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

// Windows API callback
static void CALLBACK TimerProc(void* /*lpParametar*/, BOOLEAN /*TimerOrWaitFired*/)
{
    if (Instance == nullptr)
        return; // Should never happen!

    Instance->TimerProc();
}

HookDllImpl::HookDllImpl() {
    HINSTANCE hSelf = NULL;
    ::GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (TCHAR *)::LowLevelMouseProc, &hSelf);
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
    CriticalSection::AutoLock lock(Mutex);

    if (SavedEvent.time == 0 && Remote.Detected())
        return false; // Stop if idle and in RDP

    if (wParam == WM_LBUTTONUP || wParam == WM_LBUTTONDOWN)
    {
        Log.Log("wParam=0x%016llX flags=0x%08X dwExtraInfo=%016llX (%4d,%4d) time=%u"
            , wParam, msllhs.flags, msllhs.dwExtraInfo, msllhs.pt.x, msllhs.pt.y, msllhs.time);
    }

    if (wParam == WM_LBUTTONDOWN)
    {
        if (LButtonDownCounter == 0 && !LButtonDown)
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

    if (SavedEvent.time != 0)
    { // WM_LBUTTONDOWN was detected
        const bool tooFast = SavedEvent.time <= msllhs.time && msllhs.time - SavedEvent.time < TOO_FAST_CLICK;

        if (wParam == WM_LBUTTONUP)
        {
            LButtonDownCounter = 0;
            SavedEvent.time    = 0;

            if (tooFast)
            {
                StopTimer();
                ClicksEliminated++;
                Log.Log("Skip too fast click");
                return true;
            }

            if (LButtonDown)
            {
                LButtonDown = false;
                Log.Log("Button up pass through");
                return false;
            }
            else
            {
                // After Timer implementation, this code should be unreachable:
                SimulateLeftDown();
                mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                LButtonDown = false;
                Log.Log("Button up simulated");
                return true;
            }
        }

        if (wParam == WM_MOUSEMOVE)
        {
            if (tooFast)
            {
                MovesEliminated++;
                Log.Log("Skip too fast move");
                return true;
            }

            if (LButtonDownCounter > 0 && TooClose(msllhs.pt, SavedEvent.pt))
            {
                MovesEliminated++;
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
                SimulateLeftDown();
                mouse_event(MOUSEEVENTF_MOVE, msllhs.pt.x - SavedEvent.pt.x, msllhs.pt.y - SavedEvent.pt.y, 0, 0);
                return true;
            }
        }
    }

    if (wParam == WM_LBUTTONUP)
    {
        // This code should be unreachable
        Log.Log("Button up processed");
        return true;
    }

    // This code should be unreachable
    return false;
}

void HookDllImpl::TimerProc()
{
    CriticalSection::AutoLock lock(Mutex);
    if (!Timer) return; // Too late!

    Log.Log("Button down by timer");
    SimulateLeftDown();
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

unsigned HookDll::Clicks() { return Instance == nullptr ? 0 : Instance->Clicks(); }
unsigned HookDll::Moves()  { return Instance == nullptr ? 0 : Instance->Moves();  }
