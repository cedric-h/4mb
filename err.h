#define LOG_AND_RETURN_ERROR(hr, msg) do \
{                                        \
    if (FAILED(hr)) {                    \
        log_win32_err(hr, msg);          \
        return hr;                       \
    }                                    \
} while (0)

static void log_err(const char *msg) {
    #if USE_DEBUG_MODE
    OutputDebugStringA(msg);
    OutputDebugStringA("!\n");
    #endif
}

static void log_win32_err(DWORD err, const char *msg) {
    #if USE_DEBUG_MODE
    log_err(msg);

    LPWSTR str;
    DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM;
    DWORD lang = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);
    if (FormatMessageW(flags, NULL, err, lang, (LPWSTR)&str, 0, NULL)) {
        OutputDebugStringW(str);
        LocalFree(str);
    }
    #endif
}
static void log_win32_last_err(const char *msg) {
    #if USE_DEBUG_MODE
    log_win32_err(GetLastError(), msg);
    #endif
}

static HRESULT fatal_device_lost_error() {
    #if USE_DEBUG_MODE
    wchar_t *msg = L"Cannot recreate D3D11 device, it is reset or removed!";
    MessageBoxW(NULL, msg, L"Error", MB_ICONEXCLAMATION);
    #endif
    return E_FAIL;
}
