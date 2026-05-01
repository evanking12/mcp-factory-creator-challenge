/*
 * CONTOSO CORPORATION
 * Gateway Translation Library
 * Version 0.8.0  (c) 2004-2007 Contoso Corp. All rights reserved.
 *
 * INTERNAL USE ONLY - NOT FOR DISTRIBUTION
 * Hard-tier fixture focused on wide strings and reduced discovery clarity.
 */

#include <windows.h>
#include <wchar.h>
#include <stdio.h>

#define GW_OK                 0
#define GW_ERR_ENV           -7001
#define GW_ERR_BADPARAM      -7002
#define GW_ERR_PRECONDITION  -7003
#define GW_ERR_BADHANDLE     -7004

static BOOL  g_bInitialised = FALSE;
static DWORD g_dwCallCount = 0;
static DWORD g_dwTransportHandle = 0;
static WCHAR g_wszCustomer[32] = L"";
static WCHAR g_wszLastError[128] = L"GW_OK";

static void _set_last_error(const WCHAR* pwszText) {
    lstrcpynW(g_wszLastError, pwszText, (int)(sizeof(g_wszLastError) / sizeof(g_wszLastError[0])));
}

static int _ensure_transport_bridge(void) {
    HMODULE hTransport = LoadLibraryA("contoso_transport.dll");
    if (!hTransport) {
        _set_last_error(L"contoso_transport.dll not loadable");
        return GW_ERR_ENV;
    }
    FreeLibrary(hTransport);
    return GW_OK;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    (void)hinstDLL;
    (void)lpReserved;
    if (fdwReason == DLL_PROCESS_ATTACH) {
        g_bInitialised = FALSE;
        g_dwTransportHandle = 0;
        g_wszCustomer[0] = L'\0';
        _set_last_error(L"GW_OK");
    }
    return TRUE;
}

__declspec(dllexport) int __cdecl GW_Initialize(void) {
    g_dwCallCount++;
    if (_ensure_transport_bridge() != GW_OK) {
        return GW_ERR_ENV;
    }
    g_bInitialised = TRUE;
    _set_last_error(L"GW_OK");
    return GW_OK;
}

__declspec(dllexport) int __stdcall GW_BindCustomerW(const wchar_t* lpwszCustId) {
    g_dwCallCount++;
    if (!g_bInitialised) {
        _set_last_error(L"gateway not initialized");
        return GW_ERR_PRECONDITION;
    }
    if (!lpwszCustId || lstrlenW(lpwszCustId) < 6) {
        _set_last_error(L"bad wide customer id");
        return GW_ERR_BADPARAM;
    }
    lstrcpynW(g_wszCustomer, lpwszCustId, (int)(sizeof(g_wszCustomer) / sizeof(g_wszCustomer[0])));
    _set_last_error(L"GW_OK");
    return GW_OK;
}

__declspec(dllexport) int __cdecl GW_LinkTransport(DWORD dwTransportHandle) {
    g_dwCallCount++;
    if (!g_bInitialised) {
        _set_last_error(L"gateway not initialized");
        return GW_ERR_PRECONDITION;
    }
    if (dwTransportHandle == 0) {
        _set_last_error(L"transport handle required");
        return GW_ERR_BADPARAM;
    }
    g_dwTransportHandle = dwTransportHandle;
    _set_last_error(L"GW_OK");
    return GW_OK;
}

__declspec(dllexport) int __cdecl GW_GetWideNonce(DWORD dwTransportHandle, DWORD* pdwNonceOut) {
    g_dwCallCount++;
    if (!pdwNonceOut) {
        _set_last_error(L"nonce output required");
        return GW_ERR_BADPARAM;
    }
    if (dwTransportHandle == 0 || dwTransportHandle != g_dwTransportHandle) {
        _set_last_error(L"unknown gateway transport handle");
        return GW_ERR_BADHANDLE;
    }
    *pdwNonceOut = 0xB0000000u | (dwTransportHandle & 0xFFFFu);
    _set_last_error(L"GW_OK");
    return GW_OK;
}

__declspec(dllexport) int __stdcall GW_RenderBridgeW(DWORD dwTransportHandle, wchar_t* pOutBuf, DWORD dwBufLen) {
    g_dwCallCount++;
    if (!pOutBuf || dwBufLen < 24) {
        _set_last_error(L"wide bridge buffer too small");
        return GW_ERR_BADPARAM;
    }
    if (dwTransportHandle == 0 || dwTransportHandle != g_dwTransportHandle) {
        _set_last_error(L"unknown gateway transport handle");
        return GW_ERR_BADHANDLE;
    }
    _snwprintf(pOutBuf, dwBufLen, L"transport=%lu|cust=%ls", (unsigned long)dwTransportHandle, g_wszCustomer[0] ? g_wszCustomer : L"none");
    pOutBuf[dwBufLen - 1] = L'\0';
    _set_last_error(L"GW_OK");
    return GW_OK;
}

__declspec(dllexport) int __cdecl GW_FormatModeW(DWORD dwModeCode, wchar_t* pOutBuf, DWORD dwBufLen) {
    const wchar_t* pwszMode = L"unknown";
    g_dwCallCount++;
    if (!pOutBuf || dwBufLen < 8) {
        _set_last_error(L"mode buffer too small");
        return GW_ERR_BADPARAM;
    }
    if (dwModeCode == 1) pwszMode = L"trial";
    if (dwModeCode == 2) pwszMode = L"licensed";
    lstrcpynW(pOutBuf, pwszMode, (int)dwBufLen);
    _set_last_error(L"GW_OK");
    return GW_OK;
}

__declspec(dllexport) int __cdecl GW_A1W(const wchar_t* lpwszIn, wchar_t* pOutBuf, DWORD dwBufLen) {
    g_dwCallCount++;
    if (!lpwszIn || !pOutBuf || dwBufLen < 12) {
        _set_last_error(L"reduced-clarity bridge args invalid");
        return GW_ERR_BADPARAM;
    }
    _snwprintf(pOutBuf, dwBufLen, L"A1:%ls", lpwszIn);
    pOutBuf[dwBufLen - 1] = L'\0';
    _set_last_error(L"GW_OK");
    return GW_OK;
}

__declspec(dllexport) int __stdcall GW_ResetGateway(void) {
    g_dwCallCount++;
    g_dwTransportHandle = 0;
    g_wszCustomer[0] = L'\0';
    _set_last_error(L"GW_OK");
    return GW_OK;
}

__declspec(dllexport) int __cdecl GW_GetLastErrorTextW(wchar_t* pOutBuf, DWORD dwBufLen) {
    g_dwCallCount++;
    if (!pOutBuf || dwBufLen == 0) {
        return GW_ERR_BADPARAM;
    }
    lstrcpynW(pOutBuf, g_wszLastError, (int)dwBufLen);
    return GW_OK;
}

__declspec(dllexport) int __cdecl GW_GetDiagnosticsW(wchar_t* pOutBuf, DWORD dwBufLen) {
    g_dwCallCount++;
    if (!pOutBuf || dwBufLen < 32) {
        _set_last_error(L"diagnostics buffer too small");
        return GW_ERR_BADPARAM;
    }
    _snwprintf(pOutBuf, dwBufLen, L"calls=%lu|handle=%lu", (unsigned long)g_dwCallCount, (unsigned long)g_dwTransportHandle);
    pOutBuf[dwBufLen - 1] = L'\0';
    _set_last_error(L"GW_OK");
    return GW_OK;
}
