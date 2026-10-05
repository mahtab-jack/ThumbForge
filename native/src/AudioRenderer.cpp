#include "AudioRenderer.h"
#include "VideoRenderer.h"
#include "AssetsData.h"
#include <shlwapi.h>
#include <algorithm>
#include <cmath>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "gdiplus.lib")

bool AudioRenderer::IsSupportedExtension(const std::wstring& ext) {
    std::wstring lowerExt = ext;
    std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(), ::towlower);
    for (const auto& supp : kSupportedAudioExtensions) {
        if (lowerExt == supp) return true;
    }
    return false;
}

bool AudioRenderer::ExtractCoverBmp(const std::wstring& ffmpegPath, const std::wstring& filePath, std::vector<BYTE>& outBmpData) {
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

    // ffmpeg -y -i "path" -an -vcodec bmp -vframes 1 -f image2pipe pipe:1
    std::wstring cmdLine = L"\"" + ffmpegPath + L"\" -y -i \"" + filePath + L"\" -an -vcodec bmp -vframes 1 -f image2pipe pipe:1";

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

    CloseHandle(hWritePipe);

    if (!created) {
        CloseHandle(hReadPipe);
        return false;
    }

    BYTE buffer[16384];
    DWORD bytesRead = 0;
    outBmpData.clear();

    while (ReadFile(hReadPipe, buffer, sizeof(buffer), &bytesRead, nullptr) && bytesRead > 0) {
        outBmpData.insert(outBmpData.end(), buffer, buffer + bytesRead);
    }

    CloseHandle(hReadPipe);

    WaitForSingleObject(pi.hProcess, 3000);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return !outBmpData.empty();
}

HBITMAP AudioRenderer::GdiplusBitmapToHBitmap(Gdiplus::Bitmap* pBmp, int width, int height) {
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

AudioThumbnailResult AudioRenderer::RenderThumbnail(const std::wstring& filePath, UINT cx) {
    AudioThumbnailResult res;
    int size = (int)cx;
    if (size < 48) size = 48;
    if (size > 1024) size = 1024;
    res.width = size;
    res.height = size;

    std::wstring ffmpegPath = VideoRenderer::GetFFmpegPath();
    std::vector<BYTE> bmpBytes;
    bool hasCover = ExtractCoverBmp(ffmpegPath, filePath, bmpBytes);

    auto finalBmp = std::make_unique<Gdiplus::Bitmap>(size, size, PixelFormat32bppARGB);
    {
        Gdiplus::Graphics g(finalBmp.get());
        g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.Clear(Gdiplus::Color(0, 0, 0, 0));

        // Load music.png badge
        IStream* pMusicStream = SHCreateMemStream(kMusicPngData, (UINT)kMusicPngData_size);
        std::unique_ptr<Gdiplus::Bitmap> pMusicBmp;
        if (pMusicStream) {
            pMusicBmp.reset(Gdiplus::Bitmap::FromStream(pMusicStream));
            pMusicStream->Release();
        }

        if (hasCover && bmpBytes.size() >= sizeof(BITMAPFILEHEADER)) {
            IStream* pStream = SHCreateMemStream(bmpBytes.data(), (UINT)bmpBytes.size());
            if (pStream) {
                std::unique_ptr<Gdiplus::Bitmap> pCoverBmp(Gdiplus::Bitmap::FromStream(pStream));
                pStream->Release();

                if (pCoverBmp && pCoverBmp->GetLastStatus() == Gdiplus::Ok) {
                    res.hasCoverArt = true;
                    // Draw album cover art filling the card with clean rounded border
                    float pad = size * 0.04f;
                    float cardSize = size - pad * 2;
                    g.DrawImage(pCoverBmp.get(), pad, pad, cardSize, cardSize);

                    // Subtle dark gradient scrim in the center so music icon stands out
                    float scrimSize = size * 0.46f;
                    float scrimX = (size - scrimSize) * 0.5f;
                    float scrimY = (size - scrimSize) * 0.5f;
                    Gdiplus::SolidBrush scrimBrush(Gdiplus::Color(160, 9, 9, 11)); // translucent dark disc
                    g.FillEllipse(&scrimBrush, scrimX, scrimY, scrimSize, scrimSize);

                    // Draw music.png centered above album cover
                    if (pMusicBmp) {
                        float iconSize = size * 0.38f;
                        float iconX = (size - iconSize) * 0.5f;
                        float iconY = (size - iconSize) * 0.5f;
                        g.DrawImage(pMusicBmp.get(), iconX, iconY, iconSize, iconSize);
                    }

                    // Card border
                    Gdiplus::Pen borderPen(Gdiplus::Color(255, 63, 63, 70), 1.5f);
                    g.DrawRectangle(&borderPen, pad, pad, cardSize, cardSize);

                    res.hBitmap = GdiplusBitmapToHBitmap(finalBmp.get(), size, size);
                    res.success = (res.hBitmap != nullptr);
                    return res;
                }
            }
        }

        // Fallback: No cover image, render stylish vinyl record / audio card
        float pad = size * 0.04f;
        float cardSize = size - pad * 2;
        Gdiplus::SolidBrush cardBg(Gdiplus::Color(0, 0, 0, 0)); // Transparent
        g.FillRectangle(&cardBg, pad, pad, cardSize, cardSize);

        // Concentric vinyl groove circles
        Gdiplus::Pen groovePen(Gdiplus::Color(255, 39, 39, 42), 1.0f);
        float center = size * 0.5f;
        for (float r = cardSize * 0.44f; r > cardSize * 0.22f; r -= cardSize * 0.06f) {
            g.DrawEllipse(&groovePen, center - r, center - r, r * 2, r * 2);
        }

        // Center vinyl label disc in acid yellow
        float labelR = cardSize * 0.20f;
        Gdiplus::SolidBrush labelBg(Gdiplus::Color(255, 223, 225, 4)); // #DFE104
        g.FillEllipse(&labelBg, center - labelR, center - labelR, labelR * 2, labelR * 2);

        // Center music.png
        if (pMusicBmp) {
            float iconSize = size * 0.36f;
            float iconX = (size - iconSize) * 0.5f;
            float iconY = (size - iconSize) * 0.5f;
            g.DrawImage(pMusicBmp.get(), iconX, iconY, iconSize, iconSize);
        }

        // Outer border
        Gdiplus::Pen borderPen(Gdiplus::Color(255, 63, 63, 70), 1.5f);
        g.DrawRectangle(&borderPen, pad, pad, cardSize, cardSize);
    }

    res.hBitmap = GdiplusBitmapToHBitmap(finalBmp.get(), size, size);
    res.success = (res.hBitmap != nullptr);
    return res;
}

AudioThumbnailResult AudioRenderer::RenderSample(bool withCover, UINT cx) {
    AudioThumbnailResult res;
    int size = (int)cx;
    if (size < 48) size = 48;
    if (size > 1024) size = 1024;
    res.width = size;
    res.height = size;
    res.hasCoverArt = withCover;

    auto finalBmp = std::make_unique<Gdiplus::Bitmap>(size, size, PixelFormat32bppARGB);
    {
        Gdiplus::Graphics g(finalBmp.get());
        g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.Clear(Gdiplus::Color(0, 0, 0, 0));

        IStream* pMusicStream = SHCreateMemStream(kMusicPngData, (UINT)kMusicPngData_size);
        std::unique_ptr<Gdiplus::Bitmap> pMusicBmp;
        if (pMusicStream) {
            pMusicBmp.reset(Gdiplus::Bitmap::FromStream(pMusicStream));
            pMusicStream->Release();
        }

        float pad = size * 0.04f;
        float cardSize = size - pad * 2;

        if (withCover) {
            // Simulated album cover art with geometric gradient
            Gdiplus::SolidBrush coverBrush(Gdiplus::Color(0, 0, 0, 0)); // Transparent
            g.FillRectangle(&coverBrush, pad, pad, cardSize, cardSize);

            // Artistic geometric stripes on cover
            Gdiplus::SolidBrush stripe1(Gdiplus::Color(255, 76, 29, 149)); // Violet
            g.FillRectangle(&stripe1, pad, pad + cardSize * 0.45f, cardSize, cardSize * 0.55f);
            Gdiplus::SolidBrush stripe2(Gdiplus::Color(255, 217, 70, 239)); // Fuchsia
            g.FillRectangle(&stripe2, pad, pad + cardSize * 0.65f, cardSize, cardSize * 0.35f);

            // Centered dark scrim
            float scrimSize = size * 0.44f;
            float scrimX = (size - scrimSize) * 0.5f;
            float scrimY = (size - scrimSize) * 0.5f;
            Gdiplus::SolidBrush scrimBrush(Gdiplus::Color(170, 9, 9, 11));
            g.FillEllipse(&scrimBrush, scrimX, scrimY, scrimSize, scrimSize);

            // Music.png overlay in center above cover
            if (pMusicBmp) {
                float iconSize = size * 0.38f;
                float iconX = (size - iconSize) * 0.5f;
                float iconY = (size - iconSize) * 0.5f;
                g.DrawImage(pMusicBmp.get(), iconX, iconY, iconSize, iconSize);
            }
        } else {
            // Vinyl disc style
            Gdiplus::SolidBrush cardBg(Gdiplus::Color(0, 0, 0, 0)); // Transparent
            g.FillRectangle(&cardBg, pad, pad, cardSize, cardSize);

            float center = size * 0.5f;
            Gdiplus::Pen groovePen(Gdiplus::Color(255, 39, 39, 42), 1.0f);
            for (float r = cardSize * 0.44f; r > cardSize * 0.22f; r -= cardSize * 0.06f) {
                g.DrawEllipse(&groovePen, center - r, center - r, r * 2, r * 2);
            }

            float labelR = cardSize * 0.20f;
            Gdiplus::SolidBrush labelBg(Gdiplus::Color(255, 223, 225, 4));
            g.FillEllipse(&labelBg, center - labelR, center - labelR, labelR * 2, labelR * 2);

            if (pMusicBmp) {
                float iconSize = size * 0.36f;
                float iconX = (size - iconSize) * 0.5f;
                float iconY = (size - iconSize) * 0.5f;
                g.DrawImage(pMusicBmp.get(), iconX, iconY, iconSize, iconSize);
            }
        }

        Gdiplus::Pen borderPen(Gdiplus::Color(255, 63, 63, 70), 1.5f);
        g.DrawRectangle(&borderPen, pad, pad, cardSize, cardSize);
    }

    res.hBitmap = GdiplusBitmapToHBitmap(finalBmp.get(), size, size);
    res.success = (res.hBitmap != nullptr);
    return res;
}
