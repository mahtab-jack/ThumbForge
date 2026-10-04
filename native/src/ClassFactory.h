#pragma once

#include "Common.h"
#include <atomic>

class ClassFactory : public IClassFactory {
public:
    explicit ClassFactory(CLSID clsid = CLSID_ThumbForgePdfThumbnailProvider);
    virtual ~ClassFactory();

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    // IClassFactory
    IFACEMETHODIMP CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppv) override;
    IFACEMETHODIMP LockServer(BOOL fLock) override;

private:
    CLSID m_clsid;
    std::atomic<long> m_refCount{1};
};
