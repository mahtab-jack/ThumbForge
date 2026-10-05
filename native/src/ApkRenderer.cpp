#include "ApkRenderer.h"
#include <shlwapi.h>
#include <algorithm>
#include <vector>
#include <string>
#include <memory>
#include <cmath>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "gdiplus.lib")

#pragma pack(push, 1)
struct ZipEOCD {
    DWORD signature;       // 0x06054b50
    WORD diskNumber;
    WORD startDisk;
    WORD entriesOnDisk;
    WORD totalEntries;
    DWORD cdSize;
    DWORD cdOffset;
    WORD commentLength;
};

struct ZipCentralDirHeader {
    DWORD signature;       // 0x02014b50
    WORD versionMadeBy;
    WORD versionNeeded;
    WORD flags;
    WORD method;
    WORD modTime;
    WORD modDate;
    DWORD crc32;
    DWORD compressedSize;
    DWORD uncompressedSize;
    WORD nameLength;
    WORD extraLength;
    WORD commentLength;
    WORD diskStart;
    WORD internalAttr;
    DWORD externalAttr;
    DWORD localHeaderOffset;
};
#pragma pack(pop)

bool ApkRenderer::IsSupportedExtension(const std::wstring& ext) {
    std::wstring lowerExt = ext;
    std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(), ::towlower);
    return (lowerExt == L".apk");
}

HBITMAP ApkRenderer::GdiplusBitmapToHBitmap(Gdiplus::Bitmap* pBmp, int width, int height) {
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

static bool ExtractEntryWithTar(const std::wstring& apkPath, const std::string& entryName, std::vector<BYTE>& outData) {
    outData.clear();
    if (apkPath.empty() || entryName.empty()) return false;

    std::wstring entryW(entryName.begin(), entryName.end());

    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
    HANDLE hRead = nullptr, hWrite = nullptr;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) return false;
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    HANDLE hNulIn = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
    HANDLE hNulErr = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);

    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.hStdInput = hNulIn;
    si.hStdOutput = hWrite;
    si.hStdError = hNulErr;
    PROCESS_INFORMATION pi = { 0 };

    std::wstring cmd = L"tar.exe -xf \"" + apkPath + L"\" -O \"" + entryW + L"\"";
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
        if (hNulIn != INVALID_HANDLE_VALUE) CloseHandle(hNulIn);
        if (hNulErr != INVALID_HANDLE_VALUE) CloseHandle(hNulErr);

        return !outData.empty();
    }

    if (hWrite) CloseHandle(hWrite);
    CloseHandle(hRead);
    if (hNulIn != INVALID_HANDLE_VALUE) CloseHandle(hNulIn);
    if (hNulErr != INVALID_HANDLE_VALUE) CloseHandle(hNulErr);
    return false;
}

static bool ConvertWebPToPng(const std::vector<BYTE>& webpData, std::vector<BYTE>& outPngData) {
    outPngData.clear();
    if (webpData.empty()) return false;

    // Check if it's already PNG
    if (webpData.size() >= 8 && memcmp(webpData.data(), "\x89PNG\r\n\x1a\n", 8) == 0) {
        outPngData = webpData;
        return true;
    }

    if (webpData.size() >= 2 && webpData[0] == 0xFF && webpData[1] == 0xD8) {
        outPngData = webpData;
        return true;
    }

    // Convert via ffmpeg
    std::wstring ffmpegPath = L"C:\\ffmpeg\\bin\\ffmpeg.exe";
    if (!PathFileExistsW(ffmpegPath.c_str())) {
        ffmpegPath = L"ffmpeg.exe";
    }

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

    HANDLE hNulErr = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);

    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.hStdInput = hStdInRd;
    si.hStdOutput = hStdOutWr;
    si.hStdError = hNulErr;

    PROCESS_INFORMATION pi = { 0 };
    std::wstring cmd = L"\"" + ffmpegPath + L"\" -hide_banner -loglevel error -f image2pipe -i pipe:0 -c:v png -f image2pipe pipe:1";
    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(L'\0');

    if (CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(hStdInRd);
        CloseHandle(hStdOutWr);

        // Write input webp data in a thread or direct write
        DWORD bytesWritten = 0;
        WriteFile(hStdInWr, webpData.data(), (DWORD)webpData.size(), &bytesWritten, nullptr);
        CloseHandle(hStdInWr);

        // Read output PNG data
        std::vector<BYTE> buffer(64 * 1024);
        DWORD bytesRead = 0;
        while (ReadFile(hStdOutRd, buffer.data(), (DWORD)buffer.size(), &bytesRead, nullptr) && bytesRead > 0) {
            outPngData.insert(outPngData.end(), buffer.data(), buffer.data() + bytesRead);
        }

        WaitForSingleObject(pi.hProcess, 3000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hStdOutRd);
        if (hNulErr != INVALID_HANDLE_VALUE) CloseHandle(hNulErr);

        return !outPngData.empty();
    }

    CloseHandle(hStdInRd);
    CloseHandle(hStdInWr);
    CloseHandle(hStdOutRd);
    CloseHandle(hStdOutWr);
    if (hNulErr != INVALID_HANDLE_VALUE) CloseHandle(hNulErr);
    return false;
}

static bool ParseManifestForIcon(const std::vector<BYTE>& manifest, uint32_t& outIconId, std::string& outIconStr) {
    outIconId = 0;
    outIconStr.clear();
    if (manifest.size() < 36) return false;

    // String pool header
    uint32_t stringCount = *(const uint32_t*)(manifest.data() + 16);
    uint32_t flags = *(const uint32_t*)(manifest.data() + 24);
    uint32_t stringsStart = *(const uint32_t*)(manifest.data() + 28);
    bool isUtf8 = (flags & (1 << 8)) != 0;

    if (manifest.size() < 36 + stringCount * 4) return false;
    const uint32_t* stringOffsets = (const uint32_t*)(manifest.data() + 36);
    const BYTE* stringsData = manifest.data() + 8 + stringsStart;
    size_t stringsDataLen = (manifest.size() > (size_t)(8 + stringsStart)) ? manifest.size() - (8 + stringsStart) : 0;

    std::vector<std::string> strings;
    strings.reserve(stringCount);
    for (uint32_t i = 0; i < stringCount; ++i) {
        uint32_t off = stringOffsets[i];
        if (off >= stringsDataLen) {
            strings.push_back("");
            continue;
        }
        if (isUtf8) {
            const BYTE* p = stringsData + off;
            if (*p & 0x80) p += 2; else p += 1;
            uint32_t blen = 0;
            if (*p & 0x80) {
                blen = ((*p & 0x7F) << 8) | *(p + 1);
                p += 2;
            } else {
                blen = *p;
                p += 1;
            }
            if ((size_t)(p - stringsData + blen) <= stringsDataLen) {
                strings.push_back(std::string((const char*)p, blen));
            } else {
                strings.push_back("");
            }
        } else {
            uint16_t u16len = *(const uint16_t*)(stringsData + off);
            const wchar_t* p = (const wchar_t*)(stringsData + off + 2);
            if ((size_t)(off + 2 + u16len * 2) <= stringsDataLen) {
                std::string s;
                s.reserve(u16len);
                for (uint16_t c = 0; c < u16len; ++c) {
                    s.push_back((char)(p[c] & 0xFF));
                }
                strings.push_back(s);
            } else {
                strings.push_back("");
            }
        }
    }

    size_t pos = 8;
    while (pos + 8 <= manifest.size()) {
        uint16_t chunkType = *(const uint16_t*)(manifest.data() + pos);
        uint32_t chunkSz = *(const uint32_t*)(manifest.data() + pos + 4);
        if (chunkSz < 8 || pos + chunkSz > manifest.size()) break;

        if (chunkType == 0x0102) { // START_TAG
            if (pos + 36 <= manifest.size()) {
                uint32_t nameIdx = *(const uint32_t*)(manifest.data() + pos + 20);
                uint16_t attrCount = *(const uint16_t*)(manifest.data() + pos + 28);
                std::string elemName = (nameIdx < strings.size()) ? strings[nameIdx] : "";

                if (elemName == "application" || elemName == "activity") {
                    size_t attrPos = pos + 36;
                    for (uint16_t a = 0; a < attrCount && attrPos + 20 <= manifest.size(); ++a) {
                        uint32_t attrNameIdx = *(const uint32_t*)(manifest.data() + attrPos + 4);
                        uint32_t attrRawValIdx = *(const uint32_t*)(manifest.data() + attrPos + 8);
                        uint32_t attrValType = (*(const uint32_t*)(manifest.data() + attrPos + 12)) >> 24;
                        uint32_t attrValData = *(const uint32_t*)(manifest.data() + attrPos + 16);

                        std::string attrName = (attrNameIdx < strings.size()) ? strings[attrNameIdx] : "";
                        std::string lowerAName = attrName;
                        std::transform(lowerAName.begin(), lowerAName.end(), lowerAName.begin(), ::tolower);

                        if (lowerAName == "icon" || lowerAName == "roundicon") {
                            if (attrValType == 0x01) { // TYPE_REFERENCE
                                outIconId = attrValData;
                            } else if (attrRawValIdx < strings.size() && !strings[attrRawValIdx].empty()) {
                                outIconStr = strings[attrRawValIdx];
                            }
                            if (lowerAName == "icon" && (outIconId != 0 || !outIconStr.empty())) {
                                return true;
                            }
                        }
                        attrPos += 20;
                    }
                    if (outIconId != 0 || !outIconStr.empty()) return true;
                }
            }
        }
        pos += chunkSz;
    }
    return (outIconId != 0 || !outIconStr.empty());
}

static bool ResolveIconFromArsc(const std::vector<BYTE>& arsc, uint32_t iconId, std::vector<std::string>& outPaths) {
    outPaths.clear();
    if (arsc.size() < 28) return false;

    uint16_t headerSize = *(const uint16_t*)(arsc.data() + 2);
    if (headerSize + 28 > arsc.size()) return false;

    const BYTE* sp = arsc.data() + headerSize;
    uint32_t stringCount = *(const uint32_t*)(sp + 8);
    uint32_t flags = *(const uint32_t*)(sp + 16);
    uint32_t stringsStart = *(const uint32_t*)(sp + 20);
    bool isUtf8 = (flags & (1 << 8)) != 0;

    if ((size_t)(headerSize + 28 + stringCount * 4) > arsc.size()) return false;
    const uint32_t* stringOffsets = (const uint32_t*)(sp + 28);
    const BYTE* stringsData = sp + stringsStart;
    size_t stringsDataLen = (arsc.size() > (size_t)(headerSize + stringsStart)) ? arsc.size() - (headerSize + stringsStart) : 0;

    std::vector<std::string> globalStrings;
    globalStrings.reserve(stringCount);
    for (uint32_t i = 0; i < stringCount; ++i) {
        uint32_t off = stringOffsets[i];
        if (off >= stringsDataLen) {
            globalStrings.push_back("");
            continue;
        }
        if (isUtf8) {
            const BYTE* p = stringsData + off;
            if (*p & 0x80) p += 2; else p += 1;
            uint32_t blen = 0;
            if (*p & 0x80) {
                blen = ((*p & 0x7F) << 8) | *(p + 1);
                p += 2;
            } else {
                blen = *p;
                p += 1;
            }
            if ((size_t)(p - stringsData + blen) <= stringsDataLen) {
                globalStrings.push_back(std::string((const char*)p, blen));
            } else {
                globalStrings.push_back("");
            }
        } else {
            uint16_t u16len = *(const uint16_t*)(stringsData + off);
            const wchar_t* p = (const wchar_t*)(stringsData + off + 2);
            if ((size_t)(off + 2 + u16len * 2) <= stringsDataLen) {
                std::string s;
                s.reserve(u16len);
                for (uint16_t c = 0; c < u16len; ++c) {
                    s.push_back((char)(p[c] & 0xFF));
                }
                globalStrings.push_back(s);
            } else {
                globalStrings.push_back("");
            }
        }
    }

    uint32_t targetType = (iconId >> 16) & 0xFF;
    uint32_t targetEntry = iconId & 0xFFFF;

    uint32_t spSize = *(const uint32_t*)(sp + 4);
    size_t pos = headerSize + spSize;

    while (pos + 8 <= arsc.size()) {
        uint16_t chunkType = *(const uint16_t*)(arsc.data() + pos);
        uint16_t chunkHdrSz = *(const uint16_t*)(arsc.data() + pos + 2);
        uint32_t chunkSz = *(const uint32_t*)(arsc.data() + pos + 4);
        if (chunkSz == 0 || pos + chunkSz > arsc.size()) break;

        if (chunkType == 0x0200) { // RES_TABLE_PACKAGE_TYPE
            size_t inPos = pos + chunkHdrSz;
            while (inPos + 8 <= pos + chunkSz) {
                uint16_t cType = *(const uint16_t*)(arsc.data() + inPos);
                uint16_t cHdrSz = *(const uint16_t*)(arsc.data() + inPos + 2);
                uint32_t cSz = *(const uint32_t*)(arsc.data() + inPos + 4);
                if (cSz == 0 || inPos + cSz > pos + chunkSz) break;

                if (cType == 0x0201 && inPos + 20 <= pos + chunkSz) { // RES_TABLE_TYPE_TYPE
                    uint8_t typeId = *(const uint8_t*)(arsc.data() + inPos + 8);
                    uint32_t entryCount = *(const uint32_t*)(arsc.data() + inPos + 12);
                    uint32_t entriesStart = *(const uint32_t*)(arsc.data() + inPos + 16);

                    if (typeId == targetType && targetEntry < entryCount && (size_t)(inPos + cHdrSz + entryCount * 4) <= pos + chunkSz) {
                        const int32_t* entryOffsets = (const int32_t*)(arsc.data() + inPos + cHdrSz);
                        int32_t eOff = entryOffsets[targetEntry];
                        if (eOff != -1 && (size_t)(inPos + entriesStart + eOff + 16) <= pos + chunkSz) {
                            size_t actualE = inPos + entriesStart + eOff;
                            uint8_t valDt = *(const uint8_t*)(arsc.data() + actualE + 11);
                            uint32_t valData = *(const uint32_t*)(arsc.data() + actualE + 12);
                            if (valDt == 3 && valData < globalStrings.size()) {
                                outPaths.push_back(globalStrings[valData]);
                            }
                        }
                    }
                }
                inPos += cSz;
            }
        }
        pos += chunkSz;
    }
    return !outPaths.empty();
}

bool ApkRenderer::ExtractIconFromApk(const std::wstring& apkPath, std::vector<BYTE>& outPngData) {
    outPngData.clear();

    // 1. Universal resolution via AndroidManifest.xml and resources.arsc
    std::vector<BYTE> manifestData;
    if (ExtractEntryWithTar(apkPath, "AndroidManifest.xml", manifestData)) {
        uint32_t iconId = 0;
        std::string iconStr;
        if (ParseManifestForIcon(manifestData, iconId, iconStr)) {
            std::vector<std::string> candidates;
            if (!iconStr.empty()) {
                candidates.push_back(iconStr);
            }
            if (iconId != 0) {
                std::vector<BYTE> arscData;
                if (ExtractEntryWithTar(apkPath, "resources.arsc", arscData)) {
                    ResolveIconFromArsc(arscData, iconId, candidates);
                }
            }

            // Filter for image candidates
            std::vector<std::string> imgCandidates;
            for (const auto& path : candidates) {
                std::string lower = path;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                if (lower.find(".png") != std::string::npos ||
                    lower.find(".webp") != std::string::npos ||
                    lower.find(".jpg") != std::string::npos ||
                    lower.find(".jpeg") != std::string::npos) {
                    imgCandidates.push_back(path);
                }
            }

            if (!imgCandidates.empty()) {
                // Try from highest resolution candidate (typically last in density list)
                for (int i = (int)imgCandidates.size() - 1; i >= 0; --i) {
                    std::vector<BYTE> rawData;
                    if (ExtractEntryWithTar(apkPath, imgCandidates[i], rawData) && !rawData.empty()) {
                        if (ConvertWebPToPng(rawData, outPngData)) {
                            return true;
                        }
                    }
                }
            }
        }
    }

    // 2. Fallback: Search Central Directory for launcher/icon files
    HANDLE hFile = CreateFileW(apkPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize) || fileSize.QuadPart < sizeof(ZipEOCD)) {
        CloseHandle(hFile);
        return false;
    }

    DWORD searchSize = (DWORD)(std::min<ULONGLONG>)(65536 + sizeof(ZipEOCD), fileSize.QuadPart);
    LARGE_INTEGER seekPos;
    seekPos.QuadPart = fileSize.QuadPart - searchSize;
    SetFilePointerEx(hFile, seekPos, nullptr, FILE_BEGIN);

    std::vector<BYTE> tailBuf(searchSize);
    DWORD bytesRead = 0;
    if (!ReadFile(hFile, tailBuf.data(), searchSize, &bytesRead, nullptr) || bytesRead < sizeof(ZipEOCD)) {
        CloseHandle(hFile);
        return false;
    }

    int eocdOffsetInTail = -1;
    for (int i = (int)(bytesRead - sizeof(ZipEOCD)); i >= 0; --i) {
        if (*(DWORD*)(tailBuf.data() + i) == 0x06054b50) {
            eocdOffsetInTail = i;
            break;
        }
    }

    if (eocdOffsetInTail < 0) {
        CloseHandle(hFile);
        return false;
    }

    const ZipEOCD* pEocd = (const ZipEOCD*)(tailBuf.data() + eocdOffsetInTail);
    if (pEocd->totalEntries == 0 || pEocd->cdSize == 0) {
        CloseHandle(hFile);
        return false;
    }

    seekPos.QuadPart = pEocd->cdOffset;
    SetFilePointerEx(hFile, seekPos, nullptr, FILE_BEGIN);

    std::vector<BYTE> cdBuf(pEocd->cdSize);
    if (!ReadFile(hFile, cdBuf.data(), pEocd->cdSize, &bytesRead, nullptr) || bytesRead < pEocd->cdSize) {
        CloseHandle(hFile);
        return false;
    }
    CloseHandle(hFile);

    struct CandidateEntry {
        std::string name;
        int score;
    };
    std::vector<CandidateEntry> entries;

    size_t cdPtr = 0;
    while (cdPtr + sizeof(ZipCentralDirHeader) <= cdBuf.size()) {
        const ZipCentralDirHeader* pHdr = (const ZipCentralDirHeader*)(cdBuf.data() + cdPtr);
        if (pHdr->signature != 0x02014b50) break;

        size_t nextEntry = cdPtr + sizeof(ZipCentralDirHeader) + pHdr->nameLength + pHdr->extraLength + pHdr->commentLength;
        if (nextEntry > cdBuf.size()) break;

        std::string entryName((const char*)(cdBuf.data() + cdPtr + sizeof(ZipCentralDirHeader)), pHdr->nameLength);
        std::string lowerName = entryName;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

        bool isImg = (lowerName.find(".png") != std::string::npos ||
                      lowerName.find(".webp") != std::string::npos ||
                      lowerName.find(".jpg") != std::string::npos);

        if (isImg && lowerName.find(".9.png") == std::string::npos) {
            int score = 0;
            if (lowerName.find("ic_launcher") != std::string::npos) score += 100;
            else if (lowerName.find("app_icon") != std::string::npos || lowerName.find("appicon") != std::string::npos) score += 90;
            else if (lowerName.find("icon") != std::string::npos) score += 60;
            else if (lowerName.find("logo") != std::string::npos) score += 50;

            if (score > 0) {
                if (lowerName.find("xxxhdpi") != std::string::npos) score += 40;
                else if (lowerName.find("xxhdpi") != std::string::npos) score += 30;
                else if (lowerName.find("xhdpi") != std::string::npos) score += 20;
                else if (lowerName.find("hdpi") != std::string::npos) score += 10;
                entries.push_back({entryName, score});
            }
        }
        cdPtr = nextEntry;
    }

    if (!entries.empty()) {
        std::sort(entries.begin(), entries.end(), [](const CandidateEntry& a, const CandidateEntry& b) {
            return a.score > b.score;
        });

        for (const auto& entry : entries) {
            std::vector<BYTE> rawData;
            if (ExtractEntryWithTar(apkPath, entry.name, rawData) && !rawData.empty()) {
                if (ConvertWebPToPng(rawData, outPngData)) {
                    return true;
                }
            }
        }
    }

    return false;
}

ApkThumbnailResult ApkRenderer::RenderThumbnail(const std::wstring& filePath, UINT cx) {
    ApkThumbnailResult res;
    int size = (int)cx;
    if (size < 48) size = 48;
    if (size > 1024) size = 1024;
    res.width = size;
    res.height = size;

    std::vector<BYTE> pngBytes;
    bool hasIcon = !filePath.empty() && ExtractIconFromApk(filePath, pngBytes);

    auto finalBmp = std::make_unique<Gdiplus::Bitmap>(size, size, PixelFormat32bppARGB);
    {
        Gdiplus::Graphics g(finalBmp.get());
        g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.Clear(Gdiplus::Color(0, 0, 0, 0));

        bool drewRealIcon = false;
        if (hasIcon && !pngBytes.empty()) {
            IStream* pStream = SHCreateMemStream(pngBytes.data(), (UINT)pngBytes.size());
            if (pStream) {
                std::unique_ptr<Gdiplus::Bitmap> pIconBmp(Gdiplus::Bitmap::FromStream(pStream));
                pStream->Release();

                if (pIconBmp && pIconBmp->GetLastStatus() == Gdiplus::Ok &&
                    pIconBmp->GetWidth() > 0 && pIconBmp->GetHeight() > 0) {
                    
                    // Render the real app icon cleanly and prominently
                    float pad = size * 0.04f;
                    float iconSize = size - pad * 2;
                    float iconX = pad;
                    float iconY = pad;

                    // Draw subtle soft shadow under the icon
                    Gdiplus::SolidBrush shadowBrush(Gdiplus::Color(45, 0, 0, 0));
                    g.FillEllipse(&shadowBrush, iconX + iconSize * 0.05f, iconY + iconSize * 0.90f, iconSize * 0.90f, iconSize * 0.08f);

                    // Draw the real app icon
                    g.DrawImage(pIconBmp.get(), iconX, iconY, iconSize, iconSize);
                    drewRealIcon = true;
                }
            }
        }

        // Fallback only if no icon could be extracted
        if (!drewRealIcon) {
            float pad = size * 0.05f;
            float cardW = size - pad * 2;
            float cardH = size - pad * 2;

            Gdiplus::SolidBrush cardBg(Gdiplus::Color(0, 0, 0, 0)); // Transparent
            g.FillRectangle(&cardBg, pad, pad, cardW, cardH);

            float iconSize = size * 0.48f;
            float iconX = (size - iconSize) * 0.5f;
            float iconY = (size - iconSize) * 0.44f;

            // Android robot stylized head in Bauhaus yellow
            Gdiplus::SolidBrush androidBrush(Gdiplus::Color(255, 240, 192, 32));
            g.FillPie(&androidBrush, iconX, iconY, iconSize, iconSize, 180.0f, 180.0f);

            Gdiplus::Pen antPen(Gdiplus::Color(255, 240, 192, 32), 3.0f);
            g.DrawLine(&antPen, iconX + iconSize * 0.3f, iconY + iconSize * 0.15f, iconX + iconSize * 0.18f, iconY);
            g.DrawLine(&antPen, iconX + iconSize * 0.7f, iconY + iconSize * 0.15f, iconX + iconSize * 0.82f, iconY);

            Gdiplus::SolidBrush eyeBrush(Gdiplus::Color(255, 9, 9, 11));
            float eyeR = iconSize * 0.08f;
            g.FillEllipse(&eyeBrush, iconX + iconSize * 0.28f, iconY + iconSize * 0.22f, eyeR, eyeR);
            g.FillEllipse(&eyeBrush, iconX + iconSize * 0.64f, iconY + iconSize * 0.22f, eyeR, eyeR);

            float pillW = cardW * 0.42f;
            float pillH = cardH * 0.16f;
            float pillX = (size - pillW) * 0.5f;
            float pillY = size - pad - pillH - (size * 0.06f);

            Gdiplus::SolidBrush pillBg(Gdiplus::Color(255, 34, 197, 94));
            g.FillRectangle(&pillBg, pillX, pillY, pillW, pillH);

            Gdiplus::FontFamily fontFamily(L"Arial");
            Gdiplus::Font font(&fontFamily, pillH * 0.65f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
            Gdiplus::SolidBrush textBrush(Gdiplus::Color(255, 0, 0, 0));
            Gdiplus::StringFormat format;
            format.SetAlignment(Gdiplus::StringAlignmentCenter);
            format.SetLineAlignment(Gdiplus::StringAlignmentCenter);

            Gdiplus::RectF layoutRect(pillX, pillY, pillW, pillH);
            g.DrawString(L"APK", -1, &font, layoutRect, &format, &textBrush);

            Gdiplus::Pen borderPen(Gdiplus::Color(255, 63, 63, 70), 1.5f);
            g.DrawRectangle(&borderPen, pad, pad, cardW, cardH);
        }
    }

    res.hBitmap = GdiplusBitmapToHBitmap(finalBmp.get(), size, size);
    res.success = (res.hBitmap != nullptr);
    return res;
}

ApkThumbnailResult ApkRenderer::RenderSample(UINT cx) {
    return RenderThumbnail(L"", cx);
}
