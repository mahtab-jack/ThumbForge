#pragma once

#include "Common.h"

enum class VideoAspectRatio {
    Landscape,
    Portrait,
    Square
};

struct VideoThumbnailResult {
    bool success = false;
    HBITMAP hBitmap = nullptr;
    int width = 0;
    int height = 0;
    VideoAspectRatio aspectRatio = VideoAspectRatio::Landscape;
    WTS_ALPHATYPE alphaType = WTSAT_RGB;
};

class VideoRenderer {
public:
    // Render video thumbnail using FFmpeg
    static VideoThumbnailResult RenderThumbnail(const std::wstring& filePath, UINT cx);

    // Render mock sample states for testing & UI gallery (0: Landscape, 1: Portrait, 2: Square)
    static VideoThumbnailResult RenderSample(int sampleType, UINT cx);

    // Check if extension is supported
    static bool IsSupportedExtension(const std::wstring& ext);

    // Locate FFmpeg binary
    static std::wstring GetFFmpegPath();

private:
    static bool ExtractFrameBmp(const std::wstring& ffmpegPath, const std::wstring& filePath, const std::string& timecode, std::vector<BYTE>& outBmpData);
    static void DrawSubtlePlayButton(Gdiplus::Graphics& g, float width, float height);
    static HBITMAP GdiplusBitmapToHBitmap(Gdiplus::Bitmap* pBmp, int width, int height);
};
