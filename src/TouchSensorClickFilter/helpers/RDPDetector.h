#ifndef RDPDetector_H_
#define RDPDetector_H_

class RDPDetector {
public:
    bool Detected() {
#if !_DEBUG
        DWORD now = ::GetTickCount();
        if (now < LastCheck || now - LastCheck > 5000)
            IsRDP = ::GetSystemMetrics(SM_REMOTESESSION);
#endif
        return IsRDP;
    }
private:
    DWORD LastCheck = 0;
    bool IsRDP      = false;
};

#endif // RDPDetector_H_
