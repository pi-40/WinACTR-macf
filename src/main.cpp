#include "resource.h"

LRESULT CALLBACK DisplayWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;

        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hbmMem = CreateCompatibleBitmap(hdc, w, h);
        HGDIOBJ hOldObj = SelectObject(hdcMem, hbmMem);

        Gdiplus::Graphics graphics(hdcMem);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);

        Gdiplus::Color clearColor(255, 2, 5, 15);
        graphics.Clear(clearColor);

        if (g_IsRunning && !g_Agent.frames.empty() && g_CurrentFrame < g_Agent.frames.size()) {
            Gdiplus::Bitmap* pBitmap = g_Agent.frames[g_CurrentFrame].bitmap;
            if (pBitmap && pBitmap->GetLastStatus() == Gdiplus::Ok) {
                graphics.DrawImage(pBitmap, 0, 0, w, h);
            }
        }

        BitBlt(hdc, 0, 0, w, h, hdcMem, 0, 0, SRCCOPY);

        SelectObject(hdcMem, hOldObj);
        DeleteObject(hbmMem);
        DeleteDC(hdcMem);

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_CLOSE:
        g_IsRunning = false;
        KillTimer(g_hMainWnd, 1);
        g_hDisplayWnd = NULL;
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CTLCOLORSTATIC: { 
        HDC hdcStatic = (HDC)wParam;
        SetTextColor(hdcStatic, RGB(165, 210, 255));
        SetBkMode(hdcStatic, TRANSPARENT);
        return (LRESULT)GetStockObject(NULL_BRUSH);
    }
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rc; GetClientRect(hwnd, &rc);
        HBRUSH hBmp = CreateSolidBrush(RGB(4, 15, 40));
        FillRect(hdc, &rc, hBmp);
        DeleteObject(hBmp);
        return 1;
    }
    case WM_CREATE: {
        g_hTab = CreateWindowExW(0, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
            10, 10, 560, 340, hwnd, (HMENU)ID_TABCTRL, GetModuleHandle(NULL), NULL);

        SetWindowTheme(g_hTab, L"", L"");
        g_OrgTabProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(g_hTab, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(TabSubclassProc)));

        wchar_t tabText1[] = L"Compiler";
        wchar_t tabText2[] = L"Player / Launcher";
        wchar_t tabText3[] = L"Decompiler Extraction";

        TCITEMW tie = { 0 };
        tie.mask = TCIF_TEXT;
        tie.pszText = tabText1; TabCtrl_InsertItem(g_hTab, 0, &tie);
        tie.pszText = tabText2; TabCtrl_InsertItem(g_hTab, 1, &tie);
        tie.pszText = tabText3; TabCtrl_InsertItem(g_hTab, 2, &tie);

        g_hCompLabel = CreateWindowExW(0, L"STATIC", L"Target Project Folder Path:", WS_CHILD | WS_VISIBLE, 30, 50, 200, 20, hwnd, NULL, NULL, NULL);
        g_hCompPathEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 30, 75, 380, 25, hwnd, NULL, NULL, NULL);
        g_hCompBrowseBtn = CreateWindowExW(0, L"BUTTON", L"Browse...", WS_CHILD | WS_VISIBLE, 420, 74, 110, 27, hwnd, (HMENU)ID_COMP_BROWSE, NULL, NULL);
        g_hCompCompileBtn = CreateWindowExW(0, L"BUTTON", L"COMPILE AND PACK FILE", WS_CHILD | WS_VISIBLE, 30, 120, 500, 40, hwnd, (HMENU)ID_COMP_COMPILE, NULL, NULL);
        g_hStatusText = CreateWindowExW(0, L"STATIC", L"Status: Idle - Waiting for directory inputs...", WS_CHILD | WS_VISIBLE, 30, 180, 500, 40, hwnd, NULL, NULL, NULL);

        g_hLaunchLoadBtn = CreateWindowExW(0, L"BUTTON", L"open macf", WS_CHILD, 30, 60, 500, 50, hwnd, (HMENU)ID_LAUNCH_LOAD, NULL, NULL);

        g_hDecompLabel = CreateWindowExW(0, L"STATIC", L"Extraction Output Target Workspace Directory:", WS_CHILD, 30, 50, 400, 20, hwnd, NULL, NULL, NULL);
        g_hDecompPathEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL, 30, 75, 380, 25, hwnd, NULL, NULL, NULL);
        g_hDecompBrowseBtn = CreateWindowExW(0, L"BUTTON", L"Target Dir...", WS_CHILD, 420, 74, 110, 27, hwnd, (HMENU)ID_DECOMP_BROWSE, NULL, NULL);
        g_hDecompExtractBtn = CreateWindowExW(0, L"BUTTON", L"DECOMPILE AND EXTRACT ARCHIVE IMAGES", WS_CHILD, 30, 120, 500, 45, hwnd, (HMENU)ID_DECOMP_EXTRACT, NULL, NULL);

        g_OrgBtnProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(g_hCompBrowseBtn, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(BlueControlSubclassProc)));
        SetWindowLongPtrW(g_hCompCompileBtn, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(BlueControlSubclassProc));
        SetWindowLongPtrW(g_hLaunchLoadBtn, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(BlueControlSubclassProc));
        SetWindowLongPtrW(g_hDecompBrowseBtn, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(BlueControlSubclassProc));
        SetWindowLongPtrW(g_hDecompExtractBtn, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(BlueControlSubclassProc));

        HFONT hDefaultFont = CreateFontW(15, 0, 0, 0, FW_HEAVY, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        SendMessage(g_hCompLabel, WM_SETFONT, (WPARAM)hDefaultFont, TRUE); SendMessage(g_hCompPathEdit, WM_SETFONT, (WPARAM)hDefaultFont, TRUE);
        SendMessage(g_hStatusText, WM_SETFONT, (WPARAM)hDefaultFont, TRUE); SendMessage(g_hDecompLabel, WM_SETFONT, (WPARAM)hDefaultFont, TRUE);
        SendMessage(g_hDecompPathEdit, WM_SETFONT, (WPARAM)hDefaultFont, TRUE); SendMessage(g_hCompBrowseBtn, WM_SETFONT, (WPARAM)hDefaultFont, TRUE);
        SendMessage(g_hCompCompileBtn, WM_SETFONT, (WPARAM)hDefaultFont, TRUE); SendMessage(g_hLaunchLoadBtn, WM_SETFONT, (WPARAM)hDefaultFont, TRUE);
        SendMessage(g_hDecompBrowseBtn, WM_SETFONT, (WPARAM)hDefaultFont, TRUE); SendMessage(g_hDecompExtractBtn, WM_SETFONT, (WPARAM)hDefaultFont, TRUE);

        UpdateTabVisibility(0);
        return 0;
    }
    case WM_NOTIFY: {
        LPNMHDR pnmhdr = (LPNMHDR)lParam;
        if (pnmhdr->idFrom == ID_TABCTRL && pnmhdr->code == TCN_SELCHANGE) {
            UpdateTabVisibility(TabCtrl_GetCurSel(g_hTab));
            InvalidateRect(hwnd, NULL, TRUE);
        }
        return 0;
    }
    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
        case ID_COMP_BROWSE: {
            std::wstring selected = BrowseFolderModern(hwnd);
            if (!selected.empty()) SetWindowTextW(g_hCompPathEdit, selected.c_str());
            break;
        }
        case ID_COMP_COMPILE: {
            wchar_t folder[MAX_PATH] = { 0 };
            GetWindowTextW(g_hCompPathEdit, folder, MAX_PATH);
            CompileMacf(hwnd, folder);
            break;
        }
        case ID_LAUNCH_LOAD: {
            std::wstring filepath = BrowseMacfFileModern(hwnd);
            if (!filepath.empty() && LoadMacfFile(filepath)) {
                g_CurrentFrame = 0;
                g_IsRunning = true;
                KillTimer(hwnd, 1);
                ApplyAdaptiveTimer(hwnd, g_CurrentFrame);

                if (!g_hDisplayWnd) {
                    g_hDisplayWnd = CreateWindowExW(WS_EX_TOPMOST, DISPLAY_CLASS_NAME, L"WinAct Viewer Screen",
                        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                        CW_USEDEFAULT, CW_USEDEFAULT, 400, 400, hwnd, NULL, GetModuleHandle(NULL), NULL);
                }
                ShowWindow(g_hDisplayWnd, SW_SHOW);
                UpdateWindow(g_hDisplayWnd);
            }
            break;
        }
        case ID_DECOMP_BROWSE: {
            std::wstring selected = BrowseFolderModern(hwnd);
            if (!selected.empty()) SetWindowTextW(g_hDecompPathEdit, selected.c_str());
            break;
        }
        case ID_DECOMP_EXTRACT: {
            wchar_t export_folder[MAX_PATH] = { 0 };
            GetWindowTextW(g_hDecompPathEdit, export_folder, MAX_PATH);
            std::wstring target_archive = BrowseMacfFileModern(hwnd);
            if (!target_archive.empty()) {
                DecompileMacf(hwnd, target_archive, export_folder);
            }
            break;
        }
        }
        return 0;
    }
    case WM_TIMER:
        if (g_IsRunning && !g_Agent.frames.empty()) {
            g_CurrentFrame++;
            if (g_CurrentFrame >= g_Agent.frames.size()) g_CurrentFrame = 0;
            ApplyAdaptiveTimer(hwnd, g_CurrentFrame);

            if (g_hDisplayWnd) {
                InvalidateRect(g_hDisplayWnd, NULL, FALSE);
            }
        }
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        ClearActiveBitmaps();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    UNREFERENCED_PARAMETER(hPrevInstance); UNREFERENCED_PARAMETER(lpCmdLine);
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory, reinterpret_cast<void**>(&g_pWICFactory));

    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&g_GdiplusToken, &gdiplusStartupInput, NULL);

    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_TAB_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    const wchar_t MAIN_CLASS_NAME[] = L"WinAct_Studio_Terminal";
    WNDCLASSW wcMain = { };
    wcMain.lpfnWndProc = MainWindowProc;
    wcMain.hInstance = hInstance;
    wcMain.lpszClassName = MAIN_CLASS_NAME;
    wcMain.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wcMain);

    WNDCLASSW wcDisplay = { };
    wcDisplay.lpfnWndProc = DisplayWindowProc;
    wcDisplay.hInstance = hInstance;
    wcDisplay.lpszClassName = DISPLAY_CLASS_NAME;
    wcDisplay.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wcDisplay);

    g_hMainWnd = CreateWindowExW(0, MAIN_CLASS_NAME, L"WinAct",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 400, NULL, NULL, hInstance, NULL);

    if (g_hMainWnd == NULL) return 0;
    ShowWindow(g_hMainWnd, nCmdShow);

    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    Gdiplus::GdiplusShutdown(g_GdiplusToken);
    if (g_pWICFactory) g_pWICFactory->Release();
    CoUninitialize();
    return 0;
}