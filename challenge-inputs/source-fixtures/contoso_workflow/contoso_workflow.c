/*
 * CONTOSO WORKFLOW / FULFILLMENT FIXTURE DLL
 *
 * Purpose:
 *   Deterministic interwoven fixture that depends on contoso_cs.dll and
 *   contoso_payments.dll. It stresses customer binding, reservation
 *   creation, workflow-handle passing, nonce confirmation, and shipment
 *   commit sequencing.
 *
 * Build target:
 *   contoso_workflow.dll
 */

#include <windows.h>
#include <stdio.h>
#include <string.h>

#define WF_OK                    0
#define WF_ERR_BASE_NOT_LOADED  -2001
#define WF_ERR_RESOLVE_FAILED   -2002
#define WF_ERR_BADPARAM         -2003
#define WF_ERR_PRECONDITION     -2004
#define WF_ERR_BADHANDLE        -2005
#define WF_ERR_BADNONCE         -2006
#define WF_ERR_STATE            -2007

typedef int   (__cdecl   *PFN_CS_Initialize)(void);
typedef int   (__cdecl   *PFN_CS_LookupCustomer)(const char*, char*, DWORD);
typedef DWORD (__cdecl   *PFN_CS_GetVersion)(void);

typedef int   (__cdecl   *PFN_CP_Initialize)(void);
typedef int   (__stdcall *PFN_CP_ReserveStock)(const char*, const char*, WORD, DWORD, char*, DWORD);
typedef int   (__stdcall *PFN_CP_ProcessPurchase)(const char*, const char*, WORD, DWORD, DWORD*);
typedef int   (__cdecl   *PFN_CP_CancelReservation)(const char*);
typedef int   (__stdcall *PFN_CP_CheckStock)(const char*, DWORD*);
typedef DWORD (__cdecl   *PFN_CP_GetVersion)(void);

typedef struct _WORKFLOW_API {
    PFN_CS_Initialize      pCsInitialize;
    PFN_CS_LookupCustomer  pCsLookupCustomer;
    PFN_CS_GetVersion      pCsGetVersion;
    PFN_CP_Initialize      pCpInitialize;
    PFN_CP_ReserveStock    pCpReserveStock;
    PFN_CP_ProcessPurchase pCpProcessPurchase;
    PFN_CP_CancelReservation pCpCancelReservation;
    PFN_CP_CheckStock      pCpCheckStock;
    PFN_CP_GetVersion      pCpGetVersion;
} WORKFLOW_API;

static HMODULE      g_hCs = NULL;
static HMODULE      g_hPayments = NULL;
static WORKFLOW_API g_api;

static BOOL  g_initialised = FALSE;
static BOOL  g_customer_bound = FALSE;
static BOOL  g_reservation_created = FALSE;
static BOOL  g_workflow_open = FALSE;
static BOOL  g_confirmed = FALSE;
static BOOL  g_committed = FALSE;
static BOOL  g_cancelled = FALSE;

static DWORD g_call_count = 0;
static DWORD g_last_nonce = 0;
static DWORD g_last_auth_code = 0;

static char  g_bound_customer[32] = "";
static char  g_bound_sku[32] = "";
static char  g_reservation_id[32] = "";
static char  g_workflow_handle[32] = "";
static char  g_last_error[256] = "no error";
static WORD  g_bound_qty = 0;

static void _set_last_error(const char* stage, int code) {
    _snprintf(g_last_error, sizeof(g_last_error) - 1,
        "stage=%s|code=%d|cust=%s|sku=%s|resv=%s|handle=%s",
        stage ? stage : "unknown",
        code,
        g_bound_customer[0] ? g_bound_customer : "(none)",
        g_bound_sku[0] ? g_bound_sku : "(none)",
        g_reservation_id[0] ? g_reservation_id : "(none)",
        g_workflow_handle[0] ? g_workflow_handle : "(none)");
    g_last_error[sizeof(g_last_error) - 1] = '\0';
}

static int _copy_out(char* out, DWORD out_len, const char* text) {
    if (!out || out_len == 0) return WF_ERR_BADPARAM;
    if (!text) text = "";
    _snprintf(out, out_len - 1, "%s", text);
    out[out_len - 1] = '\0';
    return WF_OK;
}

static DWORD _rol5(DWORD x) {
    return (x << 5) | (x >> 27);
}

static DWORD _compute_nonce(const char* handle) {
    DWORD acc = 0x57464C57UL;
    size_t i = 0;
    if (!handle) return 0;
    for (i = 0; handle[i]; i++) {
        acc = _rol5(acc);
        acc ^= (unsigned char)handle[i];
    }
    return acc ^ 0x13572468UL;
}

static int _load_dependencies(void) {
    char scratch[512];
    int rc = 0;

    if (g_hCs && g_hPayments && g_api.pCsLookupCustomer && g_api.pCpReserveStock) {
        return WF_OK;
    }

    g_hCs = LoadLibraryA("contoso_cs.dll");
    g_hPayments = LoadLibraryA("contoso_payments.dll");
    if (!g_hCs || !g_hPayments) {
        _set_last_error("LoadLibraryA", WF_ERR_BASE_NOT_LOADED);
        return WF_ERR_BASE_NOT_LOADED;
    }

    g_api.pCsInitialize = (PFN_CS_Initialize)GetProcAddress(g_hCs, "CS_Initialize");
    g_api.pCsLookupCustomer = (PFN_CS_LookupCustomer)GetProcAddress(g_hCs, "CS_LookupCustomer");
    g_api.pCsGetVersion = (PFN_CS_GetVersion)GetProcAddress(g_hCs, "CS_GetVersion");

    g_api.pCpInitialize = (PFN_CP_Initialize)GetProcAddress(g_hPayments, "CP_Initialize");
    g_api.pCpReserveStock = (PFN_CP_ReserveStock)GetProcAddress(g_hPayments, "CP_ReserveStock");
    g_api.pCpProcessPurchase = (PFN_CP_ProcessPurchase)GetProcAddress(g_hPayments, "CP_ProcessPurchase");
    g_api.pCpCancelReservation = (PFN_CP_CancelReservation)GetProcAddress(g_hPayments, "CP_CancelReservation");
    g_api.pCpCheckStock = (PFN_CP_CheckStock)GetProcAddress(g_hPayments, "CP_CheckStock");
    g_api.pCpGetVersion = (PFN_CP_GetVersion)GetProcAddress(g_hPayments, "CP_GetVersion");

    if (!g_api.pCsInitialize || !g_api.pCsLookupCustomer || !g_api.pCsGetVersion ||
        !g_api.pCpInitialize || !g_api.pCpReserveStock || !g_api.pCpProcessPurchase ||
        !g_api.pCpCancelReservation || !g_api.pCpCheckStock || !g_api.pCpGetVersion) {
        _set_last_error("GetProcAddress", WF_ERR_RESOLVE_FAILED);
        return WF_ERR_RESOLVE_FAILED;
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

    return WF_OK;
}

__declspec(dllexport) int __cdecl WF_Initialize(void) {
    int rc = _load_dependencies();
    g_call_count++;
    if (rc != WF_OK) return rc;
    g_initialised = TRUE;
    g_customer_bound = FALSE;
    g_reservation_created = FALSE;
    g_workflow_open = FALSE;
    g_confirmed = FALSE;
    g_committed = FALSE;
    g_cancelled = FALSE;
    g_last_nonce = 0;
    g_last_auth_code = 0;
    g_bound_customer[0] = '\0';
    g_bound_sku[0] = '\0';
    g_reservation_id[0] = '\0';
    g_workflow_handle[0] = '\0';
    _set_last_error("WF_Initialize", WF_OK);
    return WF_OK;
}

__declspec(dllexport) int __stdcall WF_BindCustomer(const char* lpszCustId) {
    int rc = 0;
    char scratch[512];
    g_call_count++;

    if (!lpszCustId || !*lpszCustId) return WF_ERR_BADPARAM;
    if (!g_initialised) {
        rc = WF_Initialize();
        if (rc != WF_OK) return rc;
    }

    rc = g_api.pCsLookupCustomer(lpszCustId, scratch, sizeof(scratch));
    if (rc != 0) {
        _set_last_error("CS_LookupCustomer", rc);
        return rc;
    }

    _snprintf(g_bound_customer, sizeof(g_bound_customer) - 1, "%s", lpszCustId);
    g_bound_customer[sizeof(g_bound_customer) - 1] = '\0';
    g_customer_bound = TRUE;
    g_reservation_created = FALSE;
    g_workflow_open = FALSE;
    g_confirmed = FALSE;
    g_committed = FALSE;
    g_cancelled = FALSE;
    _set_last_error("WF_BindCustomer", WF_OK);
    return WF_OK;
}

__declspec(dllexport) int __cdecl WF_CreateReservation(
    const char* lpszSku,
    WORD wQty,
    DWORD dwHoldMinutes,
    char* pOutResvId,
    DWORD dwBufLen)
{
    int rc = 0;
    DWORD available = 0;
    g_call_count++;

    if (!lpszSku || !*lpszSku || !pOutResvId || dwBufLen < 24 || wQty == 0) {
        return WF_ERR_BADPARAM;
    }
    if (!g_customer_bound) return WF_ERR_PRECONDITION;

    rc = g_api.pCpCheckStock(lpszSku, &available);
    if (rc != 0) {
        _set_last_error("CP_CheckStock", rc);
        return rc;
    }

    rc = g_api.pCpReserveStock(g_bound_customer, lpszSku, wQty, dwHoldMinutes, pOutResvId, dwBufLen);
    if (rc != 0) {
        _set_last_error("CP_ReserveStock", rc);
        return rc;
    }

    _snprintf(g_bound_sku, sizeof(g_bound_sku) - 1, "%s", lpszSku);
    g_bound_sku[sizeof(g_bound_sku) - 1] = '\0';
    _snprintf(g_reservation_id, sizeof(g_reservation_id) - 1, "%s", pOutResvId);
    g_reservation_id[sizeof(g_reservation_id) - 1] = '\0';
    g_bound_qty = wQty;
    g_reservation_created = TRUE;
    g_workflow_open = FALSE;
    g_confirmed = FALSE;
    g_committed = FALSE;
    g_cancelled = FALSE;
    _set_last_error("WF_CreateReservation", WF_OK);
    return WF_OK;
}

__declspec(dllexport) int __stdcall WF_OpenFulfillment(
    const char* lpszResvId,
    char* pOutHandle,
    DWORD dwBufLen)
{
    g_call_count++;
    if (!lpszResvId || !pOutHandle || dwBufLen < 24) return WF_ERR_BADPARAM;
    if (!g_reservation_created) return WF_ERR_PRECONDITION;
    if (_stricmp(lpszResvId, g_reservation_id) != 0) return WF_ERR_BADHANDLE;

    _snprintf(g_workflow_handle, sizeof(g_workflow_handle) - 1,
        "WFH-%s-%02u", g_bound_customer, (unsigned)g_bound_qty);
    g_workflow_handle[sizeof(g_workflow_handle) - 1] = '\0';
    _copy_out(pOutHandle, dwBufLen, g_workflow_handle);
    g_workflow_open = TRUE;
    g_confirmed = FALSE;
    g_committed = FALSE;
    g_cancelled = FALSE;
    _set_last_error("WF_OpenFulfillment", WF_OK);
    return WF_OK;
}

__declspec(dllexport) int __cdecl WF_GetWorkflowNonce(
    const char* lpszHandle,
    DWORD* pdwNonceOut)
{
    g_call_count++;
    if (!lpszHandle || !pdwNonceOut) return WF_ERR_BADPARAM;
    if (!g_workflow_open) return WF_ERR_PRECONDITION;
    if (_stricmp(lpszHandle, g_workflow_handle) != 0) return WF_ERR_BADHANDLE;

    g_last_nonce = _compute_nonce(lpszHandle);
    *pdwNonceOut = g_last_nonce;
    _set_last_error("WF_GetWorkflowNonce", WF_OK);
    return WF_OK;
}

__declspec(dllexport) int __stdcall WF_ConfirmWorkflow(
    const char* lpszHandle,
    DWORD dwNonce)
{
    g_call_count++;
    if (!lpszHandle) return WF_ERR_BADPARAM;
    if (!g_workflow_open) return WF_ERR_PRECONDITION;
    if (_stricmp(lpszHandle, g_workflow_handle) != 0) return WF_ERR_BADHANDLE;
    if (dwNonce != _compute_nonce(lpszHandle)) return WF_ERR_BADNONCE;

    g_confirmed = TRUE;
    _set_last_error("WF_ConfirmWorkflow", WF_OK);
    return WF_OK;
}

__declspec(dllexport) int __cdecl WF_CommitShipment(
    const char* lpszHandle,
    DWORD* pdwAuthCode)
{
    int rc = 0;
    DWORD auth = 0;
    g_call_count++;

    if (!lpszHandle || !pdwAuthCode) return WF_ERR_BADPARAM;
    if (!g_confirmed) return WF_ERR_PRECONDITION;
    if (_stricmp(lpszHandle, g_workflow_handle) != 0) return WF_ERR_BADHANDLE;
    if (g_committed || g_cancelled) return WF_ERR_STATE;

    rc = g_api.pCpProcessPurchase(g_bound_customer, g_bound_sku, g_bound_qty, 0, &auth);
    if (rc != 0) {
        _set_last_error("CP_ProcessPurchase", rc);
        return rc;
    }

    g_last_auth_code = auth;
    *pdwAuthCode = auth;
    g_committed = TRUE;
    _set_last_error("WF_CommitShipment", WF_OK);
    return WF_OK;
}

__declspec(dllexport) int __stdcall WF_GetWorkflowState(
    const char* lpszHandle,
    char* pOutBuf,
    DWORD dwBufLen)
{
    char state[256];
    g_call_count++;
    if (!lpszHandle || !pOutBuf || dwBufLen < 32) return WF_ERR_BADPARAM;
    if (!g_workflow_open) return WF_ERR_PRECONDITION;
    if (_stricmp(lpszHandle, g_workflow_handle) != 0) return WF_ERR_BADHANDLE;

    _snprintf(state, sizeof(state) - 1,
        "cust=%s|sku=%s|qty=%u|resv=%s|confirmed=%s|committed=%s|cancelled=%s",
        g_bound_customer,
        g_bound_sku,
        (unsigned)g_bound_qty,
        g_reservation_id,
        g_confirmed ? "yes" : "no",
        g_committed ? "yes" : "no",
        g_cancelled ? "yes" : "no");
    state[sizeof(state) - 1] = '\0';
    _set_last_error("WF_GetWorkflowState", WF_OK);
    return _copy_out(pOutBuf, dwBufLen, state);
}

__declspec(dllexport) int __cdecl WF_GetReservationEcho(
    const char* lpszHandle,
    char* pOutBuf,
    DWORD dwBufLen)
{
    char state[128];
    g_call_count++;
    if (!lpszHandle || !pOutBuf || dwBufLen < 32) return WF_ERR_BADPARAM;
    if (!g_workflow_open) return WF_ERR_PRECONDITION;
    if (_stricmp(lpszHandle, g_workflow_handle) != 0) return WF_ERR_BADHANDLE;

    _snprintf(state, sizeof(state) - 1,
        "reservation=%s|sku=%s|qty=%u",
        g_reservation_id,
        g_bound_sku,
        (unsigned)g_bound_qty);
    state[sizeof(state) - 1] = '\0';
    _set_last_error("WF_GetReservationEcho", WF_OK);
    return _copy_out(pOutBuf, dwBufLen, state);
}

__declspec(dllexport) int __stdcall WF_CancelWorkflow(const char* lpszHandle) {
    int rc = 0;
    g_call_count++;
    if (!lpszHandle) return WF_ERR_BADPARAM;
    if (!g_workflow_open) return WF_ERR_PRECONDITION;
    if (_stricmp(lpszHandle, g_workflow_handle) != 0) return WF_ERR_BADHANDLE;
    if (g_committed) return WF_ERR_STATE;
    if (g_cancelled) return WF_ERR_STATE;

    rc = g_api.pCpCancelReservation(g_reservation_id);
    if (rc != 0) {
        _set_last_error("CP_CancelReservation", rc);
        return rc;
    }

    g_cancelled = TRUE;
    _set_last_error("WF_CancelWorkflow", WF_OK);
    return WF_OK;
}

__declspec(dllexport) int __cdecl WF_ResetWorkflow(void) {
    g_call_count++;
    g_customer_bound = FALSE;
    g_reservation_created = FALSE;
    g_workflow_open = FALSE;
    g_confirmed = FALSE;
    g_committed = FALSE;
    g_cancelled = FALSE;
    g_last_nonce = 0;
    g_last_auth_code = 0;
    g_bound_customer[0] = '\0';
    g_bound_sku[0] = '\0';
    g_reservation_id[0] = '\0';
    g_workflow_handle[0] = '\0';
    _set_last_error("WF_ResetWorkflow", WF_OK);
    return WF_OK;
}

__declspec(dllexport) int __cdecl WF_GetDiagnostics(char* pOutBuf, DWORD dwBufLen) {
    char diag[256];
    g_call_count++;
    if (!pOutBuf || dwBufLen < 32) return WF_ERR_BADPARAM;
    _snprintf(diag, sizeof(diag) - 1,
        "calls=%lu|bound=%s|workflow=%s|confirmed=%s|committed=%s|cancelled=%s|auth=%lu",
        (unsigned long)g_call_count,
        g_customer_bound ? "yes" : "no",
        g_workflow_open ? "yes" : "no",
        g_confirmed ? "yes" : "no",
        g_committed ? "yes" : "no",
        g_cancelled ? "yes" : "no",
        (unsigned long)g_last_auth_code);
    diag[sizeof(diag) - 1] = '\0';
    _set_last_error("WF_GetDiagnostics", WF_OK);
    return _copy_out(pOutBuf, dwBufLen, diag);
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
