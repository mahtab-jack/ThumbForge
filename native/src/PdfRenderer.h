#pragma once

#include "Common.h"
#include <fpdfview.h>

enum class PdfRenderStatus {
    Success,
    PasswordProtected,
    Corrupted,
    Failed
};

struct PdfThumbnailResult {
    PdfRenderStatus status = PdfRenderStatus::Failed;
    HBITMAP hBitmap = nullptr;
    int width = 0;
    int height = 0;
    WTS_ALPHATYPE alphaType = WTSAT_RGB;
};

class PdfRenderer {
public:
    static void Initialize();
    static void Shutdown();

    // Render from file path
    static PdfThumbnailResult RenderThumbnail(const std::wstring& filePath, UINT cx);

    // Render from stream or memory buffer
    static PdfThumbnailResult RenderThumbnailFromMemory(const BYTE* data, size_t size, UINT cx);

    // Render mock sample states for testing & UI gallery
    static PdfThumbnailResult RenderSample(int sampleType, UINT cx);

private:
    static PdfThumbnailResult RenderLockedPdfIcon(UINT cx);
    static PdfThumbnailResult RenderCorruptedPdfIcon(UINT cx);
    static HBITMAP CreateDIBSectionFromPdfium(FPDF_BITMAP bmp, int width, int height);
};
