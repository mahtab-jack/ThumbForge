#pragma once

#include "Common.h"

struct CodeThumbnailResult {
    bool success = false;
    HBITMAP hBitmap = nullptr;
    int width = 0;
    int height = 0;
    WTS_ALPHATYPE alphaType = WTSAT_RGB;
};

class CodeRenderer {
public:
    static bool IsSupportedExtension(const std::wstring& ext);
    static CodeThumbnailResult RenderThumbnail(const std::wstring& filePath, UINT cx);
    static CodeThumbnailResult RenderFromMemory(const BYTE* data, size_t size, const std::wstring& ext, UINT cx);
    static CodeThumbnailResult RenderSample(const std::wstring& format, UINT cx);

private:
    static HBITMAP GdiplusBitmapToHBitmap(Gdiplus::Bitmap* pBmp, int width, int height);
};
