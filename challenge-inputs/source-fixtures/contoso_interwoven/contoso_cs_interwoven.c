/*
 * CONTOSO INTERWOVEN FIXTURE DLL
 *
 * Purpose:
 *   Ground-truth stress fixture for the MCP pipeline.
 *   This DLL intentionally depends on contoso_cs.dll at runtime and enforces
 *   call-order prerequisites so stage orchestration, retries, and regression
 *   handling can be tested deterministically.
 *
 * Build target:
 *   contoso_cs_interwoven.dll
 */

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ---------- Interwoven sentinel/error codes ---------- */
#define CI_OK                         0
#define CI_ERR_BASE_NOT_LOADED     -1001
#define CI_ERR_RESOLVE_FAILED      -1002
#define CI_ERR_BADPARAM            -1003
#define CI_ERR_PRECONDITION        -1004
#define CI_ERR_CUSTOMER_MISMATCH   -1005

/* ---------- Function pointer types from contoso_cs.dll ---------- */
typedef int   (__cdecl   *PFN_CS_Initialize)(void);
typedef int   (__cdecl   *PFN_CS_LookupCustomer)(const char*, char*, DWORD);
typedef int   (__stdcall *PFN_CS_GetAccountBalance)(const char*, DWORD*);
typedef int   (__cdecl   *PFN_CS_GetLoyaltyPoints)(const char*, DWORD*);
typedef int   (__stdcall *PFN_CS_RedeemLoyaltyPoints)(const char*, DWORD, DWORD*);
typedef int   (__cdecl   *PFN_CS_ProcessPayment)(const char*, DWORD, const char*);
typedef int   (__stdcall *PFN_CS_UnlockAccount)(const char*, const char*);
typedef DWORD (__cdecl   *PFN_CS_GetVersion)(void);
typedef int   (__cdecl   *PFN_CS_GetDiagnostics)(char*, DWORD);

typedef struct _CONTOSO_API {
    PFN_CS_Initialize       pInitialize;
    PFN_CS_LookupCustomer   pLookupCustomer;
    PFN_CS_GetAccountBalance pGetAccountBalance;
    PFN_CS_GetLoyaltyPoints pGetLoyaltyPoints;
    PFN_CS_RedeemLoyaltyPoints pRedeemLoyaltyPoints;
    PFN_CS_ProcessPayment   pProcessPayment;
    PFN_CS_UnlockAccount    pUnlockAccount;
    PFN_CS_GetVersion       pGetVersion;
    PFN_CS_GetDiagnostics   pGetDiagnostics;
} CONTOSO_API;

/* ---------- Global state ---------- */
static HMODULE     g_hContoso = NULL;
static CONTOSO_API g_api;
static BOOL        g_initialised = FALSE;
static BOOL        g_session_primed = FALSE;
static char        g_bound_customer[32] = "";
static char        g_last_error[256] = "no error";
static DWORD       g_bridge_calls = 0;

/* Token bytes that satisfy contoso XOR check (0xA5): E5 ^ 41 ^ 42 ^ 43 = A5 */
static const unsigned char g_unlock_token_bytes[5] = { 0xE5, 'A', 'B', 'C', 0x00 };
static const char* g_unlock_token_hex = "E5414243";

static void _set_last_error(const char* stage, int code) {
    _snprintf(g_last_error, sizeof(g_last_error) - 1,
        "stage=%s|code=%d|primed=%s|bound=%s",
        stage ? stage : "unknown",
        code,
        g_session_primed ? "yes" : "no",
        g_bound_customer[0] ? g_bound_customer : "(none)");
    g_last_error[sizeof(g_last_error) - 1] = '\0';
}

static int _copy_out(char* out, DWORD out_len, const char* text) {
    if (!out || out_len == 0) return CI_ERR_BADPARAM;
    if (!text) text = "";
    _snprintf(out, out_len - 1, "%s", text);
    out[out_len - 1] = '\0';
    return CI_OK;
}

static DWORD _rotate_left5(DWORD x) {
    return (x << 5) | (x >> 27);
}

static DWORD _expected_nonce_for(const char* cust_id) {
    DWORD acc = 0xC0117050UL;
    size_t i = 0;
    if (!cust_id) return 0;
    for (i = 0; cust_id[i]; i++) {
        acc = _rotate_left5(acc);
        acc ^= (unsigned char)cust_id[i];
    }
    return acc ^ 0x5A5A1357UL;
}

static int _hex_char_to_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

static int _decode_token_hex(const char* hex, unsigned char* out_bytes, size_t out_size) {
    size_t i = 0;
    if (!hex || !out_bytes || out_size < 5) return CI_ERR_BADPARAM;
    if (strlen(hex) < 8) return CI_ERR_BADPARAM;

    for (i = 0; i < 4; i++) {
        int hi = _hex_char_to_nibble(hex[i * 2]);
        int lo = _hex_char_to_nibble(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) return CI_ERR_BADPARAM;
        out_bytes[i] = (unsigned char)((hi << 4) | lo);
    }
    out_bytes[4] = 0x00;
    return CI_OK;
}

static int _load_contoso_api(void) {
    int init_rc = 0;

    if (g_hContoso && g_api.pInitialize && g_api.pLookupCustomer) {
        return CI_OK;
    }

    g_hContoso = LoadLibraryA("contoso_cs.dll");
    if (!g_hContoso) {
        _set_last_error("LoadLibraryA(contoso_cs.dll)", CI_ERR_BASE_NOT_LOADED);
        return CI_ERR_BASE_NOT_LOADED;
    }

    g_api.pInitialize = (PFN_CS_Initialize)GetProcAddress(g_hContoso, "CS_Initialize");
    g_api.pLookupCustomer = (PFN_CS_LookupCustomer)GetProcAddress(g_hContoso, "CS_LookupCustomer");
    g_api.pGetAccountBalance = (PFN_CS_GetAccountBalance)GetProcAddress(g_hContoso, "CS_GetAccountBalance");
    g_api.pGetLoyaltyPoints = (PFN_CS_GetLoyaltyPoints)GetProcAddress(g_hContoso, "CS_GetLoyaltyPoints");
    g_api.pRedeemLoyaltyPoints = (PFN_CS_RedeemLoyaltyPoints)GetProcAddress(g_hContoso, "CS_RedeemLoyaltyPoints");
    g_api.pProcessPayment = (PFN_CS_ProcessPayment)GetProcAddress(g_hContoso, "CS_ProcessPayment");
    g_api.pUnlockAccount = (PFN_CS_UnlockAccount)GetProcAddress(g_hContoso, "CS_UnlockAccount");
    g_api.pGetVersion = (PFN_CS_GetVersion)GetProcAddress(g_hContoso, "CS_GetVersion");
    g_api.pGetDiagnostics = (PFN_CS_GetDiagnostics)GetProcAddress(g_hContoso, "CS_GetDiagnostics");

    if (!g_api.pInitialize || !g_api.pLookupCustomer ||
        !g_api.pGetAccountBalance || !g_api.pGetLoyaltyPoints ||
        !g_api.pRedeemLoyaltyPoints || !g_api.pProcessPayment ||
        !g_api.pUnlockAccount || !g_api.pGetVersion || !g_api.pGetDiagnostics) {
        _set_last_error("GetProcAddress", CI_ERR_RESOLVE_FAILED);
        return CI_ERR_RESOLVE_FAILED;
    }

    init_rc = g_api.pInitialize();
    if (init_rc != 0) {
        _set_last_error("CS_Initialize", init_rc);
        return init_rc;
    }
    return CI_OK;
}

/* ---------- Exported API ---------- */

__declspec(dllexport) int __cdecl CI_Initialize(void) {
    int rc = _load_contoso_api();
    g_bridge_calls++;
    if (rc != CI_OK) return rc;
    g_initialised = TRUE;
    g_session_primed = FALSE;
    g_bound_customer[0] = '\0';
    _set_last_error("CI_Initialize", CI_OK);
    return CI_OK;
}

__declspec(dllexport) int __stdcall CI_BindCustomer(const char* lpszCustId) {
    int rc = 0;
    char scratch[512];
    g_bridge_calls++;

    if (!lpszCustId || !*lpszCustId) return CI_ERR_BADPARAM;
    if (!g_initialised) {
        rc = CI_Initialize();
        if (rc != CI_OK) return rc;
    }

    rc = g_api.pLookupCustomer(lpszCustId, scratch, sizeof(scratch));
    if (rc != 0) {
        _set_last_error("CS_LookupCustomer", rc);
        return rc;
    }

    _snprintf(g_bound_customer, sizeof(g_bound_customer) - 1, "%s", lpszCustId);
    g_bound_customer[sizeof(g_bound_customer) - 1] = '\0';
    g_session_primed = FALSE;
    _set_last_error("CI_BindCustomer", CI_OK);
    return CI_OK;
}

__declspec(dllexport) int __cdecl CI_GetExpectedNonce(DWORD* pdwNonceOut) {
    g_bridge_calls++;
    if (!pdwNonceOut) return CI_ERR_BADPARAM;
    if (!g_bound_customer[0]) return CI_ERR_PRECONDITION;
    *pdwNonceOut = _expected_nonce_for(g_bound_customer);
    _set_last_error("CI_GetExpectedNonce", CI_OK);
    return CI_OK;
}

__declspec(dllexport) int __cdecl CI_PrimeSession(DWORD dwNonce) {
    DWORD expected = 0;
    g_bridge_calls++;

    if (!g_initialised) return CI_ERR_PRECONDITION;
    if (!g_bound_customer[0]) return CI_ERR_PRECONDITION;

    expected = _expected_nonce_for(g_bound_customer);
    if (dwNonce != expected) {
        _set_last_error("CI_PrimeSession(wrong nonce)", CI_ERR_BADPARAM);
        return CI_ERR_BADPARAM;
    }

    g_session_primed = TRUE;
    _set_last_error("CI_PrimeSession", CI_OK);
    return CI_OK;
}

__declspec(dllexport) int __cdecl CI_GetUnlockTokenHex(char* pOutBuf, DWORD dwBufLen) {
    g_bridge_calls++;
    return _copy_out(pOutBuf, dwBufLen, g_unlock_token_hex);
}

__declspec(dllexport) int __stdcall CI_GetCompositeState(
    const char* lpszCustId,
    DWORD* pdwBalanceOut,
    DWORD* pdwPointsOut,
    char* pDiagOut,
    DWORD dwDiagLen) {
    int rc = 0;
    char diag[256];
    g_bridge_calls++;

    if (!lpszCustId || !pdwBalanceOut || !pdwPointsOut) return CI_ERR_BADPARAM;
    if (!g_initialised) {
        rc = CI_Initialize();
        if (rc != CI_OK) return rc;
    }

    rc = g_api.pGetAccountBalance(lpszCustId, pdwBalanceOut);
    if (rc != 0) {
        _set_last_error("CS_GetAccountBalance", rc);
        return rc;
    }

    rc = g_api.pGetLoyaltyPoints(lpszCustId, pdwPointsOut);
    if (rc != 0) {
        _set_last_error("CS_GetLoyaltyPoints", rc);
        return rc;
    }

    rc = g_api.pGetDiagnostics(diag, sizeof(diag));
    if (rc == 0 && pDiagOut && dwDiagLen > 0) {
        _copy_out(pDiagOut, dwDiagLen, diag);
    }

    _set_last_error("CI_GetCompositeState", CI_OK);
    return CI_OK;
}

__declspec(dllexport) int __cdecl CI_EarnThenRedeem(
    const char* lpszCustId,
    DWORD dwPayCents,
    DWORD dwRedeemPoints,
    DWORD* pdwCentsAddedOut) {
    int rc = 0;
    g_bridge_calls++;

    if (!lpszCustId || !pdwCentsAddedOut || dwPayCents == 0 || dwRedeemPoints == 0) {
        return CI_ERR_BADPARAM;
    }
    if (!g_initialised || !g_session_primed || !g_bound_customer[0]) {
        _set_last_error("CI_EarnThenRedeem(precondition)", CI_ERR_PRECONDITION);
        return CI_ERR_PRECONDITION;
    }
    if (_stricmp(lpszCustId, g_bound_customer) != 0) {
        _set_last_error("CI_EarnThenRedeem(customer mismatch)", CI_ERR_CUSTOMER_MISMATCH);
        return CI_ERR_CUSTOMER_MISMATCH;
    }

    rc = g_api.pProcessPayment(lpszCustId, dwPayCents, "ORD-IW-EARN");
    if (rc != 0) {
        _set_last_error("CS_ProcessPayment", rc);
        return rc;
    }

    rc = g_api.pRedeemLoyaltyPoints(lpszCustId, dwRedeemPoints, pdwCentsAddedOut);
    if (rc != 0) {
        _set_last_error("CS_RedeemLoyaltyPoints", rc);
        return rc;
    }

    _set_last_error("CI_EarnThenRedeem", CI_OK);
    return CI_OK;
}

__declspec(dllexport) int __stdcall CI_UnlockAndDebit(
    const char* lpszCustId,
    const char* lpszTokenHex,
    DWORD dwDebitCents) {
    int rc = 0;
    unsigned char token_bytes[5];
    g_bridge_calls++;

    if (!lpszCustId || dwDebitCents == 0) return CI_ERR_BADPARAM;
    if (!g_initialised || !g_session_primed) {
        _set_last_error("CI_UnlockAndDebit(precondition)", CI_ERR_PRECONDITION);
        return CI_ERR_PRECONDITION;
    }

    if (!lpszTokenHex || !*lpszTokenHex) lpszTokenHex = g_unlock_token_hex;

    rc = _decode_token_hex(lpszTokenHex, token_bytes, sizeof(token_bytes));
    if (rc != CI_OK) {
        _set_last_error("CI_UnlockAndDebit(token decode)", rc);
        return rc;
    }

    rc = g_api.pUnlockAccount(lpszCustId, (const char*)token_bytes);
    if (rc != 0) {
        _set_last_error("CS_UnlockAccount", rc);
        return rc;
    }

    rc = g_api.pProcessPayment(lpszCustId, dwDebitCents, "ORD-IW-DEBIT");
    if (rc != 0) {
        _set_last_error("CS_ProcessPayment(after unlock)", rc);
        return rc;
    }

    _set_last_error("CI_UnlockAndDebit", CI_OK);
    return CI_OK;
}

__declspec(dllexport) int __cdecl CI_GetVersionBridge(DWORD* pdwOut) {
    g_bridge_calls++;
    if (!pdwOut) return CI_ERR_BADPARAM;
    if (!g_initialised) {
        int rc = CI_Initialize();
        if (rc != CI_OK) return rc;
    }
    *pdwOut = g_api.pGetVersion();
    _set_last_error("CI_GetVersionBridge", CI_OK);
    return CI_OK;
}

__declspec(dllexport) int __cdecl CI_ResetSession(void) {
    g_bridge_calls++;
    g_session_primed = FALSE;
    g_bound_customer[0] = '\0';
    _set_last_error("CI_ResetSession", CI_OK);
    return CI_OK;
}

__declspec(dllexport) int __cdecl CI_GetLastErrorText(char* pOutBuf, DWORD dwBufLen) {
    g_bridge_calls++;
    return _copy_out(pOutBuf, dwBufLen, g_last_error);
}

__declspec(dllexport) int __cdecl CI_GetFixtureDiagnostics(char* pOutBuf, DWORD dwBufLen) {
    char s[256];
    _snprintf(s, sizeof(s) - 1,
        "calls=%lu|loaded=%s|primed=%s|bound=%s|unlock_hex=%s",
        (unsigned long)g_bridge_calls,
        g_initialised ? "yes" : "no",
        g_session_primed ? "yes" : "no",
        g_bound_customer[0] ? g_bound_customer : "(none)",
        g_unlock_token_hex);
    s[sizeof(s) - 1] = '\0';
    return _copy_out(pOutBuf, dwBufLen, s);
}

BOOL WINAPI DllMain(HINSTANCE hInstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)hInstDLL;
    (void)lpvReserved;
    if (fdwReason == DLL_PROCESS_DETACH) {
        if (g_hContoso) {
            FreeLibrary(g_hContoso);
            g_hContoso = NULL;
        }
    }
    return TRUE;
}

