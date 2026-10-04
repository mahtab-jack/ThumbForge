#pragma once

#include "Common.h"

struct ApkThumbnailResult {
    bool success = false;
    HBITMAP hBitmap = nullptr;
    int width = 0;
    int height = 0;
    WTS_ALPHATYPE alphaType = WTSAT_ARGB;
};

class ApkRenderer {
public:
    static bool IsSupportedExtension(const std::wstring& ext);
    static ApkThumbnailResult RenderThumbnail(const std::wstring& filePath, UINT cx);
    static ApkThumbnailResult RenderSample(UINT cx);

private:
    static bool ExtractIconFromApk(const std::wstring& apkPath, std::vector<BYTE>& outPngData);
    static HBITMAP GdiplusBitmapToHBitmap(Gdiplus::Bitmap* pBmp, int width, int height);
};
