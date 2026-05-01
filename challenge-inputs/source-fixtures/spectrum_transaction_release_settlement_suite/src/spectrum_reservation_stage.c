#include "..\include\spectrum_suite.h"

SP_EXPORT int SP_CALL SP_IssueReleaseHold(
    const char* operator_token,
    const char* order_id,
    uint32_t amount_cents,
    char* out_reservation_handle,
    DWORD reservation_len,
    char* out_hold_token,
    DWORD hold_len
) {
    char reservation_handle[96];
    char hold_token[96];
    const char* effective_order_id = (order_id && *order_id) ? order_id : "easy01-order";
    uint32_t effective_amount_cents = amount_cents ? amount_cents : 4200U;
    int dep_status = sp_require_dependency("spectrum_session_bootstrap.dll", "SP_OpenOperatorSession");
    if (dep_status != SP_OK) {
        return dep_status;
    }
    if (!sp_has_prefix(operator_token, "sp-operator-")) {
        return SP_ERR_BADPARAM;
    }
    if (!out_reservation_handle || reservation_len < 24 || !out_hold_token || hold_len < 24) {
        return SP_ERR_BADPARAM;
    }
    _snprintf(reservation_handle, sizeof(reservation_handle) - 1, "sp-resv-%s-%lu", effective_order_id, (unsigned long)effective_amount_cents);
    reservation_handle[sizeof(reservation_handle) - 1] = '\0';
    _snprintf(hold_token, sizeof(hold_token) - 1, "sp-hold-%s-%lu", effective_order_id, (unsigned long)(effective_amount_cents % 100000UL));
    hold_token[sizeof(hold_token) - 1] = '\0';
    if (sp_copy_out(out_reservation_handle, reservation_len, reservation_handle) != SP_OK) {
        return SP_ERR_BADPARAM;
    }
    return sp_copy_out(out_hold_token, hold_len, hold_token);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    (void)hinstDLL;
    (void)fdwReason;
    (void)lpReserved;
    return TRUE;
}
