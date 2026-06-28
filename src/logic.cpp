#include "resource.h"
#include <fstream>
#include <algorithm>

// Define the global instances
HWND g_hMainWnd = NULL;
HWND g_hDisplayWnd = NULL;
HWND g_hTab = NULL;
HWND g_hCompLabel = NULL;
HWND g_hCompPathEdit = NULL;
HWND g_hCompBrowseBtn = NULL;
HWND g_hCompCompileBtn = NULL;
HWND g_hStatusText = NULL;
HWND g_hLaunchLoadBtn = NULL;
HWND g_hDecompLabel = NULL;
HWND g_hDecompPathEdit = NULL;
HWND g_hDecompBrowseBtn = NULL;
HWND g_hDecompExtractBtn = NULL;
WNDPROC g_OrgTabProc = NULL;
WNDPROC g_OrgBtnProc = NULL;

MacfData g_Agent;
size_t g_CurrentFrame = 0;
bool g_IsRunning = false;
ULONG_PTR g_GdiplusToken = 0;
IWICImagingFactory* g_pWICFactory = NULL;
const wchar_t DISPLAY_CLASS_NAME[] = L"MACF_Display_Window";

void ClearActiveBitmaps() {
    for (auto& f : g_Agent.frames) {
        if (f.bitmap) delete f.bitmap;
    }
    g_Agent.frames.clear();
}

void UpdateTabVisibility(int selectedTab) {
    int compShow = (selectedTab == 0) ? SW_SHOW : SW_HIDE;
    ShowWindow(g_hCompLabel, compShow); ShowWindow(g_hCompPathEdit, compShow);
    ShowWindow(g_hCompBrowseBtn, compShow); ShowWindow(g_hCompCompileBtn, compShow);
    ShowWindow(g_hStatusText, compShow);

    int playShow = (selectedTab == 1) ? SW_SHOW : SW_HIDE;
    ShowWindow(g_hLaunchLoadBtn, playShow);

    int decompShow = (selectedTab == 2) ? SW_SHOW : SW_HIDE;
    ShowWindow(g_hDecompLabel, decompShow); ShowWindow(g_hDecompPathEdit, decompShow);
    ShowWindow(g_hDecompBrowseBtn, decompShow); ShowWindow(g_hDecompExtractBtn, decompShow);
}

std::wstring BrowseFolderModern(HWND hwndOwner) {
    std::wstring outPath = L"";
    IFileOpenDialog* pFileOpen = nullptr;
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (SUCCEEDED(hr)) {
        hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));
        if (SUCCEEDED(hr)) {
            DWORD dwOptions;
            if (SUCCEEDED(pFileOpen->GetOptions(&dwOptions))) {
                pFileOpen->SetOptions(dwOptions | FOS_PICKFOLDERS);
            }
            if (SUCCEEDED(pFileOpen->Show(hwndOwner))) {
                IShellItem* pItem = nullptr;
                if (SUCCEEDED(pFileOpen->GetResult(&pItem))) {
                    PWSTR pszFilePath = nullptr;
                    if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath))) {
                        outPath = pszFilePath;
                        CoTaskMemFree(pszFilePath);
                    }
                    pItem->Release();
                }
            }
            pFileOpen->Release();
        }
        CoUninitialize();
    }
    return outPath;
}

std::wstring BrowseMacfFileModern(HWND hwndOwner) {
    std::wstring outPath = L"";
    IFileOpenDialog* pFileOpen = nullptr;
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (SUCCEEDED(hr)) {
        hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));
        if (SUCCEEDED(hr)) {
            COMDLG_FILTERSPEC fileSpecs[] = { { L"MACF Archive File (*.macf)", L"*.macf" }, { L"All Files (*.*)", L"*.*" } };
            pFileOpen->SetFileTypes(2, fileSpecs);
            pFileOpen->SetDefaultExtension(L"macf");
            if (SUCCEEDED(pFileOpen->Show(hwndOwner))) {
                IShellItem* pItem = nullptr;
                if (SUCCEEDED(pFileOpen->GetResult(&pItem))) {
                    PWSTR pszFilePath = nullptr;
                    if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath))) {
                        outPath = pszFilePath;
                        CoTaskMemFree(pszFilePath);
                    }
                    pItem->Release();
                }
            }
            pFileOpen->Release();
        }
        CoUninitialize();
    }
    return outPath;
}

int ParseIniValue(const fs::path& folder_path) {
    std::ifstream file(folder_path / "config.ini");
    std::string line;
    if (file.is_open()) {
        std::getline(file, line);
        line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
        line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());
        if (!line.empty() && line.back() == 's') {
            try { return std::stoi(line.substr(0, line.size() - 1)); }
            catch (...) {}
        }
    }
    return 1;
}

void CompileMacf(HWND hwnd, const std::wstring& source_folder_str) {
    if (source_folder_str.empty()) {
        SetWindowTextW(g_hStatusText, L"Status: Error - No folder path selected.");
        return;
    }

    fs::path source_folder(source_folder_str);
    if (!fs::exists(source_folder)) {
        SetWindowTextW(g_hStatusText, L"Status: Error - Directory path does not exist.");
        return;
    }

    fs::path output_path = source_folder / "agent.macf";
    int delay_seconds = ParseIniValue(source_folder);

    std::ofstream out(output_path, std::ios::binary);
    if (!out) {
        SetWindowTextW(g_hStatusText, L"Status: Error - Cannot write output file.");
        return;
    }

    out.write("MACF", 4);
    out.write(reinterpret_cast<const char*>(&delay_seconds), sizeof(delay_seconds));

    std::vector<std::pair<fs::path, int>> image_files;
    for (int i = 1; i <= 100; ++i) {
        std::string numStr = std::to_string(i);
        std::vector<std::string> variations = {
            "act " + numStr, "Act " + numStr, "ACT " + numStr,
            "act" + numStr,  "Act" + numStr,  "ACT" + numStr
        };

        for (const auto& var : variations) {
            fs::path act_path = source_folder / var;
            if (fs::exists(act_path)) {
                std::vector<fs::path> local_files;
                for (const auto& entry : fs::directory_iterator(act_path)) {
                    if (entry.is_regular_file()) {
                        std::wstring ext = entry.path().extension().wstring();
                        std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
                        if (ext == L".png" || ext == L".webp" || ext == L".jpg" || ext == L".jpeg") {
                            local_files.push_back(entry.path());
                        }
                    }
                }
                std::sort(local_files.begin(), local_files.end());
                for (const auto& lf : local_files) {
                    image_files.push_back({ lf, i });
                }
                break;
            }
        }
    }

    size_t image_count = image_files.size();
    out.write(reinterpret_cast<const char*>(&image_count), sizeof(image_count));

    for (const auto& img_data : image_files) {
        std::ifstream img(img_data.first, std::ios::binary);
        if (!img) continue;
        std::vector<char> buffer((std::istreambuf_iterator<char>(img)), std::istreambuf_iterator<char>());
        size_t img_size = buffer.size();
        int act_id = img_data.second;

        out.write(reinterpret_cast<const char*>(&img_size), sizeof(img_size));
        out.write(reinterpret_cast<const char*>(&act_id), sizeof(act_id));
        out.write(buffer.data(), img_size);
    }

    if (image_count == 0) {
        SetWindowTextW(g_hStatusText, L"Status: Finished but found 0 images! Put files inside act folders.");
    }
    else {
        std::wstring status = L"Status: Compiled! Saved 'agent.macf' (" + std::to_wstring(image_count) + L" frames packed)";
        SetWindowTextW(g_hStatusText, status.c_str());
    }
}

void DecompileMacf(HWND hwnd, const std::wstring& macf_file_path, const std::wstring& export_dir_str) {
    if (macf_file_path.empty() || export_dir_str.empty()) {
        MessageBoxW(hwnd, L"Please select an extraction output directory path first.", L"Decompiler Error", MB_ICONERROR);
        return;
    }

    std::ifstream in(macf_file_path, std::ios::binary);
    if (!in) {
        MessageBoxW(hwnd, L"Failed to read selected archive file.", L"Decompiler Error", MB_ICONERROR);
        return;
    }

    char magic[4] = { 0 };
    in.read(magic, 4);
    if (std::string(magic, 4) != "MACF") {
        MessageBoxW(hwnd, L"Invalid header data. This is not a valid MACF archive file.", L"Decompiler Error", MB_ICONERROR);
        return;
    }

    int delay_seconds = 1;
    in.read(reinterpret_cast<char*>(&delay_seconds), sizeof(delay_seconds));

    size_t image_count = 0;
    in.read(reinterpret_cast<char*>(&image_count), sizeof(image_count));

    fs::path base_out(export_dir_str);
    fs::create_directories(base_out);

    std::ofstream ini_out(base_out / "config.ini");
    if (ini_out) {
        ini_out << delay_seconds << "s\n";
        ini_out.close();
    }

    int act_frame_counters[101] = { 0 };

    for (size_t i = 0; i < image_count; ++i) {
        size_t img_size = 0;
        if (!in.read(reinterpret_cast<char*>(&img_size), sizeof(img_size))) break;

        int act_id = 1;
        if (!in.read(reinterpret_cast<char*>(&act_id), sizeof(act_id))) break;

        if (img_size == 0) continue;

        std::vector<char> buffer(img_size);
        if (!in.read(buffer.data(), img_size)) break;

        fs::path act_folder = base_out / ("act " + std::to_string(act_id));
        fs::create_directories(act_folder);

        act_frame_counters[act_id]++;
        std::string filename = "extracted_frame_" + std::to_string(act_frame_counters[act_id]);

        if (img_size > 4 && (unsigned char)buffer[0] == 0x89 && buffer[1] == 'P' && buffer[2] == 'N' && buffer[3] == 'G') {
            filename += ".png";
        }
        else if (img_size > 12 && buffer[8] == 'W' && buffer[9] == 'E' && buffer[10] == 'B' && buffer[11] == 'P') {
            filename += ".webp";
        }
        else {
            filename += ".jpg";
        }

        std::ofstream img_out(act_folder / filename, std::ios::binary);
        if (img_out) {
            img_out.write(buffer.data(), img_size);
            img_out.close();
        }
    }

    std::wstring completion_msg = L"Decompilation complete! Unpacked contents (" + std::to_wstring(image_count) + L" frames) successfully.";
    MessageBoxW(hwnd, completion_msg.c_str(), L"Success", MB_OK | MB_ICONINFORMATION);
}

Gdiplus::Bitmap* LoadFromWICTranslation(::IStream* pStream) {
    if (!g_pWICFactory) return NULL;
    Gdiplus::Bitmap* pGdiplusBitmap = NULL;
    IWICBitmapDecoder* pDecoder = NULL;
    HRESULT hr = g_pWICFactory->CreateDecoderFromStream(pStream, NULL, WICDecodeMetadataCacheOnDemand, &pDecoder);
    if (SUCCEEDED(hr)) {
        IWICBitmapFrameDecode* pFrame = NULL;
        if (SUCCEEDED(pDecoder->GetFrame(0, &pFrame))) {
            IWICFormatConverter* pConverter = NULL;
            if (SUCCEEDED(g_pWICFactory->CreateFormatConverter(&pConverter))) {
                if (SUCCEEDED(pConverter->Initialize(pFrame, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, NULL, 0.f, WICBitmapPaletteTypeCustom))) {
                    UINT width = 0, height = 0;
                    pConverter->GetSize(&width, &height);
                    pGdiplusBitmap = new Gdiplus::Bitmap(width, height, PixelFormat32bppARGB);
                    Gdiplus::BitmapData bmpData;
                    Gdiplus::Rect rect(0, 0, width, height);
                    if (pGdiplusBitmap->LockBits(&rect, Gdiplus::ImageLockModeWrite, PixelFormat32bppARGB, &bmpData) == Gdiplus::Ok) {
                        pConverter->CopyPixels(NULL, bmpData.Stride, bmpData.Stride * height, reinterpret_cast<BYTE*>(bmpData.Scan0));
                        pGdiplusBitmap->UnlockBits(&bmpData);
                    }
                    else {
                        delete pGdiplusBitmap;
                        pGdiplusBitmap = NULL;
                    }
                }
                pConverter->Release();
            }
            pFrame->Release();
        }
        pDecoder->Release();
    }
    return pGdiplusBitmap;
}

bool LoadMacfFile(const std::wstring& filepath) {
    std::ifstream in(filepath, std::ios::binary);
    if (!in) return false;

    char magic[4];
    in.read(magic, 4);
    if (std::string(magic, 4) != "MACF") return false;

    ClearActiveBitmaps();
    in.read(reinterpret_cast<char*>(&g_Agent.delay_seconds), sizeof(g_Agent.delay_seconds));

    size_t image_count = 0;
    in.read(reinterpret_cast<char*>(&image_count), sizeof(image_count));

    for (size_t i = 0; i < image_count; ++i) {
        size_t img_size = 0;
        in.read(reinterpret_cast<char*>(&img_size), sizeof(img_size));
        int act_id = 1;
        in.read(reinterpret_cast<char*>(&act_id), sizeof(act_id));

        if (img_size == 0) continue;
        std::vector<char> buffer(img_size);
        in.read(buffer.data(), img_size);

        ::IStream* pStream = SHCreateMemStream(reinterpret_cast<const BYTE*>(buffer.data()), static_cast<UINT>(buffer.size()));
        if (pStream) {
            Gdiplus::Bitmap* pBitmap = LoadFromWICTranslation(pStream);
            if (pBitmap) {
                FrameInfo fi;
                fi.bitmap = pBitmap;
                fi.parent_act = act_id;
                g_Agent.frames.push_back(fi);
            }
            pStream->Release();
        }
    }
    return !g_Agent.frames.empty();
}

void ApplyAdaptiveTimer(HWND hwnd, size_t frameIdx) {
    if (g_Agent.frames.empty() || frameIdx >= g_Agent.frames.size()) return;
    UINT nextDelay = 50;
    if (g_Agent.frames[frameIdx].parent_act == 1) {
        nextDelay = static_cast<UINT>(g_Agent.delay_seconds * 1000);
        if (nextDelay == 0) nextDelay = 1000;
    }
    SetTimer(hwnd, 1, nextDelay, NULL);
}

LRESULT CALLBACK BlueControlSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;

        Gdiplus::Graphics graphics(hdc);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);

        Gdiplus::LinearGradientBrush linGrad(Gdiplus::Rect(0, 0, w, h),
            Gdiplus::Color(255, 25, 110, 235), Gdiplus::Color(255, 5, 25, 85), Gdiplus::LinearGradientModeVertical);
        graphics.FillRectangle(&linGrad, 0, 0, w, h);

        Gdiplus::LinearGradientBrush glossGrad(Gdiplus::Rect(0, 0, w, h / 2),
            Gdiplus::Color(140, 255, 255, 255), Gdiplus::Color(0, 255, 255, 255), Gdiplus::LinearGradientModeVertical);
        graphics.FillRectangle(&glossGrad, 0, 0, w, h / 2);

        Gdiplus::Pen edgePen(Gdiplus::Color(255, 80, 185, 255), 2.0f);
        graphics.DrawRectangle(&edgePen, 0, 0, w - 1, h - 1);

        wchar_t text[256]; GetWindowTextW(hwnd, text, 256);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));

        HFONT hFont = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0);
        HGDIOBJ hOldFont = SelectObject(hdc, hFont);
        DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdc, hOldFont);
        EndPaint(hwnd, &ps);
        return 0;
    }
    return CallWindowProcW(g_OrgBtnProc, hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK TabSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rcClient; GetClientRect(hwnd, &rcClient);
        int width = rcClient.right - rcClient.left;
        int height = rcClient.bottom - rcClient.top;

        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hbmMem = CreateCompatibleBitmap(hdc, width, height);
        HGDIOBJ hOldObj = SelectObject(hdcMem, hbmMem);

        CallWindowProcW(g_OrgTabProc, hwnd, WM_PAINT, reinterpret_cast<WPARAM>(hdcMem), 0);

        Gdiplus::Graphics graphics(hdcMem);
        Gdiplus::LinearGradientBrush bodyGrad(Gdiplus::Rect(0, 0, width, height),
            Gdiplus::Color(255, 8, 30, 75), Gdiplus::Color(255, 2, 10, 30), Gdiplus::LinearGradientModeVertical);

        RECT rcFill = { 5, 30, width - 5, height - 5 };
        Gdiplus::Rect gdiFill(rcFill.left, rcFill.top, rcFill.right - rcFill.left, rcFill.bottom - rcFill.top);
        graphics.FillRectangle(&bodyGrad, gdiFill);

        Gdiplus::Pen highlightPen(Gdiplus::Color(255, 0, 140, 255), 2.0f);
        graphics.DrawRectangle(&highlightPen, gdiFill);

        BitBlt(hdc, 0, 0, width, height, hdcMem, 0, 0, SRCCOPY);
        SelectObject(hdcMem, hOldObj);
        DeleteObject(hbmMem);
        DeleteDC(hdcMem);
        EndPaint(hwnd, &ps);
        return 0;
    }
    return CallWindowProcW(g_OrgTabProc, hwnd, uMsg, wParam, lParam);
}