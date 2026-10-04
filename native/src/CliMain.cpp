#include "Common.h"
#include "PdfRenderer.h"
#include "VideoRenderer.h"
#include "AudioRenderer.h"
#include "ApkRenderer.h"
#include "CodeRenderer.h"
#include "ShellRegistry.h"
#include <iostream>
#include <fstream>
#include <shlwapi.h>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "gdiplus.lib")

HMODULE g_hModule = nullptr;

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0;
    UINT size = 0;
    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;

    std::vector<BYTE> memory(size);
    Gdiplus::ImageCodecInfo* pImageCodecInfo = (Gdiplus::ImageCodecInfo*)memory.data();
    Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);

    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            return j;
        }
    }
    return -1;
}

bool SaveHBitmapToPng(HBITMAP hBitmap, const std::wstring& outputPath) {
    if (!hBitmap) return false;

    Gdiplus::Bitmap bmp(hBitmap, nullptr);
    CLSID pngClsid;
    if (GetEncoderClsid(L"image/png", &pngClsid) < 0) return false;

    Gdiplus::Status st = bmp.Save(outputPath.c_str(), &pngClsid, nullptr);
    return (st == Gdiplus::Ok);
}

void PrintJsonEscaped(const std::wstring& str) {
    for (wchar_t c : str) {
        if (c == L'\\') std::wcout << L"\\\\";
        else if (c == L'\"') std::wcout << L"\\\"";
        else if (c == L'\n') std::wcout << L"\\n";
        else if (c == L'\r') std::wcout << L"\\r";
        else if (c == L'\t') std::wcout << L"\\t";
        else std::wcout << c;
    }
}

int wmain(int argc, wchar_t* argv[]) {
    GdiplusScope gdiScope;

    if (argc < 2) {
        std::wcout << L"{\"error\":\"Missing command. Commands: status, register, unregister, refresh, render, sample, set-ffmpeg\"}\n";
        return 1;
    }

    std::wstring cmd = argv[1];

    if (cmd == L"status") {
        ShellStatusInfo st = ShellRegistry::GetStatus();
        std::wstring ffmpeg = VideoRenderer::GetFFmpegPath();

        std::wcout << L"{"
                   << L"\"isDllRegistered\":" << (st.isDllRegistered ? L"true" : L"false") << L","
                   << L"\"registeredDllPath\":\"";
        PrintJsonEscaped(st.registeredDllPath);
        std::wcout << L"\","
                   << L"\"isPdfEnabled\":" << (st.isPdfEnabled ? L"true" : L"false") << L","
                   << L"\"isVideoEnabled\":" << (st.isVideoEnabled ? L"true" : L"false") << L","
                   << L"\"videoExtensionsEnabledCount\":" << st.videoExtensionsEnabledCount << L","
                   << L"\"totalVideoExtensionsCount\":" << st.totalVideoExtensionsCount << L","
                   << L"\"isAudioEnabled\":" << (st.isAudioEnabled ? L"true" : L"false") << L","
                   << L"\"audioExtensionsEnabledCount\":" << st.audioExtensionsEnabledCount << L","
                   << L"\"totalAudioExtensionsCount\":" << st.totalAudioExtensionsCount << L","
                   << L"\"isApkEnabled\":" << (st.isApkEnabled ? L"true" : L"false") << L","
                   << L"\"isCodeEnabled\":" << (st.isCodeEnabled ? L"true" : L"false") << L","
                   << L"\"codeExtensionsEnabledCount\":" << st.codeExtensionsEnabledCount << L","
                   << L"\"totalCodeExtensionsCount\":" << st.totalCodeExtensionsCount << L","
                   << L"\"ffmpegPath\":\"";
        PrintJsonEscaped(ffmpeg);
        std::wcout << L"\"}\n";
        return 0;
    }

    if (cmd == L"register") {
        bool enablePdf = true;
        bool enableVideo = true;
        bool enableAudio = true;
        bool enableApk = true;
        bool enableCode = true;
        std::wstring customDll;

        for (int i = 2; i < argc; ++i) {
            std::wstring arg = argv[i];
            if (arg == L"--pdf=0" || arg == L"--no-pdf") enablePdf = false;
            if (arg == L"--pdf=1" || arg == L"--pdf") enablePdf = true;
            if (arg == L"--video=0" || arg == L"--no-video") enableVideo = false;
            if (arg == L"--video=1" || arg == L"--video") enableVideo = true;
            if (arg == L"--audio=0" || arg == L"--no-audio") enableAudio = false;
            if (arg == L"--audio=1" || arg == L"--audio") enableAudio = true;
            if (arg == L"--apk=0" || arg == L"--no-apk") enableApk = false;
            if (arg == L"--apk=1" || arg == L"--apk") enableApk = true;
            if (arg == L"--code=0" || arg == L"--no-code") enableCode = false;
            if (arg == L"--code=1" || arg == L"--code") enableCode = true;
            if (arg.rfind(L"--dll=", 0) == 0) customDll = arg.substr(6);
        }

        bool ok = ShellRegistry::Register(enablePdf, enableVideo, enableAudio, enableApk, enableCode, customDll);
        std::wcout << L"{\"success\":" << (ok ? L"true" : L"false") << L"}\n";
        return ok ? 0 : 1;
    }

    if (cmd == L"unregister" || cmd == L"reset") {
        bool ok = ShellRegistry::Unregister();
        std::wcout << L"{\"success\":" << (ok ? L"true" : L"false") << L"}\n";
        return ok ? 0 : 1;
    }

    if (cmd == L"refresh") {
        bool ok = ShellRegistry::RefreshExplorer();
        std::wcout << L"{\"success\":" << (ok ? L"true" : L"false") << L"}\n";
        return ok ? 0 : 1;
    }

    if (cmd == L"set-ffmpeg" && argc >= 3) {
        std::wstring path = argv[2];
        bool ok = ShellRegistry::SetFFmpegPath(path);
        std::wcout << L"{\"success\":" << (ok ? L"true" : L"false") << L"}\n";
        return ok ? 0 : 1;
    }

    if (cmd == L"sample") {
        if (argc < 5) {
            std::wcout << L"{\"error\":\"Usage: sample <type> <cx> <outputPath>\"}\n";
            return 1;
        }

        std::wstring type = argv[2];
        UINT cx = (UINT)_wtoi(argv[3]);
        if (cx < 32) cx = 256;
        std::wstring outPath = argv[4];

        if (type == L"pdf_normal") {
            PdfThumbnailResult res = PdfRenderer::RenderSample(0, cx);
            bool ok = SaveHBitmapToPng(res.hBitmap, outPath);
            if (res.hBitmap) DeleteObject(res.hBitmap);
            std::wcout << L"{\"success\":" << (ok ? L"true" : L"false") << L",\"width\":" << res.width << L",\"height\":" << res.height << L",\"type\":\"pdf_normal\"}\n";
            return ok ? 0 : 1;
        } else if (type == L"pdf_locked") {
            PdfThumbnailResult res = PdfRenderer::RenderSample(1, cx);
            bool ok = SaveHBitmapToPng(res.hBitmap, outPath);
            if (res.hBitmap) DeleteObject(res.hBitmap);
            std::wcout << L"{\"success\":" << (ok ? L"true" : L"false") << L",\"width\":" << res.width << L",\"height\":" << res.height << L",\"type\":\"pdf_locked\"}\n";
            return ok ? 0 : 1;
        } else if (type == L"pdf_corrupt") {
            PdfThumbnailResult res = PdfRenderer::RenderSample(2, cx);
            bool ok = SaveHBitmapToPng(res.hBitmap, outPath);
            if (res.hBitmap) DeleteObject(res.hBitmap);
            std::wcout << L"{\"success\":" << (ok ? L"true" : L"false") << L",\"width\":" << res.width << L",\"height\":" << res.height << L",\"type\":\"pdf_corrupt\"}\n";
            return ok ? 0 : 1;
        } else if (type == L"video_landscape") {
            VideoThumbnailResult res = VideoRenderer::RenderSample(0, cx);
            bool ok = SaveHBitmapToPng(res.hBitmap, outPath);
            if (res.hBitmap) DeleteObject(res.hBitmap);
            std::wcout << L"{\"success\":" << (ok ? L"true" : L"false") << L",\"width\":" << res.width << L",\"height\":" << res.height << L",\"aspect\":\"landscape\"}\n";
            return ok ? 0 : 1;
        } else if (type == L"video_portrait") {
            VideoThumbnailResult res = VideoRenderer::RenderSample(1, cx);
            bool ok = SaveHBitmapToPng(res.hBitmap, outPath);
            if (res.hBitmap) DeleteObject(res.hBitmap);
            std::wcout << L"{\"success\":" << (ok ? L"true" : L"false") << L",\"width\":" << res.width << L",\"height\":" << res.height << L",\"aspect\":\"portrait\"}\n";
            return ok ? 0 : 1;
        } else if (type == L"video_square") {
            VideoThumbnailResult res = VideoRenderer::RenderSample(2, cx);
            bool ok = SaveHBitmapToPng(res.hBitmap, outPath);
            if (res.hBitmap) DeleteObject(res.hBitmap);
            std::wcout << L"{\"success\":" << (ok ? L"true" : L"false") << L",\"width\":" << res.width << L",\"height\":" << res.height << L",\"aspect\":\"square\"}\n";
            return ok ? 0 : 1;
        } else if (type == L"audio_cover") {
            AudioThumbnailResult res = AudioRenderer::RenderSample(true, cx);
            bool ok = SaveHBitmapToPng(res.hBitmap, outPath);
            if (res.hBitmap) DeleteObject(res.hBitmap);
            std::wcout << L"{\"success\":" << (ok ? L"true" : L"false") << L",\"width\":" << res.width << L",\"height\":" << res.height << L",\"type\":\"audio_cover\"}\n";
            return ok ? 0 : 1;
        } else if (type == L"audio_nocover") {
            AudioThumbnailResult res = AudioRenderer::RenderSample(false, cx);
            bool ok = SaveHBitmapToPng(res.hBitmap, outPath);
            if (res.hBitmap) DeleteObject(res.hBitmap);
            std::wcout << L"{\"success\":" << (ok ? L"true" : L"false") << L",\"width\":" << res.width << L",\"height\":" << res.height << L",\"type\":\"audio_nocover\"}\n";
            return ok ? 0 : 1;
        } else if (type == L"apk_sample") {
            ApkThumbnailResult res = ApkRenderer::RenderSample(cx);
            bool ok = SaveHBitmapToPng(res.hBitmap, outPath);
            if (res.hBitmap) DeleteObject(res.hBitmap);
            std::wcout << L"{\"success\":" << (ok ? L"true" : L"false") << L",\"width\":" << res.width << L",\"height\":" << res.height << L",\"type\":\"apk\"}\n";
            return ok ? 0 : 1;
        } else if (type == L"code_json") {
            CodeThumbnailResult res = CodeRenderer::RenderSample(L".json", cx);
            bool ok = SaveHBitmapToPng(res.hBitmap, outPath);
            if (res.hBitmap) DeleteObject(res.hBitmap);
            std::wcout << L"{\"success\":" << (ok ? L"true" : L"false") << L",\"width\":" << res.width << L",\"height\":" << res.height << L",\"type\":\"code_json\"}\n";
            return ok ? 0 : 1;
        } else if (type == L"code_markdown") {
            CodeThumbnailResult res = CodeRenderer::RenderSample(L".md", cx);
            bool ok = SaveHBitmapToPng(res.hBitmap, outPath);
            if (res.hBitmap) DeleteObject(res.hBitmap);
            std::wcout << L"{\"success\":" << (ok ? L"true" : L"false") << L",\"width\":" << res.width << L",\"height\":" << res.height << L",\"type\":\"code_markdown\"}\n";
            return ok ? 0 : 1;
        } else if (type == L"code_html") {
            CodeThumbnailResult res = CodeRenderer::RenderSample(L".html", cx);
            bool ok = SaveHBitmapToPng(res.hBitmap, outPath);
            if (res.hBitmap) DeleteObject(res.hBitmap);
            std::wcout << L"{\"success\":" << (ok ? L"true" : L"false") << L",\"width\":" << res.width << L",\"height\":" << res.height << L",\"type\":\"code_html\"}\n";
            return ok ? 0 : 1;
        } else if (type == L"code_js") {
            CodeThumbnailResult res = CodeRenderer::RenderSample(L".js", cx);
            bool ok = SaveHBitmapToPng(res.hBitmap, outPath);
            if (res.hBitmap) DeleteObject(res.hBitmap);
            std::wcout << L"{\"success\":" << (ok ? L"true" : L"false") << L",\"width\":" << res.width << L",\"height\":" << res.height << L",\"type\":\"code_js\"}\n";
            return ok ? 0 : 1;
        } else if (type == L"code_css") {
            CodeThumbnailResult res = CodeRenderer::RenderSample(L".css", cx);
            bool ok = SaveHBitmapToPng(res.hBitmap, outPath);
            if (res.hBitmap) DeleteObject(res.hBitmap);
            std::wcout << L"{\"success\":" << (ok ? L"true" : L"false") << L",\"width\":" << res.width << L",\"height\":" << res.height << L",\"type\":\"code_css\"}\n";
            return ok ? 0 : 1;
        }

        std::wcout << L"{\"error\":\"Unknown sample type\"}\n";
        return 1;
    }

    if (cmd == L"render") {
        if (argc < 5) {
            std::wcout << L"{\"error\":\"Usage: render <filePath> <cx> <outputPath>\"}\n";
            return 1;
        }

        std::wstring filePath = argv[2];
        UINT cx = (UINT)_wtoi(argv[3]);
        if (cx < 32) cx = 256;
        std::wstring outPath = argv[4];

        std::wstring ext = PathFindExtensionW(filePath.c_str());
        std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);

        if (ext == L".pdf") {
            PdfThumbnailResult res = PdfRenderer::RenderThumbnail(filePath, cx);
            if (res.hBitmap) {
                bool ok = SaveHBitmapToPng(res.hBitmap, outPath);
                DeleteObject(res.hBitmap);
                const wchar_t* statusStr = L"success";
                if (res.status == PdfRenderStatus::PasswordProtected) statusStr = L"password_protected";
                if (res.status == PdfRenderStatus::Corrupted) statusStr = L"corrupted";
                std::wcout << L"{\"success\":" << (ok ? L"true" : L"false")
                           << L",\"status\":\"" << statusStr << L"\""
                           << L",\"width\":" << res.width
                           << L",\"height\":" << res.height
                           << L",\"type\":\"pdf\"}\n";
                return ok ? 0 : 1;
            }
            std::wcout << L"{\"success\":false,\"error\":\"PDF thumbnail generation failed\"}\n";
            return 1;
        }

        if (VideoRenderer::IsSupportedExtension(ext)) {
            VideoThumbnailResult res = VideoRenderer::RenderThumbnail(filePath, cx);
            if (res.success && res.hBitmap) {
                bool ok = SaveHBitmapToPng(res.hBitmap, outPath);
                DeleteObject(res.hBitmap);
                const wchar_t* aspectStr = L"landscape";
                if (res.aspectRatio == VideoAspectRatio::Portrait) aspectStr = L"portrait";
                if (res.aspectRatio == VideoAspectRatio::Square) aspectStr = L"square";
                std::wcout << L"{\"success\":" << (ok ? L"true" : L"false")
                           << L",\"aspect\":\"" << aspectStr << L"\""
                           << L",\"width\":" << res.width
                           << L",\"height\":" << res.height
                           << L",\"type\":\"video\"}\n";
                return ok ? 0 : 1;
            }
            std::wcout << L"{\"success\":false,\"error\":\"Video thumbnail generation failed\"}\n";
            return 1;
        }

        if (AudioRenderer::IsSupportedExtension(ext)) {
            AudioThumbnailResult res = AudioRenderer::RenderThumbnail(filePath, cx);
            if (res.success && res.hBitmap) {
                bool ok = SaveHBitmapToPng(res.hBitmap, outPath);
                DeleteObject(res.hBitmap);
                std::wcout << L"{\"success\":" << (ok ? L"true" : L"false")
                           << L",\"hasCoverArt\":" << (res.hasCoverArt ? L"true" : L"false")
                           << L",\"width\":" << res.width
                           << L",\"height\":" << res.height
                           << L",\"type\":\"audio\"}\n";
                return ok ? 0 : 1;
            }
            std::wcout << L"{\"success\":false,\"error\":\"Audio thumbnail generation failed\"}\n";
            return 1;
        }

        if (ApkRenderer::IsSupportedExtension(ext)) {
            ApkThumbnailResult res = ApkRenderer::RenderThumbnail(filePath, cx);
            if (res.success && res.hBitmap) {
                bool ok = SaveHBitmapToPng(res.hBitmap, outPath);
                DeleteObject(res.hBitmap);
                std::wcout << L"{\"success\":" << (ok ? L"true" : L"false")
                           << L",\"width\":" << res.width
                           << L",\"height\":" << res.height
                           << L",\"type\":\"apk\"}\n";
                return ok ? 0 : 1;
            }
            std::wcout << L"{\"success\":false,\"error\":\"APK thumbnail generation failed\"}\n";
            return 1;
        }

        if (CodeRenderer::IsSupportedExtension(ext)) {
            CodeThumbnailResult res = CodeRenderer::RenderThumbnail(filePath, cx);
            if (res.success && res.hBitmap) {
                bool ok = SaveHBitmapToPng(res.hBitmap, outPath);
                DeleteObject(res.hBitmap);
                std::wcout << L"{\"success\":" << (ok ? L"true" : L"false")
                           << L",\"width\":" << res.width
                           << L",\"height\":" << res.height
                           << L",\"type\":\"code\"}\n";
                return ok ? 0 : 1;
            }
            std::wcout << L"{\"success\":false,\"error\":\"Code thumbnail generation failed\"}\n";
            return 1;
        }

        std::wcout << L"{\"success\":false,\"error\":\"Unsupported file extension\"}\n";
        return 1;
    }

    std::wcout << L"{\"error\":\"Unknown command\"}\n";
    return 1;
}
