#include "..\include\spectrum_suite.h"

SP_EXPORT int SP_CALL SP_RecordUnsupportedTargetQuarantine(
    const char* unsupported_target_id,
    const char* quarantine_token,
    char* out_quarantine_audit_token,
    DWORD out_len
) {
    char audit_token[128];
    const char* target_suffix = NULL;
    if (!sp_has_prefix(unsupported_target_id, "sp-unsupported-target-") ||
        !sp_has_prefix(quarantine_token, "sp-quarantine-")) {
        return SP_ERR_BADTOKEN;
    }
    if (!out_quarantine_audit_token || out_len < 48) {
        return SP_ERR_BADPARAM;
    }
    target_suffix = unsupported_target_id + strlen("sp-unsupported-target-");
    if (!strstr(quarantine_token, target_suffix)) {
        return SP_ERR_BADTOKEN;
    }
    sp_make_token(audit_token, sizeof(audit_token), "sp-audit", target_suffix, "quarantined");
    return sp_copy_out(out_quarantine_audit_token, out_len, audit_token);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    (void)hinstDLL;
    (void)fdwReason;
    (void)lpReserved;
    return TRUE;
}
