#ifndef UNICODE
#define UNICODE
#endif

#ifndef _UNICODE
#define _UNICODE
#endif

#define WINVER 0x0600
#define _WIN32_WINNT 0x0600

#include "src/client/WoMicClient.h"
#include <tchar.h>
#include <windows.h>
#include <functional>
#include <sstream>
#include <windowsx.h>
#include <commctrl.h>
#include <Shlobj.h>
#include <Knownfolders.h>
#include "src/resource/resources.h"

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

    HWND hBufferSizeText;
    HWND hClearBufferButton;

    HWND hDevicesText;
    HWND hDevices;
    HWND hDevicesRefreshButton;
    wstring device;

    HWND hIPAddressText;
    HWND hIPAddress;
    string ipAddress;

    HWND hClientPortText;
    HWND hClientPort;
    unsigned short clientPort;

    HWND hServerPortText;
    HWND hServerPort;
    unsigned short serverPort;

    HWND hCutOffText;
    HWND hCutOff;
    float cutOff;

    HWND hSpeedOffText;
    HWND hSpeedOff;
    float speedOff;

    HWND hAutoReconnect;
    bool autoReconnect;

    HWND hConnectOnStartup;
    bool connectOnStartup;

    HWND hNotifyAutoReconnect;
    bool notifyAutoReconnect;

    HWND hApplyButton;
    HWND hSaveButton;
    HWND hDefaultButton;

    HICON refreshIcon = NULL;

    int failCode = CLIENT_E_OK;
    bool canStart = true;
    bool canStop = false;
    vector<wstring> devices;
    unique_ptr<WoMicClient> pClient = make_unique<WoMicClient>();
};

void clientStartCallback(State *pState, int status);
void clientStopCallback(State *pState, int status);
void clientFailCallback(State *pState, int status);
void enableControls(State *pState);
void getDevices(State *pState);

wstring toWStringWithPrecision(const float value, const int n);

wstring getSavePath();
bool saveSettings(State *pState);
bool loadSettings(State *pState);
void defaultSettings(State *pState);
WoMicClient* setSettings(State *pState);
void loadSettingsToUI(State *pState);
wstring getString(UINT id);
const wchar_t* getCString(UINT id);

int WINAPI WinMain (HINSTANCE hThisInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR lpszArgument,
                     int nCmdShow)
{
    HWND hwnd;               /* This is the handle for our window */
    MSG messages;            /* Here messages to the application are saved */
    WNDCLASSEX wincl;        /* Data structure for the windowclass */
    unique_ptr<State> pState = make_unique<State>();
    loadSettings(pState.get());
    InitCommonControls();
    /* The Window structure */
    wincl.hInstance = hThisInstance;
    wincl.lpszClassName = szClassName;
    wincl.lpfnWndProc = WindowProcedure;      /* This function is called by windows */
    wincl.style = CS_DBLCLKS;                 /* Catch double-clicks */
    wincl.cbSize = sizeof (WNDCLASSEX);

    /* Use default icon and mouse-pointer */
    wincl.hIcon = LoadIcon (wincl.hInstance, MAKEINTRESOURCE(IDI_APP));
    wincl.hIconSm = LoadIcon (wincl.hInstance, MAKEINTRESOURCE(IDI_APP));
    wincl.hCursor = LoadCursor (NULL, IDC_ARROW);
    wincl.lpszMenuName = NULL;                 /* No menu */
    wincl.cbClsExtra = 0;                      /* No extra bytes after the window class */
    wincl.cbWndExtra = 0;                      /* structure or the window instance */
    /* Use Windows's default colour as the background of the window */
    wincl.hbrBackground = (HBRUSH) COLOR_WINDOW;

    /* Register the window class, and if it fails quit the program */
    if (!RegisterClassEx (&wincl))
        return 0;


    //cout << "Start result \n" << client->start();
    RECT rect;
    rect.top = 0;
    rect.left = 0;
    rect.bottom = 470;
    rect.right = 320;
    AdjustWindowRect(  &rect, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, false);
    cout << rect.top << " " << rect.left << " "<< rect.bottom << " "<< rect.right << endl;
    /* The class is registered, let's create the program*/
    hwnd = CreateWindowEx (
           0,                   /* Extended possibilites for variation */
           szClassName,         /* Classname */
           getString(IDS_APP_NAME).c_str(),       /* Title Text */
           WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, /* window style*/
           CW_USEDEFAULT,       /* Windows decides the position */
           CW_USEDEFAULT,       /* where the window ends up on the screen */
           rect.right - rect.left,                 /* The programs width */
           rect.bottom - rect.top,                 /* and height in pixels */
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
        if (!IsDialogMessage(hwnd, &messages)) { // To process tab stops
            /* Translate virtual-key messages into character messages */
            TranslateMessage(&messages);
            /* Send message to WindowProcedure */
            DispatchMessage(&messages);
        }
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
            if (pState->refreshIcon != NULL) {
                DestroyIcon(pState->refreshIcon);
            }
            PostQuitMessage (0);       /* send a WM_QUIT to the message queue */
            break;
        case WM_CREATE:
            {
                cout << "WM_CREATE " << hwnd  << " " << wParam << " " << lParam << endl;
                CREATESTRUCT *pCreateStruct = reinterpret_cast<CREATESTRUCT *> (lParam);
                cout << pCreateStruct->hInstance << endl;
                pState->hWindow = hwnd;

                pState->hDevicesText = CreateWindow(TEXT("STATIC"), getString(IDS_DEVICE).c_str(), WS_VISIBLE | WS_CHILD | SS_LEFTNOWORDWRAP, 5, 5, 200, 20, hwnd, NULL, pCreateStruct->hInstance, NULL);
                pState->hDevices = CreateWindow(TEXT("COMBOBOX"), NULL, CBS_DROPDOWN | CBS_HASSTRINGS | WS_TABSTOP | WS_CHILD | WS_OVERLAPPED | WS_VISIBLE, 5, 25, 280, 200, hwnd, NULL, pCreateStruct->hInstance, NULL);
                pState->hDevicesRefreshButton = CreateWindow(TEXT("button"), TEXT("Refresh"), BS_ICON | WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON , 290, 25, 25, 25, hwnd, (HMENU) ID_BUTTON_REFRESH, pCreateStruct->hInstance, NULL);

                pState->refreshIcon = (HICON)LoadImage(pCreateStruct->hInstance, MAKEINTRESOURCE(IDI_REFRESH), IMAGE_ICON, 16, 16, LR_CREATEDIBSECTION);
                if (pState->refreshIcon == NULL) {
                    cout << "FAILED TO GET refresh icon " << GetLastError() << endl;
                }
                cout << "setting image" << endl;
                SendMessage(pState->hDevicesRefreshButton, BM_SETIMAGE, IMAGE_ICON, (LPARAM)pState->refreshIcon);
                cout << "set image" << endl;
                pState->hIPAddressText = CreateWindow(TEXT("STATIC"), getString(IDS_ADDRESS).c_str(), WS_VISIBLE | WS_CHILD | SS_LEFTNOWORDWRAP, 5, 55, 310, 20, hwnd, NULL, pCreateStruct->hInstance, NULL);
                pState->hIPAddress = CreateWindow(TEXT("EDIT"), NULL, WS_BORDER | WS_CHILD | WS_VISIBLE | WS_TABSTOP| ES_LEFT | ES_AUTOHSCROLL, 5, 75, 310, 20, hwnd, NULL, pCreateStruct->hInstance, NULL);
                SendMessage(pState->hIPAddress, EM_SETLIMITTEXT, 1024, 0);

                pState->hServerPortText = CreateWindow(TEXT("STATIC"), getString(IDS_SERVER_PORT).c_str(), WS_VISIBLE | WS_CHILD | SS_LEFTNOWORDWRAP, 5, 100, 310, 20, hwnd, NULL, pCreateStruct->hInstance, NULL);
                pState->hServerPort = CreateWindow(TEXT("EDIT"), NULL, WS_BORDER | WS_CHILD | WS_VISIBLE | WS_TABSTOP| ES_LEFT | ES_AUTOHSCROLL | ES_NUMBER, 5, 120, 310, 20, hwnd, NULL, pCreateStruct->hInstance, NULL);
                SendMessage(pState->hServerPort, EM_SETLIMITTEXT, 5, 0);

                pState->hClientPortText = CreateWindow(TEXT("STATIC"), getString(IDS_CLIENT_PORT).c_str(), WS_VISIBLE | WS_CHILD | SS_LEFTNOWORDWRAP, 5, 145, 310, 20, hwnd, NULL, pCreateStruct->hInstance, NULL);
                pState->hClientPort = CreateWindow(TEXT("EDIT"), NULL, WS_BORDER | WS_CHILD | WS_VISIBLE | WS_TABSTOP| ES_LEFT | ES_AUTOHSCROLL | ES_NUMBER, 5, 165, 310, 20, hwnd, NULL, pCreateStruct->hInstance, NULL);
                SendMessage(pState->hClientPort, EM_SETLIMITTEXT, 5, 0);

                pState->hCutOffText = CreateWindow(TEXT("STATIC"), getString(IDS_CUTOFF).c_str(), WS_VISIBLE | WS_CHILD | SS_LEFTNOWORDWRAP, 5, 190, 310, 20, hwnd, NULL, pCreateStruct->hInstance, NULL);
                pState->hCutOff = CreateWindow(TEXT("EDIT"), NULL, WS_BORDER | WS_CHILD | WS_VISIBLE | WS_TABSTOP| ES_LEFT | ES_AUTOHSCROLL, 5, 210, 310, 20, hwnd, NULL, pCreateStruct->hInstance, NULL);
                SendMessage(pState->hCutOff, EM_SETLIMITTEXT, 10, 0);

                pState->hSpeedOffText = CreateWindow(TEXT("STATIC"), getString(IDS_SPEEDOFF).c_str(), WS_VISIBLE | WS_CHILD | SS_LEFTNOWORDWRAP, 5, 235, 310, 20, hwnd, NULL, pCreateStruct->hInstance, NULL);
                pState->hSpeedOff = CreateWindow(TEXT("EDIT"), NULL, WS_BORDER | WS_CHILD | WS_VISIBLE | WS_TABSTOP| ES_LEFT | ES_AUTOHSCROLL, 5, 255, 310, 20, hwnd, NULL, pCreateStruct->hInstance, NULL);
                SendMessage(pState->hSpeedOff, EM_SETLIMITTEXT, 10, 0);

                pState->hAutoReconnect = CreateWindow(TEXT("BUTTON"), getString(IDS_AUTORECONNECT).c_str(), WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 5, 280, 310, 20, hwnd, (HMENU) ID_BUTTON_AUTORECONNECT, pCreateStruct->hInstance, NULL);
                pState->hConnectOnStartup = CreateWindow(TEXT("BUTTON"), getString(IDS_CONNECTONSTARTUP).c_str(), WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 5, 305, 310, 20, hwnd, (HMENU) ID_BUTTON_CONNECTONSTARTUP, pCreateStruct->hInstance, NULL);
                pState->hNotifyAutoReconnect = CreateWindow(TEXT("BUTTON"), getString(IDS_NOTIFYAUTORECONNECT).c_str(), WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 5, 330, 310, 20, hwnd, (HMENU) ID_BUTTON_NOTIFYAUTORECONNECT, pCreateStruct->hInstance, NULL);

                pState->hApplyButton = CreateWindow(TEXT("button"), getString(IDS_APPLY).c_str(), WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 5, 355, 100, 30, hwnd, (HMENU) ID_BUTTON_APPLY, pCreateStruct->hInstance, NULL);
                pState->hSaveButton = CreateWindow(TEXT("button"), getString(IDS_SAVE).c_str(), WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 110, 355, 100, 30, hwnd, (HMENU) ID_BUTTON_SAVE, pCreateStruct->hInstance, NULL);
                pState->hDefaultButton = CreateWindow(TEXT("button"), getString(IDS_DEFAULT).c_str(), WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 215, 355, 100, 30, hwnd, (HMENU) ID_BUTTON_DEFAULT, pCreateStruct->hInstance, NULL);

                pState->hStatusText = CreateWindow(TEXT("STATIC"), NULL, WS_VISIBLE | WS_CHILD | SS_LEFTNOWORDWRAP, 5, 390, 310, 20, hwnd, NULL, pCreateStruct->hInstance, NULL);
                pState->hBufferSizeText = CreateWindow(TEXT("STATIC"), NULL, WS_VISIBLE | WS_CHILD | SS_LEFTNOWORDWRAP, 5, 410, 310, 20, hwnd, NULL, pCreateStruct->hInstance, NULL);

                pState->hClearBufferButton = CreateWindow(TEXT("button"), getString(IDS_CLEAR_BUFFER).c_str(), WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 5, 435, 100, 30, hwnd, (HMENU) ID_BUTTON_CLEAR_BUFFER, pCreateStruct->hInstance, NULL);

                pState->hStartButton = CreateWindow(TEXT("button"), getString(IDS_START).c_str(), WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 110, 435, 100, 30, hwnd, (HMENU) ID_BUTTON_START, pCreateStruct->hInstance, NULL);
                pState->hStopButton = CreateWindow(TEXT("button"), getString(IDS_STOP).c_str(), WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON | WS_DISABLED, 215, 435, 100, 30, hwnd, (HMENU) ID_BUTTON_STOP, pCreateStruct->hInstance, NULL);


                SetTimer(hwnd,             // handle to main window
                    ID_TIMER,            // timer identifier
                    100,                 // 0.1 second interval
                    (TIMERPROC) NULL);     // no timer callback
                loadSettingsToUI(pState);
                setSettings(pState)->setFailCallback(bind(clientFailCallback, pState, _1));
                getDevices(pState);
                if (pState->connectOnStartup) {
                    pState->canStart = false;
                    pState->failCode = CLIENT_E_OK;
                    pState->pClient->startAsync(bind(clientStartCallback, pState, _1), true);
                    enableControls(pState);
                }
            }
            break;
        case WM_COMMAND:
            if (HIWORD(wParam) == 0) {
                switch (LOWORD(wParam)) {
                    case ID_BUTTON_START:
                        if (pState->canStart) {
                            pState->canStart = false;
                            pState->failCode = CLIENT_E_OK;
                            pState->pClient->startAsync(bind(clientStartCallback, pState, _1));
                            enableControls(pState);
                        }
                        break;
                    case ID_BUTTON_STOP:
                        if (pState->canStop) {
                            pState->canStop = false;
                            pState->pClient->stopAsync(bind(clientStopCallback, pState, _1));
                            enableControls(pState);
                            KillTimer(pState->hWindow, ID_TIMERRECONNECTNOTIFY);
                        }
                        break;
                    case ID_BUTTON_APPLY: //apply
                    case ID_BUTTON_SAVE: //save
                        {
                            int len;
                            bool error = false;
                            unique_ptr<wchar_t []> buffer = make_unique<wchar_t[]>(1024);
                            string address;
                            wstring device;
                            int serverPort;
                            int clientPort;
                            float cutOff;
                            float speedOff;
                            bool autoReconnect;
                            bool connectOnStartup;
                            bool notifyAutoReconnect;
                            len = GetWindowText(pState->hIPAddress, buffer.get(), 1024);
                            for (int i = 0; i < len; i++) {
                                if (!(buffer[i] == L'.' || buffer[i] == L'-' || (buffer[i] >= L'0' && buffer[i] <= L'9') || (buffer[i] >= L'a' && buffer[i] <= L'z') || (buffer[i] >= L'A' && buffer[i] <= L'Z'))){
                                    error = true;
                                    break;
                                }
                                else {
                                    address += static_cast<char>(buffer[i]);
                                }
                            }
                            if (error) {
                                cout << "errro" << endl;
                                MessageBox(hwnd, getString(IDS_VALIDATE_ADDRESS).c_str(), (getString(IDS_APP_NAME) + L" " + getString(IDS_ERROR)).c_str(), MB_ICONERROR);
                                break;
                            }

                            len = GetWindowText(pState->hServerPort, buffer.get(), 1024);
                            serverPort = _wtoi(buffer.get());
                            cout << serverPort << endl;
                            if (serverPort <= 0 || serverPort > 65535) {
                                cout << "errro" << endl;
                                MessageBox(hwnd, getString(IDS_VALIDATE_SERVER_PORT).c_str(), (getString(IDS_APP_NAME) + L" " + getString(IDS_ERROR)).c_str(), MB_ICONERROR);
                                break;
                            }

                            len = GetWindowText(pState->hClientPort, buffer.get(), 1024);
                            clientPort = _wtoi(buffer.get());
                            cout << clientPort << endl;
                            if (clientPort <= 0 || clientPort > 65535) {
                                cout << "errro" << endl;
                                MessageBox(hwnd, getString(IDS_VALIDATE_CLIENT_PORT).c_str(), (getString(IDS_APP_NAME) + L" " + getString(IDS_ERROR)).c_str(), MB_ICONERROR);
                                break;
                            }

                            len = GetWindowText(pState->hCutOff, buffer.get(), 1024);
                            cutOff = _wtof(buffer.get());
                            cout << cutOff << endl;
                            if (cutOff < 0) {
                                cout << "errro" << endl;
                                MessageBox(hwnd, getString(IDS_VALIDATE_CUTOFF).c_str(), (getString(IDS_APP_NAME) + L" " + getString(IDS_ERROR)).c_str(), MB_ICONERROR);
                                break;
                            }

                            len = GetWindowText(pState->hSpeedOff, buffer.get(), 1024);
                            speedOff = _wtof(buffer.get());
                            cout << speedOff << endl;
                            if (speedOff < 0) {
                                cout << "errro" << endl;
                                MessageBox(hwnd, getString(IDS_VALIDATE_SPEEDOFF).c_str(), (getString(IDS_APP_NAME) + L" " + getString(IDS_ERROR)).c_str(), MB_ICONERROR);
                                break;
                            }

                            autoReconnect = IsDlgButtonChecked(hwnd, ID_BUTTON_AUTORECONNECT) == BST_CHECKED;
                            connectOnStartup = IsDlgButtonChecked(hwnd, ID_BUTTON_CONNECTONSTARTUP) == BST_CHECKED;
                            notifyAutoReconnect = IsDlgButtonChecked(hwnd, ID_BUTTON_NOTIFYAUTORECONNECT) == BST_CHECKED;
                            len = GetWindowText(pState->hDevices, buffer.get(), 1024);
                            device = wstring(buffer.get());
                            cout << "validation success " << autoReconnect << endl;
                            pState->ipAddress = address;
                            pState->serverPort = serverPort;
                            pState->clientPort = clientPort;
                            pState->device = device;
                            pState->cutOff = cutOff;
                            pState->speedOff = speedOff;
                            pState->autoReconnect = autoReconnect;
                            pState->connectOnStartup = connectOnStartup;
                            pState->notifyAutoReconnect = notifyAutoReconnect;
                            loadSettingsToUI(pState);
                            setSettings(pState);
                        }
                        if (LOWORD(wParam) == ID_BUTTON_SAVE) {
                            bool result = saveSettings(pState);
                            if (!result) {
                                MessageBox(hwnd, getString(IDS_SAVE_FAILED).c_str(), (getString(IDS_APP_NAME) + L" " + getString(IDS_ERROR)).c_str(), MB_ICONERROR);
                            }
                        }
                        break;

                    case ID_BUTTON_DEFAULT:
                        defaultSettings(pState);
                        setSettings(pState);
                        loadSettingsToUI(pState);
                        break;
                    case ID_BUTTON_REFRESH:
                        getDevices(pState);
                        break;
                    case ID_BUTTON_CLEAR_BUFFER:
                        pState->pClient->setClearBufferFlag();
                        break;
                }
            }
            break;
        case WM_TIMER:
            switch (wParam)
            {
                case ID_TIMER:
                    SetWindowText(pState->hStatusText, (getString(IDS_STATUS) + getString(IDS_WAITING + pState->pClient->getStatus())).c_str());

                    SetWindowText(pState->hBufferSizeText, (getString(IDS_BUFFER_LEFT) + toWStringWithPrecision(pState->pClient->bufferLeft(), 3) + L"s (" + to_wstring(pState->pClient->getSamplesInBuffer()) + L" " + getString(IDS_SAMPLES)+ L")").c_str());
                    break;
                case ID_TIMERRECONNECTNOTIFY:
                    if (pState->pClient->getStatus() == RECONNECTING) {
                        FlashWindow(pState->hWindow, true);
                        MessageBeep(MB_ICONERROR);
                    }
                    else {
                        KillTimer(pState->hWindow, ID_TIMERRECONNECTNOTIFY);
                    }
                    break;
            }
            break;
        default:                      /* for messages that we don't deal with */
            return DefWindowProc (hwnd, message, wParam, lParam);
    }



    return DefWindowProc (hwnd, message, wParam, lParam);
}


void clientStartCallback(State *pState, int status) {
    cout << "start callback " << status << " " << pState << endl;
    bool success = status == CLIENT_E_OK;
    pState->canStart = !success;
    pState->canStop = success;
    enableControls(pState);
    pState->failCode = status;
    if (!success) {
        MessageBox(pState->hWindow, (getString(IDS_ERROR_CODE) + to_wstring(status) + L"\n" + getString(status)).c_str(), (getString(IDS_APP_NAME) + L" " + getString(IDS_ERROR)).c_str(), MB_ICONERROR);
    }
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
    if (status == CLIENT_RECONNECT) {
        SetTimer(pState->hWindow,             // handle to main window
            ID_TIMERRECONNECTNOTIFY,            // timer identifier
            30000,                 // 30 second interval
            (TIMERPROC) NULL
        );     // no timer callback
        FlashWindow(pState->hWindow, true);
        MessageBeep(MB_ICONERROR);
    } else {
        pState->canStart = true;
        pState->canStop = false;
        enableControls(pState);
        pState->failCode = status;
        MessageBox(pState->hWindow, (getString(IDS_ERROR_CODE) + to_wstring(status) + L"\n" + getString(status)).c_str(), (getString(IDS_APP_NAME) + L" " + getString(IDS_ERROR)).c_str(), MB_ICONERROR);
        KillTimer(pState->hWindow, ID_TIMERRECONNECTNOTIFY);
    }
}

void enableControls(State *pState) {
    EnableWindow(pState->hStartButton, pState->canStart);
    EnableWindow(pState->hApplyButton, pState->canStart);
    EnableWindow(pState->hSaveButton, pState->canStart);
    EnableWindow(pState->hDefaultButton, pState->canStart);

    EnableWindow(pState->hStopButton, pState->canStop);

}

void getDevices(State *pState) {
    vector<wstring> devices;
    wstring device;

    unique_ptr<wchar_t []> buffer = make_unique<wchar_t[]>(1024);
    GetWindowText(pState->hDevices, buffer.get(), 1024);
    device = wstring(buffer.get());
    if (device == L"") {
        device = pState->device;
    }
    int result = pState->pClient->getAvailableDevices(devices);
    cout << "getDevices result " << result << endl << "Got " << devices.size() << " devices" << endl;
    if (result != CLIENT_E_OK) {
        // throw error here
    }
    else {
        int deviceId = 0;
        SendMessage(pState->hDevices, CB_RESETCONTENT, 0, 0);
        for (size_t i = 0; i < devices.size(); i++) {
            SendMessage(pState->hDevices, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(devices[i].c_str()));
            if (devices[i] == device) {
                deviceId = i;
            }
        }
        SendMessage(pState->hDevices, CB_SETCURSEL, deviceId, 0);
    }
}

wstring toWStringWithPrecision(const float value, const int n) {
    std::wostringstream out;
    out.precision(n);
    out << std::fixed << value;
    return out.str();
}

wstring getSavePath() {
    PWSTR buffer;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &buffer);
    if (FAILED(hr)) {
        return L"";
    }
    else {
        wstring result = wstring(buffer) + L"\\Wireless Mic";
        CoTaskMemFree(buffer);
        return result;
    }
}

bool loadSettings(State *pState) {
    HANDLE hFile = CreateFile(wstring(getSavePath() + L"\\settings.dat").c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        cout << "Failed to open settings file: " << GetLastError() << endl;
        goto loadSettings_loadDefault;
    }
    else {
        size_t bufferSize = sizeof(unsigned short) * 2 + sizeof(float) * 2 + sizeof(bool) * 3 + sizeof(size_t);
        unique_ptr<char []> buffer = make_unique<char[]>(bufferSize);
        int pos = 0;
        DWORD read;
        bool result;
        size_t length = 0;
        result = ReadFile(hFile, buffer.get(), bufferSize, &read, NULL);
        if (!result || read != bufferSize) {
            cout << "Failed to read settings file1: " << GetLastError() << endl;
            goto loadSettings_loadDefault;
        }
        pState->clientPort = *((unsigned short*)(buffer.get() + pos));
        pos += sizeof(unsigned short);

        pState->serverPort = *((unsigned short*)(buffer.get() + pos));
        pos += sizeof(unsigned short);

        pState->cutOff = *((float*)(buffer.get() + pos));
        pos += sizeof(float);

        pState->speedOff = *((float*)(buffer.get() + pos));
        pos += sizeof(float);

        pState->autoReconnect = *((bool*)(buffer.get() + pos));
        pos += sizeof(bool);

        pState->connectOnStartup = *((bool*)(buffer.get() + pos));
        pos += sizeof(bool);

        pState->notifyAutoReconnect = *((bool*)(buffer.get() + pos));
        pos += sizeof(bool);

        length = *((size_t*)(buffer.get() + pos));

        buffer = make_unique<char[]>(length);
        result = ReadFile(hFile, buffer.get(), length, &read, NULL);
        if (!result || read != length) {
            cout << "Failed to read settings file2: " << GetLastError() << endl;
            goto loadSettings_loadDefault;
        }
        pState->device = wstring(reinterpret_cast<wchar_t*>(buffer.get()), length / 2);
        wcout << "loaded " << pState->device << endl;

        buffer = make_unique<char[]>(sizeof(size_t));
        result = ReadFile(hFile, buffer.get(), sizeof(size_t), &read, NULL);
        if (!result || read != sizeof(size_t)) {
            cout << "Failed to read settings file3: " << GetLastError() << endl;
            goto loadSettings_loadDefault;
        }
        length = *((size_t*)buffer.get());
        buffer = make_unique<char[]>(length);
        result = ReadFile(hFile, buffer.get(), length, &read, NULL);
        if (!result || read != length) {
            cout << "Failed to read settings file4: " << GetLastError() << endl;
            goto loadSettings_loadDefault;
        }
        pState->ipAddress = string(buffer.get(), length);
        CloseHandle(hFile);
    }
    return true;
    loadSettings_loadDefault:
    if (hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(hFile);
    }
    defaultSettings(pState);
    saveSettings(pState);
    return false;
}

bool saveSettings(State *pState) {
    DWORD dirAttr = GetFileAttributes(getSavePath().c_str());
    if (dirAttr == INVALID_FILE_ATTRIBUTES) {
        int result = SHCreateDirectoryEx(NULL, getSavePath().c_str(), NULL);
        if (result != ERROR_SUCCESS) {
            return false;
        }
    }

    HANDLE hFile = CreateFile(wstring(getSavePath() + L"\\settings.dat").c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        wcout << "Failed to open settings file for writing:" << wstring(getSavePath() + L"\\settings.dat").c_str() << " " << GetLastError() << endl;
        return false;
    }

    unique_ptr<unsigned char []> buffer = make_unique<unsigned char[]>(max(max(sizeof(float), sizeof(unsigned short)), sizeof(size_t)));
    int result = 0;
    *((unsigned short*)buffer.get()) = pState->clientPort;
    result += WriteFile(hFile, buffer.get(), sizeof(pState->clientPort), NULL, NULL);

    *((unsigned short*)buffer.get()) = pState->serverPort;
    result += WriteFile(hFile, buffer.get(), sizeof(pState->serverPort), NULL, NULL);

    *((float*)buffer.get()) = pState->cutOff;
    result += WriteFile(hFile, buffer.get(), sizeof(pState->cutOff), NULL, NULL);

    *((float*)buffer.get()) = pState->speedOff;
    result += WriteFile(hFile, buffer.get(), sizeof(pState->speedOff), NULL, NULL);

    *((bool*)buffer.get()) = pState->autoReconnect;
    result += WriteFile(hFile, buffer.get(), sizeof(pState->autoReconnect), NULL, NULL);

    *((bool*)buffer.get()) = pState->connectOnStartup;
    result += WriteFile(hFile, buffer.get(), sizeof(pState->connectOnStartup), NULL, NULL);

    *((bool*)buffer.get()) = pState->notifyAutoReconnect;
    result += WriteFile(hFile, buffer.get(), sizeof(pState->notifyAutoReconnect), NULL, NULL);

    *((size_t*)buffer.get()) = pState->device.size() * 2;
    result += WriteFile(hFile, buffer.get(), sizeof(pState->device.size()), NULL, NULL);
    result += WriteFile(hFile, pState->device.c_str(), pState->device.size() * 2, NULL, NULL);

    *((size_t*)buffer.get()) = pState->ipAddress.size();
    result += WriteFile(hFile, buffer.get(), sizeof(pState->ipAddress.size()), NULL, NULL);
    result += WriteFile(hFile, pState->ipAddress.c_str(), pState->ipAddress.size(), NULL, NULL);

    CloseHandle(hFile);

    return result == 11;
}

void defaultSettings(State *pState) {
    pState->device = L"";
    pState->ipAddress = "";
    pState->clientPort = 34568;
    pState->serverPort = 8125;
    pState->cutOff = 0.3;
    pState->speedOff = 0.05;
    pState->autoReconnect = true;
    pState->connectOnStartup = false;
    pState->notifyAutoReconnect = true;
}

WoMicClient* setSettings(State *pState) {
    return pState->pClient->setIp(pState->ipAddress)
        ->setServerPort(pState->serverPort)
        ->setClientPort(pState->clientPort)
        ->setDevice(pState->device)
        ->setCutOff(pState->cutOff)
        ->setSpeedOff(pState->speedOff)
        ->setAutoReconnect(pState->autoReconnect);
}

void loadSettingsToUI(State *pState) {
    SetWindowText(pState->hDevices, pState->device.c_str());
    SetWindowText(pState->hIPAddress, wstring(pState->ipAddress.begin(), pState->ipAddress.end()).c_str());
    SetWindowText(pState->hServerPort, to_wstring(pState->serverPort).c_str());
    SetWindowText(pState->hClientPort, to_wstring(pState->clientPort).c_str());
    SetWindowText(pState->hCutOff, to_wstring(pState->cutOff).c_str());
    SetWindowText(pState->hSpeedOff, to_wstring(pState->speedOff).c_str());
    SendMessage(pState->hAutoReconnect, BM_SETCHECK, pState->autoReconnect ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(pState->hConnectOnStartup, BM_SETCHECK, pState->connectOnStartup ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(pState->hNotifyAutoReconnect, BM_SETCHECK, pState->notifyAutoReconnect ? BST_CHECKED : BST_UNCHECKED, 0);
}

wstring getString(UINT id) {

    unique_ptr<wchar_t[]> buffer = make_unique<wchar_t[]>(1024 * 10);

    int len = LoadString(GetModuleHandle(NULL), id, buffer.get(), 1024 * 10);

    if (len == 0 ) {
        return L"";
    }
    else {
        return wstring(buffer.get());
    }
}
