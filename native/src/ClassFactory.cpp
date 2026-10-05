#include "ClassFactory.h"
#include "ThumbnailProvider.h"
#include <shlwapi.h>

extern std::atomic<long> g_dllRefCount;

ClassFactory::ClassFactory(CLSID clsid) : m_clsid(clsid) {
    g_dllRefCount++;
}

ClassFactory::~ClassFactory() {
    g_dllRefCount--;
}

IFACEMETHODIMP ClassFactory::QueryInterface(REFIID riid, void **ppv) {
    static const QITAB qit[] = {
        QITABENT(ClassFactory, IClassFactory),
        { 0 },
    };
    return QISearch(this, qit, riid, ppv);
}

IFACEMETHODIMP_(ULONG) ClassFactory::AddRef() {
    return ++m_refCount;
}

IFACEMETHODIMP_(ULONG) ClassFactory::Release() {
    ULONG ref = --m_refCount;
    if (ref == 0) {
        delete this;
    }
    return ref;
}

IFACEMETHODIMP ClassFactory::CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppv) {
    if (pUnkOuter != nullptr) {
        return CLASS_E_NOAGGREGATION;
    }

    if (IsEqualCLSID(m_clsid, CLSID_ThumbForgeVideoThumbnailProvider)) {
        auto provider = new (std::nothrow) ThumbForgeVideoThumbnailProvider();
        if (!provider) return E_OUTOFMEMORY;
        HRESULT hr = provider->QueryInterface(riid, ppv);
        provider->Release();
        return hr;
    } else if (IsEqualCLSID(m_clsid, CLSID_ThumbForgeAudioThumbnailProvider)) {
        auto provider = new (std::nothrow) ThumbForgeAudioThumbnailProvider();
        if (!provider) return E_OUTOFMEMORY;
        HRESULT hr = provider->QueryInterface(riid, ppv);
        provider->Release();
        return hr;
    } else if (IsEqualCLSID(m_clsid, CLSID_ThumbForgeApkThumbnailProvider)) {
        auto provider = new (std::nothrow) ThumbForgeApkThumbnailProvider();
        if (!provider) return E_OUTOFMEMORY;
        HRESULT hr = provider->QueryInterface(riid, ppv);
        provider->Release();
        return hr;
    } else if (IsEqualCLSID(m_clsid, CLSID_ThumbForgeCodeThumbnailProvider)) {
        auto provider = new (std::nothrow) ThumbForgeCodeThumbnailProvider();
        if (!provider) return E_OUTOFMEMORY;
        HRESULT hr = provider->QueryInterface(riid, ppv);
        provider->Release();
        return hr;
    } else if (IsEqualCLSID(m_clsid, CLSID_ThumbForgeHtmlThumbnailProvider)) {
        auto provider = new (std::nothrow) ThumbForgeHtmlThumbnailProvider();
        if (!provider) return E_OUTOFMEMORY;
        HRESULT hr = provider->QueryInterface(riid, ppv);
        provider->Release();
        return hr;
    } else if (IsEqualCLSID(m_clsid, CLSID_ThumbForgeEpubThumbnailProvider)) {
        auto provider = new (std::nothrow) ThumbForgeEpubThumbnailProvider();
        if (!provider) return E_OUTOFMEMORY;
        HRESULT hr = provider->QueryInterface(riid, ppv);
        provider->Release();
        return hr;
    } else {
        auto provider = new (std::nothrow) ThumbForgePdfThumbnailProvider();
        if (!provider) return E_OUTOFMEMORY;
        HRESULT hr = provider->QueryInterface(riid, ppv);
        provider->Release();
        return hr;
    }
}

IFACEMETHODIMP ClassFactory::LockServer(BOOL fLock) {
    if (fLock) {
        g_dllRefCount++;
    } else {
        g_dllRefCount--;
    }
    return S_OK;
}
