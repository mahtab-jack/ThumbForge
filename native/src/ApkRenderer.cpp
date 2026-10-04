#include "ApkRenderer.h"
#include <shlwapi.h>
#include <algorithm>
#include <vector>
#include <string>
#include <memory>
#include <cmath>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "gdiplus.lib")

#pragma pack(push, 1)
struct ZipEOCD {
    DWORD signature;       // 0x06054b50
    WORD diskNumber;
    WORD startDisk;
    WORD entriesOnDisk;
    WORD totalEntries;
    DWORD cdSize;
    DWORD cdOffset;
    WORD commentLength;
};

struct ZipCentralDirHeader {
    DWORD signature;       // 0x02014b50
    WORD versionMadeBy;
    WORD versionNeeded;
    WORD flags;
    WORD method;
    WORD modTime;
    WORD modDate;
    DWORD crc32;
    DWORD compressedSize;
    DWORD uncompressedSize;
    WORD nameLength;
    WORD extraLength;
    WORD commentLength;
    WORD diskStart;
    WORD internalAttr;
    DWORD externalAttr;
    DWORD localHeaderOffset;
};

struct ZipLocalHeader {
    DWORD signature;       // 0x04034b50
    WORD versionNeeded;
    WORD flags;
    WORD method;
    WORD modTime;
    WORD modDate;
    DWORD crc32;
    DWORD compressedSize;
    DWORD uncompressedSize;
    WORD nameLength;
    WORD extraLength;
};
#pragma pack(pop)

bool ApkRenderer::IsSupportedExtension(const std::wstring& ext) {
    std::wstring lowerExt = ext;
    std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(), ::towlower);
    return (lowerExt == L".apk");
}

HBITMAP ApkRenderer::GdiplusBitmapToHBitmap(Gdiplus::Bitmap* pBmp, int width, int height) {
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

bool ApkRenderer::ExtractIconFromApk(const std::wstring& apkPath, std::vector<BYTE>& outPngData) {
    HANDLE hFile = CreateFileW(apkPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize) || fileSize.QuadPart < sizeof(ZipEOCD)) {
        CloseHandle(hFile);
        return false;
    }

    // Locate EOCD record in the last 65KB of the file
    DWORD searchSize = (DWORD)(std::min<ULONGLONG>)(65536 + sizeof(ZipEOCD), fileSize.QuadPart);
    LARGE_INTEGER seekPos;
    seekPos.QuadPart = fileSize.QuadPart - searchSize;
    SetFilePointerEx(hFile, seekPos, nullptr, FILE_BEGIN);

    std::vector<BYTE> tailBuf(searchSize);
    DWORD bytesRead = 0;
    if (!ReadFile(hFile, tailBuf.data(), searchSize, &bytesRead, nullptr) || bytesRead < sizeof(ZipEOCD)) {
        CloseHandle(hFile);
        return false;
    }

    int eocdOffsetInTail = -1;
    for (int i = (int)(bytesRead - sizeof(ZipEOCD)); i >= 0; --i) {
        if (*(DWORD*)(tailBuf.data() + i) == 0x06054b50) {
            eocdOffsetInTail = i;
            break;
        }
    }

    if (eocdOffsetInTail < 0) {
        CloseHandle(hFile);
        return false;
    }

    const ZipEOCD* pEocd = (const ZipEOCD*)(tailBuf.data() + eocdOffsetInTail);
    if (pEocd->totalEntries == 0 || pEocd->cdSize == 0) {
        CloseHandle(hFile);
        return false;
    }

    // Read Central Directory
    seekPos.QuadPart = pEocd->cdOffset;
    SetFilePointerEx(hFile, seekPos, nullptr, FILE_BEGIN);

    std::vector<BYTE> cdBuf(pEocd->cdSize);
    if (!ReadFile(hFile, cdBuf.data(), pEocd->cdSize, &bytesRead, nullptr) || bytesRead < pEocd->cdSize) {
        CloseHandle(hFile);
        return false;
    }

    // Scan Central Directory entries for launcher icon PNGs
    struct CandidateIcon {
        std::string name;
        DWORD offset;
        DWORD compSize;
        DWORD uncompSize;
        WORD method;
        int score; // higher score for higher resolution mipmaps
    };

    std::vector<CandidateIcon> candidates;
    size_t cdPtr = 0;
    while (cdPtr + sizeof(ZipCentralDirHeader) <= cdBuf.size()) {
        const ZipCentralDirHeader* pHdr = (const ZipCentralDirHeader*)(cdBuf.data() + cdPtr);
        if (pHdr->signature != 0x02014b50) break;

        size_t nextEntry = cdPtr + sizeof(ZipCentralDirHeader) + pHdr->nameLength + pHdr->extraLength + pHdr->commentLength;
        if (nextEntry > cdBuf.size()) break;

        std::string entryName((const char*)(cdBuf.data() + cdPtr + sizeof(ZipCentralDirHeader)), pHdr->nameLength);
        std::string lowerName = entryName;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

        if (lowerName.length() > 4 && lowerName.substr(lowerName.length() - 4) == ".png") {
            int score = 0;
            if (lowerName.find("ic_launcher") != std::string::npos || lowerName.find("app_icon") != std::string::npos) {
                score = 100;
                if (lowerName.find("xxxhdpi") != std::string::npos) score += 50;
                else if (lowerName.find("xxhdpi") != std::string::npos) score += 40;
                else if (lowerName.find("xhdpi") != std::string::npos) score += 30;
                else if (lowerName.find("hdpi") != std::string::npos) score += 20;
                else if (lowerName.find("mdpi") != std::string::npos) score += 10;
                
                candidates.push_back({entryName, pHdr->localHeaderOffset, pHdr->compressedSize, pHdr->uncompressedSize, pHdr->method, score});
            }
        }

        cdPtr = nextEntry;
    }

    if (candidates.empty()) {
        CloseHandle(hFile);
        return false;
    }

    std::sort(candidates.begin(), candidates.end(), [](const CandidateIcon& a, const CandidateIcon& b) {
        return a.score > b.score;
    });

    const auto& best = candidates.front();

    // Read local header
    seekPos.QuadPart = best.offset;
    SetFilePointerEx(hFile, seekPos, nullptr, FILE_BEGIN);

    ZipLocalHeader localHdr;
    if (!ReadFile(hFile, &localHdr, sizeof(localHdr), &bytesRead, nullptr) || localHdr.signature != 0x04034b50) {
        CloseHandle(hFile);
        return false;
    }

    // Skip local name and extra
    seekPos.QuadPart = best.offset + sizeof(ZipLocalHeader) + localHdr.nameLength + localHdr.extraLength;
    SetFilePointerEx(hFile, seekPos, nullptr, FILE_BEGIN);

    std::vector<BYTE> compData(best.compSize);
    if (!ReadFile(hFile, compData.data(), best.compSize, &bytesRead, nullptr) || bytesRead < best.compSize) {
        CloseHandle(hFile);
        return false;
    }

    CloseHandle(hFile);

    if (best.method == 0) {
        // STORED (no compression)
        outPngData = compData;
        return true;
    }

    // DEFLATED / COMPRESSED: Extract via Windows built-in tar.exe to stdout
    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
    HANDLE hRead = nullptr, hWrite = nullptr;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) return false;
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = hWrite;
    si.hStdError = nullptr;

    PROCESS_INFORMATION pi = { 0 };
    std::wstring entryW(best.name.begin(), best.name.end());
    std::wstring cmd = L"tar.exe -xf \"" + apkPath + L"\" \"" + entryW + L"\" -O";
    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(L'\0');

    if (CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(hWrite);
        hWrite = nullptr;

        std::vector<BYTE> buffer(64 * 1024);
        DWORD bytesRead = 0;
        outPngData.clear();
        while (ReadFile(hRead, buffer.data(), (DWORD)buffer.size(), &bytesRead, nullptr) && bytesRead > 0) {
            outPngData.insert(outPngData.end(), buffer.data(), buffer.data() + bytesRead);
        }

        WaitForSingleObject(pi.hProcess, 3000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hRead);

        return !outPngData.empty();
    }

    if (hWrite) CloseHandle(hWrite);
    CloseHandle(hRead);
    return false;
}

ApkThumbnailResult ApkRenderer::RenderThumbnail(const std::wstring& filePath, UINT cx) {
    ApkThumbnailResult res;
    int size = (int)cx;
    if (size < 48) size = 48;
    if (size > 1024) size = 1024;
    res.width = size;
    res.height = size;

    std::vector<BYTE> pngBytes;
    bool hasIcon = ExtractIconFromApk(filePath, pngBytes);

    auto finalBmp = std::make_unique<Gdiplus::Bitmap>(size, size, PixelFormat32bppARGB);
    {
        Gdiplus::Graphics g(finalBmp.get());
        g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.Clear(Gdiplus::Color(0, 0, 0, 0));

        // 1. Draw modern application canvas / card
        float pad = size * 0.05f;
        float cardW = size - pad * 2;
        float cardH = size - pad * 2;

        Gdiplus::SolidBrush cardBg(Gdiplus::Color(255, 18, 18, 22)); // #121216 Rich Dark
        g.FillRectangle(&cardBg, pad, pad, cardW, cardH);

        // 2. Draw extracted APK icon in center
        bool drewIcon = false;
        if (hasIcon && !pngBytes.empty()) {
            IStream* pStream = SHCreateMemStream(pngBytes.data(), (UINT)pngBytes.size());
            if (pStream) {
                std::unique_ptr<Gdiplus::Bitmap> pIconBmp(Gdiplus::Bitmap::FromStream(pStream));
                pStream->Release();

                if (pIconBmp && pIconBmp->GetLastStatus() == Gdiplus::Ok) {
                    float iconSize = size * 0.58f;
                    float iconX = (size - iconSize) * 0.5f;
                    float iconY = (size - iconSize) * 0.44f;
                    g.DrawImage(pIconBmp.get(), iconX, iconY, iconSize, iconSize);
                    drewIcon = true;
                }
            }
        }

        // Fallback icon if no PNG extracted
        if (!drewIcon) {
            float iconSize = size * 0.48f;
            float iconX = (size - iconSize) * 0.5f;
            float iconY = (size - iconSize) * 0.44f;

            // Android robot stylized head in Acid Yellow #DFE104
            Gdiplus::SolidBrush androidBrush(Gdiplus::Color(255, 223, 225, 4));
            g.FillPie(&androidBrush, iconX, iconY, iconSize, iconSize, 180.0f, 180.0f);

            // Android antenna
            Gdiplus::Pen antPen(Gdiplus::Color(255, 223, 225, 4), 3.0f);
            g.DrawLine(&antPen, iconX + iconSize * 0.3f, iconY + iconSize * 0.15f, iconX + iconSize * 0.18f, iconY);
            g.DrawLine(&antPen, iconX + iconSize * 0.7f, iconY + iconSize * 0.15f, iconX + iconSize * 0.82f, iconY);

            // Android eyes
            Gdiplus::SolidBrush eyeBrush(Gdiplus::Color(255, 9, 9, 11));
            float eyeR = iconSize * 0.08f;
            g.FillEllipse(&eyeBrush, iconX + iconSize * 0.28f, iconY + iconSize * 0.22f, eyeR, eyeR);
            g.FillEllipse(&eyeBrush, iconX + iconSize * 0.64f, iconY + iconSize * 0.22f, eyeR, eyeR);
        }

        // 3. Bottom APK Badge Pill
        float pillW = cardW * 0.42f;
        float pillH = cardH * 0.16f;
        float pillX = (size - pillW) * 0.5f;
        float pillY = size - pad - pillH - (size * 0.06f);

        Gdiplus::SolidBrush pillBg(Gdiplus::Color(255, 34, 197, 94)); // Emerald green #22C55E
        g.FillRectangle(&pillBg, pillX, pillY, pillW, pillH);

        // "APK" Text
        Gdiplus::FontFamily fontFamily(L"Arial");
        Gdiplus::Font font(&fontFamily, pillH * 0.65f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush textBrush(Gdiplus::Color(255, 0, 0, 0));
        Gdiplus::StringFormat format;
        format.SetAlignment(Gdiplus::StringAlignmentCenter);
        format.SetLineAlignment(Gdiplus::StringAlignmentCenter);

        Gdiplus::RectF layoutRect(pillX, pillY, pillW, pillH);
        g.DrawString(L"APK", -1, &font, layoutRect, &format, &textBrush);

        // Outer border
        Gdiplus::Pen borderPen(Gdiplus::Color(255, 63, 63, 70), 1.5f);
        g.DrawRectangle(&borderPen, pad, pad, cardW, cardH);
    }

    res.hBitmap = GdiplusBitmapToHBitmap(finalBmp.get(), size, size);
    res.success = (res.hBitmap != nullptr);
    return res;
}

ApkThumbnailResult ApkRenderer::RenderSample(UINT cx) {
    return RenderThumbnail(L"", cx);
}
