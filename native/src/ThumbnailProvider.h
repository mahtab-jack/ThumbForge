#pragma once

#include "Common.h"
#include <atomic>
#include <shobjidl.h>

// PDF Provider: supports stream & file
class ThumbForgePdfThumbnailProvider : public IThumbnailProvider,
                                        public IInitializeWithStream,
                                        public IInitializeWithFile
{
public:
    ThumbForgePdfThumbnailProvider();
    virtual ~ThumbForgePdfThumbnailProvider();

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    // IInitializeWithStream
    IFACEMETHODIMP Initialize(IStream *pstream, DWORD grfMode) override;

    // IInitializeWithFile
    IFACEMETHODIMP Initialize(LPCWSTR pszFilePath, DWORD grfMode) override;

    // IThumbnailProvider
    IFACEMETHODIMP GetThumbnail(UINT cx, HBITMAP *phbmp, WTS_ALPHATYPE *pdwAlpha) override;

private:
    std::atomic<long> m_refCount{1};
    IStream* m_pStream{nullptr};
    std::wstring m_filePath;
};

// Video Provider: supports stream, item & file
class ThumbForgeVideoThumbnailProvider : public IThumbnailProvider,
                                          public IInitializeWithStream,
                                          public IInitializeWithItem,
                                          public IInitializeWithFile
{
public:
    ThumbForgeVideoThumbnailProvider();
    virtual ~ThumbForgeVideoThumbnailProvider();

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    // IInitializeWithStream
    IFACEMETHODIMP Initialize(IStream *pstream, DWORD grfMode) override;

    // IInitializeWithItem
    IFACEMETHODIMP Initialize(IShellItem *psi, DWORD grfMode) override;

    // IInitializeWithFile
    IFACEMETHODIMP Initialize(LPCWSTR pszFilePath, DWORD grfMode) override;

    // IThumbnailProvider
    IFACEMETHODIMP GetThumbnail(UINT cx, HBITMAP *phbmp, WTS_ALPHATYPE *pdwAlpha) override;

private:
    std::atomic<long> m_refCount{1};
    IStream* m_pStream{nullptr};
    std::wstring m_filePath;
    std::wstring m_tempFilePath;
};

// Audio Provider: supports stream, item & file
class ThumbForgeAudioThumbnailProvider : public IThumbnailProvider,
                                          public IInitializeWithStream,
                                          public IInitializeWithItem,
                                          public IInitializeWithFile
{
public:
    ThumbForgeAudioThumbnailProvider();
    virtual ~ThumbForgeAudioThumbnailProvider();

    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    IFACEMETHODIMP Initialize(IStream *pstream, DWORD grfMode) override;
    IFACEMETHODIMP Initialize(IShellItem *psi, DWORD grfMode) override;
    IFACEMETHODIMP Initialize(LPCWSTR pszFilePath, DWORD grfMode) override;
    IFACEMETHODIMP GetThumbnail(UINT cx, HBITMAP *phbmp, WTS_ALPHATYPE *pdwAlpha) override;

private:
    std::atomic<long> m_refCount{1};
    IStream* m_pStream{nullptr};
    std::wstring m_filePath;
    std::wstring m_tempFilePath;
};

// APK Provider: supports stream, item & file
class ThumbForgeApkThumbnailProvider : public IThumbnailProvider,
                                        public IInitializeWithStream,
                                        public IInitializeWithItem,
                                        public IInitializeWithFile
{
public:
    ThumbForgeApkThumbnailProvider();
    virtual ~ThumbForgeApkThumbnailProvider();

    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    IFACEMETHODIMP Initialize(IStream *pstream, DWORD grfMode) override;
    IFACEMETHODIMP Initialize(IShellItem *psi, DWORD grfMode) override;
    IFACEMETHODIMP Initialize(LPCWSTR pszFilePath, DWORD grfMode) override;
    IFACEMETHODIMP GetThumbnail(UINT cx, HBITMAP *phbmp, WTS_ALPHATYPE *pdwAlpha) override;

private:
    std::atomic<long> m_refCount{1};
    IStream* m_pStream{nullptr};
    std::wstring m_filePath;
    std::wstring m_tempFilePath;
};

// Code / Markup Provider: supports stream & file
class ThumbForgeCodeThumbnailProvider : public IThumbnailProvider,
                                         public IInitializeWithStream,
                                         public IInitializeWithFile
{
public:
    ThumbForgeCodeThumbnailProvider();
    virtual ~ThumbForgeCodeThumbnailProvider();

    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    IFACEMETHODIMP Initialize(IStream *pstream, DWORD grfMode) override;
    IFACEMETHODIMP Initialize(LPCWSTR pszFilePath, DWORD grfMode) override;
    IFACEMETHODIMP GetThumbnail(UINT cx, HBITMAP *phbmp, WTS_ALPHATYPE *pdwAlpha) override;

private:
    std::atomic<long> m_refCount{1};
    IStream* m_pStream{nullptr};
    std::wstring m_filePath;
};

using ThumbForgeThumbnailProvider = ThumbForgePdfThumbnailProvider;
