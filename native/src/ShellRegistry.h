#pragma once

#include "Common.h"

struct ShellStatusInfo {
    bool isDllRegistered = false;
    std::wstring registeredDllPath;
    bool isPdfEnabled = false;
    bool isVideoEnabled = false;
    bool isAudioEnabled = false;
    bool isApkEnabled = false;
    bool isCodeEnabled = false;
    int videoExtensionsEnabledCount = 0;
    int totalVideoExtensionsCount = 0;
    int audioExtensionsEnabledCount = 0;
    int totalAudioExtensionsCount = 0;
    int codeExtensionsEnabledCount = 0;
    int totalCodeExtensionsCount = 0;
};

class ShellRegistry {
public:
    // Register COM CLSIDs and associate extensions
    static bool Register(bool enablePdf, bool enableVideo, bool enableAudio = true, bool enableApk = true, bool enableCode = true, const std::wstring& customDllPath = L"");

    // Reset/Unregister: remove custom thumbnail associations, restore defaults, clean up
    static bool Unregister();

    // Query current status
    static ShellStatusInfo GetStatus();

    // Clear Windows Explorer thumbnail cache and broadcast changes
    static bool RefreshExplorer();

    // Store custom FFmpeg path
    static bool SetFFmpegPath(const std::wstring& path);
    static std::wstring GetFFmpegPath();

private:
    static bool BackupExtension(const std::wstring& ext);
    static bool RestoreExtension(const std::wstring& ext);
    static bool SetExtensionHandler(const std::wstring& ext, const std::wstring& clsid);
    static bool RemoveExtensionHandler(const std::wstring& ext);
    static std::wstring GetModuleDllPath();
};
