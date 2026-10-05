#pragma once

#include "Common.h"

struct EpubThumbnailResult {
    bool success = false;
    HBITMAP hBitmap = nullptr;
    int width = 0;
    int height = 0;
    WTS_ALPHATYPE alphaType = WTSAT_RGB;
};

class EpubRenderer {
public:
    static bool IsSupportedExtension(const std::wstring& ext);
    static EpubThumbnailResult RenderThumbnail(const std::wstring& filePath, UINT cx);
    static EpubThumbnailResult RenderSample(UINT cx);

private:
    static HBITMAP GdiplusBitmapToHBitmap(Gdiplus::Bitmap* pBmp, int width, int height);
};
