#include <iostream>
#include <windows.h>
#include <shellapi.h>
#include <csignal>
#include <stdio.h>
#include <algorithm>
#include <psapi.h>
#include <tlhelp32.h>
#include <ctime>
#include <iomanip>
#include <string>
#include <thread>
#include <atomic>
#include <filesystem>
#include "resource.h"

// Default path
std::string ODTPath = "C:\\Program Files\\Meta Horizon\\Support\\oculus-diagnostics\\";

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_EXIT 1001

NOTIFYICONDATA nid = { 0 };
std::atomic<bool> running(true);

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_TRAYICON) {
        if (lParam == WM_RBUTTONUP) {
            POINT curPoint;
            GetCursorPos(&curPoint);
            HMENU hMenu = CreatePopupMenu();
            AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, TEXT("Exit"));
            SetForegroundWindow(hWnd);
            TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, curPoint.x, curPoint.y, 0, hWnd, NULL);
            PostMessage(hWnd, WM_NULL, 0, 0);
            DestroyMenu(hMenu);
        }
        else if (lParam == WM_LBUTTONDBLCLK) {
            HWND hConsole = GetConsoleWindow();
            if (IsWindowVisible(hConsole)) {
                ShowWindow(hConsole, SW_HIDE);
            }
            else {
                ShowWindow(hConsole, SW_RESTORE);
                ShowWindow(hConsole, SW_SHOW);
                SetForegroundWindow(hConsole);
            }
        }
    }
    else if (uMsg == WM_COMMAND) {
        if (LOWORD(wParam) == ID_TRAY_EXIT) {
            running = false;
            PostQuitMessage(0);
        }
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

int get_pid(const std::wstring& processName) {
    int pid = 0;
    // Take a snapshot of all running processes
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        std::cout << "CreateToolhelp32Snapshot failed: " << GetLastError() << std::endl;
        return pid;
    }

    // Fill in the size of the structure before using it.
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    // Walk the snapshot of the processes, and for each process,
    // display information
    if (Process32First(hSnapshot, &pe32)) {
        do {
            if (processName == pe32.szExeFile) {
                pid = pe32.th32ProcessID;
                break;
            }
        } while (Process32Next(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
    return pid;
}

bool fileExists(const std::string& filePath) {
    return std::filesystem::exists(filePath) && std::filesystem::is_regular_file(filePath);
}

bool checkODTPath() {
    std::string ODTCLI_path = ODTPath + "OculusDebugToolCLI.exe\"";
    std::string ODTGUI_path = ODTPath + "OculusDebugTool.exe\"";

    if (fileExists(ODTCLI_path) && fileExists(ODTGUI_path)) {
        return true;
    }

    return false;
}

bool is24HourFormat() {
    wchar_t timeFormat[80];
    
    int result = GetLocaleInfoEx(
        LOCALE_NAME_USER_DEFAULT, 
        LOCALE_STIMEFORMAT, 
        timeFormat, 
        80
    );

    if (result > 0) {
        std::wstring formatStr(timeFormat);
        // "H" means 24h format, "h" means 12h format
        if (formatStr.find(L"H") != std::wstring::npos) {
            return true;
        }
    }
    
    return false;
}

tm time_now() {
    time_t now = time(0);
    tm ltm;
    localtime_s(&ltm, &now);
    if (!is24HourFormat()) {
        //convert to 12 hour format
        if (ltm.tm_hour > 12) {
            ltm.tm_hour -= 12;
        }
        else if (ltm.tm_hour == 0) {
            ltm.tm_hour = 12;
        }
    }
    return ltm;
}

int minutes(int minutes) {
    return minutes * 60000;
}

int seconds(int seconds) {
    return seconds * 1000;
}

void executed_at(std::string message = "Executed at: ") {
    tm time = time_now();
    std::cout << message << std::put_time(&time, "%H:%M:%S") << std::endl;
}

void next_tk(ULONGLONG refresh_loop) {
    ULONGLONG current_tk = GetTickCount64();
    long long diff_ms = static_cast<long long>(refresh_loop) - static_cast<long long>(current_tk);
    
    if (diff_ms < 0) diff_ms = 0;

    time_t raw_next = time(0) + (diff_ms / 1000);

    tm ltm_next;
    localtime_s(&ltm_next, &raw_next);

    if (!is24HourFormat()) {
        if (ltm_next.tm_hour > 12) {
            ltm_next.tm_hour -= 12;
        }
        else if (ltm_next.tm_hour == 0) {
            ltm_next.tm_hour = 12;
        }
    }

    std::stringstream ss; // Avoid issue where something is cout-ting at the same time
    ss << "Next tracking refresh : " << std::put_time(&ltm_next, "%H:%M:%S") << "\n";
    std::cout << ss.str();
       
}

HWND get_winhandle(LPCWSTR Target_window_Name) {
    HWND hWindowHandle = FindWindow(NULL, Target_window_Name);
    return hWindowHandle;
}

HWND get_vxwin(HWND hWindowHandle) {
    HWND PropertGrid = FindWindowEx(hWindowHandle, NULL, L"wxWindowNR", NULL);
    HWND wxWindow = FindWindowEx(PropertGrid, NULL, L"wxWindow", NULL);
    return wxWindow;
}

void forceForegroundWindow(HWND hwnd) { // https://stackoverflow.com/questions/19136365/win32-setforegroundwindow-not-working-all-the-time
    DWORD windowThreadProcessId = GetWindowThreadProcessId(GetForegroundWindow(), LPDWORD(0));
    DWORD currentThreadId = GetCurrentThreadId();
    DWORD CONST_SW_SHOW = 5;
    AttachThreadInput(windowThreadProcessId, currentThreadId, true);
    BringWindowToTop(hwnd);
    ShowWindow(hwnd, CONST_SW_SHOW);
    AttachThreadInput(windowThreadProcessId, currentThreadId, false);
}

void Press_key(int key, HWND wxWindow) {
    if (wxWindow != NULL) {
        SendMessage(wxWindow, WM_KEYDOWN, key, 0);
        SendMessage(wxWindow, WM_KEYUP, key, 0);
    }
    else {
        keybd_event(key, 0, KEYEVENTF_EXTENDEDKEY | 0, 0);
        keybd_event(key, 0, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
    }
}

void ODT_CLI() {

    //Sets "set-pixels-per-display-pixel-override" to 0.01 to decrease performance overhead
    std::cout << "Setting 'set-pixels-per-display-pixel-override' to 0.01..." << std::endl;
    std::string temp = "(echo service set-pixels-per-display-pixel-override 0.01 & echo exit) | \"" + ODTPath + "OculusDebugToolCLI.exe\"";
    system(temp.c_str());

    //Turn off ASW, we do not need it
    std::cout << "Disabing ASW..." << std::endl;
    temp = "(echo server: asw.Off & echo exit) | \"" + ODTPath + "OculusDebugToolCLI.exe\"";
    system(temp.c_str());
}

void killODT(int param) {
    if (param != 0) {
        //Reverse ODT cli commands
        std::cout << "Reversing ODT CLI commands" << std::endl;
        std::string temp = "echo service set-pixels-per-display-pixel-override 0 | \"" + ODTPath + "OculusDebugToolCLI.exe\"";
        // ASW stays disabled, but honestly that's better for everyone
        system(temp.c_str());
    }

    //Kill ODT
    int attempts = 100;

    std::cout << "Killing ODT" << std::endl;
    HWND hWindowHandle;
    while ((hWindowHandle = get_winhandle((LPCWSTR)L"Oculus Debug Tool")) != NULL || attempts <= 0) {
        forceForegroundWindow(hWindowHandle);
        SendMessage(hWindowHandle, WM_CLOSE, 0, 0);
        Sleep(15);

        attempts--;
    }

    std::cout << "ODT Closed!" << std::endl;
}

std::atomic<bool> doKillODTThread(false);

void start_process(std::string path) {
    if (get_winhandle((LPCWSTR)L"Oculus Debug Tool") != NULL) {
        killODT(0);
        Sleep(100);
    }

    std::string tempstr = path + "OculusDebugTool.exe";
    std::cout << "Starting: " << tempstr << std::endl;
    //start exe
    ShellExecute(NULL, L"open", (LPCWSTR)std::wstring(tempstr.begin(), tempstr.end()).c_str(), NULL, NULL, SW_SHOWDEFAULT);
    HWND hWindowHandle;

    Sleep(500);

    //wait for window to load
    std::cout << "Waiting for window to load" << std::endl;
    while ((hWindowHandle = get_winhandle((LPCWSTR)L"Oculus Debug Tool")) == NULL && !doKillODTThread) {
        Sleep(100);
    }

    // Kill the thing if user starts doing stuff
    if (doKillODTThread) return;

    //system("cls"); //Clear screen
    std::cout << "ODT window found!" << std::endl;
    HWND wxWindow = get_vxwin(hWindowHandle);

    std::cout << "Waiting for window to be focused" << std::endl;
    while (GetForegroundWindow() != hWindowHandle && !doKillODTThread) {
        if (doKillODTThread) return;
        forceForegroundWindow(hWindowHandle);
        Sleep(250);
    }
    std::cout << "ODT window focused!" << std::endl;

    // Kill the thing if user starts doing stuff
    if (doKillODTThread) return;

    std::cout << "Going to 'Bypass Proximity Sensor Check'..." << std::endl;

    // Go to the setting "Bypass Proximity Sensor Check"
    for (int i = 0; i < 7; i++) {
        Press_key(VK_DOWN, wxWindow);
        std::cout << "Pressed Arrow Down" << std::endl;
    }
    Sleep(50);

    std::cout << "Refreshing the setting..." << std::endl;
    Press_key(VK_TAB, wxWindow);
    std::cout << "Pressed Tab" << std::endl;
    Sleep(50);

    // Kill the thing if user starts doing stuff
    if (doKillODTThread) return;

    // Toogle the option "Bypass Proximity Sensor Check"
    Press_key(VK_UP, wxWindow);
    std::cout << "Set the option to 'Off'" << std::endl;
    Sleep(100);
    Press_key(VK_DOWN, wxWindow);
    std::cout << "Set the option to 'On'" << std::endl;
    Sleep(100);

    std::cout << "Done!" << std::endl;
}

void send_notification(std::wstring infoTitle, std::wstring info) {
    nid.uFlags |= NIF_INFO;
    nid.dwInfoFlags = NIIF_INFO;
    
    lstrcpyW(nid.szInfoTitle, infoTitle.c_str());
    lstrcpyW(nid.szInfo, info.c_str());

    Shell_NotifyIcon(NIM_MODIFY, &nid);
}

// Hiding the app into the system tray when minimizing the console window
void MonitorConsoleMinimize() {
    HWND hConsole = GetConsoleWindow();
    while (running) {
        if (IsWindowVisible(hConsole)) {
            if (IsIconic(hConsole)) {
                ShowWindow(hConsole, SW_HIDE);
            
            }
        }
        Sleep(500);
    }
}

void parse_args(int argc, char* argv[]) {
    for (int i = 0; i < argc; i++) {
        if (std::string(argv[i]) == "--path") {
            ODTPath = std::string(argv[i+1]) + (argv[i+1][strlen(argv[i+1])] == '\\' ? "" : "\\");  // adds \ if user forgot to add it
            ODTPath.erase(std::remove(ODTPath.begin(), ODTPath.end(), '"'), ODTPath.end());     // removes " from the input
        }
    }
}

std::atomic<bool> threadRunning(false);

void doToggle() {
    threadRunning = true;
    start_process(ODTPath);
    if (!doKillODTThread) 
        Sleep(100);
    killODT(0);

    threadRunning = false;
}

int main(int argc, char* argv[]) {

    // check for another instance
    HANDLE hMutex = CreateMutexA(NULL, FALSE, "Local\\ODTKRA_Unique_Mutex_Name");
    if (hMutex != NULL && GetLastError() == ERROR_ALREADY_EXISTS) {
        std::cout << "Another instance is already running. Closing it..." << std::endl;
        
        HWND hExistingWnd = FindWindow(TEXT("DummyWindowClass"), TEXT("HiddenWindow"));
        if (hExistingWnd != NULL) {
            SendMessage(hExistingWnd, WM_COMMAND, ID_TRAY_EXIT, 0);
        }
        
        CloseHandle(hMutex);
        Sleep(1000); 
        
        hMutex = CreateMutexA(NULL, FALSE, "Local\\ODTKRA_Unique_Mutex_Name");
    }

    // create a console and hide it instantly
    AllocConsole();
    FILE* fDummy;
    freopen_s(&fDummy, "CONIN$", "r", stdin);
    freopen_s(&fDummy, "CONOUT$", "w", stdout);
    freopen_s(&fDummy, "CONOUT$", "w", stderr);
    
    HWND hConsole = GetConsoleWindow();

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int windowWidth = 800;
    int windowHeight = 450;
    int posX = (screenWidth - windowWidth) / 2;
    int posY = (screenHeight - windowHeight) / 2;
    
    SetWindowPos(hConsole, NULL, posX, posY, windowWidth, windowHeight, SWP_NOZORDER);

    ShowWindow(hConsole, SW_HIDE);

    std::thread consoleMonitor(MonitorConsoleMinimize);
    consoleMonitor.detach();

    // Check launch arguments
    parse_args(argc, argv);
    
    std::cout << "ODT Path: " << ODTPath << std::endl;

    SetConsoleTitleA("ODTKRA nicolas-riera Edition");

    signal(SIGABRT, killODT);
    signal(SIGTERM, killODT);
    signal(SIGBREAK, killODT);

    HINSTANCE hInstance = GetModuleHandle(NULL);
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = TEXT("DummyWindowClass");
    RegisterClass(&wc);

    HWND hWndHidden = CreateWindowEx(0, TEXT("DummyWindowClass"), TEXT("HiddenWindow"), 
                                     0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, NULL);

    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hWndHidden;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIcon(NIM_SETVERSION, &nid);
    nid.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
    lstrcpy(nid.szTip, TEXT("ODTKRA nicolas-riera Edition"));

    Shell_NotifyIcon(NIM_ADD, &nid);

    ULONGLONG tracking_refresh_timer = GetTickCount64();
    ULONGLONG refresh_loop = tracking_refresh_timer;
    ULONGLONG lastIdle = GetTickCount64();

    int refresh_tracking = 9;   //refresh tracking every X minutes
    int refresh_tracking_times = 0;
    bool mouseMovedThisCycle = false;
    bool forcedActive = false;

    POINT lastCursor;
    GetCursorPos(&lastCursor);

    std::thread worker([&]() {

        // Check path, and close the program if ODT isn't found
        if (!checkODTPath()) {
            std::wstring message = L"Oculus Diagnostic Tool not found.";
            std::wstring titre = L"Unable to find ODT at " + std::wstring(ODTPath.begin(), ODTPath.end()) + L". Make sure you have the program installed or change the path with the launch argument '--path'.";
            MessageBox(
                NULL,
                titre.c_str(),
                message.c_str(),
                MB_OK | MB_ICONERROR
            );
            ExitProcess(1);
        }

        send_notification(L"ODTKRA is running!", L"You can double click on the system tray icon to show the console.");
        
        // Disable ASW and set "set-pixels-per-display-pixel-override" to 0.01
        ODT_CLI();

        while (running) {
            auto tk = GetTickCount64();

            POINT p;
            GetCursorPos(&p);
            
            if (p.x != lastCursor.x || p.y != lastCursor.y) {
                lastIdle = tk;
                mouseMovedThisCycle = true; // Track that the mouse moved during this 9-minute window
            }
            lastCursor = p;

            // Reset forcedActive when the thread is done
            if (forcedActive && !threadRunning) {
                forcedActive = false; 
            }

            if (tk - lastIdle < seconds(15)) {
                // Do not trigger a kill if the activation was forced
                if (!forcedActive) {
                    doKillODTThread = true;
                }
            }
            else {
                doKillODTThread = false;
            }

            // Anticipated refresh
            bool refreshIsSoon = (refresh_loop > tk && (refresh_loop - tk) <= minutes(5));
            bool inactiveForOneMinute = (tk - lastIdle >= minutes(1));
            bool anticipateRefresh = refreshIsSoon && inactiveForOneMinute && mouseMovedThisCycle;

            // Force refresh to not go over the 10 minutes mark
            bool forceRefresh = (tk >= refresh_loop + minutes(1));

            // Global trigger conditions
            if ((tk >= refresh_loop && tk - lastIdle > seconds(15)) || anticipateRefresh || forceRefresh) {
                
                // If the refresh is forced, temporarily disable the kill signal to let the thread work for 3 seconds
                if (forceRefresh) {
                    forcedActive = true;
                    doKillODTThread = false;
                }

                refresh_tracking_times++;
                
                if (forceRefresh) {
                    executed_at("Tracking refresh #" + std::to_string(refresh_tracking_times) + " FORCED (Safety timeout) at : ");
                } else if (anticipateRefresh) {
                    executed_at("Tracking refresh #" + std::to_string(refresh_tracking_times) + " executed EARLY (Anticipation) at : ");
                } else {
                    executed_at("Tracking refresh #" + std::to_string(refresh_tracking_times) + " executed at : ");
                }

                std::thread killThread(doToggle);
                killThread.detach();

                mouseMovedThisCycle = false;

                // Reschedule the next execution from this exact moment
                refresh_loop = tk + minutes(refresh_tracking);
                next_tk(refresh_loop);
            }

            Sleep(250);
        }
    });

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    killODT(1);

    if (worker.joinable()) worker.join();
    Shell_NotifyIcon(NIM_DELETE, &nid);
    DestroyWindow(hWndHidden);

    if (fDummy) {
        fclose(stdin);
        fclose(stdout);
        fclose(stderr);
    }

    if (hMutex != NULL) {
        CloseHandle(hMutex);
    }

    return 0;
}