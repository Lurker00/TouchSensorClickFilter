#ifndef Logger_H_
#define Logger_H_

#include <stdio.h>

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

#endif // Logger_H_
