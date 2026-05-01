#include "..\include\spectrum_suite.h"

SP_EXPORT int SP_CALL SP_QuarantineUnsupportedTarget(
    const char* operator_token,
    const char* reservation_handle,
    const char* unsupported_target_id,
    char* out_quarantine_token,
    DWORD out_len
) {
    char quarantine_token[128];
    const char* target_suffix = NULL;
    if (!sp_has_prefix(operator_token, "sp-operator-") ||
        !sp_has_prefix(reservation_handle, "sp-resv-") ||
        !sp_has_prefix(unsupported_target_id, "sp-unsupported-target-")) {
        return SP_ERR_BADTOKEN;
    }
    if (!out_quarantine_token || out_len < 48) {
        return SP_ERR_BADPARAM;
    }
    target_suffix = unsupported_target_id + strlen("sp-unsupported-target-");
    sp_make_token(quarantine_token, sizeof(quarantine_token), "sp-quarantine", target_suffix, "denied");
    return sp_copy_out(out_quarantine_token, out_len, quarantine_token);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    (void)hinstDLL;
    (void)fdwReason;
    (void)lpReserved;
    return TRUE;
}
