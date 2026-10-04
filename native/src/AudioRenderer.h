#pragma once

#include "Common.h"

struct AudioThumbnailResult {
    bool success = false;
    HBITMAP hBitmap = nullptr;
    int width = 0;
    int height = 0;
    bool hasCoverArt = false;
    WTS_ALPHATYPE alphaType = WTSAT_RGB;
};

class AudioRenderer {
public:
    static bool IsSupportedExtension(const std::wstring& ext);
    static AudioThumbnailResult RenderThumbnail(const std::wstring& filePath, UINT cx);
    static AudioThumbnailResult RenderSample(bool withCover, UINT cx);

private:
    static bool ExtractCoverBmp(const std::wstring& ffmpegPath, const std::wstring& filePath, std::vector<BYTE>& outBmpData);
    static HBITMAP GdiplusBitmapToHBitmap(Gdiplus::Bitmap* pBmp, int width, int height);
};
