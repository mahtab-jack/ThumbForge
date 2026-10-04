#include "ThumbnailProvider.h"
#include "PdfRenderer.h"
#include "VideoRenderer.h"
#include "AudioRenderer.h"
#include "ApkRenderer.h"
#include "CodeRenderer.h"
#include <shlwapi.h>

extern std::atomic<long> g_dllRefCount;

// ============================================================================
// PreviewIconPdfThumbnailProvider Implementation
// ============================================================================

PreviewIconPdfThumbnailProvider::PreviewIconPdfThumbnailProvider() {
    g_dllRefCount++;
}

PreviewIconPdfThumbnailProvider::~PreviewIconPdfThumbnailProvider() {
    if (m_pStream) {
        m_pStream->Release();
        m_pStream = nullptr;
    }
    g_dllRefCount--;
}

IFACEMETHODIMP PreviewIconPdfThumbnailProvider::QueryInterface(REFIID riid, void **ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;

    if (riid == IID_IUnknown || riid == __uuidof(IThumbnailProvider)) {
        *ppv = static_cast<IThumbnailProvider*>(this);
    } else if (riid == __uuidof(IInitializeWithStream)) {
        *ppv = static_cast<IInitializeWithStream*>(this);
    } else if (riid == __uuidof(IInitializeWithFile)) {
        *ppv = static_cast<IInitializeWithFile*>(this);
    } else {
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

IFACEMETHODIMP_(ULONG) PreviewIconPdfThumbnailProvider::AddRef() {
    return ++m_refCount;
}

IFACEMETHODIMP_(ULONG) PreviewIconPdfThumbnailProvider::Release() {
    ULONG ref = --m_refCount;
    if (ref == 0) {
        delete this;
    }
    return ref;
}

IFACEMETHODIMP PreviewIconPdfThumbnailProvider::Initialize(IStream *pstream, DWORD /*grfMode*/) {
    if (m_pStream) {
        m_pStream->Release();
        m_pStream = nullptr;
    }
    if (pstream) {
        m_pStream = pstream;
        m_pStream->AddRef();
    }
    return S_OK;
}

IFACEMETHODIMP PreviewIconPdfThumbnailProvider::Initialize(LPCWSTR pszFilePath, DWORD /*grfMode*/) {
    if (pszFilePath) {
        m_filePath = pszFilePath;
    }
    return S_OK;
}

IFACEMETHODIMP PreviewIconPdfThumbnailProvider::GetThumbnail(UINT cx, HBITMAP *phbmp, WTS_ALPHATYPE *pdwAlpha) {
    if (!phbmp || !pdwAlpha) {
        return E_POINTER;
    }

    *phbmp = nullptr;
    *pdwAlpha = WTSAT_RGB;

    if (m_pStream) {
        STATSTG stat;
        if (SUCCEEDED(m_pStream->Stat(&stat, STATFLAG_NONAME)) && stat.cbSize.QuadPart > 0) {
            size_t size = (size_t)stat.cbSize.QuadPart;
            if (size <= 100 * 1024 * 1024) {
                std::vector<BYTE> buffer(size);
                LARGE_INTEGER zero = {0};
                m_pStream->Seek(zero, STREAM_SEEK_SET, nullptr);
                ULONG bytesRead = 0;
                if (SUCCEEDED(m_pStream->Read(buffer.data(), (ULONG)size, &bytesRead)) && bytesRead > 4) {
                    PdfThumbnailResult res = PdfRenderer::RenderThumbnailFromMemory(buffer.data(), buffer.size(), cx);
                    if (res.hBitmap) {
                        *phbmp = res.hBitmap;
                        *pdwAlpha = res.alphaType;
                        return S_OK;
                    }
                }
            }
        }
    }

    if (!m_filePath.empty() && PathFileExistsW(m_filePath.c_str())) {
        PdfThumbnailResult res = PdfRenderer::RenderThumbnail(m_filePath, cx);
        if (res.hBitmap) {
            *phbmp = res.hBitmap;
            *pdwAlpha = res.alphaType;
            return S_OK;
        }
    }

    return E_FAIL;
}

static bool DumpStreamToTempFile(IStream* pStream, const std::wstring& ext, ULONGLONG maxBytes, std::wstring& outTempPath) {
    if (!pStream) return false;
    wchar_t tempDir[MAX_PATH];
    if (GetTempPathW(MAX_PATH, tempDir) == 0) return false;
    wchar_t tempFile[MAX_PATH];
    if (GetTempFileNameW(tempDir, L"pit", 0, tempFile) == 0) return false;

    std::wstring finalPath = tempFile;
    if (!ext.empty()) {
        finalPath += ext;
        MoveFileW(tempFile, finalPath.c_str());
    }

    HANDLE hFile = CreateFileW(finalPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    LARGE_INTEGER zero = {0};
    pStream->Seek(zero, STREAM_SEEK_SET, nullptr);

    BYTE buf[65536];
    ULONG bytesRead = 0;
    DWORD bytesWritten = 0;
    ULONGLONG totalWritten = 0;

    while (SUCCEEDED(pStream->Read(buf, sizeof(buf), &bytesRead)) && bytesRead > 0) {
        WriteFile(hFile, buf, bytesRead, &bytesWritten, nullptr);
        totalWritten += bytesWritten;
        if (maxBytes > 0 && totalWritten >= maxBytes) break;
    }

    CloseHandle(hFile);
    outTempPath = finalPath;
    return true;
}

static bool IsBinaryOrMediaHeader(const BYTE* data, size_t size) {
    if (!data || size < 4) return false;
    // ID3 (MP3)
    if (size >= 3 && data[0] == 'I' && data[1] == 'D' && data[2] == '3') return true;
    // OggS (OGG/Opus)
    if (size >= 4 && data[0] == 'O' && data[1] == 'g' && data[2] == 'g' && data[3] == 'S') return true;
    // fLaC
    if (size >= 4 && data[0] == 'f' && data[1] == 'L' && data[2] == 'a' && data[3] == 'C') return true;
    // RIFF (WAV, AVI, WEBP)
    if (size >= 4 && data[0] == 'R' && data[1] == 'I' && data[2] == 'F' && data[3] == 'F') return true;
    // PK\x03\x04 (ZIP, APK, JAR)
    if (size >= 4 && data[0] == 'P' && data[1] == 'K' && data[2] == 0x03 && data[3] == 0x04) return true;
    // MP4 / MOV / 3GP ftyp box
    if (size >= 8 && memcmp(data + 4, "ftyp", 4) == 0) return true;
    if (size >= 8 && memcmp(data + 4, "moov", 4) == 0) return true;
    // Matroska / WebM EBML
    if (size >= 4 && data[0] == 0x1A && data[1] == 0x45 && (BYTE)data[2] == 0xDF && (BYTE)data[3] == 0xA3) return true;
    // Windows Executable MZ
    if (size >= 2 && data[0] == 'M' && data[1] == 'Z') return true;
    // Check for null bytes in the first 256 bytes
    size_t checkLen = (std::min)(size, (size_t)256);
    size_t nullCount = 0;
    for (size_t i = 0; i < checkLen; ++i) {
        if (data[i] == 0) nullCount++;
    }
    if (nullCount > 2) return true;
    return false;
}

// ============================================================================
// PreviewIconVideoThumbnailProvider Implementation
// ============================================================================

PreviewIconVideoThumbnailProvider::PreviewIconVideoThumbnailProvider() {
    g_dllRefCount++;
}

PreviewIconVideoThumbnailProvider::~PreviewIconVideoThumbnailProvider() {
    if (m_pStream) {
        m_pStream->Release();
        m_pStream = nullptr;
    }
    if (!m_tempFilePath.empty()) {
        DeleteFileW(m_tempFilePath.c_str());
    }
    g_dllRefCount--;
}

IFACEMETHODIMP PreviewIconVideoThumbnailProvider::QueryInterface(REFIID riid, void **ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;

    if (riid == IID_IUnknown || riid == __uuidof(IThumbnailProvider)) {
        *ppv = static_cast<IThumbnailProvider*>(this);
    } else if (riid == __uuidof(IInitializeWithStream)) {
        *ppv = static_cast<IInitializeWithStream*>(this);
    } else if (riid == __uuidof(IInitializeWithItem)) {
        *ppv = static_cast<IInitializeWithItem*>(this);
    } else if (riid == __uuidof(IInitializeWithFile)) {
        *ppv = static_cast<IInitializeWithFile*>(this);
    } else {
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

IFACEMETHODIMP_(ULONG) PreviewIconVideoThumbnailProvider::AddRef() {
    return ++m_refCount;
}

IFACEMETHODIMP_(ULONG) PreviewIconVideoThumbnailProvider::Release() {
    ULONG ref = --m_refCount;
    if (ref == 0) {
        delete this;
    }
    return ref;
}

IFACEMETHODIMP PreviewIconVideoThumbnailProvider::Initialize(IStream *pstream, DWORD /*grfMode*/) {
    if (m_pStream) {
        m_pStream->Release();
        m_pStream = nullptr;
    }
    if (pstream) {
        m_pStream = pstream;
        m_pStream->AddRef();

        STATSTG stat = {0};
        if (SUCCEEDED(pstream->Stat(&stat, STATFLAG_DEFAULT)) && stat.pwcsName) {
            std::wstring path = stat.pwcsName;
            CoTaskMemFree(stat.pwcsName);
            if (!path.empty() && PathFileExistsW(path.c_str())) {
                m_filePath = path;
            }
        }
    }
    return S_OK;
}

IFACEMETHODIMP PreviewIconVideoThumbnailProvider::Initialize(IShellItem *psi, DWORD /*grfMode*/) {
    if (psi) {
        LPWSTR pszPath = nullptr;
        if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath)) && pszPath) {
            m_filePath = pszPath;
            CoTaskMemFree(pszPath);
        }
    }
    return S_OK;
}

IFACEMETHODIMP PreviewIconVideoThumbnailProvider::Initialize(LPCWSTR pszFilePath, DWORD /*grfMode*/) {
    if (pszFilePath) {
        m_filePath = pszFilePath;
    }
    return S_OK;
}

IFACEMETHODIMP PreviewIconVideoThumbnailProvider::GetThumbnail(UINT cx, HBITMAP *phbmp, WTS_ALPHATYPE *pdwAlpha) {
    if (!phbmp || !pdwAlpha) {
        return E_POINTER;
    }

    *phbmp = nullptr;
    *pdwAlpha = WTSAT_RGB;

    std::wstring renderPath = m_filePath;
    if ((renderPath.empty() || !PathFileExistsW(renderPath.c_str())) && m_pStream) {
        std::wstring ext = PathFindExtensionW(m_filePath.c_str());
        if (ext.empty()) ext = L".mp4";
        if (DumpStreamToTempFile(m_pStream, ext, 35 * 1024 * 1024, m_tempFilePath)) {
            renderPath = m_tempFilePath;
        }
    }

    if (!renderPath.empty() && PathFileExistsW(renderPath.c_str())) {
        VideoThumbnailResult res = VideoRenderer::RenderThumbnail(renderPath, cx);
        if (res.success && res.hBitmap) {
            *phbmp = res.hBitmap;
            *pdwAlpha = res.alphaType;
            return S_OK;
        }
    }

    return E_FAIL;
}

// ============================================================================
// PreviewIconAudioThumbnailProvider Implementation
// ============================================================================

PreviewIconAudioThumbnailProvider::PreviewIconAudioThumbnailProvider() {
    g_dllRefCount++;
}

PreviewIconAudioThumbnailProvider::~PreviewIconAudioThumbnailProvider() {
    if (m_pStream) {
        m_pStream->Release();
        m_pStream = nullptr;
    }
    if (!m_tempFilePath.empty()) {
        DeleteFileW(m_tempFilePath.c_str());
    }
    g_dllRefCount--;
}

IFACEMETHODIMP PreviewIconAudioThumbnailProvider::QueryInterface(REFIID riid, void **ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;

    if (riid == IID_IUnknown || riid == __uuidof(IThumbnailProvider)) {
        *ppv = static_cast<IThumbnailProvider*>(this);
    } else if (riid == __uuidof(IInitializeWithStream)) {
        *ppv = static_cast<IInitializeWithStream*>(this);
    } else if (riid == __uuidof(IInitializeWithItem)) {
        *ppv = static_cast<IInitializeWithItem*>(this);
    } else if (riid == __uuidof(IInitializeWithFile)) {
        *ppv = static_cast<IInitializeWithFile*>(this);
    } else {
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

IFACEMETHODIMP_(ULONG) PreviewIconAudioThumbnailProvider::AddRef() {
    return ++m_refCount;
}

IFACEMETHODIMP_(ULONG) PreviewIconAudioThumbnailProvider::Release() {
    ULONG ref = --m_refCount;
    if (ref == 0) {
        delete this;
    }
    return ref;
}

IFACEMETHODIMP PreviewIconAudioThumbnailProvider::Initialize(IStream *pstream, DWORD /*grfMode*/) {
    if (m_pStream) {
        m_pStream->Release();
        m_pStream = nullptr;
    }
    if (pstream) {
        m_pStream = pstream;
        m_pStream->AddRef();

        STATSTG stat = {0};
        if (SUCCEEDED(pstream->Stat(&stat, STATFLAG_DEFAULT)) && stat.pwcsName) {
            std::wstring path = stat.pwcsName;
            CoTaskMemFree(stat.pwcsName);
            if (!path.empty() && PathFileExistsW(path.c_str())) {
                m_filePath = path;
            }
        }
    }
    return S_OK;
}

IFACEMETHODIMP PreviewIconAudioThumbnailProvider::Initialize(IShellItem *psi, DWORD /*grfMode*/) {
    if (psi) {
        LPWSTR pszPath = nullptr;
        if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath)) && pszPath) {
            m_filePath = pszPath;
            CoTaskMemFree(pszPath);
        }
    }
    return S_OK;
}

IFACEMETHODIMP PreviewIconAudioThumbnailProvider::Initialize(LPCWSTR pszFilePath, DWORD /*grfMode*/) {
    if (pszFilePath) {
        m_filePath = pszFilePath;
    }
    return S_OK;
}

IFACEMETHODIMP PreviewIconAudioThumbnailProvider::GetThumbnail(UINT cx, HBITMAP *phbmp, WTS_ALPHATYPE *pdwAlpha) {
    if (!phbmp || !pdwAlpha) {
        return E_POINTER;
    }

    *phbmp = nullptr;
    *pdwAlpha = WTSAT_RGB;

    std::wstring renderPath = m_filePath;
    if ((renderPath.empty() || !PathFileExistsW(renderPath.c_str())) && m_pStream) {
        std::wstring ext = PathFindExtensionW(m_filePath.c_str());
        if (ext.empty()) ext = L".mp3";
        if (DumpStreamToTempFile(m_pStream, ext, 20 * 1024 * 1024, m_tempFilePath)) {
            renderPath = m_tempFilePath;
        }
    }

    if (!renderPath.empty() && PathFileExistsW(renderPath.c_str())) {
        AudioThumbnailResult res = AudioRenderer::RenderThumbnail(renderPath, cx);
        if (res.success && res.hBitmap) {
            *phbmp = res.hBitmap;
            *pdwAlpha = res.alphaType;
            return S_OK;
        }
    }

    return E_FAIL;
}

// ============================================================================
// PreviewIconApkThumbnailProvider Implementation
// ============================================================================

PreviewIconApkThumbnailProvider::PreviewIconApkThumbnailProvider() {
    g_dllRefCount++;
}

PreviewIconApkThumbnailProvider::~PreviewIconApkThumbnailProvider() {
    if (m_pStream) {
        m_pStream->Release();
        m_pStream = nullptr;
    }
    if (!m_tempFilePath.empty()) {
        DeleteFileW(m_tempFilePath.c_str());
    }
    g_dllRefCount--;
}

IFACEMETHODIMP PreviewIconApkThumbnailProvider::QueryInterface(REFIID riid, void **ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;

    if (riid == IID_IUnknown || riid == __uuidof(IThumbnailProvider)) {
        *ppv = static_cast<IThumbnailProvider*>(this);
    } else if (riid == __uuidof(IInitializeWithStream)) {
        *ppv = static_cast<IInitializeWithStream*>(this);
    } else if (riid == __uuidof(IInitializeWithItem)) {
        *ppv = static_cast<IInitializeWithItem*>(this);
    } else if (riid == __uuidof(IInitializeWithFile)) {
        *ppv = static_cast<IInitializeWithFile*>(this);
    } else {
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

IFACEMETHODIMP_(ULONG) PreviewIconApkThumbnailProvider::AddRef() {
    return ++m_refCount;
}

IFACEMETHODIMP_(ULONG) PreviewIconApkThumbnailProvider::Release() {
    ULONG ref = --m_refCount;
    if (ref == 0) {
        delete this;
    }
    return ref;
}

IFACEMETHODIMP PreviewIconApkThumbnailProvider::Initialize(IStream *pstream, DWORD /*grfMode*/) {
    if (m_pStream) {
        m_pStream->Release();
        m_pStream = nullptr;
    }
    if (pstream) {
        m_pStream = pstream;
        m_pStream->AddRef();

        STATSTG stat = {0};
        if (SUCCEEDED(pstream->Stat(&stat, STATFLAG_DEFAULT)) && stat.pwcsName) {
            std::wstring path = stat.pwcsName;
            CoTaskMemFree(stat.pwcsName);
            if (!path.empty() && PathFileExistsW(path.c_str())) {
                m_filePath = path;
            }
        }
    }
    return S_OK;
}

IFACEMETHODIMP PreviewIconApkThumbnailProvider::Initialize(IShellItem *psi, DWORD /*grfMode*/) {
    if (psi) {
        LPWSTR pszPath = nullptr;
        if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath)) && pszPath) {
            m_filePath = pszPath;
            CoTaskMemFree(pszPath);
        }
    }
    return S_OK;
}

IFACEMETHODIMP PreviewIconApkThumbnailProvider::Initialize(LPCWSTR pszFilePath, DWORD /*grfMode*/) {
    if (pszFilePath) {
        m_filePath = pszFilePath;
    }
    return S_OK;
}

IFACEMETHODIMP PreviewIconApkThumbnailProvider::GetThumbnail(UINT cx, HBITMAP *phbmp, WTS_ALPHATYPE *pdwAlpha) {
    if (!phbmp || !pdwAlpha) {
        return E_POINTER;
    }

    *phbmp = nullptr;
    *pdwAlpha = WTSAT_ARGB;

    std::wstring renderPath = m_filePath;
    if ((renderPath.empty() || !PathFileExistsW(renderPath.c_str())) && m_pStream) {
        if (DumpStreamToTempFile(m_pStream, L".apk", 100 * 1024 * 1024, m_tempFilePath)) {
            renderPath = m_tempFilePath;
        }
    }

    if (!renderPath.empty() && PathFileExistsW(renderPath.c_str())) {
        ApkThumbnailResult res = ApkRenderer::RenderThumbnail(renderPath, cx);
        if (res.success && res.hBitmap) {
            *phbmp = res.hBitmap;
            *pdwAlpha = res.alphaType;
            return S_OK;
        }
    }

    return E_FAIL;
}

// ============================================================================
// PreviewIconCodeThumbnailProvider Implementation
// ============================================================================

PreviewIconCodeThumbnailProvider::PreviewIconCodeThumbnailProvider() {
    g_dllRefCount++;
}

PreviewIconCodeThumbnailProvider::~PreviewIconCodeThumbnailProvider() {
    if (m_pStream) {
        m_pStream->Release();
        m_pStream = nullptr;
    }
    g_dllRefCount--;
}

IFACEMETHODIMP PreviewIconCodeThumbnailProvider::QueryInterface(REFIID riid, void **ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;

    if (riid == IID_IUnknown || riid == __uuidof(IThumbnailProvider)) {
        *ppv = static_cast<IThumbnailProvider*>(this);
    } else if (riid == __uuidof(IInitializeWithStream)) {
        *ppv = static_cast<IInitializeWithStream*>(this);
    } else if (riid == __uuidof(IInitializeWithFile)) {
        *ppv = static_cast<IInitializeWithFile*>(this);
    } else {
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

IFACEMETHODIMP_(ULONG) PreviewIconCodeThumbnailProvider::AddRef() {
    return ++m_refCount;
}

IFACEMETHODIMP_(ULONG) PreviewIconCodeThumbnailProvider::Release() {
    ULONG ref = --m_refCount;
    if (ref == 0) {
        delete this;
    }
    return ref;
}

IFACEMETHODIMP PreviewIconCodeThumbnailProvider::Initialize(IStream *pstream, DWORD /*grfMode*/) {
    if (m_pStream) {
        m_pStream->Release();
        m_pStream = nullptr;
    }
    if (pstream) {
        m_pStream = pstream;
        m_pStream->AddRef();
    }
    return S_OK;
}

IFACEMETHODIMP PreviewIconCodeThumbnailProvider::Initialize(LPCWSTR pszFilePath, DWORD /*grfMode*/) {
    if (pszFilePath) {
        m_filePath = pszFilePath;
    }
    return S_OK;
}

IFACEMETHODIMP PreviewIconCodeThumbnailProvider::GetThumbnail(UINT cx, HBITMAP *phbmp, WTS_ALPHATYPE *pdwAlpha) {
    if (!phbmp || !pdwAlpha) {
        return E_POINTER;
    }

    *phbmp = nullptr;
    *pdwAlpha = WTSAT_RGB;

    if (m_pStream) {
        STATSTG stat;
        if (SUCCEEDED(m_pStream->Stat(&stat, STATFLAG_NONAME)) && stat.cbSize.QuadPart > 0) {
            size_t size = (size_t)(std::min<ULONGLONG>)(stat.cbSize.QuadPart, 8192);
            std::vector<BYTE> buffer(size);
            LARGE_INTEGER zero = {0};
            m_pStream->Seek(zero, STREAM_SEEK_SET, nullptr);
            ULONG bytesRead = 0;
            if (SUCCEEDED(m_pStream->Read(buffer.data(), (ULONG)size, &bytesRead)) && bytesRead > 0) {
                // Safety guard: reject binary, audio, video files immediately
                if (IsBinaryOrMediaHeader(buffer.data(), (size_t)bytesRead)) {
                    return E_FAIL;
                }

                std::wstring ext = PathFindExtensionW(m_filePath.c_str());
                CodeThumbnailResult res = CodeRenderer::RenderFromMemory(buffer.data(), (size_t)bytesRead, ext, cx);
                if (res.success && res.hBitmap) {
                    *phbmp = res.hBitmap;
                    *pdwAlpha = res.alphaType;
                    return S_OK;
                }
            }
        }
    }

    if (!m_filePath.empty() && PathFileExistsW(m_filePath.c_str())) {
        // Read initial bytes to check magic headers
        HANDLE hFile = CreateFileW(m_filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile != INVALID_HANDLE_VALUE) {
            BYTE checkBuf[256] = {0};
            DWORD readCount = 0;
            if (ReadFile(hFile, checkBuf, sizeof(checkBuf), &readCount, nullptr) && readCount > 0) {
                if (IsBinaryOrMediaHeader(checkBuf, (size_t)readCount)) {
                    CloseHandle(hFile);
                    return E_FAIL;
                }
            }
            CloseHandle(hFile);
        }

        CodeThumbnailResult res = CodeRenderer::RenderThumbnail(m_filePath, cx);
        if (res.success && res.hBitmap) {
            *phbmp = res.hBitmap;
            *pdwAlpha = res.alphaType;
            return S_OK;
        }
    }

    return E_FAIL;
}
