/*
 * CONTOSO CORPORATION
 * Reporting & Export Services Library
 * Version 0.9.7 (c) 2004-2006 Contoso Corp. All rights reserved.
 *
 * INTERNAL USE ONLY - NOT FOR DISTRIBUTION
 * Intended compiler profile: MSVC 7.1 / 8.0 transition-era Win32 C
 * Local build fallback: modern MSVC with old-style source/ABI discipline
 */

#include <windows.h>
#include <stdio.h>
#include <string.h>

#define RP_OK                    0
#define RP_ERR_BASE_NOT_LOADED  -4001
#define RP_ERR_RESOLVE_FAILED   -4002
#define RP_ERR_BADPARAM         -4003
#define RP_ERR_PRECONDITION     -4004
#define RP_ERR_BADREPORT        -4005
#define RP_ERR_BADNONCE         -4006
#define RP_ERR_STATE            -4007

#define RP_FORMAT_TXT            1
#define RP_FORMAT_CSV            2
#define RP_FORMAT_AUDIT          3

typedef int (__cdecl   *PFN_CS_Initialize)(void);
typedef int (__cdecl   *PFN_CS_LookupCustomer)(const char*, char*, DWORD);
typedef int (__cdecl   *PFN_CP_Initialize)(void);
typedef int (__cdecl   *PFN_CP_GetTransactionHistory)(const char*, char*, DWORD, DWORD*);
typedef int (__cdecl   *PFN_WF_Initialize)(void);
typedef int (__stdcall *PFN_WF_BindCustomer)(const char*);
typedef int (__cdecl   *PFN_WF_CreateReservation)(const char*, WORD, DWORD, char*, DWORD);
typedef int (__stdcall *PFN_WF_OpenFulfillment)(const char*, char*, DWORD);
typedef int (__stdcall *PFN_WF_GetWorkflowState)(const char*, char*, DWORD);
typedef int (__cdecl   *PFN_WF_GetReservationEcho)(const char*, char*, DWORD);

typedef struct _REPORT_API {
    PFN_CS_Initialize         pCsInitialize;
    PFN_CS_LookupCustomer     pCsLookupCustomer;
    PFN_CP_Initialize         pCpInitialize;
    PFN_CP_GetTransactionHistory pCpGetTransactionHistory;
    PFN_WF_Initialize         pWfInitialize;
    PFN_WF_BindCustomer       pWfBindCustomer;
    PFN_WF_CreateReservation  pWfCreateReservation;
    PFN_WF_OpenFulfillment    pWfOpenFulfillment;
    PFN_WF_GetWorkflowState   pWfGetWorkflowState;
    PFN_WF_GetReservationEcho pWfGetReservationEcho;
} REPORT_API;

static HMODULE g_hCs = NULL;
static HMODULE g_hPayments = NULL;
static HMODULE g_hWorkflow = NULL;
static REPORT_API g_api;

static BOOL  g_initialised = FALSE;
static BOOL  g_customer_bound = FALSE;
static BOOL  g_report_open = FALSE;
static BOOL  g_format_selected = FALSE;
static BOOL  g_preview_ready = FALSE;
static BOOL  g_exported = FALSE;

static char  g_bound_customer[32] = "";
static char  g_bound_sku[32] = "";
static char  g_reservation_id[32] = "";
static char  g_workflow_handle[32] = "";
static char  g_report_id[32] = "";
static char  g_artifact_id[40] = "";
static char  g_last_error[256] = "no error";
static DWORD g_report_nonce = 0;
static DWORD g_format_code = 0;
static WORD  g_bound_qty = 0;
static DWORD g_call_count = 0;

static void _set_last_error(const char* stage, int code) {
    _snprintf(g_last_error, sizeof(g_last_error) - 1,
        "stage=%s|code=%d|cust=%s|sku=%s|report=%s|workflow=%s",
        stage ? stage : "unknown",
        code,
        g_bound_customer[0] ? g_bound_customer : "(none)",
        g_bound_sku[0] ? g_bound_sku : "(none)",
        g_report_id[0] ? g_report_id : "(none)",
        g_workflow_handle[0] ? g_workflow_handle : "(none)");
    g_last_error[sizeof(g_last_error) - 1] = '\0';
}

static int _copy_out(char* out, DWORD out_len, const char* text) {
    if (!out || out_len == 0) return RP_ERR_BADPARAM;
    if (!text) text = "";
    _snprintf(out, out_len - 1, "%s", text);
    out[out_len - 1] = '\0';
    return RP_OK;
}

static DWORD _rol7(DWORD x) {
    return (x << 7) | (x >> 25);
}

static DWORD _compute_report_nonce(const char* report_id) {
    DWORD acc = 0x52505430UL;
    size_t i = 0;
    if (!report_id) return 0;
    for (i = 0; report_id[i]; i++) {
        acc = _rol7(acc);
        acc ^= (unsigned char)report_id[i];
    }
    return acc ^ 0x19460712UL;
}

static const char* _format_name(DWORD code) {
    switch (code) {
        case RP_FORMAT_TXT: return "TXT";
        case RP_FORMAT_CSV: return "CSV";
        case RP_FORMAT_AUDIT: return "AUDIT";
        default: return "UNKNOWN";
    }
}

static int _load_dependencies(void) {
    char scratch[512];
    int rc = 0;

    if (g_hCs && g_hPayments && g_hWorkflow && g_api.pWfOpenFulfillment) return RP_OK;

    g_hCs = LoadLibraryA("contoso_cs.dll");
    g_hPayments = LoadLibraryA("contoso_payments.dll");
    g_hWorkflow = LoadLibraryA("contoso_workflow.dll");
    if (!g_hCs || !g_hPayments || !g_hWorkflow) {
        _set_last_error("LoadLibraryA", RP_ERR_BASE_NOT_LOADED);
        return RP_ERR_BASE_NOT_LOADED;
    }

    g_api.pCsInitialize = (PFN_CS_Initialize)GetProcAddress(g_hCs, "CS_Initialize");
    g_api.pCsLookupCustomer = (PFN_CS_LookupCustomer)GetProcAddress(g_hCs, "CS_LookupCustomer");
    g_api.pCpInitialize = (PFN_CP_Initialize)GetProcAddress(g_hPayments, "CP_Initialize");
    g_api.pCpGetTransactionHistory = (PFN_CP_GetTransactionHistory)GetProcAddress(g_hPayments, "CP_GetTransactionHistory");
    g_api.pWfInitialize = (PFN_WF_Initialize)GetProcAddress(g_hWorkflow, "WF_Initialize");
    g_api.pWfBindCustomer = (PFN_WF_BindCustomer)GetProcAddress(g_hWorkflow, "WF_BindCustomer");
    g_api.pWfCreateReservation = (PFN_WF_CreateReservation)GetProcAddress(g_hWorkflow, "WF_CreateReservation");
    g_api.pWfOpenFulfillment = (PFN_WF_OpenFulfillment)GetProcAddress(g_hWorkflow, "WF_OpenFulfillment");
    g_api.pWfGetWorkflowState = (PFN_WF_GetWorkflowState)GetProcAddress(g_hWorkflow, "WF_GetWorkflowState");
    g_api.pWfGetReservationEcho = (PFN_WF_GetReservationEcho)GetProcAddress(g_hWorkflow, "WF_GetReservationEcho");

    if (!g_api.pCsInitialize || !g_api.pCsLookupCustomer || !g_api.pCpInitialize ||
        !g_api.pCpGetTransactionHistory || !g_api.pWfInitialize || !g_api.pWfBindCustomer ||
        !g_api.pWfCreateReservation || !g_api.pWfOpenFulfillment ||
        !g_api.pWfGetWorkflowState || !g_api.pWfGetReservationEcho) {
        _set_last_error("GetProcAddress", RP_ERR_RESOLVE_FAILED);
        return RP_ERR_RESOLVE_FAILED;
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
    rc = g_api.pWfInitialize();
    if (rc != 0) {
        _set_last_error("WF_Initialize", rc);
        return rc;
    }
    rc = g_api.pCsLookupCustomer("CUST-001", scratch, sizeof(scratch));
    if (rc != 0) {
        _set_last_error("CS_LookupCustomer", rc);
        return rc;
    }

    return RP_OK;
}

__declspec(dllexport) int __cdecl RP_Initialize(void) {
    int rc = _load_dependencies();
    g_call_count++;
    if (rc != RP_OK) return rc;
    g_initialised = TRUE;
    g_customer_bound = FALSE;
    g_report_open = FALSE;
    g_format_selected = FALSE;
    g_preview_ready = FALSE;
    g_exported = FALSE;
    g_bound_customer[0] = '\0';
    g_bound_sku[0] = '\0';
    g_reservation_id[0] = '\0';
    g_workflow_handle[0] = '\0';
    g_report_id[0] = '\0';
    g_artifact_id[0] = '\0';
    g_report_nonce = 0;
    g_format_code = 0;
    _set_last_error("RP_Initialize", RP_OK);
    return RP_OK;
}

__declspec(dllexport) int __stdcall RP_BindCustomer(const char* lpszCustId) {
    int rc = 0;
    char scratch[512];
    g_call_count++;
    if (!lpszCustId || !*lpszCustId) return RP_ERR_BADPARAM;
    if (!g_initialised) {
        rc = RP_Initialize();
        if (rc != RP_OK) return rc;
    }

    rc = g_api.pCsLookupCustomer(lpszCustId, scratch, sizeof(scratch));
    if (rc != 0) {
        _set_last_error("CS_LookupCustomer", rc);
        return rc;
    }

    _snprintf(g_bound_customer, sizeof(g_bound_customer) - 1, "%s", lpszCustId);
    g_bound_customer[sizeof(g_bound_customer) - 1] = '\0';
    g_customer_bound = TRUE;
    g_report_open = FALSE;
    g_format_selected = FALSE;
    g_preview_ready = FALSE;
    g_exported = FALSE;
    _set_last_error("RP_BindCustomer", RP_OK);
    return RP_OK;
}

__declspec(dllexport) int __cdecl RP_PrepareReservationReport(
    const char* lpszSku,
    WORD wQty,
    DWORD dwHoldMinutes,
    char* pOutReportId,
    DWORD dwBufLen) {
    int rc = 0;
    g_call_count++;
    if (!lpszSku || !*lpszSku || wQty == 0 || !pOutReportId || dwBufLen < 24) {
        return RP_ERR_BADPARAM;
    }
    if (!g_customer_bound) return RP_ERR_PRECONDITION;

    rc = g_api.pWfInitialize();
    if (rc != 0) {
        _set_last_error("WF_Initialize", rc);
        return rc;
    }
    rc = g_api.pWfBindCustomer(g_bound_customer);
    if (rc != 0) {
        _set_last_error("WF_BindCustomer", rc);
        return rc;
    }
    rc = g_api.pWfCreateReservation(lpszSku, wQty, dwHoldMinutes, g_reservation_id, sizeof(g_reservation_id));
    if (rc != 0) {
        _set_last_error("WF_CreateReservation", rc);
        return rc;
    }
    rc = g_api.pWfOpenFulfillment(g_reservation_id, g_workflow_handle, sizeof(g_workflow_handle));
    if (rc != 0) {
        _set_last_error("WF_OpenFulfillment", rc);
        return rc;
    }

    _snprintf(g_bound_sku, sizeof(g_bound_sku) - 1, "%s", lpszSku);
    g_bound_sku[sizeof(g_bound_sku) - 1] = '\0';
    g_bound_qty = wQty;
    _snprintf(g_report_id, sizeof(g_report_id) - 1, "RPT-%s-%02u", g_bound_customer, (unsigned)wQty);
    g_report_id[sizeof(g_report_id) - 1] = '\0';
    g_report_nonce = _compute_report_nonce(g_report_id);
    g_report_open = TRUE;
    g_format_selected = FALSE;
    g_preview_ready = FALSE;
    g_exported = FALSE;
    g_artifact_id[0] = '\0';
    _set_last_error("RP_PrepareReservationReport", RP_OK);
    return _copy_out(pOutReportId, dwBufLen, g_report_id);
}

__declspec(dllexport) int __stdcall RP_GetReportNonce(const char* lpszReportId, DWORD* pdwNonceOut) {
    g_call_count++;
    if (!lpszReportId || !pdwNonceOut) return RP_ERR_BADPARAM;
    if (!g_report_open) return RP_ERR_PRECONDITION;
    if (_stricmp(lpszReportId, g_report_id) != 0) return RP_ERR_BADREPORT;
    *pdwNonceOut = g_report_nonce;
    _set_last_error("RP_GetReportNonce", RP_OK);
    return RP_OK;
}

__declspec(dllexport) int __cdecl RP_SelectFormat(const char* lpszReportId, DWORD dwFormatCode) {
    g_call_count++;
    if (!lpszReportId) return RP_ERR_BADPARAM;
    if (!g_report_open) return RP_ERR_PRECONDITION;
    if (_stricmp(lpszReportId, g_report_id) != 0) return RP_ERR_BADREPORT;
    if (dwFormatCode < RP_FORMAT_TXT || dwFormatCode > RP_FORMAT_AUDIT) return RP_ERR_BADPARAM;
    g_format_code = dwFormatCode;
    g_format_selected = TRUE;
    g_preview_ready = FALSE;
    _set_last_error("RP_SelectFormat", RP_OK);
    return RP_OK;
}

__declspec(dllexport) int __stdcall RP_RenderPreview(const char* lpszReportId, char* pOutBuf, DWORD dwBufLen) {
    char history[512];
    DWORD txn_count = 0;
    char preview[768];
    int rc = 0;
    g_call_count++;
    if (!lpszReportId || !pOutBuf || dwBufLen < 64) return RP_ERR_BADPARAM;
    if (!g_report_open || !g_format_selected) return RP_ERR_PRECONDITION;
    if (_stricmp(lpszReportId, g_report_id) != 0) return RP_ERR_BADREPORT;

    rc = g_api.pCpGetTransactionHistory(g_bound_customer, history, sizeof(history), &txn_count);
    if (rc != 0) {
        history[0] = '\0';
        txn_count = 0;
    }

    _snprintf(preview, sizeof(preview) - 1,
        "report=%s|format=%s|customer=%s|sku=%s|qty=%u|workflow=%s|txns=%lu|preview=%s",
        g_report_id,
        _format_name(g_format_code),
        g_bound_customer,
        g_bound_sku,
        (unsigned)g_bound_qty,
        g_workflow_handle,
        (unsigned long)txn_count,
        history[0] ? "history-loaded" : "history-empty");
    preview[sizeof(preview) - 1] = '\0';
    g_preview_ready = TRUE;
    _set_last_error("RP_RenderPreview", RP_OK);
    return _copy_out(pOutBuf, dwBufLen, preview);
}

__declspec(dllexport) int __cdecl RP_ExportLedger(
    const char* lpszReportId,
    DWORD dwNonce,
    char* pOutArtifactId,
    DWORD dwBufLen) {
    char workflow_state[512];
    int rc = 0;
    g_call_count++;
    if (!lpszReportId || !pOutArtifactId || dwBufLen < 24) return RP_ERR_BADPARAM;
    if (!g_preview_ready) return RP_ERR_PRECONDITION;
    if (_stricmp(lpszReportId, g_report_id) != 0) return RP_ERR_BADREPORT;
    if (dwNonce != g_report_nonce) return RP_ERR_BADNONCE;

    rc = g_api.pWfGetWorkflowState(g_workflow_handle, workflow_state, sizeof(workflow_state));
    if (rc != 0) {
        _set_last_error("WF_GetWorkflowState", rc);
        return rc;
    }

    _snprintf(g_artifact_id, sizeof(g_artifact_id) - 1, "ART-%s-%s", _format_name(g_format_code), g_bound_customer);
    g_artifact_id[sizeof(g_artifact_id) - 1] = '\0';
    g_exported = TRUE;
    _set_last_error("RP_ExportLedger", RP_OK);
    return _copy_out(pOutArtifactId, dwBufLen, g_artifact_id);
}

__declspec(dllexport) int __stdcall RP_GetReportState(char* pOutBuf, DWORD dwBufLen) {
    char state[512];
    g_call_count++;
    if (!pOutBuf || dwBufLen < 64) return RP_ERR_BADPARAM;
    if (!g_report_open) return RP_ERR_PRECONDITION;

    _snprintf(state, sizeof(state) - 1,
        "cust=%s|sku=%s|qty=%u|report=%s|format=%s|workflow=%s|preview=%s|exported=%s|artifact=%s",
        g_bound_customer,
        g_bound_sku,
        (unsigned)g_bound_qty,
        g_report_id,
        _format_name(g_format_code),
        g_workflow_handle,
        g_preview_ready ? "yes" : "no",
        g_exported ? "yes" : "no",
        g_artifact_id[0] ? g_artifact_id : "(none)");
    state[sizeof(state) - 1] = '\0';
    _set_last_error("RP_GetReportState", RP_OK);
    return _copy_out(pOutBuf, dwBufLen, state);
}

__declspec(dllexport) int __cdecl RP_GetWorkflowHandle(const char* lpszReportId, char* pOutBuf, DWORD dwBufLen) {
    g_call_count++;
    if (!lpszReportId || !pOutBuf || dwBufLen < 24) return RP_ERR_BADPARAM;
    if (!g_report_open) return RP_ERR_PRECONDITION;
    if (_stricmp(lpszReportId, g_report_id) != 0) return RP_ERR_BADREPORT;
    _set_last_error("RP_GetWorkflowHandle", RP_OK);
    return _copy_out(pOutBuf, dwBufLen, g_workflow_handle);
}

__declspec(dllexport) int __stdcall RP_GetLastErrorText(char* pOutBuf, DWORD dwBufLen) {
    g_call_count++;
    return _copy_out(pOutBuf, dwBufLen, g_last_error);
}

__declspec(dllexport) int __cdecl RP_CloseReport(const char* lpszReportId) {
    g_call_count++;
    if (!lpszReportId) return RP_ERR_BADPARAM;
    if (!g_report_open) return RP_ERR_PRECONDITION;
    if (_stricmp(lpszReportId, g_report_id) != 0) return RP_ERR_BADREPORT;
    g_report_open = FALSE;
    g_format_selected = FALSE;
    g_preview_ready = FALSE;
    g_exported = FALSE;
    _set_last_error("RP_CloseReport", RP_OK);
    return RP_OK;
}

__declspec(dllexport) int __cdecl RP_GetDiagnostics(char* pOutBuf, DWORD dwBufLen) {
    char diag[256];
    g_call_count++;
    if (!pOutBuf || dwBufLen < 32) return RP_ERR_BADPARAM;
    _snprintf(diag, sizeof(diag) - 1,
        "calls=%lu|report=%s|format=%s|workflow=%s|preview=%s|exported=%s",
        (unsigned long)g_call_count,
        g_report_id[0] ? g_report_id : "(none)",
        _format_name(g_format_code),
        g_workflow_handle[0] ? g_workflow_handle : "(none)",
        g_preview_ready ? "yes" : "no",
        g_exported ? "yes" : "no");
    diag[sizeof(diag) - 1] = '\0';
    _set_last_error("RP_GetDiagnostics", RP_OK);
    return _copy_out(pOutBuf, dwBufLen, diag);
}

BOOL WINAPI DllMain(HINSTANCE hInstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)hInstDLL;
    (void)lpvReserved;
    if (fdwReason == DLL_PROCESS_DETACH) {
        if (g_hWorkflow) {
            FreeLibrary(g_hWorkflow);
            g_hWorkflow = NULL;
        }
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
