#include "Common.h"
#include "ClassFactory.h"
#include "ShellRegistry.h"
#include "PdfRenderer.h"
#include <olectl.h>

HMODULE g_hModule = nullptr;
std::atomic<long> g_dllRefCount{0};
static std::unique_ptr<GdiplusScope> g_gdiplusScope;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID /*lpReserved*/) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
        g_gdiplusScope = std::make_unique<GdiplusScope>();
        break;
    case DLL_PROCESS_DETACH:
        PdfRenderer::Shutdown();
        g_gdiplusScope.reset();
        break;
    }
    return TRUE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void **ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;

    if (IsEqualCLSID(rclsid, CLSID_ThumbForgePdfThumbnailProvider) ||
        IsEqualCLSID(rclsid, CLSID_ThumbForgeVideoThumbnailProvider) ||
        IsEqualCLSID(rclsid, CLSID_ThumbForgeAudioThumbnailProvider) ||
        IsEqualCLSID(rclsid, CLSID_ThumbForgeApkThumbnailProvider) ||
        IsEqualCLSID(rclsid, CLSID_ThumbForgeCodeThumbnailProvider)) {
        auto factory = new (std::nothrow) ClassFactory(rclsid);
        if (!factory) return E_OUTOFMEMORY;

        HRESULT hr = factory->QueryInterface(riid, ppv);
        factory->Release();
        return hr;
    }

    return CLASS_E_CLASSNOTAVAILABLE;
}

STDAPI DllCanUnloadNow() {
    return (g_dllRefCount == 0) ? S_OK : S_FALSE;
}

STDAPI DllRegisterServer() {
    // Default: enable both PDF and Video
    bool success = ShellRegistry::Register(true, true);
    return success ? S_OK : SELFREG_E_CLASS;
}

STDAPI DllUnregisterServer() {
    bool success = ShellRegistry::Unregister();
    return success ? S_OK : SELFREG_E_CLASS;
}
