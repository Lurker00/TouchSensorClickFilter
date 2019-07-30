#ifndef CriticalSection_H_
#define CriticalSection_H_

class CriticalSection
{
public:
    CriticalSection()  { ::InitializeCriticalSection(&CS); }
    ~CriticalSection() { ::DeleteCriticalSection(&CS); }

    void Enter() { ::EnterCriticalSection(&CS); }
    void Leave() { ::LeaveCriticalSection(&CS); }

private:
    CRITICAL_SECTION CS;

public:
    class AutoLock
    {
    public:
        AutoLock(CriticalSection& cs) : CS(cs) { CS.Enter(); }
        ~AutoLock() { CS.Leave(); }
    protected:
        CriticalSection& CS;
    };
};

#endif // CriticalSection_H_
