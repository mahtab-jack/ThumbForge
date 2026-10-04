#include "VideoRenderer.h"
#include <shlwapi.h>
#include <sstream>
#include <cmath>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "gdiplus.lib")

bool VideoRenderer::IsSupportedExtension(const std::wstring& ext) {
    std::wstring lowerExt = ext;
    std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(), ::towlower);
    for (const auto& supp : kSupportedVideoExtensions) {
        if (lowerExt == supp) return true;
    }
    return false;
}

std::wstring VideoRenderer::GetFFmpegPath() {
    // 1. Check registry override
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\PreviewIcon", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t buf[MAX_PATH] = {0};
        DWORD bufSize = sizeof(buf);
        if (RegQueryValueExW(hKey, L"FFmpegPath", nullptr, nullptr, (LPBYTE)buf, &bufSize) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            if (PathFileExistsW(buf)) return buf;
        }
        RegCloseKey(hKey);
    }

    // 2. Check alongside PreviewIconProvider.dll directory
    extern HMODULE g_hModule;
    if (g_hModule) {
        wchar_t dllDir[MAX_PATH] = {0};
        if (GetModuleFileNameW(g_hModule, dllDir, MAX_PATH) > 0) {
            PathRemoveFileSpecW(dllDir);
            PathAppendW(dllDir, L"ffmpeg.exe");
            if (PathFileExistsW(dllDir)) return dllDir;
        }
    }

    // 3. Check standard system location
    const wchar_t* standardPath = L"C:\\ffmpeg\\bin\\ffmpeg.exe";
    if (PathFileExistsW(standardPath)) {
        return standardPath;
    }

    // 4. Check alongside calling process executable
    wchar_t modulePath[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    PathRemoveFileSpecW(modulePath);
    PathAppendW(modulePath, L"ffmpeg.exe");
    if (PathFileExistsW(modulePath)) {
        return modulePath;
    }

    // 4. Fall back to search in PATH
    wchar_t pathBuf[MAX_PATH] = {0};
    if (SearchPathW(nullptr, L"ffmpeg.exe", nullptr, MAX_PATH, pathBuf, nullptr) > 0) {
        return pathBuf;
    }

    return L"ffmpeg.exe";
}

bool VideoRenderer::ExtractFrameBmp(const std::wstring& ffmpegPath, const std::wstring& filePath, const std::string& timecode, std::vector<BYTE>& outBmpData) {
    HANDLE hReadPipe = nullptr;
    HANDLE hWritePipe = nullptr;

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        return false;
    }
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    // Build command line: ffmpeg -y -ss <timecode> -i "path" -vframes 1 -f image2pipe -vcodec bmp pipe:1
    std::wstring cmdLine = L"\"" + ffmpegPath + L"\" -y -ss " + std::wstring(timecode.begin(), timecode.end())
        + L" -i \"" + filePath + L"\" -vframes 1 -f image2pipe -vcodec bmp pipe:1";

    STARTUPINFOW si = {0};
    si.cb = sizeof(STARTUPINFOW);
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = hWritePipe;
    si.hStdError = nullptr;
    si.hStdInput = nullptr;


    PROCESS_INFORMATION pi = {0};

    std::vector<wchar_t> cmdBuf(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back(L'\0');

    BOOL created = CreateProcessW(
        nullptr,
        cmdBuf.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &si,
        &pi
    );

    CloseHandle(hWritePipe); // Close write end in parent so ReadFile hits EOF

    if (!created) {
        CloseHandle(hReadPipe);
        return false;
    }

    // Read all bytes from pipe
    BYTE buffer[16384];
    DWORD bytesRead = 0;
    outBmpData.clear();

    while (ReadFile(hReadPipe, buffer, sizeof(buffer), &bytesRead, nullptr) && bytesRead > 0) {
        outBmpData.insert(outBmpData.end(), buffer, buffer + bytesRead);
    }

    CloseHandle(hReadPipe);

    WaitForSingleObject(pi.hProcess, 5000);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return !outBmpData.empty();
}

void VideoRenderer::DrawSubtlePlayButton(Gdiplus::Graphics& g, float width, float height) {
    g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);

    float cx0 = width * 0.5f;
    float cy0 = height * 0.5f;

    // Radius proportional to smallest dimension, clamped
    float minDim = (std::min)(width, height);
    float r = minDim * 0.17f;
    if (r < 11.0f) r = 11.0f;
    if (r > 38.0f) r = 38.0f;

    // Frosted dark circular backdrop
    Gdiplus::SolidBrush discBg(Gdiplus::Color(115, 9, 9, 11)); // Rich black with ~45% opacity
    Gdiplus::Pen rimPen(Gdiplus::Color(140, 250, 250, 250), 1.2f); // Subtle translucent off-white rim

    g.FillEllipse(&discBg, cx0 - r, cy0 - r, 2.0f * r, 2.0f * r);
    g.DrawEllipse(&rimPen, cx0 - r, cy0 - r, 2.0f * r, 2.0f * r);

    // Play triangle pointing right, precisely centered
    // Centroid of equilateral triangle is at 1/3 height, adjust visually
    float triHeight = r * 0.85f;
    float triHalfW = triHeight * 0.58f;
    float xOffset = r * 0.08f; // Slight nudge to balance visual weight

    Gdiplus::PointF pts[3];
    pts[0] = Gdiplus::PointF(cx0 - triHalfW * 0.7f + xOffset, cy0 - triHalfW);
    pts[1] = Gdiplus::PointF(cx0 - triHalfW * 0.7f + xOffset, cy0 + triHalfW);
    pts[2] = Gdiplus::PointF(cx0 + triHalfW * 1.15f + xOffset, cy0);

    Gdiplus::SolidBrush playBrush(Gdiplus::Color(235, 250, 250, 250)); // Crisp off-white
    g.FillPolygon(&playBrush, pts, 3);
}

HBITMAP VideoRenderer::GdiplusBitmapToHBitmap(Gdiplus::Bitmap* pBmp, int width, int height) {
    if (!pBmp) return nullptr;

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; // Top-down
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

VideoThumbnailResult VideoRenderer::RenderThumbnail(const std::wstring& filePath, UINT cx) {
    VideoThumbnailResult res;
    std::wstring ffmpegPath = GetFFmpegPath();

    std::vector<BYTE> bmpBytes;
    // Attempt seek at 2 seconds
    bool extracted = ExtractFrameBmp(ffmpegPath, filePath, "00:00:02", bmpBytes);
    if (!extracted || bmpBytes.size() < sizeof(BITMAPFILEHEADER)) {
        // Fallback to 0.3 seconds for short clips
        extracted = ExtractFrameBmp(ffmpegPath, filePath, "00:00:00.3", bmpBytes);
    }

    if (!extracted || bmpBytes.size() < sizeof(BITMAPFILEHEADER)) {
        return res;
    }

    IStream* pStream = SHCreateMemStream(bmpBytes.data(), (UINT)bmpBytes.size());
    if (!pStream) return res;
    std::unique_ptr<Gdiplus::Bitmap> pRawBmp(Gdiplus::Bitmap::FromStream(pStream));
    pStream->Release();

    if (!pRawBmp || pRawBmp->GetLastStatus() != Gdiplus::Ok) {
        return res;
    }

    int origW = pRawBmp->GetWidth();
    int origH = pRawBmp->GetHeight();
    if (origW <= 0 || origH <= 0) return res;

    // Detect aspect ratio: Landscape, Portrait, or Square
    if (origW > origH) {
        res.aspectRatio = VideoAspectRatio::Landscape;
    } else if (origH > origW) {
        res.aspectRatio = VideoAspectRatio::Portrait;
    } else {
        res.aspectRatio = VideoAspectRatio::Square;
    }

    // Preserve the video's original aspect ratio; never stretch or distort
    double scale = (std::min)((double)cx / origW, (double)cx / origH);
    int targetW = (std::max)(1, (int)std::round(origW * scale));
    int targetH = (std::max)(1, (int)std::round(origH * scale));

    res.width = targetW;
    res.height = targetH;

    auto finalBmp = std::make_unique<Gdiplus::Bitmap>(targetW, targetH, PixelFormat32bppARGB);
    {
        Gdiplus::Graphics g(finalBmp.get());
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);

        // Draw video frame preserving exact aspect ratio
        g.DrawImage(pRawBmp.get(), 0, 0, targetW, targetH);

        // Add subtle play-button overlay
        DrawSubtlePlayButton(g, (float)targetW, (float)targetH);
    }

    res.hBitmap = GdiplusBitmapToHBitmap(finalBmp.get(), targetW, targetH);
    res.success = (res.hBitmap != nullptr);
    res.alphaType = WTSAT_RGB;
    return res;
}

VideoThumbnailResult VideoRenderer::RenderSample(int sampleType, UINT cx) {
    VideoThumbnailResult res;
    int targetW = 0, targetH = 0;

    if (sampleType == 0) {
        // Landscape (16:9)
        res.aspectRatio = VideoAspectRatio::Landscape;
        targetW = (int)cx;
        targetH = (int)std::round(cx * 9.0 / 16.0);
    } else if (sampleType == 1) {
        // Portrait (9:16 Shorts/Reels)
        res.aspectRatio = VideoAspectRatio::Portrait;
        targetH = (int)cx;
        targetW = (int)std::round(cx * 9.0 / 16.0);
    } else {
        // Square (1:1)
        res.aspectRatio = VideoAspectRatio::Square;
        targetW = (int)cx;
        targetH = (int)cx;
    }

    if (targetW < 1) targetW = 1;
    if (targetH < 1) targetH = 1;
    res.width = targetW;
    res.height = targetH;

    auto composite = std::make_unique<Gdiplus::Bitmap>(targetW, targetH, PixelFormat32bppARGB);
    {
        Gdiplus::Graphics g(composite.get());
        g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);

        // Dark rich cinematic background
        Gdiplus::SolidBrush bgBrush(Gdiplus::Color(255, 18, 18, 22));
        g.FillRectangle(&bgBrush, 0, 0, targetW, targetH);

        // Geometric aesthetic lines for mock scene
        Gdiplus::Pen gridPen(Gdiplus::Color(255, 39, 39, 42), 1.0f); // #27272A
        for (int x = 0; x < targetW; x += (std::max)(12, targetW / 8)) {
            g.DrawLine(&gridPen, x, 0, x, targetH);
        }
        for (int y = 0; y < targetH; y += (std::max)(12, targetH / 8)) {
            g.DrawLine(&gridPen, 0, y, targetW, y);
        }

        // Minimal horizon mountain / cinematic geometry
        Gdiplus::PointF horizon[5];
        horizon[0] = Gdiplus::PointF(0, (float)targetH);
        horizon[1] = Gdiplus::PointF(0, targetH * 0.72f);
        horizon[2] = Gdiplus::PointF(targetW * 0.45f, targetH * 0.48f);
        horizon[3] = Gdiplus::PointF((float)targetW, targetH * 0.65f);
        horizon[4] = Gdiplus::PointF((float)targetW, (float)targetH);

        Gdiplus::SolidBrush mountainBrush(Gdiplus::Color(255, 28, 28, 34));
        g.FillPolygon(&mountainBrush, horizon, 5);

        // Accent indicator bar in top left (#DFE104)
        Gdiplus::SolidBrush accentBrush(Gdiplus::Color(255, 223, 225, 4));
        g.FillRectangle(&accentBrush, (int)(targetW * 0.08f), (int)(targetH * 0.08f), (int)(targetW * 0.16f), (std::max)(3, (int)(targetH * 0.025f)));

        // Timestamp pill in bottom right
        float pillW = targetW * 0.22f;
        float pillH = (std::max)(14.0f, targetH * 0.09f);
        float pillX = targetW - pillW - targetW * 0.06f;
        float pillY = targetH - pillH - targetH * 0.06f;
        Gdiplus::SolidBrush pillBg(Gdiplus::Color(200, 9, 9, 11));
        g.FillRectangle(&pillBg, pillX, pillY, pillW, pillH);

        // Add subtle play-button overlay
        DrawSubtlePlayButton(g, (float)targetW, (float)targetH);

        // Crisp border
        Gdiplus::Pen borderPen(Gdiplus::Color(255, 63, 63, 70), 1.0f);
        g.DrawRectangle(&borderPen, 0, 0, targetW - 1, targetH - 1);
    }

    res.hBitmap = GdiplusBitmapToHBitmap(composite.get(), targetW, targetH);
    res.success = (res.hBitmap != nullptr);
    res.alphaType = WTSAT_RGB;
    return res;
}
