#include "CodeRenderer.h"
#include <shlwapi.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "gdiplus.lib")

bool CodeRenderer::IsSupportedExtension(const std::wstring& ext) {
    std::wstring lowerExt = ext;
    std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(), ::towlower);
    for (const auto& supp : kSupportedCodeExtensions) {
        if (lowerExt == supp) return true;
    }
    return false;
}

HBITMAP CodeRenderer::GdiplusBitmapToHBitmap(Gdiplus::Bitmap* pBmp, int width, int height) {
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

CodeThumbnailResult CodeRenderer::RenderThumbnail(const std::wstring& filePath, UINT cx) {
    std::wstring ext = PathFindExtensionW(filePath.c_str());
    std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);

    // Read up to 8KB of text
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        return CodeThumbnailResult();
    }

    std::vector<BYTE> buffer(8192);
    file.read((char*)buffer.data(), buffer.size());
    std::streamsize bytesRead = file.gcount();
    buffer.resize((size_t)bytesRead);

    return RenderFromMemory(buffer.data(), buffer.size(), ext, cx);
}

CodeThumbnailResult CodeRenderer::RenderFromMemory(const BYTE* data, size_t size, const std::wstring& ext, UINT cx) {
    CodeThumbnailResult res;
    int targetSize = (int)cx;
    if (targetSize < 48) targetSize = 48;
    if (targetSize > 1024) targetSize = 1024;
    res.width = targetSize;
    res.height = targetSize;

    // Convert raw bytes to lines of wide strings
    std::string text((const char*)data, (std::min)(size, (size_t)4096));
    std::vector<std::wstring> lines;
    std::stringstream ss(text);
    std::string line;
    while (std::getline(ss, line) && lines.size() < 16) {
        // Simple ASCII/UTF-8 to wstring conversion
        std::wstring wline;
        for (char c : line) {
            if (c == '\t') wline += L"    ";
            else if ((unsigned char)c >= 32 && (unsigned char)c <= 126) wline += (wchar_t)c;
            else if ((unsigned char)c > 126) wline += (wchar_t)(unsigned char)c;
        }
        if (wline.length() > 32) {
            wline = wline.substr(0, 30) + L"..";
        }
        lines.push_back(wline);
    }

    if (lines.empty()) {
        lines.push_back(L"// Empty Document");
    }

    auto finalBmp = std::make_unique<Gdiplus::Bitmap>(targetSize, targetSize, PixelFormat32bppARGB);
    {
        Gdiplus::Graphics g(finalBmp.get());
        g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
        g.Clear(Gdiplus::Color(0, 0, 0, 0));

        // 1. Modern Editor Sheet Background
        float pad = targetSize * 0.04f;
        float sheetW = targetSize - pad * 2;
        float sheetH = targetSize - pad * 2;

        Gdiplus::SolidBrush editorBg(Gdiplus::Color(255, 15, 15, 19)); // #0F0F13 Editor Black
        g.FillRectangle(&editorBg, pad, pad, sheetW, sheetH);

        // 2. Title Header Bar with Format Badge
        float headerH = sheetH * 0.16f;
        Gdiplus::SolidBrush headerBg(Gdiplus::Color(255, 24, 24, 28)); // #18181C
        g.FillRectangle(&headerBg, pad, pad, sheetW, headerH);

        // Determine badge label and color based on format
        std::wstring badgeText = L"CODE";
        Gdiplus::Color badgeColor(255, 223, 225, 4); // #DFE104
        Gdiplus::Color badgeTextColor(255, 0, 0, 0);

        if (ext == L".json") {
            badgeText = L"JSON";
            badgeColor = Gdiplus::Color(255, 245, 158, 11); // Amber #F59E0B
        } else if (ext == L".md" || ext == L".markdown") {
            badgeText = L"MARKDOWN";
            badgeColor = Gdiplus::Color(255, 168, 85, 247); // Purple #A855F7
        } else if (ext == L".js" || ext == L".mjs") {
            badgeText = L"JAVASCRIPT";
            badgeColor = Gdiplus::Color(255, 223, 225, 4); // Acid Yellow #DFE104
        } else if (ext == L".css") {
            badgeText = L"CSS";
            badgeColor = Gdiplus::Color(255, 6, 182, 212); // Cyan #06B6D4
        }

        // Draw Window Control Dots (red, yellow, green) on left
        float dotR = headerH * 0.14f;
        float dotY = pad + (headerH - dotR * 2) * 0.5f;
        Gdiplus::SolidBrush dotRed(Gdiplus::Color(255, 239, 68, 68));
        Gdiplus::SolidBrush dotYellow(Gdiplus::Color(255, 234, 179, 8));
        Gdiplus::SolidBrush dotGreen(Gdiplus::Color(255, 34, 197, 94));

        g.FillEllipse(&dotRed, pad + targetSize * 0.04f, dotY, dotR * 2, dotR * 2);
        g.FillEllipse(&dotYellow, pad + targetSize * 0.04f + dotR * 2.8f, dotY, dotR * 2, dotR * 2);
        g.FillEllipse(&dotGreen, pad + targetSize * 0.04f + dotR * 5.6f, dotY, dotR * 2, dotR * 2);

        // Draw Format Pill Badge on right
        float pillH = headerH * 0.65f;
        float pillW = sheetW * 0.38f;
        float pillX = targetSize - pad - pillW - (targetSize * 0.03f);
        float pillY = pad + (headerH - pillH) * 0.5f;

        Gdiplus::SolidBrush pillBrush(badgeColor);
        g.FillRectangle(&pillBrush, pillX, pillY, pillW, pillH);

        Gdiplus::FontFamily sansFont(L"Arial");
        Gdiplus::Font badgeFont(&sansFont, pillH * 0.55f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush badgeTextBrush(badgeTextColor);
        Gdiplus::StringFormat badgeFmt;
        badgeFmt.SetAlignment(Gdiplus::StringAlignmentCenter);
        badgeFmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        Gdiplus::RectF pillRect(pillX, pillY, pillW, pillH);
        g.DrawString(badgeText.c_str(), -1, &badgeFont, pillRect, &badgeFmt, &badgeTextBrush);

        // Header separator line
        Gdiplus::Pen sepPen(Gdiplus::Color(255, 63, 63, 70), 1.0f);
        g.DrawLine(&sepPen, pad, pad + headerH, pad + sheetW, pad + headerH);

        // 3. Code Gutter & Syntax Highlighted Lines
        float bodyY = pad + headerH + (targetSize * 0.03f);
        float bodyH = sheetH - headerH - (targetSize * 0.06f);
        float gutterW = sheetW * 0.16f;

        // Gutter line
        g.DrawLine(&sepPen, pad + gutterW, bodyY, pad + gutterW, pad + sheetH);

        // Fonts for code
        Gdiplus::FontFamily monoFont(L"Consolas");
        float fontSize = (std::min)(14.0f, (bodyH / 11.0f) * 0.72f);
        Gdiplus::Font codeFont(&monoFont, fontSize, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);

        Gdiplus::SolidBrush gutterTextBrush(Gdiplus::Color(255, 82, 82, 91)); // Zinc #52525B
        Gdiplus::SolidBrush defaultTextBrush(Gdiplus::Color(255, 244, 244, 245)); // Off-white
        Gdiplus::SolidBrush keyBrush(Gdiplus::Color(255, 223, 225, 4)); // Acid yellow
        Gdiplus::SolidBrush stringBrush(Gdiplus::Color(255, 52, 211, 153)); // Emerald green #34D399
        Gdiplus::SolidBrush tagBrush(Gdiplus::Color(255, 249, 115, 22)); // Orange #F97316
        Gdiplus::SolidBrush keywordBrush(Gdiplus::Color(255, 192, 132, 252)); // Purple #C084FC

        float lineHeight = bodyH / 10.0f;
        float codeX = pad + gutterW + targetSize * 0.02f;
        float maxCodeW = (pad + sheetW - (targetSize * 0.025f)) - codeX;
        if (maxCodeW < 10.0f) maxCodeW = 10.0f;

        Gdiplus::StringFormat codeFmt;
        codeFmt.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
        codeFmt.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);

        // Clip strictly inside the editor card body to prevent any overflow outside the border
        Gdiplus::RectF clipRect(pad + gutterW + 1.0f, bodyY, sheetW - gutterW - 2.0f, bodyH);
        g.SetClip(clipRect);

        for (size_t i = 0; i < lines.size() && i < 10; ++i) {
            float y = bodyY + i * lineHeight;

            // Line number: 01, 02...
            wchar_t lineNumStr[8] = {0};
            swprintf_s(lineNumStr, L"%02d", (int)(i + 1));
            g.DrawString(lineNumStr, -1, &codeFont, Gdiplus::PointF(pad + targetSize * 0.02f, y), &gutterTextBrush);

            // Syntax highlighting color selection
            const std::wstring& curLine = lines[i];
            Gdiplus::Brush* lineBrush = &defaultTextBrush;

            if (ext == L".json") {
                if (curLine.find(L"\":") != std::wstring::npos) lineBrush = &keyBrush;
                else if (curLine.find(L"\"") != std::wstring::npos) lineBrush = &stringBrush;
            } else if (ext == L".md" || ext == L".markdown") {
                if (curLine.rfind(L"#", 0) == 0) lineBrush = &keyBrush;
                else if (curLine.rfind(L"-", 0) == 0 || curLine.rfind(L"*", 0) == 0) lineBrush = &stringBrush;
                else if (curLine.find(L"`") != std::wstring::npos) lineBrush = &keywordBrush;
            } else if (ext == L".js" || ext == L".mjs") {
                if (curLine.find(L"const ") != std::wstring::npos ||
                    curLine.find(L"let ") != std::wstring::npos ||
                    curLine.find(L"function") != std::wstring::npos ||
                    curLine.find(L"import ") != std::wstring::npos ||
                    curLine.find(L"return ") != std::wstring::npos) {
                    lineBrush = &keywordBrush;
                } else if (curLine.find(L"//") != std::wstring::npos) {
                    lineBrush = &gutterTextBrush;
                }
            } else if (ext == L".css") {
                if (curLine.find(L"{") != std::wstring::npos) lineBrush = &tagBrush;
                else if (curLine.find(L":") != std::wstring::npos) lineBrush = &keyBrush;
            }

            Gdiplus::RectF lineRect(codeX, y, maxCodeW, lineHeight);
            g.DrawString(curLine.c_str(), -1, &codeFont, lineRect, &codeFmt, lineBrush);
        }

        g.ResetClip();

        // Outer Border
        Gdiplus::Pen borderPen(Gdiplus::Color(255, 63, 63, 70), 1.5f);
        g.DrawRectangle(&borderPen, pad, pad, sheetW, sheetH);
    }

    res.hBitmap = GdiplusBitmapToHBitmap(finalBmp.get(), targetSize, targetSize);
    res.success = (res.hBitmap != nullptr);
    return res;
}

CodeThumbnailResult CodeRenderer::RenderSample(const std::wstring& format, UINT cx) {
    std::string sampleText;
    std::wstring ext;

    if (format == L"json") {
        ext = L".json";
        sampleText = "{\n  \"name\": \"PreviewIcon\",\n  \"version\": \"1.0.0\",\n  \"theme\": \"kinetic\",\n  \"active\": true,\n  \"modules\": [\n    \"pdf\",\n    \"video\",\n    \"audio\"\n  ]\n}";
    } else if (format == L"md" || format == L"markdown") {
        ext = L".md";
        sampleText = "# PreviewIcon Shell\n\nHigh-performance native thumbnail provider.\n\n- Zero-lag caching\n- Brutalist kinetic UI\n- Full format coverage\n\n```shell\n$ preview_icon --active\n```";
    } else if (format == L"html") {
        ext = L".html";
        sampleText = "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n  <title>PreviewIcon</title>\n  <link rel=\"stylesheet\" href=\"style.css\">\n</head>\n<body>\n  <div id=\"root\"></div>\n</body>\n</html>";
    } else if (format == L"css") {
        ext = L".css";
        sampleText = ":root {\n  --accent: #DFE104;\n  --background: #09090B;\n  --foreground: #FAFAFA;\n}\n\n.card {\n  border: 2px solid #3F3F46;\n  background: var(--background);\n}";
    } else {
        // JavaScript / TypeScript default
        ext = L".js";
        sampleText = "import { ShellService } from './shell';\n\nexport const initializeProvider = () => {\n  const status = ShellService.getStatus();\n  console.log('Online', status);\n  return status.active;\n};";
    }

    return RenderFromMemory((const BYTE*)sampleText.data(), sampleText.length(), ext, cx);
}
