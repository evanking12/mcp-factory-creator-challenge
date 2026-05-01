#include "..\include\spectrum_suite.h"

SP_EXPORT int SP_CALL SP_StageReleaseFeeQuote(
    const char* reservation_handle,
    const char* operator_token,
    char* out_fee_quote_id,
    DWORD out_len
) {
    char fee_quote_id[96];
    int dep_status = sp_require_dependency("spectrum_reservation_stage.dll", "SP_IssueReleaseHold");
    if (dep_status != SP_OK) {
        return dep_status;
    }
    if (!sp_has_prefix(reservation_handle, "sp-resv-") || !sp_has_prefix(operator_token, "sp-operator-")) {
        return SP_ERR_BADTOKEN;
    }
    if (!out_fee_quote_id || out_len < 24) {
        return SP_ERR_BADPARAM;
    }
    sp_make_token(fee_quote_id, sizeof(fee_quote_id), "sp-fee", reservation_handle + 8, "quote");
    return sp_copy_out(out_fee_quote_id, out_len, fee_quote_id);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    (void)hinstDLL;
    (void)fdwReason;
    (void)lpReserved;
    return TRUE;
}
