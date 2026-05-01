/*
 * CONTOSO CORPORATION
 * Pricing, Rules & Quote Services Library
 * Version 1.1.2 (c) 2003-2006 Contoso Corp. All rights reserved.
 *
 * INTERNAL USE ONLY - NOT FOR DISTRIBUTION
 * Intended compiler profile: MSVC 7.1 / 8.0 transition-era Win32 C
 * Local build fallback: modern MSVC with old-style source/ABI discipline
 */

#include <windows.h>
#include <stdio.h>
#include <string.h>

#define PR_OK                    0
#define PR_ERR_BASE_NOT_LOADED  -3001
#define PR_ERR_RESOLVE_FAILED   -3002
#define PR_ERR_BADPARAM         -3003
#define PR_ERR_PRECONDITION     -3004
#define PR_ERR_RULEDENIED       -3005
#define PR_ERR_BADQUOTE         -3006
#define PR_ERR_STATE            -3007

#define PR_FMT_AUDIT             1
#define PR_FMT_LEDGER            2

typedef int (__cdecl   *PFN_CS_Initialize)(void);
typedef int (__cdecl   *PFN_CS_LookupCustomer)(const char*, char*, DWORD);
typedef int (__cdecl   *PFN_CP_Initialize)(void);
typedef int (__cdecl   *PFN_CP_LookupProduct)(const char*, char*, DWORD);
typedef int (__stdcall *PFN_CP_CheckStock)(const char*, DWORD*);
typedef int (__cdecl   *PFN_CP_CalculateTotal)(const char*, WORD, DWORD, DWORD*, DWORD*, DWORD*);

typedef struct _PRICING_API {
    PFN_CS_Initialize      pCsInitialize;
    PFN_CS_LookupCustomer  pCsLookupCustomer;
    PFN_CP_Initialize      pCpInitialize;
    PFN_CP_LookupProduct   pCpLookupProduct;
    PFN_CP_CheckStock      pCpCheckStock;
    PFN_CP_CalculateTotal  pCpCalculateTotal;
} PRICING_API;

static HMODULE g_hCs = NULL;
static HMODULE g_hPayments = NULL;
static PRICING_API g_api;

static BOOL  g_initialised = FALSE;
static BOOL  g_customer_bound = FALSE;
static BOOL  g_sku_loaded = FALSE;
static BOOL  g_quote_open = FALSE;
static BOOL  g_rule_approved = FALSE;
static BOOL  g_quote_sealed = FALSE;

static char  g_bound_customer[32] = "";
static char  g_bound_sku[32] = "";
static char  g_customer_tier[16] = "Standard";
static char  g_quote_id[32] = "";
static char  g_last_rule_text[128] = "no rule";
static char  g_last_error[256] = "no error";
static WORD  g_bound_qty = 0;
static DWORD g_last_nonce = 0;
static DWORD g_discount_bps = 0;
static DWORD g_last_subtotal = 0;
static DWORD g_last_tax = 0;
static DWORD g_last_total = 0;
static DWORD g_call_count = 0;

static void _set_last_error(const char* stage, int code) {
    _snprintf(g_last_error, sizeof(g_last_error) - 1,
        "stage=%s|code=%d|cust=%s|sku=%s|quote=%s|tier=%s",
        stage ? stage : "unknown",
        code,
        g_bound_customer[0] ? g_bound_customer : "(none)",
        g_bound_sku[0] ? g_bound_sku : "(none)",
        g_quote_id[0] ? g_quote_id : "(none)",
        g_customer_tier);
    g_last_error[sizeof(g_last_error) - 1] = '\0';
}

static int _copy_out(char* out, DWORD out_len, const char* text) {
    if (!out || out_len == 0) return PR_ERR_BADPARAM;
    if (!text) text = "";
    _snprintf(out, out_len - 1, "%s", text);
    out[out_len - 1] = '\0';
    return PR_OK;
}

static DWORD _rol3(DWORD x) {
    return (x << 3) | (x >> 29);
}

static DWORD _compute_quote_nonce(const char* quote_id) {
    DWORD acc = 0x50525154UL;
    size_t i = 0;
    if (!quote_id) return 0;
    for (i = 0; quote_id[i]; i++) {
        acc = _rol3(acc);
        acc ^= (unsigned char)quote_id[i];
    }
    return acc ^ 0x22773311UL;
}

static BOOL _tier_is_goldish(void) {
    return (_stricmp(g_customer_tier, "Gold") == 0 || _stricmp(g_customer_tier, "Platinum") == 0);
}

static int _refresh_total(DWORD discount_bps) {
    int rc = 0;
    if (!g_sku_loaded || g_bound_qty == 0) return PR_ERR_PRECONDITION;
    rc = g_api.pCpCalculateTotal(
        g_bound_sku,
        g_bound_qty,
        discount_bps,
        &g_last_subtotal,
        &g_last_tax,
        &g_last_total);
    if (rc != 0) {
        _set_last_error("CP_CalculateTotal", rc);
        return rc;
    }
    g_discount_bps = discount_bps;
    return PR_OK;
}

static void _extract_tier(const char* record_text) {
    const char* p = strstr(record_text, "tier=");
    if (!p) {
        lstrcpyA(g_customer_tier, "Standard");
        return;
    }
    p += 5;
    if (_strnicmp(p, "Platinum", 8) == 0) lstrcpyA(g_customer_tier, "Platinum");
    else if (_strnicmp(p, "Gold", 4) == 0) lstrcpyA(g_customer_tier, "Gold");
    else if (_strnicmp(p, "Silver", 6) == 0) lstrcpyA(g_customer_tier, "Silver");
    else lstrcpyA(g_customer_tier, "Standard");
}

static int _load_dependencies(void) {
    char scratch[512];
    int rc = 0;

    if (g_hCs && g_hPayments && g_api.pCsLookupCustomer && g_api.pCpCalculateTotal) return PR_OK;

    g_hCs = LoadLibraryA("contoso_cs.dll");
    g_hPayments = LoadLibraryA("contoso_payments.dll");
    if (!g_hCs || !g_hPayments) {
        _set_last_error("LoadLibraryA", PR_ERR_BASE_NOT_LOADED);
        return PR_ERR_BASE_NOT_LOADED;
    }

    g_api.pCsInitialize = (PFN_CS_Initialize)GetProcAddress(g_hCs, "CS_Initialize");
    g_api.pCsLookupCustomer = (PFN_CS_LookupCustomer)GetProcAddress(g_hCs, "CS_LookupCustomer");
    g_api.pCpInitialize = (PFN_CP_Initialize)GetProcAddress(g_hPayments, "CP_Initialize");
    g_api.pCpLookupProduct = (PFN_CP_LookupProduct)GetProcAddress(g_hPayments, "CP_LookupProduct");
    g_api.pCpCheckStock = (PFN_CP_CheckStock)GetProcAddress(g_hPayments, "CP_CheckStock");
    g_api.pCpCalculateTotal = (PFN_CP_CalculateTotal)GetProcAddress(g_hPayments, "CP_CalculateTotal");

    if (!g_api.pCsInitialize || !g_api.pCsLookupCustomer || !g_api.pCpInitialize ||
        !g_api.pCpLookupProduct || !g_api.pCpCheckStock || !g_api.pCpCalculateTotal) {
        _set_last_error("GetProcAddress", PR_ERR_RESOLVE_FAILED);
        return PR_ERR_RESOLVE_FAILED;
    }

    rc = g_api.pCsInitialize();
    if (rc != 0) {
        _set_last_error("CS_Initialize", rc);
        return rc;
    }
    rc = g_api.pCpInitialize();
    if (rc != 0) {
        _set_last_error("CP_Initialize", rc);
        return rc;
    }
    rc = g_api.pCsLookupCustomer("CUST-001", scratch, sizeof(scratch));
    if (rc != 0) {
        _set_last_error("CS_LookupCustomer", rc);
        return rc;
    }

    return PR_OK;
}

__declspec(dllexport) int __cdecl PR_Initialize(void) {
    int rc = _load_dependencies();
    g_call_count++;
    if (rc != PR_OK) return rc;
    g_initialised = TRUE;
    g_customer_bound = FALSE;
    g_sku_loaded = FALSE;
    g_quote_open = FALSE;
    g_rule_approved = FALSE;
    g_quote_sealed = FALSE;
    g_bound_customer[0] = '\0';
    g_bound_sku[0] = '\0';
    g_quote_id[0] = '\0';
    lstrcpyA(g_customer_tier, "Standard");
    lstrcpyA(g_last_rule_text, "initialized");
    _set_last_error("PR_Initialize", PR_OK);
    return PR_OK;
}

__declspec(dllexport) int __stdcall PR_BindCustomer(const char* lpszCustId) {
    int rc = 0;
    char scratch[512];
    g_call_count++;
    if (!lpszCustId || !*lpszCustId) return PR_ERR_BADPARAM;
    if (!g_initialised) {
        rc = PR_Initialize();
        if (rc != PR_OK) return rc;
    }

    rc = g_api.pCsLookupCustomer(lpszCustId, scratch, sizeof(scratch));
    if (rc != 0) {
        _set_last_error("CS_LookupCustomer", rc);
        return rc;
    }

    _snprintf(g_bound_customer, sizeof(g_bound_customer) - 1, "%s", lpszCustId);
    g_bound_customer[sizeof(g_bound_customer) - 1] = '\0';
    _extract_tier(scratch);
    g_customer_bound = TRUE;
    g_sku_loaded = FALSE;
    g_quote_open = FALSE;
    g_rule_approved = FALSE;
    g_quote_sealed = FALSE;
    lstrcpyA(g_last_rule_text, "customer-bound");
    _set_last_error("PR_BindCustomer", PR_OK);
    return PR_OK;
}

__declspec(dllexport) int __cdecl PR_LoadSkuContext(const char* lpszSku, WORD wQty) {
    int rc = 0;
    char scratch[512];
    DWORD available = 0;
    g_call_count++;
    if (!lpszSku || !*lpszSku || wQty == 0) return PR_ERR_BADPARAM;
    if (!g_customer_bound) return PR_ERR_PRECONDITION;

    rc = g_api.pCpLookupProduct(lpszSku, scratch, sizeof(scratch));
    if (rc != 0) {
        _set_last_error("CP_LookupProduct", rc);
        return rc;
    }
    rc = g_api.pCpCheckStock(lpszSku, &available);
    if (rc != 0) {
        _set_last_error("CP_CheckStock", rc);
        return rc;
    }
    if (available < wQty) {
        _set_last_error("PR_LoadSkuContext(stock)", PR_ERR_RULEDENIED);
        return PR_ERR_RULEDENIED;
    }

    _snprintf(g_bound_sku, sizeof(g_bound_sku) - 1, "%s", lpszSku);
    g_bound_sku[sizeof(g_bound_sku) - 1] = '\0';
    g_bound_qty = wQty;
    g_sku_loaded = TRUE;
    g_quote_open = FALSE;
    g_rule_approved = FALSE;
    g_quote_sealed = FALSE;
    lstrcpyA(g_last_rule_text, "sku-loaded");
    _set_last_error("PR_LoadSkuContext", PR_OK);
    return PR_OK;
}

__declspec(dllexport) int __stdcall PR_BeginQuote(char* pOutQuoteId, DWORD dwBufLen) {
    int rc = 0;
    g_call_count++;
    if (!pOutQuoteId || dwBufLen < 24) return PR_ERR_BADPARAM;
    if (!g_customer_bound || !g_sku_loaded) return PR_ERR_PRECONDITION;

    rc = _refresh_total(0);
    if (rc != PR_OK) return rc;

    _snprintf(g_quote_id, sizeof(g_quote_id) - 1, "PRQ-%s-%02u", g_bound_customer, (unsigned)g_bound_qty);
    g_quote_id[sizeof(g_quote_id) - 1] = '\0';
    g_last_nonce = _compute_quote_nonce(g_quote_id);
    g_quote_open = TRUE;
    g_rule_approved = FALSE;
    g_quote_sealed = FALSE;
    lstrcpyA(g_last_rule_text, "quote-open");
    _set_last_error("PR_BeginQuote", PR_OK);
    return _copy_out(pOutQuoteId, dwBufLen, g_quote_id);
}

__declspec(dllexport) int __cdecl PR_GetQuoteNonce(const char* lpszQuoteId, DWORD* pdwNonceOut) {
    g_call_count++;
    if (!lpszQuoteId || !pdwNonceOut) return PR_ERR_BADPARAM;
    if (!g_quote_open) return PR_ERR_PRECONDITION;
    if (_stricmp(lpszQuoteId, g_quote_id) != 0) return PR_ERR_BADQUOTE;

    *pdwNonceOut = g_last_nonce;
    _set_last_error("PR_GetQuoteNonce", PR_OK);
    return PR_OK;
}

__declspec(dllexport) int __stdcall PR_ApproveRuleWindow(const char* lpszQuoteId, DWORD dwNonce) {
    g_call_count++;
    if (!lpszQuoteId) return PR_ERR_BADPARAM;
    if (!g_quote_open) return PR_ERR_PRECONDITION;
    if (_stricmp(lpszQuoteId, g_quote_id) != 0) return PR_ERR_BADQUOTE;
    if (dwNonce != g_last_nonce) return PR_ERR_RULEDENIED;

    g_rule_approved = TRUE;
    lstrcpyA(g_last_rule_text, "rule-window-approved");
    _set_last_error("PR_ApproveRuleWindow", PR_OK);
    return PR_OK;
}

__declspec(dllexport) int __cdecl PR_ApplyTierDiscount(
    const char* lpszQuoteId,
    DWORD dwRequestedBps,
    DWORD* pdwAppliedBpsOut) {
    int rc = 0;
    DWORD ceiling = 0;
    g_call_count++;
    if (!lpszQuoteId || !pdwAppliedBpsOut) return PR_ERR_BADPARAM;
    if (!g_rule_approved) return PR_ERR_PRECONDITION;
    if (_stricmp(lpszQuoteId, g_quote_id) != 0) return PR_ERR_BADQUOTE;

    if (_stricmp(g_customer_tier, "Platinum") == 0) ceiling = 2200;
    else if (_stricmp(g_customer_tier, "Gold") == 0) ceiling = 1500;
    else if (_stricmp(g_customer_tier, "Silver") == 0) ceiling = 500;
    else ceiling = 0;

    if (dwRequestedBps == 0 || dwRequestedBps > ceiling) {
        _set_last_error("PR_ApplyTierDiscount(denied)", PR_ERR_RULEDENIED);
        return PR_ERR_RULEDENIED;
    }

    rc = _refresh_total(dwRequestedBps);
    if (rc != PR_OK) return rc;
    *pdwAppliedBpsOut = dwRequestedBps;
    lstrcpyA(g_last_rule_text, "tier-discount-applied");
    _set_last_error("PR_ApplyTierDiscount", PR_OK);
    return PR_OK;
}

__declspec(dllexport) int __stdcall PR_ApplyBundleOverride(
    const char* lpszQuoteId,
    DWORD dwFormatCode,
    DWORD* pdwTotalOut) {
    int rc = 0;
    DWORD override_bps = 0;
    g_call_count++;
    if (!lpszQuoteId || !pdwTotalOut) return PR_ERR_BADPARAM;
    if (!g_rule_approved) return PR_ERR_PRECONDITION;
    if (_stricmp(lpszQuoteId, g_quote_id) != 0) return PR_ERR_BADQUOTE;

    if (dwFormatCode == PR_FMT_AUDIT && g_bound_qty >= 3 && _tier_is_goldish()) {
        override_bps = g_discount_bps + 300;
    } else if (dwFormatCode == PR_FMT_LEDGER && g_bound_qty >= 2) {
        override_bps = g_discount_bps + 150;
    } else {
        _set_last_error("PR_ApplyBundleOverride(denied)", PR_ERR_RULEDENIED);
        return PR_ERR_RULEDENIED;
    }

    rc = _refresh_total(override_bps);
    if (rc != PR_OK) return rc;
    *pdwTotalOut = g_last_total;
    lstrcpyA(g_last_rule_text, "bundle-override-applied");
    _set_last_error("PR_ApplyBundleOverride", PR_OK);
    return PR_OK;
}

__declspec(dllexport) int __cdecl PR_SealQuote(
    const char* lpszQuoteId,
    char* pOutSealCode,
    DWORD dwBufLen) {
    char seal[64];
    g_call_count++;
    if (!lpszQuoteId || !pOutSealCode || dwBufLen < 16) return PR_ERR_BADPARAM;
    if (!g_rule_approved) return PR_ERR_PRECONDITION;
    if (_stricmp(lpszQuoteId, g_quote_id) != 0) return PR_ERR_BADQUOTE;

    _snprintf(seal, sizeof(seal) - 1, "SEAL-%04X-%04X",
        (unsigned)(g_last_total & 0xFFFF),
        (unsigned)(g_discount_bps & 0xFFFF));
    seal[sizeof(seal) - 1] = '\0';
    g_quote_sealed = TRUE;
    lstrcpyA(g_last_rule_text, "quote-sealed");
    _set_last_error("PR_SealQuote", PR_OK);
    return _copy_out(pOutSealCode, dwBufLen, seal);
}

__declspec(dllexport) int __stdcall PR_GetQuoteState(char* pOutBuf, DWORD dwBufLen) {
    char text[256];
    g_call_count++;
    if (!pOutBuf || dwBufLen < 32) return PR_ERR_BADPARAM;
    if (!g_quote_open) return PR_ERR_PRECONDITION;

    _snprintf(text, sizeof(text) - 1,
        "cust=%s|tier=%s|sku=%s|qty=%u|discount_bps=%lu|total=$%.2f|approved=%s|sealed=%s",
        g_bound_customer,
        g_customer_tier,
        g_bound_sku,
        (unsigned)g_bound_qty,
        (unsigned long)g_discount_bps,
        g_last_total / 100.0,
        g_rule_approved ? "yes" : "no",
        g_quote_sealed ? "yes" : "no");
    text[sizeof(text) - 1] = '\0';
    _set_last_error("PR_GetQuoteState", PR_OK);
    return _copy_out(pOutBuf, dwBufLen, text);
}

__declspec(dllexport) int __cdecl PR_GetLastRuleText(char* pOutBuf, DWORD dwBufLen) {
    g_call_count++;
    _set_last_error("PR_GetLastRuleText", PR_OK);
    return _copy_out(pOutBuf, dwBufLen, g_last_rule_text);
}

__declspec(dllexport) int __cdecl PR_GetDiagnostics(char* pOutBuf, DWORD dwBufLen) {
    char text[256];
    g_call_count++;
    if (!pOutBuf || dwBufLen < 32) return PR_ERR_BADPARAM;
    _snprintf(text, sizeof(text) - 1,
        "calls=%lu|cust=%s|sku=%s|quote=%s|tier=%s|total=%lu|approved=%s",
        (unsigned long)g_call_count,
        g_bound_customer[0] ? g_bound_customer : "(none)",
        g_bound_sku[0] ? g_bound_sku : "(none)",
        g_quote_id[0] ? g_quote_id : "(none)",
        g_customer_tier,
        (unsigned long)g_last_total,
        g_rule_approved ? "yes" : "no");
    text[sizeof(text) - 1] = '\0';
    _set_last_error("PR_GetDiagnostics", PR_OK);
    return _copy_out(pOutBuf, dwBufLen, text);
}

BOOL WINAPI DllMain(HINSTANCE hInstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)hInstDLL;
    (void)lpvReserved;
    if (fdwReason == DLL_PROCESS_DETACH) {
        if (g_hPayments) {
            FreeLibrary(g_hPayments);
            g_hPayments = NULL;
        }
        if (g_hCs) {
            FreeLibrary(g_hCs);
            g_hCs = NULL;
        }
    }
    return TRUE;
}
