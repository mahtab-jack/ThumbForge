#include "EpubRenderer.h"
#include "HtmlRenderer.h"
#include <shlwapi.h>
#include <algorithm>
#include <vector>
#include <string>
#include <memory>
#include <sstream>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "gdiplus.lib")

bool EpubRenderer::IsSupportedExtension(const std::wstring& ext) {
    std::wstring lowerExt = ext;
    std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(), ::towlower);
    return (lowerExt == L".epub");
}

HBITMAP EpubRenderer::GdiplusBitmapToHBitmap(Gdiplus::Bitmap* pBmp, int width, int height) {
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

static bool ExtractEntryWithTar(const std::wstring& epubPath, const std::string& entryName, std::vector<BYTE>& outData) {
    outData.clear();
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
    std::wstring entryW(entryName.begin(), entryName.end());
    std::wstring cmd = L"tar.exe -xf \"" + epubPath + L"\" -O \"" + entryW + L"\"";
    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(L'\0');

    if (CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(hWrite);
        hWrite = nullptr;

        std::vector<BYTE> buffer(64 * 1024);
        DWORD bytesRead = 0;
        while (ReadFile(hRead, buffer.data(), (DWORD)buffer.size(), &bytesRead, nullptr) && bytesRead > 0) {
            outData.insert(outData.end(), buffer.data(), buffer.data() + bytesRead);
        }

        WaitForSingleObject(pi.hProcess, 3000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hRead);

        return !outData.empty();
    }

    if (hWrite) CloseHandle(hWrite);
    CloseHandle(hRead);
    return false;
}

static bool ConvertWebPToPng(const std::vector<BYTE>& webpData, std::vector<BYTE>& outPngData) {
    outPngData.clear();
    if (webpData.empty()) return false;

    if (webpData.size() >= 8 && memcmp(webpData.data(), "\x89PNG\r\n\x1a\n", 8) == 0) {
        outPngData = webpData;
        return true;
    }

    if (webpData.size() >= 2 && webpData[0] == 0xFF && webpData[1] == 0xD8) {
        outPngData = webpData;
        return true;
    }

    std::wstring ffmpegPath = L"C:\\ffmpeg\\bin\\ffmpeg.exe";
    if (!PathFileExistsW(ffmpegPath.c_str())) ffmpegPath = L"ffmpeg.exe";

    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
    HANDLE hStdInRd = nullptr, hStdInWr = nullptr;
    HANDLE hStdOutRd = nullptr, hStdOutWr = nullptr;

    if (!CreatePipe(&hStdInRd, &hStdInWr, &sa, 0)) return false;
    SetHandleInformation(hStdInWr, HANDLE_FLAG_INHERIT, 0);

    if (!CreatePipe(&hStdOutRd, &hStdOutWr, &sa, 0)) {
        CloseHandle(hStdInRd);
        CloseHandle(hStdInWr);
        return false;
    }
    SetHandleInformation(hStdOutRd, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.hStdInput = hStdInRd;
    si.hStdOutput = hStdOutWr;
    si.hStdError = nullptr;

    PROCESS_INFORMATION pi = { 0 };
    std::wstring cmd = L"\"" + ffmpegPath + L"\" -v error -i pipe:0 -f image2pipe -vcodec png pipe:1";
    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(L'\0');

    if (CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(hStdInRd);
        CloseHandle(hStdOutWr);

        DWORD bytesWritten = 0;
        WriteFile(hStdInWr, webpData.data(), (DWORD)webpData.size(), &bytesWritten, nullptr);
        CloseHandle(hStdInWr);

        std::vector<BYTE> buffer(64 * 1024);
        DWORD bytesRead = 0;
        while (ReadFile(hStdOutRd, buffer.data(), (DWORD)buffer.size(), &bytesRead, nullptr) && bytesRead > 0) {
            outPngData.insert(outPngData.end(), buffer.data(), buffer.data() + bytesRead);
        }

        WaitForSingleObject(pi.hProcess, 3000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hStdOutRd);

        return !outPngData.empty();
    }

    CloseHandle(hStdInRd);
    CloseHandle(hStdInWr);
    CloseHandle(hStdOutRd);
    CloseHandle(hStdOutWr);
    return false;
}

static std::string FindAttrValue(const std::string& tag, const std::string& attrName) {
    size_t p = tag.find(attrName + "=\"");
    if (p != std::string::npos) {
        size_t start = p + attrName.length() + 2;
        size_t end = tag.find("\"", start);
        if (end != std::string::npos) return tag.substr(start, end - start);
    }
    p = tag.find(attrName + "='");
    if (p != std::string::npos) {
        size_t start = p + attrName.length() + 2;
        size_t end = tag.find("'", start);
        if (end != std::string::npos) return tag.substr(start, end - start);
    }
    return "";
}

EpubThumbnailResult EpubRenderer::RenderThumbnail(const std::wstring& filePath, UINT cx) {
    EpubThumbnailResult res;
    // A4 Aspect Ratio: Width / Height = 1 / 1.4142 = ~0.707
    int targetH = (int)cx;
    if (targetH < 64) targetH = 64;
    if (targetH > 1024) targetH = 1024;
    int targetW = (int)(targetH / 1.4142f);
    if (targetW < 48) targetW = 48;

    res.width = targetW;
    res.height = targetH;

    auto finalBmp = std::make_unique<Gdiplus::Bitmap>(targetW, targetH, PixelFormat32bppARGB);
    {
        Gdiplus::Graphics g(finalBmp.get());
        g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.Clear(Gdiplus::Color(255, 255, 255, 255)); // Crisp book sheet canvas

        bool rendered = false;

        // 1. Parse META-INF/container.xml to find OPF path
        std::vector<BYTE> containerBytes;
        std::string opfPath;
        if (ExtractEntryWithTar(filePath, "META-INF/container.xml", containerBytes)) {
            std::string cXml((const char*)containerBytes.data(), containerBytes.size());
            size_t rfPos = cXml.find("<rootfile");
            if (rfPos != std::string::npos) {
                size_t rfEnd = cXml.find(">", rfPos);
                if (rfEnd != std::string::npos) {
                    std::string tag = cXml.substr(rfPos, rfEnd - rfPos + 1);
                    opfPath = FindAttrValue(tag, "full-path");
                }
            }
        }

        std::string opfBaseDir;
        if (!opfPath.empty()) {
            size_t slash = opfPath.find_last_of("/\\");
            if (slash != std::string::npos) {
                opfBaseDir = opfPath.substr(0, slash + 1);
            }
        }

        // 2. Read OPF file
        std::vector<BYTE> opfBytes;
        std::string coverHref;
        std::string firstChapterHref;
        std::string bookTitle;
        std::string bookAuthor;

        if (!opfPath.empty() && ExtractEntryWithTar(filePath, opfPath, opfBytes)) {
            std::string opfXml((const char*)opfBytes.data(), opfBytes.size());

            // Extract Title
            size_t tStart = opfXml.find("<dc:title");
            if (tStart != std::string::npos) {
                size_t tOpen = opfXml.find(">", tStart);
                size_t tEnd = opfXml.find("</dc:title>", tOpen);
                if (tOpen != std::string::npos && tEnd != std::string::npos) {
                    bookTitle = opfXml.substr(tOpen + 1, tEnd - tOpen - 1);
                }
            }

            // Extract Author
            size_t aStart = opfXml.find("<dc:creator");
            if (aStart != std::string::npos) {
                size_t aOpen = opfXml.find(">", aStart);
                size_t aEnd = opfXml.find("</dc:creator>", aOpen);
                if (aOpen != std::string::npos && aEnd != std::string::npos) {
                    bookAuthor = opfXml.substr(aOpen + 1, aEnd - aOpen - 1);
                }
            }

            // Look for cover in metadata or manifest
            size_t metaCoverPos = opfXml.find("name=\"cover\"");
            std::string coverId;
            if (metaCoverPos != std::string::npos) {
                size_t tagStart = opfXml.rfind("<meta", metaCoverPos);
                size_t tagEnd = opfXml.find(">", metaCoverPos);
                if (tagStart != std::string::npos && tagEnd != std::string::npos) {
                    std::string metaTag = opfXml.substr(tagStart, tagEnd - tagStart + 1);
                    coverId = FindAttrValue(metaTag, "content");
                }
            }

            // Scan manifest items
            size_t mPos = 0;
            while ((mPos = opfXml.find("<item ", mPos)) != std::string::npos) {
                size_t itemEnd = opfXml.find(">", mPos);
                if (itemEnd == std::string::npos) break;
                std::string itemTag = opfXml.substr(mPos, itemEnd - mPos + 1);
                std::string id = FindAttrValue(itemTag, "id");
                std::string href = FindAttrValue(itemTag, "href");
                std::string media = FindAttrValue(itemTag, "media-type");
                std::string props = FindAttrValue(itemTag, "properties");

                if (props.find("cover-image") != std::string::npos ||
                    (!coverId.empty() && id == coverId) ||
                    (coverHref.empty() && id.find("cover") != std::string::npos && media.find("image/") != std::string::npos)) {
                    coverHref = href;
                }

                if (firstChapterHref.empty() &&
                    (media == "application/xhtml+xml" || media == "text/html") &&
                    props.find("nav") == std::string::npos &&
                    id.find("toc") == std::string::npos) {
                    firstChapterHref = href;
                }

                mPos = itemEnd + 1;
            }
        }

        // 3. Try to extract and render cover image
        if (!coverHref.empty()) {
            std::string fullCoverPath = opfBaseDir + coverHref;
            std::vector<BYTE> imgData;
            if (ExtractEntryWithTar(filePath, fullCoverPath, imgData) && !imgData.empty()) {
                std::vector<BYTE> pngData;
                if (ConvertWebPToPng(imgData, pngData)) {
                    IStream* pStream = SHCreateMemStream(pngData.data(), (UINT)pngData.size());
                    if (pStream) {
                        std::unique_ptr<Gdiplus::Bitmap> pCoverBmp(Gdiplus::Bitmap::FromStream(pStream));
                        pStream->Release();
                        if (pCoverBmp && pCoverBmp->GetLastStatus() == Gdiplus::Ok &&
                            pCoverBmp->GetWidth() > 0 && pCoverBmp->GetHeight() > 0) {
                            
                            // Fit cover inside A4 sheet canvas with neat margins
                            float padX = targetW * 0.04f;
                            float padY = targetH * 0.04f;
                            float maxW = targetW - padX * 2;
                            float maxH = targetH - padY * 2;

                            float scale = (std::min)(maxW / (float)pCoverBmp->GetWidth(), maxH / (float)pCoverBmp->GetHeight());
                            float drawW = pCoverBmp->GetWidth() * scale;
                            float drawH = pCoverBmp->GetHeight() * scale;
                            float drawX = (targetW - drawW) * 0.5f;
                            float drawY = (targetH - drawH) * 0.5f;

                            g.DrawImage(pCoverBmp.get(), drawX, drawY, drawW, drawH);
                            rendered = true;
                        }
                    }
                }
            }
        }

        // 4. If no cover image, render first chapter XHTML via headless browser in A4 size
        if (!rendered && !firstChapterHref.empty()) {
            std::string fullXhtmlPath = opfBaseDir + firstChapterHref;
            std::vector<BYTE> xhtmlData;
            if (ExtractEntryWithTar(filePath, fullXhtmlPath, xhtmlData) && !xhtmlData.empty()) {
                std::wstring browser = HtmlRenderer::FindBrowserPath();
                if (!browser.empty()) {
                    wchar_t tempDir[MAX_PATH] = {0};
                    GetTempPathW(MAX_PATH, tempDir);
                    wchar_t tempFile[MAX_PATH] = {0};
                    GetTempFileNameW(tempDir, L"tfe", 0, tempFile);
                    std::wstring tempXhtml = tempFile;
                    tempXhtml += L".xhtml";
                    std::wstring tempPng = tempFile;
                    tempPng += L".png";
                    DeleteFileW(tempFile);

                    HANDLE hOut = CreateFileW(tempXhtml.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                    if (hOut != INVALID_HANDLE_VALUE) {
                        DWORD written = 0;
                        WriteFile(hOut, xhtmlData.data(), (DWORD)xhtmlData.size(), &written, nullptr);
                        CloseHandle(hOut);

                        std::wstring cmd = L"\"" + browser + L"\" --headless=new --disable-gpu --screenshot=\"" + tempPng +
                                           L"\" --window-size=1240,1754 --hide-scrollbars --virtual-time-budget=1000 \"file:///" + tempXhtml + L"\"";
                        std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
                        cmdBuf.push_back(L'\0');

                        STARTUPINFOW si = { sizeof(si) };
                        si.dwFlags = STARTF_USESHOWWINDOW;
                        si.wShowWindow = SW_HIDE;
                        PROCESS_INFORMATION pi = { 0 };

                        if (CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
                            WaitForSingleObject(pi.hProcess, 5000);
                            CloseHandle(pi.hProcess);
                            CloseHandle(pi.hThread);

                            if (PathFileExistsW(tempPng.c_str())) {
                                std::unique_ptr<Gdiplus::Bitmap> pShot(Gdiplus::Bitmap::FromFile(tempPng.c_str()));
                                if (pShot && pShot->GetLastStatus() == Gdiplus::Ok &&
                                    pShot->GetWidth() > 0 && pShot->GetHeight() > 0) {
                                    g.DrawImage(pShot.get(), 0, 0, targetW, targetH);
                                    rendered = true;
                                }
                                DeleteFileW(tempPng.c_str());
                            }
                        }
                        DeleteFileW(tempXhtml.c_str());
                    }
                }
            }
        }

        // 5. Fallback: Clean stylized book cover sheet with Title & Author
        if (!rendered) {
            std::wstring wTitle = bookTitle.empty() ? L"EPUB Book" : std::wstring(bookTitle.begin(), bookTitle.end());
            std::wstring wAuthor = bookAuthor.empty() ? L"" : std::wstring(bookAuthor.begin(), bookAuthor.end());

            // Header banner (Bauhaus Red)
            Gdiplus::SolidBrush bannerBrush(Gdiplus::Color(255, 208, 32, 32));
            g.FillRectangle(&bannerBrush, 0, 0, targetW, (int)(targetH * 0.05f));

            // Book icon / frame in center
            float bookBoxW = targetW * 0.76f;
            float bookBoxH = targetH * 0.40f;
            float bookBoxX = (targetW - bookBoxW) * 0.5f;
            float bookBoxY = targetH * 0.12f;

            Gdiplus::SolidBrush bookBg(Gdiplus::Color(255, 245, 245, 248));
            g.FillRectangle(&bookBg, bookBoxX, bookBoxY, bookBoxW, bookBoxH);
            Gdiplus::Pen bookBorder(Gdiplus::Color(255, 210, 210, 215), 1.5f);
            g.DrawRectangle(&bookBorder, bookBoxX, bookBoxY, bookBoxW, bookBoxH);

            // Title inside book box
            Gdiplus::FontFamily fontFamily(L"Segoe UI");
            Gdiplus::Font titleFont(&fontFamily, targetH * 0.045f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
            Gdiplus::SolidBrush textBrush(Gdiplus::Color(255, 18, 18, 22));
            Gdiplus::RectF titleRect(bookBoxX + targetW * 0.05f, bookBoxY + targetH * 0.04f, bookBoxW - targetW * 0.10f, bookBoxH * 0.60f);
            g.DrawString(wTitle.c_str(), -1, &titleFont, titleRect, nullptr, &textBrush);

            if (!wAuthor.empty()) {
                Gdiplus::Font authorFont(&fontFamily, targetH * 0.035f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
                Gdiplus::SolidBrush authorBrush(Gdiplus::Color(255, 100, 100, 110));
                Gdiplus::RectF authorRect(bookBoxX + targetW * 0.05f, bookBoxY + bookBoxH * 0.70f, bookBoxW - targetW * 0.10f, bookBoxH * 0.25f);
                g.DrawString(wAuthor.c_str(), -1, &authorFont, authorRect, nullptr, &authorBrush);
            }

            // Subtle simulated book pages below
            Gdiplus::SolidBrush pBrush(Gdiplus::Color(255, 235, 235, 240));
            for (int i = 0; i < 5; ++i) {
                float y = targetH * 0.58f + i * (targetH * 0.08f);
                float w = (i % 2 == 1) ? targetW * 0.50f : targetW * 0.76f;
                g.FillRectangle(&pBrush, (float)(targetW * 0.12f), y, w, (float)(targetH * 0.035f));
            }
        }

        // Draw crisp A4 document sheet border
        Gdiplus::Pen borderPen(Gdiplus::Color(255, 200, 200, 205), 1.0f);
        g.DrawRectangle(&borderPen, 0, 0, targetW - 1, targetH - 1);
    }

    res.hBitmap = GdiplusBitmapToHBitmap(finalBmp.get(), targetW, targetH);
    res.success = (res.hBitmap != nullptr);
    return res;
}

EpubThumbnailResult EpubRenderer::RenderSample(UINT cx) {
    return RenderThumbnail(L"", cx);
}
