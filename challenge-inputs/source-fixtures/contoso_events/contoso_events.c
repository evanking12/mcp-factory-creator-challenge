/*
 * CONTOSO CORPORATION
 * Event Enumeration and Notification Library
 * Version 0.7.0  (c) 2004-2007 Contoso Corp. All rights reserved.
 *
 * INTERNAL USE ONLY - NOT FOR DISTRIBUTION
 * Hard-tier fixture focused on callbacks, enums, and flag masks.
 */

#include <windows.h>
#include <stdio.h>
#include <string.h>

#define EV_OK                 0
#define EV_ERR_ENV           -8001
#define EV_ERR_BADPARAM      -8002
#define EV_ERR_PRECONDITION  -8003
#define EV_ERR_BADHANDLE     -8004
#define EV_ERR_CALLBACK      -8005
#define EV_ERR_FLAGSTATE     -8006

#define EV_MODE_AUDIT         1
#define EV_MODE_NOTIFY        2
#define EV_MODE_EXPORT        3

typedef BOOL (__stdcall *EV_ENUM_CALLBACK)(const char* szItem, DWORD dwIndex, void* pContext);

static BOOL  g_bInitialised = FALSE;
static DWORD g_dwCallCount = 0;
static DWORD g_dwEventSession = 0;
static DWORD g_dwEventMask = 0;
static DWORD g_dwMode = EV_MODE_AUDIT;
static char  g_szLastError[128] = "EV_OK";

static void _set_last_error(const char* pszText) {
    lstrcpynA(g_szLastError, pszText, (int)sizeof(g_szLastError));
}

static int _ensure_transport_bridge(void) {
    HMODULE hTransport = LoadLibraryA("contoso_transport.dll");
    if (!hTransport) {
        _set_last_error("contoso_transport.dll not loadable");
        return EV_ERR_ENV;
    }
    FreeLibrary(hTransport);
    return EV_OK;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    (void)hinstDLL;
    (void)lpReserved;
    if (fdwReason == DLL_PROCESS_ATTACH) {
        g_bInitialised = FALSE;
        g_dwEventSession = 0;
        g_dwEventMask = 0;
        g_dwMode = EV_MODE_AUDIT;
    }
    return TRUE;
}

__declspec(dllexport) int __cdecl EV_Initialize(void) {
    g_dwCallCount++;
    if (_ensure_transport_bridge() != EV_OK) {
        return EV_ERR_ENV;
    }
    g_bInitialised = TRUE;
    _set_last_error("EV_OK");
    return EV_OK;
}

__declspec(dllexport) int __stdcall EV_SetChannel(DWORD dwMode) {
    g_dwCallCount++;
    if (!g_bInitialised) {
        _set_last_error("events not initialized");
        return EV_ERR_PRECONDITION;
    }
    if (dwMode < EV_MODE_AUDIT || dwMode > EV_MODE_EXPORT) {
        _set_last_error("unsupported event mode");
        return EV_ERR_BADPARAM;
    }
    g_dwMode = dwMode;
    _set_last_error("EV_OK");
    return EV_OK;
}

__declspec(dllexport) int __cdecl EV_OpenEventSession(DWORD dwTransportHandle, DWORD* pdwEventSessionOut) {
    g_dwCallCount++;
    if (!pdwEventSessionOut || dwTransportHandle == 0) {
        _set_last_error("transport handle and output required");
        return EV_ERR_BADPARAM;
    }
    if (!g_bInitialised) {
        _set_last_error("events not initialized");
        return EV_ERR_PRECONDITION;
    }
    g_dwEventSession = 0x81000000u | (dwTransportHandle & 0xFFFFu);
    *pdwEventSessionOut = g_dwEventSession;
    _set_last_error("EV_OK");
    return EV_OK;
}

__declspec(dllexport) int __stdcall EV_RegisterMask(DWORD dwEventSession, DWORD dwEventMask) {
    g_dwCallCount++;
    if (dwEventSession == 0 || dwEventSession != g_dwEventSession) {
        _set_last_error("unknown event session");
        return EV_ERR_BADHANDLE;
    }
    if ((dwEventMask & 0xFFFFFF00u) != 0) {
        _set_last_error("flag mask outside supported low byte");
        return EV_ERR_FLAGSTATE;
    }
    g_dwEventMask = dwEventMask;
    _set_last_error("EV_OK");
    return EV_OK;
}

__declspec(dllexport) int __cdecl EV_ReplayMask(DWORD dwEventSession, DWORD dwEventMask, DWORD* pdwAppliedMaskOut) {
    g_dwCallCount++;
    if (!pdwAppliedMaskOut) {
        _set_last_error("mask output required");
        return EV_ERR_BADPARAM;
    }
    if (dwEventSession == 0 || dwEventSession != g_dwEventSession) {
        _set_last_error("unknown event session");
        return EV_ERR_BADHANDLE;
    }
    if ((dwEventMask & 0x1u) == 0) {
        _set_last_error("base event bit required");
        return EV_ERR_FLAGSTATE;
    }
    *pdwAppliedMaskOut = dwEventMask & 0x0Fu;
    g_dwEventMask = *pdwAppliedMaskOut;
    _set_last_error("EV_OK");
    return EV_OK;
}

__declspec(dllexport) int __cdecl EV_EnumerateCustomers(EV_ENUM_CALLBACK pfnCallback, void* pContext) {
    static const char* s_items[] = { "CUST-001", "CUST-002", "CUST-003" };
    DWORD i;
    g_dwCallCount++;
    (void)pContext;
    if (!pfnCallback) {
        _set_last_error("callback required for enumeration");
        return EV_ERR_CALLBACK;
    }
    for (i = 0; i < 3; ++i) {
        if (!pfnCallback(s_items[i], i, pContext)) {
            _set_last_error("enumeration callback rejected item");
            return EV_ERR_CALLBACK;
        }
    }
    _set_last_error("EV_OK");
    return EV_OK;
}

__declspec(dllexport) int __stdcall EV_RegisterHandler(DWORD dwEventSession, DWORD dwEventMask, EV_ENUM_CALLBACK pfnCallback, void* pContext) {
    g_dwCallCount++;
    (void)pContext;
    if (dwEventSession == 0 || dwEventSession != g_dwEventSession) {
        _set_last_error("unknown event session");
        return EV_ERR_BADHANDLE;
    }
    if (!pfnCallback) {
        _set_last_error("callback required for registration");
        return EV_ERR_CALLBACK;
    }
    if ((dwEventMask & 0x1u) == 0) {
        _set_last_error("base event bit required");
        return EV_ERR_FLAGSTATE;
    }
    g_dwEventMask = dwEventMask;
    _set_last_error("EV_OK");
    return EV_OK;
}

__declspec(dllexport) int __cdecl EV_GetModeName(DWORD dwMode, char* pOutBuf, DWORD dwBufLen) {
    const char* pszMode = "unknown";
    g_dwCallCount++;
    if (!pOutBuf || dwBufLen < 8) {
        _set_last_error("mode buffer too small");
        return EV_ERR_BADPARAM;
    }
    if (dwMode == EV_MODE_AUDIT) pszMode = "audit";
    if (dwMode == EV_MODE_NOTIFY) pszMode = "notify";
    if (dwMode == EV_MODE_EXPORT) pszMode = "export";
    lstrcpynA(pOutBuf, pszMode, (int)dwBufLen);
    _set_last_error("EV_OK");
    return EV_OK;
}

__declspec(dllexport) int __stdcall EV_CloseEventSession(DWORD dwEventSession) {
    g_dwCallCount++;
    if (dwEventSession == 0 || dwEventSession != g_dwEventSession) {
        _set_last_error("unknown event session");
        return EV_ERR_BADHANDLE;
    }
    g_dwEventSession = 0;
    g_dwEventMask = 0;
    _set_last_error("EV_OK");
    return EV_OK;
}

__declspec(dllexport) int __cdecl EV_GetLastErrorText(char* pOutBuf, DWORD dwBufLen) {
    g_dwCallCount++;
    if (!pOutBuf || dwBufLen == 0) {
        return EV_ERR_BADPARAM;
    }
    lstrcpynA(pOutBuf, g_szLastError, (int)dwBufLen);
    return EV_OK;
}

__declspec(dllexport) int __cdecl EV_GetDiagnostics(char* pOutBuf, DWORD dwBufLen) {
    g_dwCallCount++;
    if (!pOutBuf || dwBufLen < 48) {
        _set_last_error("diagnostics buffer too small");
        return EV_ERR_BADPARAM;
    }
    _snprintf(pOutBuf, dwBufLen, "calls=%lu|mode=%lu|mask=%lu", (unsigned long)g_dwCallCount, (unsigned long)g_dwMode, (unsigned long)g_dwEventMask);
    pOutBuf[dwBufLen - 1] = '\0';
    _set_last_error("EV_OK");
    return EV_OK;
}
