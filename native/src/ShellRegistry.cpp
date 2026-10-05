#include "ShellRegistry.h"
#include <shlobj.h>
#include <shlwapi.h>
#include <tlhelp32.h>
#include <iostream>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")

extern HMODULE g_hModule;

static const wchar_t* kThumbnailProviderGuid = L"{e357fccd-a995-4576-b01f-234630154e96}";
static const wchar_t* kBackupRoot = L"Software\\ThumbForge\\Backup";
static const wchar_t* kSettingsRoot = L"Software\\ThumbForge";

std::wstring ShellRegistry::GetModuleDllPath() {
    wchar_t buf[MAX_PATH] = {0};
    if (g_hModule) {
        GetModuleFileNameW(g_hModule, buf, MAX_PATH);
        return buf;
    }
    // Fall back to module path next to current executable
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    PathRemoveFileSpecW(buf);
    PathAppendW(buf, L"ThumbForgeProvider.dll");
    return buf;
}

static bool IsOurClsid(const wchar_t* val) {
    if (!val) return false;
    return (_wcsicmp(val, kPdfClsidString) == 0 ||
            _wcsicmp(val, kVideoClsidString) == 0 ||
            _wcsicmp(val, kAudioClsidString) == 0 ||
            _wcsicmp(val, kApkClsidString) == 0 ||
            _wcsicmp(val, kCodeClsidString) == 0 ||
            _wcsicmp(val, kHtmlClsidString) == 0 ||
            _wcsicmp(val, kEpubClsidString) == 0);
}

bool ShellRegistry::BackupExtension(const std::wstring& ext) {
    // Read current value under HKCU\Software\Classes\<ext>\ShellEx\{e357fccd...}
    std::wstring subKey = L"Software\\Classes\\" + ext + L"\\ShellEx\\" + kThumbnailProviderGuid;
    HKEY hKey = nullptr;
    wchar_t prevVal[256] = {0};
    DWORD prevSize = sizeof(prevVal);
    bool hadVal = false;

    if (RegOpenKeyExW(HKEY_CURRENT_USER, subKey.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, nullptr, nullptr, nullptr, (LPBYTE)prevVal, &prevSize) == ERROR_SUCCESS) {
            hadVal = true;
        }
        RegCloseKey(hKey);
    }

    // Save into backup key
    HKEY hBackupKey = nullptr;
    std::wstring backupSubKey = std::wstring(kBackupRoot) + L"\\" + ext;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, backupSubKey.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &hBackupKey, nullptr) == ERROR_SUCCESS) {
        if (hadVal && !IsOurClsid(prevVal)) {
            RegSetValueExW(hBackupKey, L"PrevClsid", 0, REG_SZ, (const BYTE*)prevVal, (DWORD)(wcslen(prevVal) + 1) * sizeof(wchar_t));
            DWORD exists = 1;
            RegSetValueExW(hBackupKey, L"Existed", 0, REG_DWORD, (const BYTE*)&exists, sizeof(DWORD));
        } else if (!hadVal) {
            DWORD exists = 0;
            RegSetValueExW(hBackupKey, L"Existed", 0, REG_DWORD, (const BYTE*)&exists, sizeof(DWORD));
        }
        RegCloseKey(hBackupKey);
    }
    return true;
}

bool ShellRegistry::RestoreExtension(const std::wstring& ext) {
    std::wstring backupSubKey = std::wstring(kBackupRoot) + L"\\" + ext;
    HKEY hBackupKey = nullptr;
    DWORD existed = 0;
    DWORD existedSize = sizeof(existed);
    wchar_t prevClsid[256] = {0};
    DWORD prevSize = sizeof(prevClsid);

    bool hasBackup = false;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, backupSubKey.c_str(), 0, KEY_READ, &hBackupKey) == ERROR_SUCCESS) {
        RegQueryValueExW(hBackupKey, L"Existed", nullptr, nullptr, (LPBYTE)&existed, &existedSize);
        if (existed == 1) {
            RegQueryValueExW(hBackupKey, L"PrevClsid", nullptr, nullptr, (LPBYTE)prevClsid, &prevSize);
        }
        RegCloseKey(hBackupKey);
        hasBackup = true;
    }

    if (hasBackup && existed == 1 && wcslen(prevClsid) > 0) {
        // Restore original
        SetExtensionHandler(ext, prevClsid);
    } else {
        // Remove our override
        RemoveExtensionHandler(ext);
    }

    // Delete backup subkey
    RegDeleteKeyW(HKEY_CURRENT_USER, backupSubKey.c_str());
    return true;
}

static std::wstring GetProgIdForExtension(const std::wstring& ext) {
    // 1. Check UserChoice
    std::wstring subKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\" + ext + L"\\UserChoice";
    HKEY hKey = nullptr;
    wchar_t progId[256] = {0};
    DWORD size = sizeof(progId);
    if (RegOpenKeyExW(HKEY_CURRENT_USER, subKey.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, L"ProgId", nullptr, nullptr, (LPBYTE)progId, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            if (wcslen(progId) > 0) return progId;
        }
        RegCloseKey(hKey);
    }

    // 2. Check HKCU\Software\Classes\<ext> (Default)
    std::wstring hkcuExt = L"Software\\Classes\\" + ext;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, hkcuExt.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        size = sizeof(progId);
        if (RegQueryValueExW(hKey, nullptr, nullptr, nullptr, (LPBYTE)progId, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            if (wcslen(progId) > 0) return progId;
        }
        RegCloseKey(hKey);
    }

    // 3. Check HKCR\<ext> (Default)
    if (RegOpenKeyExW(HKEY_CLASSES_ROOT, ext.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        size = sizeof(progId);
        if (RegQueryValueExW(hKey, nullptr, nullptr, nullptr, (LPBYTE)progId, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            if (wcslen(progId) > 0) return progId;
        }
        RegCloseKey(hKey);
    }

    return L"";
}

static bool IsAppXProgId(const std::wstring& progId) {
    return (_wcsnicmp(progId.c_str(), L"AppX", 4) == 0);
}

static void CleanAppXRegistrations() {
    HKEY hClasses = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Classes", 0, KEY_READ | KEY_WRITE, &hClasses) == ERROR_SUCCESS) {
        wchar_t keyName[256];
        DWORD index = 0;
        DWORD nameLen = sizeof(keyName) / sizeof(keyName[0]);
        std::vector<std::wstring> toClean;

        while (RegEnumKeyExW(hClasses, index++, keyName, &nameLen, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
            nameLen = sizeof(keyName) / sizeof(keyName[0]);
            if (_wcsnicmp(keyName, L"AppX", 4) == 0) {
                std::wstring shellExKey = std::wstring(keyName) + L"\\ShellEx\\" + kThumbnailProviderGuid;
                HKEY hSub = nullptr;
                if (RegOpenKeyExW(hClasses, shellExKey.c_str(), 0, KEY_READ, &hSub) == ERROR_SUCCESS) {
                    wchar_t val[256] = {0};
                    DWORD valSize = sizeof(val);
                    if (RegQueryValueExW(hSub, nullptr, nullptr, nullptr, (LPBYTE)val, &valSize) == ERROR_SUCCESS) {
                        if (IsOurClsid(val)) {
                            toClean.push_back(keyName);
                        }
                    }
                    RegCloseKey(hSub);
                }
            }
        }

        for (const auto& appX : toClean) {
            std::wstring guidKey = appX + L"\\ShellEx\\" + kThumbnailProviderGuid;
            RegDeleteKeyW(hClasses, guidKey.c_str());
            std::wstring shellExKey = appX + L"\\ShellEx";
            RegDeleteKeyW(hClasses, shellExKey.c_str());
        }

        RegCloseKey(hClasses);
    }
}

bool ShellRegistry::SetExtensionHandler(const std::wstring& ext, const std::wstring& clsid) {
    // 1. HKCU\Software\Classes\<ext>\ShellEx\{e357fccd-a995-4576-b01f-234630154e96}
    std::wstring key1 = L"Software\\Classes\\" + ext + L"\\ShellEx\\" + kThumbnailProviderGuid;
    HKEY hKey1 = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, key1.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &hKey1, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(hKey1, nullptr, 0, REG_SZ, (const BYTE*)clsid.c_str(), (DWORD)(clsid.length() + 1) * sizeof(wchar_t));
        RegCloseKey(hKey1);
    }

    // 2. HKCU\Software\Classes\SystemFileAssociations\<ext>\ShellEx\{e357fccd-a995-4576-b01f-234630154e96}
    std::wstring key2 = L"Software\\Classes\\SystemFileAssociations\\" + ext + L"\\ShellEx\\" + kThumbnailProviderGuid;
    HKEY hKey2 = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, key2.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &hKey2, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(hKey2, nullptr, 0, REG_SZ, (const BYTE*)clsid.c_str(), (DWORD)(clsid.length() + 1) * sizeof(wchar_t));
        RegCloseKey(hKey2);
    }

    // 3. Register on resolved ProgId if not an AppX shared package
    std::wstring progId = GetProgIdForExtension(ext);
    if (!progId.empty() && !IsAppXProgId(progId)) {
        std::wstring keyProg = L"Software\\Classes\\" + progId + L"\\ShellEx\\" + kThumbnailProviderGuid;
        HKEY hKeyProg = nullptr;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, keyProg.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &hKeyProg, nullptr) == ERROR_SUCCESS) {
            RegSetValueExW(hKeyProg, nullptr, 0, REG_SZ, (const BYTE*)clsid.c_str(), (DWORD)(clsid.length() + 1) * sizeof(wchar_t));
            RegCloseKey(hKeyProg);
        }
    }

    return true;
}

bool ShellRegistry::RemoveExtensionHandler(const std::wstring& ext) {
    // 1. Remove HKCU\Software\Classes\<ext>\ShellEx\{e357fccd-a995-4576-b01f-234630154e96}
    std::wstring key1 = L"Software\\Classes\\" + ext + L"\\ShellEx\\" + kThumbnailProviderGuid;
    RegDeleteKeyW(HKEY_CURRENT_USER, key1.c_str());

    // 2. Remove HKCU\Software\Classes\SystemFileAssociations\<ext>\ShellEx\{e357fccd-a995-4576-b01f-234630154e96}
    std::wstring key2 = L"Software\\Classes\\SystemFileAssociations\\" + ext + L"\\ShellEx\\" + kThumbnailProviderGuid;
    RegDeleteKeyW(HKEY_CURRENT_USER, key2.c_str());

    // 3. Remove ProgID ShellEx if applicable
    std::wstring progId = GetProgIdForExtension(ext);
    if (!progId.empty() && !IsAppXProgId(progId)) {
        std::wstring keyProg = L"Software\\Classes\\" + progId + L"\\ShellEx\\" + kThumbnailProviderGuid;
        RegDeleteKeyW(HKEY_CURRENT_USER, keyProg.c_str());
    }

    return true;
}

static bool RegisterClsid(const std::wstring& clsidStr, const std::wstring& desc, const std::wstring& dllPath) {
    std::wstring clsidKey = L"Software\\Classes\\CLSID\\" + clsidStr;
    HKEY hKey = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, clsidKey.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, nullptr, 0, REG_SZ, (const BYTE*)desc.c_str(), (DWORD)(desc.length() + 1) * sizeof(wchar_t));
        RegCloseKey(hKey);
    } else {
        return false;
    }

    std::wstring inprocKey = clsidKey + L"\\InprocServer32";
    if (RegCreateKeyExW(HKEY_CURRENT_USER, inprocKey.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, nullptr, 0, REG_SZ, (const BYTE*)dllPath.c_str(), (DWORD)(dllPath.length() + 1) * sizeof(wchar_t));
        const wchar_t* threading = L"Apartment";
        RegSetValueExW(hKey, L"ThreadingModel", 0, REG_SZ, (const BYTE*)threading, (DWORD)(wcslen(threading) + 1) * sizeof(wchar_t));
        DWORD disableIsolation = 1;
        RegSetValueExW(hKey, L"DisableProcessIsolation", 0, REG_DWORD, (const BYTE*)&disableIsolation, sizeof(DWORD));
        RegCloseKey(hKey);
    } else {
        return false;
    }
    return true;
}

bool ShellRegistry::Register(bool enablePdf, bool enableVideo, bool enableAudio, bool enableApk, bool enableCode, bool enableHtml, bool enableEpub, const std::wstring& customDllPath) {
    std::wstring dllPath = customDllPath.empty() ? GetModuleDllPath() : customDllPath;

    // Clean up rogue registrations
    CleanAppXRegistrations();
    RestoreExtension(L".ts");

    // 1. Register CLSIDs
    RegisterClsid(kPdfClsidString, L"ThumbForge PDF Thumbnail Provider", dllPath);
    RegisterClsid(kVideoClsidString, L"ThumbForge Video Thumbnail Provider", dllPath);
    RegisterClsid(kAudioClsidString, L"ThumbForge Audio Thumbnail Provider", dllPath);
    RegisterClsid(kApkClsidString, L"ThumbForge APK Thumbnail Provider", dllPath);
    RegisterClsid(kCodeClsidString, L"ThumbForge Code Thumbnail Provider", dllPath);
    RegisterClsid(kHtmlClsidString, L"ThumbForge HTML Thumbnail Provider", dllPath);
    RegisterClsid(kEpubClsidString, L"ThumbForge EPUB Thumbnail Provider", dllPath);

    // 2. Handle PDF
    if (enablePdf) {
        BackupExtension(L".pdf");
        SetExtensionHandler(L".pdf", kPdfClsidString);
    } else {
        RestoreExtension(L".pdf");
    }

    // 3. Handle Video extensions
    for (const auto& ext : kSupportedVideoExtensions) {
        if (enableVideo) {
            BackupExtension(ext);
            SetExtensionHandler(ext, kVideoClsidString);
        } else {
            RestoreExtension(ext);
        }
    }
    if (enableVideo) {
        std::wstring genVideoKey = L"Software\\Classes\\SystemFileAssociations\\video\\ShellEx\\" + std::wstring(kThumbnailProviderGuid);
        HKEY hGenKey = nullptr;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, genVideoKey.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &hGenKey, nullptr) == ERROR_SUCCESS) {
            RegSetValueExW(hGenKey, nullptr, 0, REG_SZ, (const BYTE*)kVideoClsidString, (DWORD)(wcslen(kVideoClsidString) + 1) * sizeof(wchar_t));
            RegCloseKey(hGenKey);
        }
    } else {
        std::wstring genVideoKey = L"Software\\Classes\\SystemFileAssociations\\video\\ShellEx\\" + std::wstring(kThumbnailProviderGuid);
        RegDeleteKeyW(HKEY_CURRENT_USER, genVideoKey.c_str());
    }

    // 4. Handle Audio extensions
    for (const auto& ext : kSupportedAudioExtensions) {
        if (enableAudio) {
            BackupExtension(ext);
            SetExtensionHandler(ext, kAudioClsidString);
        } else {
            RestoreExtension(ext);
        }
    }
    if (enableAudio) {
        std::wstring genAudioKey = L"Software\\Classes\\SystemFileAssociations\\audio\\ShellEx\\" + std::wstring(kThumbnailProviderGuid);
        HKEY hGenKey = nullptr;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, genAudioKey.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &hGenKey, nullptr) == ERROR_SUCCESS) {
            RegSetValueExW(hGenKey, nullptr, 0, REG_SZ, (const BYTE*)kAudioClsidString, (DWORD)(wcslen(kAudioClsidString) + 1) * sizeof(wchar_t));
            RegCloseKey(hGenKey);
        }
    } else {
        std::wstring genAudioKey = L"Software\\Classes\\SystemFileAssociations\\audio\\ShellEx\\" + std::wstring(kThumbnailProviderGuid);
        RegDeleteKeyW(HKEY_CURRENT_USER, genAudioKey.c_str());
    }

    // 5. Handle APK extensions
    for (const auto& ext : kSupportedApkExtensions) {
        if (enableApk) {
            BackupExtension(ext);
            SetExtensionHandler(ext, kApkClsidString);
        } else {
            RestoreExtension(ext);
        }
    }

    // 6. Handle Code / Markup extensions
    for (const auto& ext : kSupportedCodeExtensions) {
        if (enableCode) {
            BackupExtension(ext);
            SetExtensionHandler(ext, kCodeClsidString);
        } else {
            RestoreExtension(ext);
        }
    }

    // 7. Handle HTML extensions
    for (const auto& ext : kSupportedHtmlExtensions) {
        if (enableHtml) {
            BackupExtension(ext);
            SetExtensionHandler(ext, kHtmlClsidString);
        } else {
            RestoreExtension(ext);
        }
    }

    // 8. Handle EPUB extensions
    for (const auto& ext : kSupportedEpubExtensions) {
        if (enableEpub) {
            BackupExtension(ext);
            SetExtensionHandler(ext, kEpubClsidString);
        } else {
            RestoreExtension(ext);
        }
    }

    // 9. Save state & settings
    HKEY hSettings = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kSettingsRoot, 0, nullptr, 0, KEY_WRITE, nullptr, &hSettings, nullptr) == ERROR_SUCCESS) {
        DWORD pdfVal = enablePdf ? 1 : 0;
        DWORD vidVal = enableVideo ? 1 : 0;
        DWORD audVal = enableAudio ? 1 : 0;
        DWORD apkVal = enableApk ? 1 : 0;
        DWORD codVal = enableCode ? 1 : 0;
        DWORD htmVal = enableHtml ? 1 : 0;
        DWORD epbVal = enableEpub ? 1 : 0;
        RegSetValueExW(hSettings, L"PdfEnabled", 0, REG_DWORD, (const BYTE*)&pdfVal, sizeof(DWORD));
        RegSetValueExW(hSettings, L"VideoEnabled", 0, REG_DWORD, (const BYTE*)&vidVal, sizeof(DWORD));
        RegSetValueExW(hSettings, L"AudioEnabled", 0, REG_DWORD, (const BYTE*)&audVal, sizeof(DWORD));
        RegSetValueExW(hSettings, L"ApkEnabled", 0, REG_DWORD, (const BYTE*)&apkVal, sizeof(DWORD));
        RegSetValueExW(hSettings, L"CodeEnabled", 0, REG_DWORD, (const BYTE*)&codVal, sizeof(DWORD));
        RegSetValueExW(hSettings, L"HtmlEnabled", 0, REG_DWORD, (const BYTE*)&htmVal, sizeof(DWORD));
        RegSetValueExW(hSettings, L"EpubEnabled", 0, REG_DWORD, (const BYTE*)&epbVal, sizeof(DWORD));
        RegSetValueExW(hSettings, L"DllPath", 0, REG_SZ, (const BYTE*)dllPath.c_str(), (DWORD)(dllPath.length() + 1) * sizeof(wchar_t));

        std::wstring ffmpeg = GetFFmpegPath();
        RegSetValueExW(hSettings, L"FFmpegPath", 0, REG_SZ, (const BYTE*)ffmpeg.c_str(), (DWORD)(ffmpeg.length() + 1) * sizeof(wchar_t));
        RegCloseKey(hSettings);
    }

    // 8. Broadcast notification to Explorer
    RefreshExplorer();

    return true;
}

bool ShellRegistry::Unregister() {
    // 1. Clean AppX registrations and legacy extensions
    CleanAppXRegistrations();
    RestoreExtension(L".ts");

    // 2. Restore all extensions
    RestoreExtension(L".pdf");

    for (const auto& ext : kSupportedVideoExtensions) {
        RestoreExtension(ext);
    }
    std::wstring genVideoKey = L"Software\\Classes\\SystemFileAssociations\\video\\ShellEx\\" + std::wstring(kThumbnailProviderGuid);
    RegDeleteKeyW(HKEY_CURRENT_USER, genVideoKey.c_str());

    for (const auto& ext : kSupportedAudioExtensions) {
        RestoreExtension(ext);
    }
    std::wstring genAudioKey = L"Software\\Classes\\SystemFileAssociations\\audio\\ShellEx\\" + std::wstring(kThumbnailProviderGuid);
    RegDeleteKeyW(HKEY_CURRENT_USER, genAudioKey.c_str());

    for (const auto& ext : kSupportedApkExtensions) {
        RestoreExtension(ext);
    }

    for (const auto& ext : kSupportedCodeExtensions) {
        RestoreExtension(ext);
    }

    for (const auto& ext : kSupportedHtmlExtensions) {
        RestoreExtension(ext);
    }

    for (const auto& ext : kSupportedEpubExtensions) {
        RestoreExtension(ext);
    }

    // 2. Remove CLSIDs
    auto deleteClsid = [](const std::wstring& clsidStr) {
        std::wstring inprocKey = L"Software\\Classes\\CLSID\\" + clsidStr + L"\\InprocServer32";
        RegDeleteKeyW(HKEY_CURRENT_USER, inprocKey.c_str());
        std::wstring clsidKey = L"Software\\Classes\\CLSID\\" + clsidStr;
        RegDeleteKeyW(HKEY_CURRENT_USER, clsidKey.c_str());
    };
    deleteClsid(kPdfClsidString);
    deleteClsid(kVideoClsidString);
    deleteClsid(kAudioClsidString);
    deleteClsid(kApkClsidString);
    deleteClsid(kCodeClsidString);
    deleteClsid(kHtmlClsidString);
    deleteClsid(kEpubClsidString);

    // 3. Update settings
    HKEY hSettings = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kSettingsRoot, 0, nullptr, 0, KEY_WRITE, nullptr, &hSettings, nullptr) == ERROR_SUCCESS) {
        DWORD zero = 0;
        RegSetValueExW(hSettings, L"PdfEnabled", 0, REG_DWORD, (const BYTE*)&zero, sizeof(DWORD));
        RegSetValueExW(hSettings, L"VideoEnabled", 0, REG_DWORD, (const BYTE*)&zero, sizeof(DWORD));
        RegSetValueExW(hSettings, L"AudioEnabled", 0, REG_DWORD, (const BYTE*)&zero, sizeof(DWORD));
        RegSetValueExW(hSettings, L"ApkEnabled", 0, REG_DWORD, (const BYTE*)&zero, sizeof(DWORD));
        RegSetValueExW(hSettings, L"CodeEnabled", 0, REG_DWORD, (const BYTE*)&zero, sizeof(DWORD));
        RegSetValueExW(hSettings, L"HtmlEnabled", 0, REG_DWORD, (const BYTE*)&zero, sizeof(DWORD));
        RegSetValueExW(hSettings, L"EpubEnabled", 0, REG_DWORD, (const BYTE*)&zero, sizeof(DWORD));
        RegCloseKey(hSettings);
    }

    // 4. Refresh Explorer
    RefreshExplorer();

    return true;
}

ShellStatusInfo ShellRegistry::GetStatus() {
    ShellStatusInfo info;
    info.totalVideoExtensionsCount = (int)kSupportedVideoExtensions.size();
    info.totalAudioExtensionsCount = (int)kSupportedAudioExtensions.size();
    info.totalCodeExtensionsCount = (int)kSupportedCodeExtensions.size();

    // Check if PDF CLSID is registered
    std::wstring inprocKey = L"Software\\Classes\\CLSID\\" + std::wstring(kPdfClsidString) + L"\\InprocServer32";
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, inprocKey.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        info.isDllRegistered = true;
        wchar_t buf[MAX_PATH] = {0};
        DWORD bufSize = sizeof(buf);
        if (RegQueryValueExW(hKey, nullptr, nullptr, nullptr, (LPBYTE)buf, &bufSize) == ERROR_SUCCESS) {
            info.registeredDllPath = buf;
        }
        RegCloseKey(hKey);
    }

    // Check PDF
    std::wstring pdfKey = L"Software\\Classes\\.pdf\\ShellEx\\" + std::wstring(kThumbnailProviderGuid);
    if (RegOpenKeyExW(HKEY_CURRENT_USER, pdfKey.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t val[256] = {0};
        DWORD valSize = sizeof(val);
        if (RegQueryValueExW(hKey, nullptr, nullptr, nullptr, (LPBYTE)val, &valSize) == ERROR_SUCCESS) {
            if (_wcsicmp(val, kPdfClsidString) == 0) {
                info.isPdfEnabled = true;
            }
        }
        RegCloseKey(hKey);
    }

    // Check Video extensions
    int videoCount = 0;
    for (const auto& ext : kSupportedVideoExtensions) {
        std::wstring vKey = L"Software\\Classes\\" + ext + L"\\ShellEx\\" + std::wstring(kThumbnailProviderGuid);
        if (RegOpenKeyExW(HKEY_CURRENT_USER, vKey.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            wchar_t val[256] = {0};
            DWORD valSize = sizeof(val);
            if (RegQueryValueExW(hKey, nullptr, nullptr, nullptr, (LPBYTE)val, &valSize) == ERROR_SUCCESS) {
                if (_wcsicmp(val, kVideoClsidString) == 0) {
                    videoCount++;
                }
            }
            RegCloseKey(hKey);
        }
    }
    info.videoExtensionsEnabledCount = videoCount;
    info.isVideoEnabled = (videoCount > 0);

    // Check Audio extensions
    int audioCount = 0;
    for (const auto& ext : kSupportedAudioExtensions) {
        std::wstring aKey = L"Software\\Classes\\" + ext + L"\\ShellEx\\" + std::wstring(kThumbnailProviderGuid);
        if (RegOpenKeyExW(HKEY_CURRENT_USER, aKey.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            wchar_t val[256] = {0};
            DWORD valSize = sizeof(val);
            if (RegQueryValueExW(hKey, nullptr, nullptr, nullptr, (LPBYTE)val, &valSize) == ERROR_SUCCESS) {
                if (_wcsicmp(val, kAudioClsidString) == 0) {
                    audioCount++;
                }
            }
            RegCloseKey(hKey);
        }
    }
    info.audioExtensionsEnabledCount = audioCount;
    info.isAudioEnabled = (audioCount > 0);

    // Check APK
    std::wstring apkKey = L"Software\\Classes\\.apk\\ShellEx\\" + std::wstring(kThumbnailProviderGuid);
    if (RegOpenKeyExW(HKEY_CURRENT_USER, apkKey.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t val[256] = {0};
        DWORD valSize = sizeof(val);
        if (RegQueryValueExW(hKey, nullptr, nullptr, nullptr, (LPBYTE)val, &valSize) == ERROR_SUCCESS) {
            if (_wcsicmp(val, kApkClsidString) == 0) {
                info.isApkEnabled = true;
            }
        }
        RegCloseKey(hKey);
    }

    // Check Code extensions
    int codeCount = 0;
    for (const auto& ext : kSupportedCodeExtensions) {
        std::wstring cKey = L"Software\\Classes\\" + ext + L"\\ShellEx\\" + std::wstring(kThumbnailProviderGuid);
        if (RegOpenKeyExW(HKEY_CURRENT_USER, cKey.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            wchar_t val[256] = {0};
            DWORD valSize = sizeof(val);
            if (RegQueryValueExW(hKey, nullptr, nullptr, nullptr, (LPBYTE)val, &valSize) == ERROR_SUCCESS) {
                if (_wcsicmp(val, kCodeClsidString) == 0) {
                    codeCount++;
                }
            }
            RegCloseKey(hKey);
        }
    }
    info.codeExtensionsEnabledCount = codeCount;
    info.isCodeEnabled = (codeCount > 0);

    // Check HTML extensions
    int htmlCount = 0;
    for (const auto& ext : kSupportedHtmlExtensions) {
        std::wstring hKeyPath = L"Software\\Classes\\" + ext + L"\\ShellEx\\" + std::wstring(kThumbnailProviderGuid);
        if (RegOpenKeyExW(HKEY_CURRENT_USER, hKeyPath.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            wchar_t val[256] = {0};
            DWORD valSize = sizeof(val);
            if (RegQueryValueExW(hKey, nullptr, nullptr, nullptr, (LPBYTE)val, &valSize) == ERROR_SUCCESS) {
                if (_wcsicmp(val, kHtmlClsidString) == 0) {
                    htmlCount++;
                }
            }
            RegCloseKey(hKey);
        }
    }
    info.htmlExtensionsEnabledCount = htmlCount;
    info.isHtmlEnabled = (htmlCount > 0);
    info.totalHtmlExtensionsCount = (int)kSupportedHtmlExtensions.size();

    // Check EPUB extensions
    int epubCount = 0;
    for (const auto& ext : kSupportedEpubExtensions) {
        std::wstring eKeyPath = L"Software\\Classes\\" + ext + L"\\ShellEx\\" + std::wstring(kThumbnailProviderGuid);
        if (RegOpenKeyExW(HKEY_CURRENT_USER, eKeyPath.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            wchar_t val[256] = {0};
            DWORD valSize = sizeof(val);
            if (RegQueryValueExW(hKey, nullptr, nullptr, nullptr, (LPBYTE)val, &valSize) == ERROR_SUCCESS) {
                if (_wcsicmp(val, kEpubClsidString) == 0) {
                    epubCount++;
                }
            }
            RegCloseKey(hKey);
        }
    }
    info.epubExtensionsEnabledCount = epubCount;
    info.isEpubEnabled = (epubCount > 0);
    info.totalEpubExtensionsCount = (int)kSupportedEpubExtensions.size();

    return info;
}

bool ShellRegistry::RefreshExplorer() {
    // 0. Terminate dllhost (COM Surrogate) so Windows drops any cached provider DLL handles
    {
        STARTUPINFOW si = { sizeof(si) };
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION pi = { 0 };
        wchar_t cmd[] = L"taskkill.exe /f /im dllhost.exe";
        if (CreateProcessW(nullptr, cmd, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, 1000);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }

    // 1. Tell Windows Shell that file associations / thumbnail handlers changed
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

    // 2. Broadcast WM_SETTINGCHANGE
    DWORD_PTR result = 0;
    SendMessageTimeoutW(
        HWND_BROADCAST,
        WM_SETTINGCHANGE,
        0,
        (LPARAM)L"Environment",
        SMTO_ABORTIFHUNG,
        1000,
        &result
    );

    // 3. Clear thumbnail cache files in %LOCALAPPDATA%\Microsoft\Windows\Explorer\thumbcache_*.db
    wchar_t localAppData[MAX_PATH] = {0};
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH) > 0) {
        std::wstring cacheDir = std::wstring(localAppData) + L"\\Microsoft\\Windows\\Explorer";
        std::wstring searchPattern = cacheDir + L"\\thumbcache_*.db";

        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileW(searchPattern.c_str(), &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                std::wstring file = cacheDir + L"\\" + fd.cFileName;
                DeleteFileW(file.c_str());
            } while (FindNextFileW(hFind, &fd));
            FindClose(hFind);
        }
    }

    return true;
}

bool ShellRegistry::SetFFmpegPath(const std::wstring& path) {
    HKEY hSettings = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kSettingsRoot, 0, nullptr, 0, KEY_WRITE, nullptr, &hSettings, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(hSettings, L"FFmpegPath", 0, REG_SZ, (const BYTE*)path.c_str(), (DWORD)(path.length() + 1) * sizeof(wchar_t));
        RegCloseKey(hSettings);
        return true;
    }
    return false;
}

std::wstring ShellRegistry::GetFFmpegPath() {
    HKEY hSettings = nullptr;
    wchar_t buf[MAX_PATH] = {0};
    DWORD bufSize = sizeof(buf);
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kSettingsRoot, 0, KEY_READ, &hSettings) == ERROR_SUCCESS) {
        RegQueryValueExW(hSettings, L"FFmpegPath", nullptr, nullptr, (LPBYTE)buf, &bufSize);
        RegCloseKey(hSettings);
    }
    if (wcslen(buf) > 0) return buf;
    return L"C:\\ffmpeg\\bin\\ffmpeg.exe";
}
