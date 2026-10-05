#pragma once

#include "Common.h"

struct HtmlThumbnailResult {
    bool success = false;
    HBITMAP hBitmap = nullptr;
    int width = 0;
    int height = 0;
    WTS_ALPHATYPE alphaType = WTSAT_RGB;
};

class HtmlRenderer {
public:
    static bool IsSupportedExtension(const std::wstring& ext);
    static HtmlThumbnailResult RenderThumbnail(const std::wstring& filePath, UINT cx);
    static HtmlThumbnailResult RenderSample(UINT cx);
    static std::wstring FindBrowserPath();

private:
    static HBITMAP GdiplusBitmapToHBitmap(Gdiplus::Bitmap* pBmp, int width, int height);
};
