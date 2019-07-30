#ifndef EventReporter_H_
#define EventReporter_H_

class EventReporter
{
public:
    EventReporter(const TCHAR* eventSource)
    {
        hEventSource = RegisterEventSource(NULL, eventSource);
        pInstance = this;
    }
    ~EventReporter()
    {
        if (hEventSource != NULL)
            ::DeregisterEventSource(hEventSource);
        pInstance = nullptr;
    }

    enum Events { eGeneric, eStart, eStop, eDaemonize, eAlreadyRunning };
    void Report(const TCHAR* event, Events eventId, WORD wType = EVENTLOG_SUCCESS)
    {
        if (hEventSource != NULL)
            ReportEvent(hEventSource, wType, 0, eventId, NULL, 1, 0, &event, NULL);
    }

    static EventReporter& Instance() { return *pInstance; }
private:
    HANDLE hEventSource;
    static EventReporter* pInstance;
};
EventReporter* EventReporter::pInstance = nullptr;

#endif // EventReporter_H_
