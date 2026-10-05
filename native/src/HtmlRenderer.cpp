#include "HtmlRenderer.h"
#include <shlwapi.h>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>
#include <memory>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "gdiplus.lib")

bool HtmlRenderer::IsSupportedExtension(const std::wstring& ext) {
    std::wstring lowerExt = ext;
    std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(), ::towlower);
    return (lowerExt == L".html" || lowerExt == L".htm");
}

std::wstring HtmlRenderer::FindBrowserPath() {
    static const wchar_t* kPaths[] = {
        L"C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe",
        L"C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe",
        L"C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe",
        L"C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe",
        L"C:\\Program Files\\BraveSoftware\\Brave-Browser\\Application\\brave.exe",
    };

    for (const auto* p : kPaths) {
        if (PathFileExistsW(p)) return p;
    }

    // Registry check
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\chrome.exe", 0, KEY_READ, &hKey) == ERROR_SUCCESS ||
        RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\msedge.exe", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t buf[MAX_PATH] = {0};
        DWORD bufSize = sizeof(buf);
        if (RegQueryValueExW(hKey, nullptr, nullptr, nullptr, (LPBYTE)buf, &bufSize) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            if (PathFileExistsW(buf)) return buf;
        } else {
            RegCloseKey(hKey);
        }
    }

    return L"";
}

HBITMAP HtmlRenderer::GdiplusBitmapToHBitmap(Gdiplus::Bitmap* pBmp, int width, int height) {
    if (!pBmp) return nullptr;

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HDC hdc = GetDC(nullptr);
    HBITMAP hBitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
    ReleaseDC(nullptr, hdc);

    if (!hBitmap || !pBits) {
        if (hBitmap) DeleteObject(hBitmap);
        return nullptr;
    }

    Gdiplus::BitmapData bmpData;
    Gdiplus::Rect rect(0, 0, width, height);
    if (pBmp->LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bmpData) == Gdiplus::Ok) {
        int dstStride = width * 4;
        for (int y = 0; y < height; ++y) {
            memcpy((BYTE*)pBits + y * dstStride, (BYTE*)bmpData.Scan0 + y * bmpData.Stride, dstStride);
        }
        pBmp->UnlockBits(&bmpData);
    }

    return hBitmap;
}

HtmlThumbnailResult HtmlRenderer::RenderThumbnail(const std::wstring& filePath, UINT cx) {
    HtmlThumbnailResult res;
    // A4 Aspect Ratio: Width / Height = 1 / 1.4142 = ~0.707
    int targetH = (int)cx;
    if (targetH < 64) targetH = 64;
    if (targetH > 1024) targetH = 1024;
    int targetW = (int)(targetH / 1.4142f);
    if (targetW < 48) targetW = 48;

    res.width = targetW;
    res.height = targetH;

    std::wstring browser = FindBrowserPath();
    bool renderedWithBrowser = false;

    auto finalBmp = std::make_unique<Gdiplus::Bitmap>(targetW, targetH, PixelFormat32bppARGB);
    {
        Gdiplus::Graphics g(finalBmp.get());
        g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.Clear(Gdiplus::Color(255, 255, 255, 255)); // White page sheet

        if (!browser.empty() && PathFileExistsW(filePath.c_str())) {
            wchar_t tempDir[MAX_PATH] = {0};
            GetTempPathW(MAX_PATH, tempDir);
            wchar_t tempFile[MAX_PATH] = {0};
            GetTempFileNameW(tempDir, L"tfh", 0, tempFile);
            std::wstring tempPng = tempFile;
            tempPng += L".png";
            DeleteFileW(tempFile);

            // Execute headless browser screenshot in A4 page size
            // 1240 x 1754 corresponds to 150 DPI A4
            std::wstring cmd = L"\"" + browser + L"\" --headless=new --disable-gpu --screenshot=\"" + tempPng +
                               L"\" --window-size=1240,1754 --hide-scrollbars --virtual-time-budget=1000 \"file:///" + filePath + L"\"";
            std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
            cmdBuf.push_back(L'\0');

            STARTUPINFOW si = { sizeof(si) };
            si.dwFlags = STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_HIDE;
            PROCESS_INFORMATION pi = { 0 };

            if (CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
                WaitForSingleObject(pi.hProcess, 5000);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);

                if (PathFileExistsW(tempPng.c_str())) {
                    std::unique_ptr<Gdiplus::Bitmap> pShot(Gdiplus::Bitmap::FromFile(tempPng.c_str()));
                    if (pShot && pShot->GetLastStatus() == Gdiplus::Ok &&
                        pShot->GetWidth() > 0 && pShot->GetHeight() > 0) {
                        
                        // Draw browser screenshot fitting the entire A4 canvas
                        g.DrawImage(pShot.get(), 0, 0, targetW, targetH);
                        renderedWithBrowser = true;
                    }
                    DeleteFileW(tempPng.c_str());
                }
            }
        }

        // Fallback: If browser failed, render a clean styled A4 document preview sheet (NOT code!)
        if (!renderedWithBrowser) {
            // Read title from HTML
            std::wstring title = L"HTML Document";
            if (PathFileExistsW(filePath.c_str())) {
                std::ifstream f(filePath);
                if (f.is_open()) {
                    std::stringstream ss;
                    ss << f.rdbuf();
                    std::string content = ss.str();
                    size_t t1 = content.find("<title>");
                    size_t t2 = content.find("</title>");
                    if (t1 != std::string::npos && t2 != std::string::npos && t2 > t1 + 7) {
                        std::string rawTitle = content.substr(t1 + 7, t2 - (t1 + 7));
                        if (!rawTitle.empty()) {
                            title = std::wstring(rawTitle.begin(), rawTitle.end());
                        }
                    }
                }
            }

            // Draw clean document page header
            Gdiplus::SolidBrush barBrush(Gdiplus::Color(255, 16, 64, 192)); // Bauhaus Blue top bar
            g.FillRectangle(&barBrush, 0, 0, targetW, (int)(targetH * 0.04f));

            // Document Title
            Gdiplus::FontFamily fontFamily(L"Segoe UI");
            Gdiplus::Font titleFont(&fontFamily, targetH * 0.05f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
            Gdiplus::SolidBrush titleBrush(Gdiplus::Color(255, 18, 18, 18));
            Gdiplus::RectF titleRect((float)(targetW * 0.08f), (float)(targetH * 0.08f), (float)(targetW * 0.84f), (float)(targetH * 0.15f));
            g.DrawString(title.c_str(), -1, &titleFont, titleRect, nullptr, &titleBrush);

            // Subtle styled paragraphs
            Gdiplus::SolidBrush pBrush(Gdiplus::Color(255, 230, 230, 235));
            for (int i = 0; i < 7; ++i) {
                float y = targetH * 0.26f + i * (targetH * 0.09f);
                float w = (i % 2 == 1) ? targetW * 0.55f : targetW * 0.84f;
                g.FillRectangle(&pBrush, (float)(targetW * 0.08f), y, w, (float)(targetH * 0.035f));
            }
        }

        // Draw crisp A4 document sheet border
        Gdiplus::Pen borderPen(Gdiplus::Color(255, 200, 200, 205), 1.0f);
        g.DrawRectangle(&borderPen, 0, 0, targetW - 1, targetH - 1);
    }

    res.hBitmap = GdiplusBitmapToHBitmap(finalBmp.get(), targetW, targetH);
    res.success = (res.hBitmap != nullptr);
    return res;
}

HtmlThumbnailResult HtmlRenderer::RenderSample(UINT cx) {
    return RenderThumbnail(L"", cx);
}
