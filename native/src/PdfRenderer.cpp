#include "PdfRenderer.h"
#include "AssetsData.h"
#include <shlwapi.h>
#include <mutex>
#include <cmath>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "gdiplus.lib")

static std::once_flag s_pdfiumInitFlag;
static bool s_pdfiumInitialized = false;

void PdfRenderer::Initialize() {
    std::call_once(s_pdfiumInitFlag, []() {
        FPDF_LIBRARY_CONFIG config;
        config.version = 2;
        config.m_pUserFontPaths = nullptr;
        config.m_pIsolate = nullptr;
        config.m_v8EmbedderSlot = 0;
        FPDF_InitLibraryWithConfig(&config);
        s_pdfiumInitialized = true;
    });
}

void PdfRenderer::Shutdown() {
    if (s_pdfiumInitialized) {
        FPDF_DestroyLibrary();
        s_pdfiumInitialized = false;
    }
}

HBITMAP PdfRenderer::CreateDIBSectionFromPdfium(FPDF_BITMAP bmp, int width, int height) {
    if (!bmp) return nullptr;

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

    const BYTE* srcBuffer = (const BYTE*)FPDFBitmap_GetBuffer(bmp);
    int srcStride = FPDFBitmap_GetStride(bmp);
    int dstStride = width * 4;

    for (int y = 0; y < height; ++y) {
        memcpy((BYTE*)pBits + y * dstStride, srcBuffer + y * srcStride, dstStride);
    }

    return hBitmap;
}

static HBITMAP GdiplusBitmapToHBitmap(Gdiplus::Bitmap* pBmp, int width, int height) {
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

PdfThumbnailResult PdfRenderer::RenderLockedPdfIcon(UINT cx) {
    PdfThumbnailResult res;
    res.status = PdfRenderStatus::PasswordProtected;
    res.alphaType = WTSAT_ARGB;

    int size = (int)cx;
    if (size < 48) size = 48;
    if (size > 1024) size = 1024;
    res.width = size;
    res.height = size;

    IStream* pPdfStream = SHCreateMemStream(kPdfPngData, (UINT)kPdfPngData_size);
    if (!pPdfStream) return res;
    std::unique_ptr<Gdiplus::Bitmap> pPdfBmp(Gdiplus::Bitmap::FromStream(pPdfStream));
    pPdfStream->Release();

    IStream* pLockStream = SHCreateMemStream(kLockPngData, (UINT)kLockPngData_size);
    if (!pLockStream) return res;
    std::unique_ptr<Gdiplus::Bitmap> pLockBmp(Gdiplus::Bitmap::FromStream(pLockStream));
    pLockStream->Release();

    if (!pPdfBmp || !pLockBmp) return res;

    auto composite = std::make_unique<Gdiplus::Bitmap>(size, size, PixelFormat32bppARGB);
    {
        Gdiplus::Graphics g(composite.get());
        g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.Clear(Gdiplus::Color(0, 0, 0, 0));

        // 1. Draw clean base document sheet
        float sheetW = size * 0.80f;
        float sheetH = size * 0.94f;
        float sheetX = (size - sheetW) * 0.5f;
        float sheetY = (size - sheetH) * 0.5f;

        Gdiplus::SolidBrush sheetBg(Gdiplus::Color(255, 245, 245, 248)); // Clean document sheet
        g.FillRectangle(&sheetBg, sheetX, sheetY, sheetW, sheetH);

        // Subtle document lines to signify document structure
        Gdiplus::SolidBrush lineBrush(Gdiplus::Color(255, 220, 220, 225));
        for (int i = 0; i < 6; ++i) {
            float y = sheetY + sheetH * 0.18f + i * (sheetH * 0.08f);
            float w = (i % 2 == 1) ? sheetW * 0.55f : sheetW * 0.72f;
            g.FillRectangle(&lineBrush, sheetX + sheetW * 0.14f, y, w, sheetH * 0.035f);
        }

        // Sheet border
        Gdiplus::Pen sheetBorder(Gdiplus::Color(255, 200, 200, 205), 1.5f);
        g.DrawRectangle(&sheetBorder, sheetX, sheetY, sheetW, sheetH);

        // 2. Lock icon in the exact CENTRE on the white canvas (clean & prominent)
        float lockSize = size * 0.52f;
        float origLockW = pLockBmp->GetWidth();
        float origLockH = pLockBmp->GetHeight();
        float lockScale = (std::min)(lockSize / origLockW, lockSize / origLockH);
        float drawW = origLockW * lockScale;
        float drawH = origLockH * lockScale;
        float lockX = (size - drawW) * 0.5f;
        float lockY = (size - drawH) * 0.5f;

        // Draw centered lock icon directly on the white canvas
        g.DrawImage(pLockBmp.get(), lockX, lockY, drawW, drawH);

        // 3. Bottom-right: intentionally left clean with NO custom badges
        // Windows File Explorer overlays the user's default app badge (e.g. Chrome) cleanly here
    }

    res.hBitmap = GdiplusBitmapToHBitmap(composite.get(), size, size);
    return res;
}

PdfThumbnailResult PdfRenderer::RenderCorruptedPdfIcon(UINT cx) {
    PdfThumbnailResult res;
    res.status = PdfRenderStatus::Corrupted;
    res.alphaType = WTSAT_ARGB;

    int size = (int)cx;
    if (size < 48) size = 48;
    if (size > 1024) size = 1024;
    res.width = size;
    res.height = size;

    auto composite = std::make_unique<Gdiplus::Bitmap>(size, size, PixelFormat32bppARGB);
    {
        Gdiplus::Graphics g(composite.get());
        g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.Clear(Gdiplus::Color(0, 0, 0, 0));

        // 1. Draw clean base document sheet
        float sheetW = size * 0.80f;
        float sheetH = size * 0.94f;
        float sheetX = (size - sheetW) * 0.5f;
        float sheetY = (size - sheetH) * 0.5f;

        Gdiplus::SolidBrush sheetBg(Gdiplus::Color(255, 245, 245, 248));
        g.FillRectangle(&sheetBg, sheetX, sheetY, sheetW, sheetH);

        // Distorted/broken horizontal indicator lines
        Gdiplus::SolidBrush lineBrush(Gdiplus::Color(255, 220, 220, 225));
        for (int i = 0; i < 5; ++i) {
            float y = sheetY + sheetH * 0.18f + i * (sheetH * 0.08f);
            float w = (i == 2) ? sheetW * 0.4f : sheetW * 0.68f;
            g.FillRectangle(&lineBrush, sheetX + sheetW * 0.14f, y, w, sheetH * 0.035f);
        }

        // Sheet border
        Gdiplus::Pen sheetBorder(Gdiplus::Color(255, 200, 200, 205), 1.5f);
        g.DrawRectangle(&sheetBorder, sheetX, sheetY, sheetW, sheetH);

        // 2. Corrupted / Warning badge in the exact CENTRE on the white canvas
        float badgeSize = size * 0.50f;
        float badgeX = (size - badgeSize) * 0.5f;
        float badgeY = (size - badgeSize) * 0.5f;

        // Acid yellow warning triangle with bold border
        Gdiplus::PointF pts[3];
        pts[0] = Gdiplus::PointF(badgeX + badgeSize * 0.5f, badgeY + badgeSize * 0.12f); // Top
        pts[1] = Gdiplus::PointF(badgeX + badgeSize * 0.06f, badgeY + badgeSize * 0.88f); // Bottom left
        pts[2] = Gdiplus::PointF(badgeX + badgeSize * 0.94f, badgeY + badgeSize * 0.88f); // Bottom right

        Gdiplus::SolidBrush triangleBrush(Gdiplus::Color(255, 223, 225, 4)); // #DFE104 Acid yellow
        g.FillPolygon(&triangleBrush, pts, 3);

        Gdiplus::Pen triangleBorder(Gdiplus::Color(255, 9, 9, 11), 3.0f); // Sharp black border
        g.DrawPolygon(&triangleBorder, pts, 3);

        // Black exclamation mark inside triangle
        Gdiplus::SolidBrush blackBrush(Gdiplus::Color(255, 9, 9, 11));
        float stemW = badgeSize * 0.09f;
        float stemH = badgeSize * 0.32f;
        float stemX = badgeX + (badgeSize - stemW) * 0.5f;
        float stemY = badgeY + badgeSize * 0.38f;
        g.FillRectangle(&blackBrush, stemX, stemY, stemW, stemH);

        float dotSize = stemW * 1.15f;
        float dotX = badgeX + (badgeSize - dotSize) * 0.5f;
        float dotY = stemY + stemH + badgeSize * 0.07f;
        g.FillEllipse(&blackBrush, dotX, dotY, dotSize, dotSize);

        // 3. Bottom-right: intentionally left clean with NO custom badges
        // Windows File Explorer overlays the user's default app badge (e.g. Chrome) cleanly here
    }

    res.hBitmap = GdiplusBitmapToHBitmap(composite.get(), size, size);
    return res;
}

PdfThumbnailResult PdfRenderer::RenderThumbnail(const std::wstring& filePath, UINT cx) {
    Initialize();

    std::string pathUtf8;
    int len = WideCharToMultiByte(CP_UTF8, 0, filePath.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len > 0) {
        pathUtf8.resize(len);
        WideCharToMultiByte(CP_UTF8, 0, filePath.c_str(), -1, &pathUtf8[0], len, nullptr, nullptr);
        if (!pathUtf8.empty() && pathUtf8.back() == '\0') {
            pathUtf8.pop_back();
        }
    }

    FPDF_DOCUMENT doc = FPDF_LoadDocument(pathUtf8.c_str(), nullptr);
    if (!doc) {
        unsigned long err = FPDF_GetLastError();
        if (err == FPDF_ERR_PASSWORD) {
            return RenderLockedPdfIcon(cx);
        } else {
            return RenderCorruptedPdfIcon(cx);
        }
    }

    int pageCount = FPDF_GetPageCount(doc);
    if (pageCount < 1) {
        FPDF_CloseDocument(doc);
        return RenderCorruptedPdfIcon(cx);
    }

    // Render page 1 (0-indexed)
    FPDF_PAGE page = FPDF_LoadPage(doc, 0);
    if (!page) {
        FPDF_CloseDocument(doc);
        return RenderCorruptedPdfIcon(cx);
    }

    double origW = FPDF_GetPageWidth(page);
    double origH = FPDF_GetPageHeight(page);
    if (origW <= 0 || origH <= 0) {
        FPDF_ClosePage(page);
        FPDF_CloseDocument(doc);
        return RenderCorruptedPdfIcon(cx);
    }

    double scale = (std::min)((double)cx / origW, (double)cx / origH);
    int targetW = (std::max)(1, (int)(origW * scale));
    int targetH = (std::max)(1, (int)(origH * scale));

    FPDF_BITMAP bmp = FPDFBitmap_Create(targetW, targetH, 0); // Opaque RGB
    if (!bmp) {
        FPDF_ClosePage(page);
        FPDF_CloseDocument(doc);
        return RenderCorruptedPdfIcon(cx);
    }

    FPDFBitmap_FillRect(bmp, 0, 0, targetW, targetH, 0xFFFFFFFF);
    FPDF_RenderPageBitmap(bmp, page, 0, 0, targetW, targetH, 0, FPDF_ANNOT | FPDF_PRINTING);

    HBITMAP hBmp = CreateDIBSectionFromPdfium(bmp, targetW, targetH);
    FPDFBitmap_Destroy(bmp);
    FPDF_ClosePage(page);
    FPDF_CloseDocument(doc);

    PdfThumbnailResult res;
    res.status = PdfRenderStatus::Success;
    res.hBitmap = hBmp;
    res.width = targetW;
    res.height = targetH;
    res.alphaType = WTSAT_RGB;
    return res;
}

PdfThumbnailResult PdfRenderer::RenderThumbnailFromMemory(const BYTE* data, size_t size, UINT cx) {
    Initialize();

    if (!data || size == 0) {
        return RenderCorruptedPdfIcon(cx);
    }

    FPDF_DOCUMENT doc = FPDF_LoadMemDocument(data, (int)size, nullptr);
    if (!doc) {
        unsigned long err = FPDF_GetLastError();
        if (err == FPDF_ERR_PASSWORD) {
            return RenderLockedPdfIcon(cx);
        } else {
            return RenderCorruptedPdfIcon(cx);
        }
    }

    int pageCount = FPDF_GetPageCount(doc);
    if (pageCount < 1) {
        FPDF_CloseDocument(doc);
        return RenderCorruptedPdfIcon(cx);
    }

    FPDF_PAGE page = FPDF_LoadPage(doc, 0);
    if (!page) {
        FPDF_CloseDocument(doc);
        return RenderCorruptedPdfIcon(cx);
    }

    double origW = FPDF_GetPageWidth(page);
    double origH = FPDF_GetPageHeight(page);
    if (origW <= 0 || origH <= 0) {
        FPDF_ClosePage(page);
        FPDF_CloseDocument(doc);
        return RenderCorruptedPdfIcon(cx);
    }

    double scale = (std::min)((double)cx / origW, (double)cx / origH);
    int targetW = (std::max)(1, (int)(origW * scale));
    int targetH = (std::max)(1, (int)(origH * scale));

    FPDF_BITMAP bmp = FPDFBitmap_Create(targetW, targetH, 0);
    if (!bmp) {
        FPDF_ClosePage(page);
        FPDF_CloseDocument(doc);
        return RenderCorruptedPdfIcon(cx);
    }

    FPDFBitmap_FillRect(bmp, 0, 0, targetW, targetH, 0xFFFFFFFF);
    FPDF_RenderPageBitmap(bmp, page, 0, 0, targetW, targetH, 0, FPDF_ANNOT | FPDF_PRINTING);

    HBITMAP hBmp = CreateDIBSectionFromPdfium(bmp, targetW, targetH);
    FPDFBitmap_Destroy(bmp);
    FPDF_ClosePage(page);
    FPDF_CloseDocument(doc);

    PdfThumbnailResult res;
    res.status = PdfRenderStatus::Success;
    res.hBitmap = hBmp;
    res.width = targetW;
    res.height = targetH;
    res.alphaType = WTSAT_RGB;
    return res;
}

PdfThumbnailResult PdfRenderer::RenderSample(int sampleType, UINT cx) {
    if (sampleType == 1) {
        return RenderLockedPdfIcon(cx);
    } else if (sampleType == 2) {
        return RenderCorruptedPdfIcon(cx);
    }

    // Normal sample document: render clean document page
    int targetW = (int)(cx * 0.77f);
    int targetH = (int)cx;
    if (targetW < 1) targetW = 1;
    if (targetH < 1) targetH = 1;

    auto composite = std::make_unique<Gdiplus::Bitmap>(targetW, targetH, PixelFormat32bppARGB);
    {
        Gdiplus::Graphics g(composite.get());
        g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
        g.Clear(Gdiplus::Color(255, 255, 255, 255)); // White sheet

        // Document header title block
        Gdiplus::SolidBrush headerBrush(Gdiplus::Color(255, 9, 9, 11)); // Rich black
        g.FillRectangle(&headerBrush, (int)(targetW * 0.1f), (int)(targetH * 0.12f), (int)(targetW * 0.55f), (int)(targetH * 0.045f));

        // Subheader line
        Gdiplus::SolidBrush subBrush(Gdiplus::Color(255, 161, 161, 170)); // Zinc 400
        g.FillRectangle(&subBrush, (int)(targetW * 0.1f), (int)(targetH * 0.18f), (int)(targetW * 0.35f), (int)(targetH * 0.02f));

        // Horizontal accent divider
        Gdiplus::SolidBrush accentBrush(Gdiplus::Color(255, 223, 225, 4)); // Acid yellow #DFE104
        g.FillRectangle(&accentBrush, (int)(targetW * 0.1f), (int)(targetH * 0.22f), (int)(targetW * 0.8f), (int)(targetH * 0.012f));

        // Mock paragraphs
        Gdiplus::SolidBrush lineBrush(Gdiplus::Color(255, 200, 200, 205));
        for (int i = 0; i < 7; ++i) {
            float y = targetH * 0.27f + i * (targetH * 0.04f);
            float w = (i == 6) ? targetW * 0.45f : targetW * 0.8f;
            g.FillRectangle(&lineBrush, targetW * 0.1f, y, w, targetH * 0.018f);
        }

        // Mock visual chart block in page 1
        Gdiplus::SolidBrush chartBg(Gdiplus::Color(255, 240, 240, 245));
        g.FillRectangle(&chartBg, (int)(targetW * 0.1f), (int)(targetH * 0.60f), (int)(targetW * 0.8f), (int)(targetH * 0.25f));
        Gdiplus::SolidBrush chartBar(Gdiplus::Color(255, 9, 9, 11));
        g.FillRectangle(&chartBar, (int)(targetW * 0.2f), (int)(targetH * 0.70f), (int)(targetW * 0.12f), (int)(targetH * 0.12f));
        g.FillRectangle(&accentBrush, (int)(targetW * 0.4f), (int)(targetH * 0.64f), (int)(targetW * 0.12f), (int)(targetH * 0.18f));
        g.FillRectangle(&chartBar, (int)(targetW * 0.6f), (int)(targetH * 0.67f), (int)(targetW * 0.12f), (int)(targetH * 0.15f));

        // Outer sheet subtle border
        Gdiplus::Pen pageBorder(Gdiplus::Color(255, 210, 210, 215), 1.0f);
        g.DrawRectangle(&pageBorder, 0, 0, targetW - 1, targetH - 1);
    }

    PdfThumbnailResult res;
    res.status = PdfRenderStatus::Success;
    res.hBitmap = GdiplusBitmapToHBitmap(composite.get(), targetW, targetH);
    res.width = targetW;
    res.height = targetH;
    res.alphaType = WTSAT_RGB;
    return res;
}
