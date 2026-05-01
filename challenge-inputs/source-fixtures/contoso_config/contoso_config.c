/*
 * CONTOSO CORPORATION
 * Configuration and Licensing Services Library
 * Version 1.1.0  (c) 2003-2006 Contoso Corp. All rights reserved.
 *
 * INTERNAL USE ONLY - NOT FOR DISTRIBUTION
 * Compiled with: MSVC-style mid-2000s profile /O2 /GS-
 */

#include <windows.h>
#include <stdio.h>
#include <string.h>

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

#define CFG_OK                   0
#define CFG_ERR_ENV_MISSING     -5001
#define CFG_ERR_BADPARAM        -5002
#define CFG_ERR_PRECONDITION    -5003
#define CFG_ERR_BADTICKET       -5004
#define CFG_ERR_BADNONCE        -5005
#define CFG_ERR_NOTLICENSED     -5006

#define CFG_MODE_MISSING         0
#define CFG_MODE_TRIAL           1
#define CFG_MODE_LICENSED        2

static BOOL  g_bInitialised = FALSE;
static DWORD g_dwCallCount = 0;
static DWORD g_dwMode = CFG_MODE_MISSING;
static DWORD g_dwLicenseNonce = 0;
static BOOL  g_bApproved = FALSE;
static char  g_szCustomer[16] = "";
static char  g_szTicket[24] = "";
static char  g_szLastError[128] = "CFG_OK";

static void _set_last_error(const char* pszText) {
    lstrcpynA(g_szLastError, pszText, (int)sizeof(g_szLastError));
}

static void _reset_runtime(void) {
    g_bInitialised = FALSE;
    g_dwMode = CFG_MODE_MISSING;
    g_dwLicenseNonce = 0;
    g_bApproved = FALSE;
    g_szCustomer[0] = '\0';
    g_szTicket[0] = '\0';
    _set_last_error("CFG_OK");
}

static void _get_config_path(char* pszOut, DWORD dwLen) {
    char szModulePath[MAX_PATH];
    char* pSlash;
    DWORD dwCount = GetModuleFileNameA((HMODULE)&__ImageBase, szModulePath, MAX_PATH);
    if (dwCount == 0 || dwCount >= MAX_PATH) {
        lstrcpynA(pszOut, "contoso_config.ini", (int)dwLen);
        return;
    }
    pSlash = strrchr(szModulePath, '\\');
    if (pSlash) {
        *(pSlash + 1) = '\0';
    } else {
        szModulePath[0] = '\0';
    }
    _snprintf(pszOut, dwLen, "%scontoso_config.ini", szModulePath);
    pszOut[dwLen - 1] = '\0';
}

static DWORD _read_mode_from_disk(void) {
    char szPath[MAX_PATH];
    char szBuf[64];
    DWORD dwRead = 0;
    HANDLE hFile;

    _get_config_path(szPath, sizeof(szPath));
    hFile = CreateFileA(szPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return CFG_MODE_MISSING;
    }

    ZeroMemory(szBuf, sizeof(szBuf));
    ReadFile(hFile, szBuf, sizeof(szBuf) - 1, &dwRead, NULL);
    CloseHandle(hFile);

    if (strstr(szBuf, "mode=licensed") != NULL) {
        return CFG_MODE_LICENSED;
    }
    if (strstr(szBuf, "mode=trial") != NULL) {
        return CFG_MODE_TRIAL;
    }
    return CFG_MODE_MISSING;
}

static int _write_mode_to_disk(DWORD dwMode) {
    char szPath[MAX_PATH];
    const char* pszBody = "mode=trial\n";
    DWORD dwWritten = 0;
    HANDLE hFile;

    if (dwMode == CFG_MODE_LICENSED) {
        pszBody = "mode=licensed\n";
    }

    _get_config_path(szPath, sizeof(szPath));
    hFile = CreateFileA(szPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        _set_last_error("failed to create config artifact");
        return CFG_ERR_ENV_MISSING;
    }

    if (!WriteFile(hFile, pszBody, (DWORD)lstrlenA(pszBody), &dwWritten, NULL)) {
        CloseHandle(hFile);
        _set_last_error("failed to write config artifact");
        return CFG_ERR_ENV_MISSING;
    }

    CloseHandle(hFile);
    return CFG_OK;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    (void)hinstDLL;
    (void)lpReserved;
    if (fdwReason == DLL_PROCESS_ATTACH) {
        _reset_runtime();
    }
    return TRUE;
}

__declspec(dllexport) int __cdecl CFG_SetupEnvironment(DWORD dwMode) {
    g_dwCallCount++;
    if (dwMode != CFG_MODE_TRIAL && dwMode != CFG_MODE_LICENSED) {
        _set_last_error("unsupported config mode");
        return CFG_ERR_BADPARAM;
    }
    if (_write_mode_to_disk(dwMode) != CFG_OK) {
        return CFG_ERR_ENV_MISSING;
    }
    g_dwMode = dwMode;
    _set_last_error("CFG_OK");
    return CFG_OK;
}

__declspec(dllexport) int __cdecl CFG_Initialize(void) {
    g_dwCallCount++;
    g_dwMode = _read_mode_from_disk();
    if (g_dwMode == CFG_MODE_MISSING) {
        g_bInitialised = FALSE;
        _set_last_error("config artifact missing");
        return CFG_ERR_ENV_MISSING;
    }
    g_bInitialised = TRUE;
    g_bApproved = FALSE;
    g_dwLicenseNonce = 0xC0DE0000u | (g_dwCallCount & 0x0FFFu);
    _set_last_error("CFG_OK");
    return CFG_OK;
}

__declspec(dllexport) int __stdcall CFG_BindCustomer(const char* lpszCustId) {
    g_dwCallCount++;
    if (!g_bInitialised) {
        _set_last_error("not initialized");
        return CFG_ERR_PRECONDITION;
    }
    if (!lpszCustId || lstrlenA(lpszCustId) < 6) {
        _set_last_error("bad customer id");
        return CFG_ERR_BADPARAM;
    }
    lstrcpynA(g_szCustomer, lpszCustId, (int)sizeof(g_szCustomer));
    _set_last_error("CFG_OK");
    return CFG_OK;
}

__declspec(dllexport) int __cdecl CFG_GetMode(DWORD* pdwModeOut) {
    g_dwCallCount++;
    if (!pdwModeOut) {
        _set_last_error("missing mode output");
        return CFG_ERR_BADPARAM;
    }
    *pdwModeOut = g_dwMode;
    _set_last_error("CFG_OK");
    return CFG_OK;
}

__declspec(dllexport) int __cdecl CFG_OpenLicensedWindow(char* pOutTicket, DWORD dwBufLen) {
    g_dwCallCount++;
    if (!g_bInitialised || g_szCustomer[0] == '\0') {
        _set_last_error("customer binding required");
        return CFG_ERR_PRECONDITION;
    }
    if (!pOutTicket || dwBufLen < 16) {
        _set_last_error("ticket buffer too small");
        return CFG_ERR_BADPARAM;
    }
    if (g_dwMode != CFG_MODE_LICENSED) {
        _set_last_error("licensed mode required");
        return CFG_ERR_NOTLICENSED;
    }
    _snprintf(g_szTicket, sizeof(g_szTicket), "CFG-%s-%02lu", g_szCustomer, (unsigned long)(g_dwCallCount & 0xFFu));
    g_szTicket[sizeof(g_szTicket) - 1] = '\0';
    lstrcpynA(pOutTicket, g_szTicket, (int)dwBufLen);
    _set_last_error("CFG_OK");
    return CFG_OK;
}

__declspec(dllexport) int __stdcall CFG_GetLicenseNonce(const char* lpszTicket, DWORD* pdwNonceOut) {
    g_dwCallCount++;
    if (!lpszTicket || !pdwNonceOut) {
        _set_last_error("ticket and nonce out required");
        return CFG_ERR_BADPARAM;
    }
    if (lstrcmpiA(lpszTicket, g_szTicket) != 0) {
        _set_last_error("unknown config ticket");
        return CFG_ERR_BADTICKET;
    }
    *pdwNonceOut = g_dwLicenseNonce;
    _set_last_error("CFG_OK");
    return CFG_OK;
}

__declspec(dllexport) int __cdecl CFG_ApproveLicensedCall(const char* lpszTicket, DWORD dwNonce) {
    g_dwCallCount++;
    if (!lpszTicket) {
        _set_last_error("ticket required");
        return CFG_ERR_BADPARAM;
    }
    if (lstrcmpiA(lpszTicket, g_szTicket) != 0) {
        _set_last_error("unknown config ticket");
        return CFG_ERR_BADTICKET;
    }
    if (dwNonce != g_dwLicenseNonce) {
        _set_last_error("license nonce mismatch");
        return CFG_ERR_BADNONCE;
    }
    g_bApproved = TRUE;
    _set_last_error("CFG_OK");
    return CFG_OK;
}

__declspec(dllexport) int __stdcall CFG_RunCustomerSync(const char* lpszTicket) {
    g_dwCallCount++;
    if (!g_bApproved) {
        _set_last_error("licensed approval required");
        return CFG_ERR_PRECONDITION;
    }
    if (!lpszTicket || lstrcmpiA(lpszTicket, g_szTicket) != 0) {
        _set_last_error("unknown config ticket");
        return CFG_ERR_BADTICKET;
    }
    _set_last_error("CFG_OK");
    return CFG_OK;
}

__declspec(dllexport) int __cdecl CFG_GetConfigState(char* pOutBuf, DWORD dwBufLen) {
    const char* pszMode = "missing";
    g_dwCallCount++;
    if (!pOutBuf || dwBufLen < 24) {
        _set_last_error("state buffer too small");
        return CFG_ERR_BADPARAM;
    }
    if (g_dwMode == CFG_MODE_TRIAL) pszMode = "trial";
    if (g_dwMode == CFG_MODE_LICENSED) pszMode = "licensed";
    _snprintf(pOutBuf, dwBufLen, "mode=%s|cust=%s|approved=%lu", pszMode, g_szCustomer[0] ? g_szCustomer : "none", (unsigned long)(g_bApproved ? 1 : 0));
    pOutBuf[dwBufLen - 1] = '\0';
    _set_last_error("CFG_OK");
    return CFG_OK;
}

__declspec(dllexport) int __cdecl CFG_ResetEnvironment(void) {
    char szPath[MAX_PATH];
    g_dwCallCount++;
    _get_config_path(szPath, sizeof(szPath));
    DeleteFileA(szPath);
    _reset_runtime();
    return CFG_OK;
}

__declspec(dllexport) int __cdecl CFG_GetLastErrorText(char* pOutBuf, DWORD dwBufLen) {
    g_dwCallCount++;
    if (!pOutBuf || dwBufLen == 0) {
        return CFG_ERR_BADPARAM;
    }
    lstrcpynA(pOutBuf, g_szLastError, (int)dwBufLen);
    return CFG_OK;
}

__declspec(dllexport) int __cdecl CFG_GetDiagnostics(char* pOutBuf, DWORD dwBufLen) {
    g_dwCallCount++;
    if (!pOutBuf || dwBufLen < 48) {
        _set_last_error("diagnostics buffer too small");
        return CFG_ERR_BADPARAM;
    }
    _snprintf(pOutBuf, dwBufLen, "calls=%lu|mode=%lu|cust=%s", (unsigned long)g_dwCallCount, (unsigned long)g_dwMode, g_szCustomer[0] ? g_szCustomer : "none");
    pOutBuf[dwBufLen - 1] = '\0';
    _set_last_error("CFG_OK");
    return CFG_OK;
}
