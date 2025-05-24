#include <windows.h>
#include <string>
#include <commdlg.h>
#include <vector>
#include <tlhelp32.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <gdiplus.h>
#include <psapi.h>
#include <set>
#include <shellapi.h>
#include "resource.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "shell32.lib")

// Control IDs
#define ID_BUTTON_SELECT_FILE 1
#define ID_EDIT_FILE_PATH 2
#define ID_BUTTON_PROCESS_LIST 3
#define ID_LISTVIEW_PROCESSES 4
#define ID_BUTTON_INJECT 5
#define ID_STATIC_TITLE 6
#define ID_STATIC_FILE 7
#define ID_STATIC_PROCESS 8
#define ID_IMAGE_LOGO 9
#define ID_CHECKBOX_SHOW_ALL 10
#define ID_MENU_ABOUT 11

// Colors
#define COLOR_BACKGROUND RGB(240, 240, 240)
#define COLOR_HEADER RGB(51, 51, 51)
#define COLOR_TEXT RGB(33, 33, 33)
#define COLOR_BUTTON RGB(0, 120, 215)
#define COLOR_BUTTON_HOVER RGB(0, 102, 204)
#define COLOR_BUTTON_TEXT RGB(255, 255, 255)

// Global variables
HWND g_hEditFilePath = NULL;
HWND g_hListViewProcesses = NULL;
HWND g_hButtonInject = NULL;
HWND g_hCheckboxShowAll = NULL;
HIMAGELIST g_hImageList = NULL;
ULONG_PTR g_gdiplusToken = 0;
HFONT g_hTitleFont = NULL;
HFONT g_hNormalFont = NULL;
bool g_showAllProcesses = false;
HBITMAP g_hLogoBitmap = NULL;
HICON g_hAppIcon = NULL;

// Exclude system processes
static const std::set<std::wstring> SYSTEM_PROCESSES = {
    L"svchost.exe", L"winlogon.exe", L"services.exe", L"dllhost.exe",
    L"spoolsv.exe", L"taskhostw.exe", L"lsass.exe", L"csrss.exe",
    L"smss.exe", L"wininit.exe", L"System", L"Registry", L"Idle",
    L"conhost.exe", L"fontdrvhost.exe", L"SearchIndexer.exe", L"explorer.exe",
    L"dwm.exe", L"sihost.exe", L"ctfmon.exe", L"RuntimeBroker.exe", L"WmiPrvSE.exe"
};

// Forward declarations
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void InitializeCustomControls(HWND hwnd);
void SelectFile(HWND hwnd);
void UpdateProcessList(HWND hListView);
bool InjectDLL(DWORD processId, const wchar_t* dllPath);
DWORD GetSelectedProcessId(HWND hListView);
HICON GetProcessIcon(const wchar_t* processName);

// Window class name
const wchar_t CLASS_NAME[] = L"NextGenLiteWindowClass";

// Get process icon
HICON GetProcessIcon(const wchar_t* processName) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return NULL;

    PROCESSENTRY32W pe32 = { 0 };
    pe32.dwSize = sizeof(pe32);
    HANDLE hProcess = NULL;
    wchar_t processPath[MAX_PATH] = { 0 };

    if (Process32FirstW(hSnapshot, &pe32)) {
        do {
            if (_wcsicmp(pe32.szExeFile, processName) == 0) {
                hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pe32.th32ProcessID);
                if (hProcess) {
                    if (GetModuleFileNameExW(hProcess, NULL, processPath, MAX_PATH)) {
                        CloseHandle(hProcess);
                        CloseHandle(hSnapshot);

                        SHFILEINFOW sfi = { 0 };
                        if (SUCCEEDED(SHGetFileInfoW(processPath, 0, &sfi, sizeof(sfi),
                            SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES))) {
                            return sfi.hIcon;
                        }
                    }
                    CloseHandle(hProcess);
                }
                break;
            }
        } while (Process32NextW(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
    return NULL;
}

// File selection function
void SelectFile(HWND hwnd) {
    wchar_t szFile[260] = { 0 };
    OPENFILENAME ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = L"DLL Files\0*.dll\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&ofn)) {
        SetWindowText(g_hEditFilePath, szFile);
        EnableWindow(g_hButtonInject, TRUE);
    }
}

// Process list update function
void UpdateProcessList(HWND hListView) {
    ListView_DeleteAllItems(hListView);
    ImageList_RemoveAll(g_hImageList);

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32W pe32 = { 0 };
    pe32.dwSize = sizeof(pe32);

    if (Process32FirstW(hSnapshot, &pe32)) {
        int index = 0;
        do {
            // Only user applications: those with a visible window and not a system process
            if (!g_showAllProcesses) {
                // Exclude system processes
                if (SYSTEM_PROCESSES.find(pe32.szExeFile) != SYSTEM_PROCESSES.end())
                    continue;
                // Check if process has visible windows
                bool hasVisibleWindow = false;
                HWND hwnd = NULL;
                while ((hwnd = FindWindowEx(NULL, hwnd, NULL, NULL)) != NULL) {
                    DWORD processId = 0;
                    GetWindowThreadProcessId(hwnd, &processId);
                    if (processId == pe32.th32ProcessID) {
                        if (IsWindowVisible(hwnd) && !IsIconic(hwnd)) {
                            hasVisibleWindow = true;
                            break;
                        }
                    }
                }
                if (!hasVisibleWindow) continue;
            }

            // Get process icon
            HICON hIcon = GetProcessIcon(pe32.szExeFile);
            if (hIcon) {
                ImageList_AddIcon(g_hImageList, hIcon);
                DestroyIcon(hIcon);
            } else {
                // Add default icon if process icon couldn't be retrieved
                HICON defaultIcon = LoadIcon(NULL, IDI_APPLICATION);
                ImageList_AddIcon(g_hImageList, defaultIcon);
                DestroyIcon(defaultIcon);
            }

            // Add to list view
            LVITEMW lvi = { 0 };
            lvi.mask = LVIF_TEXT | LVIF_IMAGE;
            lvi.iItem = index++;
            lvi.iSubItem = 0;
            lvi.iImage = index - 1;
            lvi.pszText = pe32.szExeFile;
            ListView_InsertItem(hListView, &lvi);

            // Add PID
            wchar_t pidStr[16] = { 0 };
            swprintf_s(pidStr, L"%d", pe32.th32ProcessID);
            lvi.iSubItem = 1;
            lvi.pszText = pidStr;
            ListView_SetItem(hListView, &lvi);

        } while (Process32NextW(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
}

// DLL Injection function
bool InjectDLL(DWORD processId, const wchar_t* dllPath) {
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (!hProcess) return false;

    size_t dllPathSize = (wcslen(dllPath) + 1) * sizeof(wchar_t);
    LPVOID dllPathAddr = VirtualAllocEx(hProcess, NULL, dllPathSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!dllPathAddr) {
        CloseHandle(hProcess);
        return false;
    }

    if (!WriteProcessMemory(hProcess, dllPathAddr, dllPath, dllPathSize, NULL)) {
        VirtualFreeEx(hProcess, dllPathAddr, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
    if (!hKernel32) {
        VirtualFreeEx(hProcess, dllPathAddr, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    LPTHREAD_START_ROUTINE loadLibraryAddr = (LPTHREAD_START_ROUTINE)GetProcAddress(hKernel32, "LoadLibraryW");
    if (!loadLibraryAddr) {
        VirtualFreeEx(hProcess, dllPathAddr, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, loadLibraryAddr, dllPathAddr, 0, NULL);
    if (!hThread) {
        VirtualFreeEx(hProcess, dllPathAddr, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    WaitForSingleObject(hThread, INFINITE);

    CloseHandle(hThread);
    VirtualFreeEx(hProcess, dllPathAddr, 0, MEM_RELEASE);
    CloseHandle(hProcess);

    return true;
}

// Get selected process ID
DWORD GetSelectedProcessId(HWND hListView) {
    int selectedIndex = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
    if (selectedIndex == -1) return 0;

    wchar_t pidStr[16] = { 0 };
    LVITEMW lvi = { 0 };
    lvi.iItem = selectedIndex;
    lvi.iSubItem = 1;
    lvi.mask = LVIF_TEXT;
    lvi.pszText = pidStr;
    lvi.cchTextMax = sizeof(pidStr) / sizeof(wchar_t);
    ListView_GetItem(hListView, &lvi);

    return _wtoi(pidStr);
}

// Initialize custom controls
void InitializeCustomControls(HWND hwnd) {
    // Create menu
    HMENU hMenu = CreateMenu();
    HMENU hHelpMenu = CreatePopupMenu();
    AppendMenu(hHelpMenu, MF_STRING, ID_MENU_ABOUT, L"About");
    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hHelpMenu, L"Help");
    SetMenu(hwnd, hMenu);

    // Create modern fonts
    g_hTitleFont = CreateFont(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    g_hNormalFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    // Get the current window width
    RECT clientRect;
    GetClientRect(hwnd, &clientRect);
    int windowWidth = clientRect.right - clientRect.left;

    // Create title
    HWND hTitle = CreateWindow(L"STATIC", L"NextGen Lite DLL Injector",
        WS_VISIBLE | WS_CHILD | SS_CENTER,
        0, 0, windowWidth, 60, hwnd, (HMENU)ID_STATIC_TITLE, GetModuleHandle(NULL), NULL);
    SendMessage(hTitle, WM_SETFONT, (WPARAM)g_hTitleFont, TRUE);

    // Create logo image control
    // Adjust these values based on the logo size and desired layout
    int logoWidth = 200; // Example Width
    int logoHeight = 100; // Example Height (Reduced)
    int logoX = (windowWidth - logoWidth) / 2; // Center horizontally based on window width
    int logoY = 60; // Below the title

    HWND hLogoImage = CreateWindow(L"STATIC", NULL,
        WS_VISIBLE | WS_CHILD | SS_BITMAP, // SS_BITMAP for bitmap image
        logoX, logoY, logoWidth, logoHeight,
        hwnd, (HMENU)ID_IMAGE_LOGO, GetModuleHandle(NULL), NULL);

    // Load and set the logo bitmap from resources
    g_hLogoBitmap = LoadBitmap(GetModuleHandle(NULL), MAKEINTRESOURCE(IDB_APP_LOGO));
    if (g_hLogoBitmap) {
        SendMessage(hLogoImage, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)g_hLogoBitmap);
    } else {
        // Optional: Log or handle the error silently if MessageBox is removed
    }

    // Set the starting Y position for file selection controls below the logo
    int controlsStartY = logoY + logoHeight + 10; // Reduced space below the logo

    // Create file selection controls
    int labelWidth = 100;
    int browseButtonWidth = 100;
    int padding = 20; // Padding from the window edges
    int elementSpacing = 10; // Spacing between elements

    CreateWindow(L"STATIC", L"Select DLL File:", // Label text
        WS_VISIBLE | WS_CHILD,
        padding, controlsStartY, labelWidth, 20, hwnd, (HMENU)ID_STATIC_FILE, GetModuleHandle(NULL), NULL);

    CreateWindow(L"BUTTON", L"Browse...", // Button text
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        padding, controlsStartY + 25, browseButtonWidth, 30, hwnd, (HMENU)ID_BUTTON_SELECT_FILE, GetModuleHandle(NULL), NULL);

    int editFilePathX = padding + browseButtonWidth + elementSpacing;
    int editFilePathWidth = windowWidth - editFilePathX - padding; // Adjust width based on window width
    g_hEditFilePath = CreateWindow(L"EDIT", L"",
        WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | ES_READONLY,
        editFilePathX, controlsStartY + 25, editFilePathWidth, 30, hwnd, (HMENU)ID_EDIT_FILE_PATH, GetModuleHandle(NULL), NULL);

    // Create process list controls
    int processControlsStartY = controlsStartY + 25 + 30 + 20; // Space below file controls
    int refreshButtonWidth = 150;
    int checkboxWidth = 150;

    CreateWindow(L"STATIC", L"Select Target Process:", // Label text
        WS_VISIBLE | WS_CHILD,
        padding, processControlsStartY, 150, 20, hwnd, (HMENU)ID_STATIC_PROCESS, GetModuleHandle(NULL), NULL);

    CreateWindow(L"BUTTON", L"Refresh Process List", // Button text
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        padding, processControlsStartY + 25, refreshButtonWidth, 30, hwnd, (HMENU)ID_BUTTON_PROCESS_LIST, GetModuleHandle(NULL), NULL);

    // Create checkbox for showing all processes
    int checkboxX = padding + refreshButtonWidth + elementSpacing;
    g_hCheckboxShowAll = CreateWindow(L"BUTTON", L"Show All Processes", // Checkbox text
        WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
        checkboxX, processControlsStartY + 25, checkboxWidth, 30, hwnd, (HMENU)ID_CHECKBOX_SHOW_ALL, GetModuleHandle(NULL), NULL);

    // Create ListView
    int listViewStartY = processControlsStartY + 25 + 30 + 20; // Space below process controls
    int listViewHeight = 150; // ListView height (Reduced)
    int listViewWidth = windowWidth - 2 * padding; // Adjust width based on window width
    g_hListViewProcesses = CreateWindow(WC_LISTVIEW, NULL,
        WS_VISIBLE | WS_CHILD | WS_BORDER | LVS_REPORT | LVS_SINGLESEL,
        padding, listViewStartY, listViewWidth, listViewHeight, hwnd, (HMENU)ID_LISTVIEW_PROCESSES, GetModuleHandle(NULL), NULL);

    // Create image list
    g_hImageList = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 0, 100);
    ListView_SetImageList(g_hListViewProcesses, g_hImageList, LVSIL_SMALL);

    // Add columns (Adjust based on ListView width)
    LVCOLUMNW lvc = { 0 };
    lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    // Adjust column widths proportionally to ListView width
    int col1Width = (int)(listViewWidth * 0.7); // 70% for Process Name
    int col2Width = (int)(listViewWidth * 0.3); // 30% for PID

    lvc.iSubItem = 0;
    lvc.pszText = (LPWSTR)L"Process Name"; // Column header text
    lvc.cx = col1Width;
    ListView_InsertColumn(g_hListViewProcesses, 0, &lvc);

    lvc.iSubItem = 1;
    lvc.pszText = (LPWSTR)L"PID"; // Column header text
    lvc.cx = col2Width;
    ListView_InsertColumn(g_hListViewProcesses, 1, &lvc);

    // Create inject button
    // Set the Y position of the Inject button below the ListView
    int injectButtonStartY = listViewStartY + listViewHeight + 20; // ListView height + 20 pixels space
    int injectButtonWidth = windowWidth - 2 * padding; // Adjust width based on window width
    g_hButtonInject = CreateWindow(L"BUTTON", L"Inject DLL", // Button text
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | WS_DISABLED,
        padding, injectButtonStartY, injectButtonWidth, 40, hwnd, (HMENU)ID_BUTTON_INJECT, GetModuleHandle(NULL), NULL);

    // Set fonts for all controls
    HWND controls[] = { g_hEditFilePath, g_hListViewProcesses, g_hButtonInject, g_hCheckboxShowAll };
    for (HWND control : controls) {
        SendMessage(control, WM_SETFONT, (WPARAM)g_hNormalFont, TRUE);
    }
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    // Initialize GDI+
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL);

    // Initialize Common Controls
    INITCOMMONCONTROLSEX icex = { 0 };
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES; // ICC_STANDARD_CLASSES is needed for Static controls
    InitCommonControlsEx(&icex);

    // Register window class
    // Using WNDCLASSEXW structure
    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXW); // Specify the size
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    // Load the application icon from resources
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP_ICON));
    wc.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP_ICON)); // Load small icon from resources

    // Use RegisterClassExW instead of RegisterClass
    if (!RegisterClassExW(&wc)) {
        MessageBox(NULL, L"Window class registration failed!", L"Error", MB_ICONERROR);
        return 0;
    }

    // Create main window
    // Adjust window height based on UI elements and disable resizing
    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"NextGen Lite - GitHub AliAkgun0", // Window title
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX , // WS_THICKFRAME removed, resizing disabled
        CW_USEDEFAULT, CW_USEDEFAULT,
        600, 500, // Window width reduced (900 -> 600), height adjusted
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (hwnd == NULL) return 0;

    // Initialize controls
    InitializeCustomControls(hwnd);

    // Show window
    ShowWindow(hwnd, nCmdShow); // Can be changed to ShowWindow(hwnd, SW_SHOWNORMAL);
    UpdateWindow(hwnd);

    // Message loop
    MSG msg = { 0 };
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup
    DeleteObject(g_hTitleFont);
    DeleteObject(g_hNormalFont);
    ImageList_Destroy(g_hImageList);
    if (g_hLogoBitmap) DeleteObject(g_hLogoBitmap); // Cleanup the bitmap handle
    if (g_hAppIcon) DestroyIcon(g_hAppIcon); // Cleanup the icon handle
    Gdiplus::GdiplusShutdown(g_gdiplusToken);

    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        switch (wmId)
        {
        case ID_MENU_ABOUT:
            MessageBox(hwnd, L"NextGen Lite DLL Injector\nVersion 1.0\nDeveloped by Ali Akgun\nGitHub: github.com/AliAkgun0\nLicensed under MIT License", L"About", MB_ICONINFORMATION); // About text
            break;
        case ID_BUTTON_SELECT_FILE:
            SelectFile(hwnd);
            break;
        case ID_BUTTON_PROCESS_LIST:
            UpdateProcessList(g_hListViewProcesses);
            break;
        case ID_CHECKBOX_SHOW_ALL:
            g_showAllProcesses = (SendMessage(g_hCheckboxShowAll, BM_GETCHECK, 0, 0) == BST_CHECKED);
            UpdateProcessList(g_hListViewProcesses);
            break;
        case ID_BUTTON_INJECT:
        {
            wchar_t dllPath[MAX_PATH] = { 0 };
            GetWindowText(g_hEditFilePath, dllPath, MAX_PATH);
            DWORD processId = GetSelectedProcessId(g_hListViewProcesses);
            
            if (processId && wcslen(dllPath) > 0) {
                if (InjectDLL(processId, dllPath)) {
                    MessageBox(hwnd, L"DLL successfully injected!", L"Success", MB_ICONINFORMATION); // Success message
                } else {
                    MessageBox(hwnd, L"Failed to inject DLL!", L"Error", MB_ICONERROR); // Error message
                }
            }
            break;
        }
        }
        break;
    }

    case WM_DESTROY:
        // Cleanup logo bitmap and icon handles (already done in Cleanup section)
        PostQuitMessage(0);
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
        EndPaint(hwnd, &ps);
        return 0;
    }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
