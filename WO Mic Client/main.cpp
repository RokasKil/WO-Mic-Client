#ifndef UNICODE
#define UNICODE
#endif

#ifndef _UNICODE
#define _UNICODE
#endif

#include "src/client/WoMicClient.h"
#include <tchar.h>
#include <windows.h>
#include <functional>
#include <sstream>
#include <windowsx.h>
#include <commctrl.h>

using namespace std;
using namespace std::placeholders;

/*  Declare Windows procedure  */
LRESULT CALLBACK WindowProcedure (HWND, UINT, WPARAM, LPARAM);

TCHAR szClassName[ ] = _T("WoMicClient");

struct State {
    HWND hWindow;
    HWND hStartButton;
    HWND hStopButton;

    HWND hStatusText;

    HWND hFailCodeText;

    HWND hBufferSizeText;
    HWND hClearBufferButton;

    HWND hDevicesText;
    HWND hDevices;
    HWND hDevicesRefreshButton;
    wstring device;

    HWND hIPAddressText;
    HWND hIPAddress;
    wstring ipAddress;

    HWND hClientPortText;
    HWND hClientPort;
    bool clientPortInvalid = false;
    unsigned short clientPort;

    HWND hServerPortText;
    HWND hServerPort;
    bool serverPortInvalid = false;
    unsigned short serverPort;

    HWND hCutOffText;
    HWND hCutOff;
    bool cutOffInvalid = false;
    float cutOff = 0;

    HWND hSpeedOffText;
    HWND hSpeedOff;
    bool speedOffInvalid = false;
    float speedOff = 0;

    HWND hAutoReconnect;
    bool autoReconnect;

    HWND hApplyButton;
    HWND hSaveButton;

    int failCode = CLIENT_E_OK;
    bool canStart = true;
    bool canStop = false;
    unique_ptr<WoMicClient> pClient = make_unique<WoMicClient>();
};

void clientStartCallback(State *pState, int status);
void clientStopCallback(State *pState, int status);
void clientFailCallback(State *pState, int status);
void enableControls(State *pState);
void getDevices(State *pState);

wstring toWStringWithPrecision(const float value, const int n);

int WINAPI WinMain (HINSTANCE hThisInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR lpszArgument,
                     int nCmdShow)
{
    HWND hwnd;               /* This is the handle for our window */
    MSG messages;            /* Here messages to the application are saved */
    WNDCLASSEX wincl;        /* Data structure for the windowclass */
    unique_ptr<State> pState = make_unique<State>();

    /* The Window structure */
    wincl.hInstance = hThisInstance;
    wincl.lpszClassName = szClassName;
    wincl.lpfnWndProc = WindowProcedure;      /* This function is called by windows */
    wincl.style = CS_DBLCLKS;                 /* Catch double-clicks */
    wincl.cbSize = sizeof (WNDCLASSEX);

    /* Use default icon and mouse-pointer */
    wincl.hIcon = LoadIcon (NULL, IDI_APPLICATION);
    wincl.hIconSm = LoadIcon (NULL, IDI_APPLICATION);
    wincl.hCursor = LoadCursor (NULL, IDC_ARROW);
    wincl.lpszMenuName = NULL;                 /* No menu */
    wincl.cbClsExtra = 0;                      /* No extra bytes after the window class */
    wincl.cbWndExtra = 0;                      /* structure or the window instance */
    /* Use Windows's default colour as the background of the window */
    wincl.hbrBackground = (HBRUSH) COLOR_BACKGROUND;

    /* Register the window class, and if it fails quit the program */
    if (!RegisterClassEx (&wincl))
        return 0;


    //cout << "Start result \n" << client->start();

    /* The class is registered, let's create the program*/
    hwnd = CreateWindowEx (
           0,                   /* Extended possibilites for variation */
           szClassName,         /* Classname */
           _T("Code::Blocks Template Windows App"),       /* Title Text */
           WS_OVERLAPPEDWINDOW, /* default window */
           CW_USEDEFAULT,       /* Windows decides the position */
           CW_USEDEFAULT,       /* where the window ends up on the screen */
           340,                 /* The programs width */
           550,                 /* and height in pixels */
           HWND_DESKTOP,        /* The window is a child-window to desktop */
           NULL,                /* No menu */
           hThisInstance,       /* Program Instance handler */
           pState.get()
           );

    /* Make the window visible on the screen */
    ShowWindow (hwnd, nCmdShow);

    /* Run the message loop. It will run until GetMessage() returns 0 */
    while (GetMessage (&messages, NULL, 0, 0))
    {
        /* Translate virtual-key messages into character messages */
        TranslateMessage(&messages);
        /* Send message to WindowProcedure */
        DispatchMessage(&messages);
    }
    /* The program return-value is 0 - The value that PostQuitMessage() gave */
    return messages.wParam;
}


/*  This function is called by the Windows function DispatchMessage()  */

LRESULT CALLBACK WindowProcedure (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {

    State *pState;
    if (message == WM_CREATE){
        CREATESTRUCT *pCreateStruct = reinterpret_cast<CREATESTRUCT*>(lParam);
        pState = reinterpret_cast<State*>(pCreateStruct->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pState);
    }
    else {
        pState = reinterpret_cast<State*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
    switch (message) {                  /* handle the messages */
        case WM_DESTROY:
            PostQuitMessage (0);       /* send a WM_QUIT to the message queue */
            break;
        case WM_CREATE:
            {
                cout << "WM_CREATE " << hwnd  << " " << wParam << " " << lParam << endl;
                CREATESTRUCT *pCreateStruct = reinterpret_cast<CREATESTRUCT *> (lParam);
                cout << pCreateStruct->hInstance;
                pState->hWindow = hwnd;

                pState->hDevicesText = CreateWindow(TEXT("STATIC"), TEXT("Device:"), WS_VISIBLE | WS_CHILD | SS_LEFTNOWORDWRAP, 5, 5, 200, 20, hwnd, (HMENU) 6, pCreateStruct->hInstance, NULL);
                pState->hDevices = CreateWindow(TEXT("COMBOBOX"), TEXT(""), CBS_DROPDOWN | CBS_HASSTRINGS | WS_CHILD | WS_OVERLAPPED | WS_VISIBLE, 5, 25, 305, 200, hwnd, (HMENU) 7, pCreateStruct->hInstance, NULL);
                pState->hDevicesRefreshButton = CreateWindow(TEXT("button"), TEXT("Refresh"), WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 210, 5, 100, 20, hwnd, (HMENU) 19, pCreateStruct->hInstance, NULL);


                pState->hIPAddressText = CreateWindow(TEXT("STATIC"), TEXT("Address:"), WS_VISIBLE | WS_CHILD | SS_LEFTNOWORDWRAP, 5, 55, 200, 20, hwnd, (HMENU) 8, pCreateStruct->hInstance, NULL);
                pState->hIPAddress = CreateWindow(TEXT("EDIT"), NULL, WS_BORDER | WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOHSCROLL, 5, 75, 200, 20, hwnd, (HMENU) 9, pCreateStruct->hInstance, NULL);
                SendMessage(pState->hIPAddress, EM_SETLIMITTEXT, 1024, 0);

                pState->hServerPortText = CreateWindow(TEXT("STATIC"), TEXT("Port:"), WS_VISIBLE | WS_CHILD | SS_LEFTNOWORDWRAP, 5, 105, 200, 20, hwnd, (HMENU) 10, pCreateStruct->hInstance, NULL);
                pState->hServerPort = CreateWindow(TEXT("EDIT"), NULL, WS_BORDER | WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOHSCROLL | ES_NUMBER, 5, 125, 200, 20, hwnd, (HMENU) 11, pCreateStruct->hInstance, NULL);
                SendMessage(pState->hServerPort, EM_SETLIMITTEXT, 5, 0);

                pState->hClientPortText = CreateWindow(TEXT("STATIC"), TEXT("UDP server port:"), WS_VISIBLE | WS_CHILD | SS_LEFTNOWORDWRAP, 5, 155, 200, 20, hwnd, (HMENU) 12, pCreateStruct->hInstance, NULL);
                pState->hClientPort = CreateWindow(TEXT("EDIT"), NULL, WS_BORDER | WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOHSCROLL | ES_NUMBER, 5, 175, 200, 20, hwnd, (HMENU) 13, pCreateStruct->hInstance, NULL);
                SendMessage(pState->hClientPort, EM_SETLIMITTEXT, 5, 0);

                pState->hCutOffText = CreateWindow(TEXT("STATIC"), TEXT("Cut off in seconds:"), WS_VISIBLE | WS_CHILD | SS_LEFTNOWORDWRAP, 5, 205, 200, 20, hwnd, (HMENU) 14, pCreateStruct->hInstance, NULL);
                pState->hCutOff = CreateWindow(TEXT("EDIT"), NULL, WS_BORDER | WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOHSCROLL, 5, 225, 200, 20, hwnd, (HMENU) 15, pCreateStruct->hInstance, NULL);
                SendMessage(pState->hCutOff, EM_SETLIMITTEXT, 10, 0);

                pState->hSpeedOffText = CreateWindow(TEXT("STATIC"), TEXT("Speed up in seconds:"), WS_VISIBLE | WS_CHILD | SS_LEFTNOWORDWRAP, 5, 255, 200, 20, hwnd, (HMENU) 16, pCreateStruct->hInstance, NULL);
                pState->hSpeedOff = CreateWindow(TEXT("EDIT"), NULL, WS_BORDER | WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOHSCROLL, 5, 275, 200, 20, hwnd, (HMENU) 17, pCreateStruct->hInstance, NULL);
                SendMessage(pState->hSpeedOff, EM_SETLIMITTEXT, 10, 0);

                pState->hAutoReconnect = CreateWindow(TEXT("BUTTON"), TEXT("Auto reconnect"), WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_CHECKBOX, 5, 305, 200, 20, hwnd, (HMENU) 18, pCreateStruct->hInstance, NULL);

                pState->hApplyButton = CreateWindow(TEXT("button"), TEXT("Apply"), WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 5, 335, 100, 30, hwnd, (HMENU) 19, pCreateStruct->hInstance, NULL);
                pState->hSaveButton = CreateWindow(TEXT("button"), TEXT("Save"), WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON | WS_DISABLED, 110, 335, 100, 30, hwnd, (HMENU) 20, pCreateStruct->hInstance, NULL);

                pState->hStatusText = CreateWindow(TEXT("STATIC"), TEXT("status"), WS_VISIBLE | WS_CHILD | SS_LEFTNOWORDWRAP, 5, 375, 400, 20, hwnd, (HMENU) 3, pCreateStruct->hInstance, NULL);
                pState->hFailCodeText = CreateWindow(TEXT("STATIC"), TEXT("failcode"), WS_VISIBLE | WS_CHILD | SS_LEFTNOWORDWRAP, 5, 395, 400, 20, hwnd, (HMENU) 4, pCreateStruct->hInstance, NULL);
                pState->hBufferSizeText = CreateWindow(TEXT("STATIC"), TEXT("bufferSize"), WS_VISIBLE | WS_CHILD | SS_LEFTNOWORDWRAP, 5, 415, 400, 20, hwnd, (HMENU) 5, pCreateStruct->hInstance, NULL);

                pState->hClearBufferButton = CreateWindow(TEXT("button"), TEXT("Clear buffer"), WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 5, 445, 100, 30, hwnd, (HMENU) 21, pCreateStruct->hInstance, NULL);

                pState->hStartButton = CreateWindow(TEXT("button"), TEXT("Start"), WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 110, 445, 100, 30, hwnd, (HMENU) 1, pCreateStruct->hInstance, NULL);
                pState->hStopButton = CreateWindow(TEXT("button"), TEXT("Stop"), WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON | WS_DISABLED, 215, 445, 100, 30, hwnd, (HMENU) 2, pCreateStruct->hInstance, NULL);


                SetTimer(hwnd,             // handle to main window
                    6,            // timer identifier
                    100,                 // 0.1 second interval
                    (TIMERPROC) NULL);     // no timer callback

                pState->pClient->setIp("192.168.1.114")->setServerPort(8125)->setClientPort(34568)->setDevice(L"")->setCutOff(0.3)->setSpeedOff(0.05)->setAutoReconnect(true)->setFailCallback(bind(clientFailCallback, pState, _1));
                getDevices(pState);
            }
            return 0;
        case WM_COMMAND:
            cout << "WM_COMMAND " << hwnd << " " << HIWORD(wParam) << " " << LOWORD(wParam) << " " << lParam << " " << endl;
            if (HIWORD(wParam) == 0) {
                switch (LOWORD(wParam)) {
                    case 1:
                        if (pState->canStart) {
                            pState->canStart = false;
                            pState->pClient->startAsync(bind(clientStartCallback, pState, _1));
                            EnableWindow(pState->hStartButton, false);
                            pState->failCode = CLIENT_E_OK;
                        }
                        break;
                    case 2:
                        if (pState->canStop) {
                            pState->canStop = false;
                            pState->pClient->stopAsync(bind(clientStopCallback, pState, _1));
                            EnableWindow(pState->hStopButton, false);
                        }
                        break;
                    case 19:
                        getDevices(pState);
                        break;
                }
            }
            return 0;
        case WM_TIMER:
            switch (wParam)
            {
                case 6:
                    SetWindowText(pState->hStatusText, (L"Status: " + to_wstring(pState->pClient->getStatus())).c_str());

                    SetWindowText(pState->hBufferSizeText, (L"Buffer left: " + toWStringWithPrecision(pState->pClient->bufferLeft(), 3) + L"s (" + to_wstring(pState->pClient->getSamplesInBuffer())+ L" samples)").c_str());
                    break;
            }
            return 0;
        default:                      /* for messages that we don't deal with */
            return DefWindowProc (hwnd, message, wParam, lParam);
    }

    return 0;
}


void clientStartCallback(State *pState, int status) {
    cout << "start callback " << status << " " << pState << endl;
    bool success = status == CLIENT_E_OK;
    pState->canStart = !success;
    pState->canStop = success;
    enableControls(pState);
    pState->failCode = status;
    SetWindowText(pState->hFailCodeText, to_wstring(status).c_str());
}

void clientStopCallback(State *pState, int status) {
    cout << "stop callback " << status << " " << pState << endl;

    bool success = status == CLIENT_E_OK;
    pState->canStart = success;
    pState->canStop = !success;
    enableControls(pState);
}

void clientFailCallback(State *pState, int status) {
    cout << "fail callback " << status << " " << pState << endl;

    pState->canStart = true;
    pState->canStop = false;
    enableControls(pState);
    pState->failCode = status;
    SetWindowText(pState->hFailCodeText, to_wstring(status).c_str());
}

void enableControls(State *pState) {
    EnableWindow(pState->hStartButton, pState->canStart);
    EnableWindow(pState->hApplyButton, pState->canStart);
    EnableWindow(pState->hSaveButton, pState->canStart);

    EnableWindow(pState->hStopButton, pState->canStop);

}

void getDevices(State *pState) {
    vector<wstring> devices;
    int result = pState->pClient->getAvailableDevices(devices);
    cout << "getDevices result " << result << endl << "Got " << devices.size() << " devices" << endl;
    if (result != CLIENT_E_OK) {
        // throw error here
    }
    else {
        SendMessage(pState->hDevices, CB_RESETCONTENT, 0, 0);
        for (auto it = devices.begin(); it != devices.end(); it++) {
            SendMessage(pState->hDevices, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>((*it).c_str()));
        }
        SendMessage(pState->hDevices, CB_SETCURSEL, 0, 0);
    }
}

wstring toWStringWithPrecision(const float value, const int n) {
    std::wostringstream out;
    out.precision(n);
    out << std::fixed << value;
    return out.str();
}
