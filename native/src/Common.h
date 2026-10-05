#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <unknwn.h>
#include <thumbcache.h>
#include <propsys.h>
#include <shlobj.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>

// PDF Provider CLSID: {5A18E5B4-1174-4F0F-8E1B-933D5A1CB092}
inline const CLSID CLSID_ThumbForgePdfThumbnailProvider = 
    { 0x5a18e5b4, 0x1174, 0x4f0f, { 0x8e, 0x1b, 0x93, 0x3d, 0x5a, 0x1c, 0xb0, 0x92 } };
inline const wchar_t* kPdfClsidString = L"{5A18E5B4-1174-4F0F-8E1B-933D5A1CB092}";

// Video Provider CLSID: {5A18E5B4-1174-4F0F-8E1B-933D5A1CB093}
inline const CLSID CLSID_ThumbForgeVideoThumbnailProvider = 
    { 0x5a18e5b4, 0x1174, 0x4f0f, { 0x8e, 0x1b, 0x93, 0x3d, 0x5a, 0x1c, 0xb0, 0x93 } };
inline const wchar_t* kVideoClsidString = L"{5A18E5B4-1174-4F0F-8E1B-933D5A1CB093}";

// Audio Provider CLSID: {5A18E5B4-1174-4F0F-8E1B-933D5A1CB094}
inline const CLSID CLSID_ThumbForgeAudioThumbnailProvider = 
    { 0x5a18e5b4, 0x1174, 0x4f0f, { 0x8e, 0x1b, 0x93, 0x3d, 0x5a, 0x1c, 0xb0, 0x94 } };
inline const wchar_t* kAudioClsidString = L"{5A18E5B4-1174-4F0F-8E1B-933D5A1CB094}";

// APK Provider CLSID: {5A18E5B4-1174-4F0F-8E1B-933D5A1CB095}
inline const CLSID CLSID_ThumbForgeApkThumbnailProvider = 
    { 0x5a18e5b4, 0x1174, 0x4f0f, { 0x8e, 0x1b, 0x93, 0x3d, 0x5a, 0x1c, 0xb0, 0x95 } };
inline const wchar_t* kApkClsidString = L"{5A18E5B4-1174-4F0F-8E1B-933D5A1CB095}";

// Code / Markup Provider CLSID: {5A18E5B4-1174-4F0F-8E1B-933D5A1CB096}
inline const CLSID CLSID_ThumbForgeCodeThumbnailProvider = 
    { 0x5a18e5b4, 0x1174, 0x4f0f, { 0x8e, 0x1b, 0x93, 0x3d, 0x5a, 0x1c, 0xb0, 0x96 } };
inline const wchar_t* kCodeClsidString = L"{5A18E5B4-1174-4F0F-8E1B-933D5A1CB096}";

// HTML Provider CLSID: {5A18E5B4-1174-4F0F-8E1B-933D5A1CB097}
inline const CLSID CLSID_ThumbForgeHtmlThumbnailProvider = 
    { 0x5a18e5b4, 0x1174, 0x4f0f, { 0x8e, 0x1b, 0x93, 0x3d, 0x5a, 0x1c, 0xb0, 0x97 } };
inline const wchar_t* kHtmlClsidString = L"{5A18E5B4-1174-4F0F-8E1B-933D5A1CB097}";

// EPUB Provider CLSID: {5A18E5B4-1174-4F0F-8E1B-933D5A1CB098}
inline const CLSID CLSID_ThumbForgeEpubThumbnailProvider = 
    { 0x5a18e5b4, 0x1174, 0x4f0f, { 0x8e, 0x1b, 0x93, 0x3d, 0x5a, 0x1c, 0xb0, 0x98 } };
inline const wchar_t* kEpubClsidString = L"{5A18E5B4-1174-4F0F-8E1B-933D5A1CB098}";

// Legacy / alias definitions
inline const CLSID CLSID_ThumbForgeThumbnailProvider = CLSID_ThumbForgePdfThumbnailProvider;
inline const wchar_t* kClsidString = kPdfClsidString;
inline const wchar_t* kProviderDescription = L"ThumbForge Shell Thumbnail Provider";

// Supported video extensions
inline const std::vector<std::wstring> kSupportedVideoExtensions = {
    L".mp4", L".mkv", L".avi", L".mov", L".webm",
    L".wmv", L".flv", L".m4v", L".ts",  L".3gp"
};

// Supported audio extensions
inline const std::vector<std::wstring> kSupportedAudioExtensions = {
    L".mp3", L".flac", L".wav", L".m4a", L".aac", L".ogg", L".wma", L".opus"
};

// Supported APK extensions
inline const std::vector<std::wstring> kSupportedApkExtensions = {
    L".apk"
};

// Supported Code / Markup extensions
inline const std::vector<std::wstring> kSupportedCodeExtensions = {
    L".json", L".md", L".markdown", L".js", L".mjs", L".css"
};

// Supported HTML extensions
inline const std::vector<std::wstring> kSupportedHtmlExtensions = {
    L".html", L".htm"
};

// Supported EPUB extensions
inline const std::vector<std::wstring> kSupportedEpubExtensions = {
    L".epub"
};

// GDI+ RAII helper
class GdiplusScope {
public:
    GdiplusScope() {
        Gdiplus::GdiplusStartupInput input;
        Gdiplus::GdiplusStartup(&m_token, &input, nullptr);
    }
    ~GdiplusScope() {
        if (m_token) {
            Gdiplus::GdiplusShutdown(m_token);
        }
    }
private:
    ULONG_PTR m_token = 0;
};
