// SysTrayDemo.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "resource.h"

#include "HookDll.h"

#define MAX_LOADSTRING 100
#define	WM_USER_SHELLICON WM_USER + 1

// Global Variables:
HINSTANCE hInst;                             // current instance
NOTIFYICONDATA nidApp;
TCHAR szTitle[MAX_LOADSTRING];               // The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];         // the main window class name
TCHAR szApplicationToolTip[MAX_LOADSTRING];  // the systray icon tooltip

// Forward declarations of functions included in this code module:
ATOM             MyRegisterClass(HINSTANCE hInstance);
BOOL             InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

class EventReporter
{
public:
    EventReporter(const TCHAR* eventSource)
    {
        hEventSource = RegisterEventSource(NULL, eventSource);
    }
    ~EventReporter()
    {
        if (hEventSource != NULL)
            ::DeregisterEventSource(hEventSource);
    }
    enum Events
    {
        eGeneric, eStart, eStop, eDaemonize, eAlreadyRunning
    };
    void Report(const TCHAR* event, Events eventId, WORD wType = EVENTLOG_SUCCESS)
    {
        if (hEventSource != NULL)
            ReportEvent(hEventSource, wType, 0, eventId, NULL, 1, 0, &event, NULL);
    }
private:
    HANDLE hEventSource;
};

int APIENTRY _tWinMain(HINSTANCE hInstance,
                       HINSTANCE hPrevInstance,
                       LPTSTR    lpCmdLine,
                       int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);

    // Initialize global strings
    LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadString(hInstance, IDC_APP_CLASS, szWindowClass, MAX_LOADSTRING);

    EventReporter Reporter(szWindowClass);

    // Only one instance is allowed:
    if (FindWindow(szWindowClass, szTitle) != NULL)
    {
        Reporter.Report(_T("Already running, quit"), EventReporter::eAlreadyRunning, EVENTLOG_WARNING_TYPE);
        return 0;
    }

    if (_tcsstr(lpCmdLine, _T("daemonize")) != NULL)
    { // "Fork" itself and quit
        STARTUPINFO si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        TCHAR const* pgmptr =
#ifdef _UNICODE
            _wpgmptr;
#else
            _pgmptr;
#endif
        if (CreateProcess(pgmptr, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
        {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            Reporter.Report(_T("Daemonizing"), EventReporter::eDaemonize);
        }
        else
            Reporter.Report(_T("Failed to daemonize, quit"), EventReporter::eGeneric, EVENTLOG_ERROR_TYPE);
        return 0;
    }

    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance(hInstance, nCmdShow))
    {
        Reporter.Report(_T("InitInstance failed, quit"), EventReporter::eGeneric, EVENTLOG_ERROR_TYPE);
        return 0;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_APP_CLASS));

    Reporter.Report(_T("Started"), EventReporter::eStart);

    HookDll hook; // Install mouse hook

    // Main message loop:
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    Reporter.Report(_T("Stopped"), EventReporter::eStop);

    return (int)msg.wParam;
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
//    This function and its usage are only necessary if you want this code
//    to be compatible with Win32 systems prior to the 'RegisterClassEx'
//    function that was added to Windows 95. It is important to call this function
//    so that the application will get 'well formed' small icons associated
//    with it.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style         = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc   = WndProc;
    wcex.cbClsExtra    = 0;
    wcex.cbWndExtra    = 0;
    wcex.hInstance     = hInstance;
    wcex.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP_ICON));
    wcex.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName  = MAKEINTRESOURCE(IDC_APP_CLASS);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm       = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP_ICON));

    return RegisterClassEx(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int /*nCmdShow*/)
{
    hInst = hInstance; // Store instance handle in our global variable

    HWND hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

    if (!hWnd) return FALSE;

    HICON hMainIcon = LoadIcon(hInstance, (LPCTSTR)MAKEINTRESOURCE(IDI_APP_ICON));

    nidApp.cbSize = sizeof(NOTIFYICONDATA);           // sizeof the struct in bytes 
    nidApp.hWnd = (HWND)hWnd;                         // handle of the window which will process this app. messages 
    nidApp.uID = IDI_APP_ICON;                        // ID of the icon that will appear in the system tray 
    nidApp.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP; // ORing of all the flags 
    nidApp.hIcon = hMainIcon;                         // handle of the Icon to be displayed, obtained from LoadIcon 
    nidApp.uCallbackMessage = WM_USER_SHELLICON;
    LoadString(hInstance, IDS_APPTOOLTIP, nidApp.szTip, MAX_LOADSTRING);
    Shell_NotifyIcon(NIM_ADD, &nidApp);

    return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int wmId, wmEvent;
    POINT lpClickPoint;

    switch (message)
    {
    case WM_USER_SHELLICON:
        // systray msg callback 
        switch (LOWORD(lParam))
        {
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
            GetCursorPos(&lpClickPoint);
            HMENU hPopMenu = CreatePopupMenu();
            InsertMenu(hPopMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, IDM_ABOUT, _T("About"));
            InsertMenu(hPopMenu, 0xFFFFFFFF, MF_SEPARATOR, IDM_SEP, _T("SEP"));
            if (HookDll::Disabled())
                InsertMenu(hPopMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, IDM_ENABLE, _T("Enable"));
            else
                InsertMenu(hPopMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, IDM_DISABLE, _T("Disable"));
            InsertMenu(hPopMenu, 0xFFFFFFFF, MF_SEPARATOR, IDM_SEP, _T("SEP"));
            InsertMenu(hPopMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, IDM_EXIT, _T("Exit"));

            SetForegroundWindow(hWnd);
            TrackPopupMenu(hPopMenu, TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_BOTTOMALIGN, lpClickPoint.x, lpClickPoint.y, 0, hWnd, NULL);
            return TRUE;
        }
        break;
    case WM_COMMAND:
        wmId    = LOWORD(wParam);
        wmEvent = HIWORD(wParam);
        // Parse the menu selections:
        switch (wmId)
        {
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_DISABLE:
            HookDll::Enable(false);
            break;
        case IDM_ENABLE:
            HookDll::Enable(true);
            break;
        case IDM_EXIT:
            Shell_NotifyIcon(NIM_DELETE, &nidApp);
            DestroyWindow(hWnd);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;
    /*
    case WM_PAINT:
        hdc = BeginPaint(hWnd, &ps);
        // TODO: Add any drawing code here...
        EndPaint(hWnd, &ps);
        break;
    */
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
