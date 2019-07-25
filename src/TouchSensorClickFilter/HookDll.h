#ifndef HOOKDLL_H_
#define HOOKDLL_H_

// Actually, WH_MOUSE_LL does not require DLL!
#if defined(HOOKDLL_EXPORTS)
#   define HookDllSpec __declspec(dllexport)
#else
//#   define HookDllSpec __declspec(dllimport)
#   define HookDllSpec
#endif

class HookDllSpec HookDll {
public:
    HookDll();
    ~HookDll();

    static bool Disabled();
    static void Enable(bool enable);

    unsigned Clicks() const;
    unsigned Moves()  const;
};

#endif // HOOKDLL_H_
