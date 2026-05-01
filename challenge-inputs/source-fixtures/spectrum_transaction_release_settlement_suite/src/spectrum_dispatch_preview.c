#include "..\include\spectrum_suite.h"

SP_EXPORT int SP_CALL SP_PrepareReleasePreview(
    const char* reservation_handle,
    const char* operator_token,
    char* out_preview_ticket,
    DWORD out_len
) {
    char preview_ticket[96];
    int dep_status = sp_require_dependency("spectrum_reservation_stage.dll", "SP_IssueReleaseHold");
    if (dep_status != SP_OK) {
        return dep_status;
    }
    if (!sp_has_prefix(reservation_handle, "sp-resv-") || !sp_has_prefix(operator_token, "sp-operator-")) {
        return SP_ERR_BADTOKEN;
    }
    if (!out_preview_ticket || out_len < 24) {
        return SP_ERR_BADPARAM;
    }
    sp_make_token(preview_ticket, sizeof(preview_ticket), "sp-preview", reservation_handle + 8, "gate");
    return sp_copy_out(out_preview_ticket, out_len, preview_ticket);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    (void)hinstDLL;
    (void)fdwReason;
    (void)lpReserved;
    return TRUE;
}
