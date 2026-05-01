/*
 * CONTOSO CORPORATION
 * Transport and Session Services Library
 * Version 0.9.0  (c) 2004-2006 Contoso Corp. All rights reserved.
 *
 * INTERNAL USE ONLY - NOT FOR DISTRIBUTION
 * Hard-tier fixture focused on structs and opaque numeric handles.
 */

#include <windows.h>
#include <stdio.h>
#include <string.h>

#define TP_OK                  0
#define TP_ERR_ENV            -6001
#define TP_ERR_BADPARAM       -6002
#define TP_ERR_PRECONDITION   -6003
#define TP_ERR_BADHANDLE      -6004
#define TP_ERR_BADNONCE       -6005
#define TP_ERR_LIMIT          -6006

typedef struct _TP_OPEN_REQ {
    char  szCustId[16];
    DWORD dwFlags;
    DWORD dwMode;
} TP_OPEN_REQ;

typedef struct _TP_PACKET_REQ {
    DWORD dwSessionHandle;
    char  szSku[24];
    WORD  wQty;
    DWORD dwNonce;
} TP_PACKET_REQ;

typedef struct _TP_SESSION_STATE {
    DWORD dwSessionHandle;
    DWORD dwLastNonce;
    DWORD dwFlags;
    DWORD dwBlobToken;
} TP_SESSION_STATE;

typedef struct _TP_SESSION_REC {
    DWORD dwHandle;
    DWORD dwNonce;
    DWORD dwFlags;
    DWORD dwBlobToken;
    char  szCustId[16];
    char  szSku[24];
    WORD  wQty;
    BOOL  bOpen;
} TP_SESSION_REC;

static BOOL g_bInitialised = FALSE;
static DWORD g_dwCallCount = 0;
static char g_szBoundCustomer[16] = "";
static char g_szLastError[128] = "TP_OK";
static TP_SESSION_REC g_arSessions[8];
static DWORD g_dwSessionCount = 0;

static void _set_last_error(const char* pszText) {
    lstrcpynA(g_szLastError, pszText, (int)sizeof(g_szLastError));
}

static TP_SESSION_REC* _find_session(DWORD dwHandle) {
    DWORD i;
    for (i = 0; i < g_dwSessionCount; ++i) {
        if (g_arSessions[i].bOpen && g_arSessions[i].dwHandle == dwHandle) {
            return &g_arSessions[i];
        }
    }
    return NULL;
}

static int _ensure_config_bridge(void) {
    HMODULE hCfg = LoadLibraryA("contoso_config.dll");
    if (!hCfg) {
        _set_last_error("contoso_config.dll not loadable");
        return TP_ERR_ENV;
    }
    FreeLibrary(hCfg);
    return TP_OK;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    (void)hinstDLL;
    (void)lpReserved;
    if (fdwReason == DLL_PROCESS_ATTACH) {
        ZeroMemory(g_arSessions, sizeof(g_arSessions));
        g_dwSessionCount = 0;
        g_szBoundCustomer[0] = '\0';
    }
    return TRUE;
}

__declspec(dllexport) int __cdecl TP_Initialize(void) {
    g_dwCallCount++;
    if (_ensure_config_bridge() != TP_OK) {
        return TP_ERR_ENV;
    }
    g_bInitialised = TRUE;
    _set_last_error("TP_OK");
    return TP_OK;
}

__declspec(dllexport) int __stdcall TP_BindCustomer(const char* lpszCustId) {
    g_dwCallCount++;
    if (!g_bInitialised) {
        _set_last_error("transport not initialized");
        return TP_ERR_PRECONDITION;
    }
    if (!lpszCustId || lstrlenA(lpszCustId) < 6) {
        _set_last_error("bad customer id");
        return TP_ERR_BADPARAM;
    }
    lstrcpynA(g_szBoundCustomer, lpszCustId, (int)sizeof(g_szBoundCustomer));
    _set_last_error("TP_OK");
    return TP_OK;
}

__declspec(dllexport) int __cdecl TP_GetStructVersion(DWORD* pdwVersionOut) {
    g_dwCallCount++;
    if (!pdwVersionOut) {
        _set_last_error("missing version output");
        return TP_ERR_BADPARAM;
    }
    *pdwVersionOut = 0x00010002u;
    _set_last_error("TP_OK");
    return TP_OK;
}

__declspec(dllexport) int __stdcall TP_OpenSession(TP_OPEN_REQ* pReq, DWORD* pdwHandleOut) {
    TP_SESSION_REC* pRec;
    g_dwCallCount++;
    if (!g_bInitialised || g_szBoundCustomer[0] == '\0') {
        _set_last_error("customer bind required");
        return TP_ERR_PRECONDITION;
    }
    if (!pReq || !pdwHandleOut) {
        _set_last_error("open session struct required");
        return TP_ERR_BADPARAM;
    }
    if (g_dwSessionCount >= 8) {
        _set_last_error("session limit reached");
        return TP_ERR_LIMIT;
    }
    if (lstrcmpiA(pReq->szCustId, g_szBoundCustomer) != 0) {
        _set_last_error("session customer mismatch");
        return TP_ERR_BADPARAM;
    }
    pRec = &g_arSessions[g_dwSessionCount++];
    ZeroMemory(pRec, sizeof(*pRec));
    pRec->dwHandle = 0x40001000u + g_dwSessionCount;
    pRec->dwNonce = 0x0100A000u + g_dwSessionCount;
    pRec->dwFlags = pReq->dwFlags;
    pRec->dwBlobToken = 0;
    lstrcpynA(pRec->szCustId, pReq->szCustId, (int)sizeof(pRec->szCustId));
    pRec->bOpen = TRUE;
    *pdwHandleOut = pRec->dwHandle;
    _set_last_error("TP_OK");
    return TP_OK;
}

__declspec(dllexport) int __cdecl TP_GetSessionNonce(DWORD dwSessionHandle, DWORD* pdwNonceOut) {
    TP_SESSION_REC* pRec;
    g_dwCallCount++;
    if (!pdwNonceOut) {
        _set_last_error("missing nonce output");
        return TP_ERR_BADPARAM;
    }
    pRec = _find_session(dwSessionHandle);
    if (!pRec) {
        _set_last_error("unknown transport handle");
        return TP_ERR_BADHANDLE;
    }
    *pdwNonceOut = pRec->dwNonce;
    _set_last_error("TP_OK");
    return TP_OK;
}

__declspec(dllexport) int __stdcall TP_SubmitPacket(TP_PACKET_REQ* pReq, DWORD* pdwAuthCodeOut) {
    TP_SESSION_REC* pRec;
    g_dwCallCount++;
    if (!pReq || !pdwAuthCodeOut) {
        _set_last_error("packet struct required");
        return TP_ERR_BADPARAM;
    }
    pRec = _find_session(pReq->dwSessionHandle);
    if (!pRec) {
        _set_last_error("unknown transport handle");
        return TP_ERR_BADHANDLE;
    }
    if (pReq->dwNonce != pRec->dwNonce) {
        _set_last_error("transport nonce mismatch");
        return TP_ERR_BADNONCE;
    }
    lstrcpynA(pRec->szSku, pReq->szSku, (int)sizeof(pRec->szSku));
    pRec->wQty = pReq->wQty;
    *pdwAuthCodeOut = 0x55000000u | (pRec->dwHandle & 0xFFFFu);
    _set_last_error("TP_OK");
    return TP_OK;
}

__declspec(dllexport) int __cdecl TP_ReadSessionState(DWORD dwSessionHandle, TP_SESSION_STATE* pOutState) {
    TP_SESSION_REC* pRec;
    g_dwCallCount++;
    if (!pOutState) {
        _set_last_error("session state out required");
        return TP_ERR_BADPARAM;
    }
    pRec = _find_session(dwSessionHandle);
    if (!pRec) {
        _set_last_error("unknown transport handle");
        return TP_ERR_BADHANDLE;
    }
    pOutState->dwSessionHandle = pRec->dwHandle;
    pOutState->dwLastNonce = pRec->dwNonce;
    pOutState->dwFlags = pRec->dwFlags;
    pOutState->dwBlobToken = pRec->dwBlobToken;
    _set_last_error("TP_OK");
    return TP_OK;
}

__declspec(dllexport) int __stdcall TP_AllocBlobToken(DWORD dwSessionHandle, DWORD* pdwBlobTokenOut) {
    TP_SESSION_REC* pRec;
    g_dwCallCount++;
    if (!pdwBlobTokenOut) {
        _set_last_error("blob token out required");
        return TP_ERR_BADPARAM;
    }
    pRec = _find_session(dwSessionHandle);
    if (!pRec) {
        _set_last_error("unknown transport handle");
        return TP_ERR_BADHANDLE;
    }
    pRec->dwBlobToken = 0x70000000u | (dwSessionHandle & 0x0FFFu);
    *pdwBlobTokenOut = pRec->dwBlobToken;
    _set_last_error("TP_OK");
    return TP_OK;
}

__declspec(dllexport) int __cdecl TP_FreeBlobToken(DWORD dwBlobToken) {
    DWORD i;
    g_dwCallCount++;
    for (i = 0; i < g_dwSessionCount; ++i) {
        if (g_arSessions[i].dwBlobToken == dwBlobToken) {
            g_arSessions[i].dwBlobToken = 0;
            _set_last_error("TP_OK");
            return TP_OK;
        }
    }
    _set_last_error("unknown blob token");
    return TP_ERR_BADHANDLE;
}

__declspec(dllexport) int __stdcall TP_CloseSession(DWORD dwSessionHandle) {
    TP_SESSION_REC* pRec;
    g_dwCallCount++;
    pRec = _find_session(dwSessionHandle);
    if (!pRec) {
        _set_last_error("unknown transport handle");
        return TP_ERR_BADHANDLE;
    }
    pRec->bOpen = FALSE;
    _set_last_error("TP_OK");
    return TP_OK;
}

__declspec(dllexport) int __cdecl TP_GetLastErrorText(char* pOutBuf, DWORD dwBufLen) {
    g_dwCallCount++;
    if (!pOutBuf || dwBufLen == 0) {
        return TP_ERR_BADPARAM;
    }
    lstrcpynA(pOutBuf, g_szLastError, (int)dwBufLen);
    return TP_OK;
}

__declspec(dllexport) int __cdecl TP_GetDiagnostics(char* pOutBuf, DWORD dwBufLen) {
    g_dwCallCount++;
    if (!pOutBuf || dwBufLen < 48) {
        _set_last_error("diagnostics buffer too small");
        return TP_ERR_BADPARAM;
    }
    _snprintf(pOutBuf, dwBufLen, "calls=%lu|cust=%s|sessions=%lu", (unsigned long)g_dwCallCount, g_szBoundCustomer[0] ? g_szBoundCustomer : "none", (unsigned long)g_dwSessionCount);
    pOutBuf[dwBufLen - 1] = '\0';
    _set_last_error("TP_OK");
    return TP_OK;
}
