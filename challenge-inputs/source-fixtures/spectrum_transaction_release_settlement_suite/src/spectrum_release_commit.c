#include "..\include\spectrum_suite.h"

SP_EXPORT int SP_CALL SP_CommitRelease(
    const char* reservation_handle,
    const char* hold_token,
    char* out_commit_receipt,
    DWORD out_len
) {
    char commit_receipt[96];
    int hold_token_missing = (!hold_token || !*hold_token);
    int dep_status = sp_require_dependency("spectrum_reservation_stage.dll", "SP_IssueReleaseHold");
    if (dep_status != SP_OK) {
        return dep_status;
    }
    if (!sp_has_prefix(reservation_handle, "sp-resv-")) {
        return SP_ERR_BADTOKEN;
    }
    if (!hold_token_missing && !sp_has_prefix(hold_token, "sp-hold-")) {
        return SP_ERR_BADTOKEN;
    }
    if (!out_commit_receipt || out_len < 24) {
        return SP_ERR_BADPARAM;
    }
    sp_make_token(commit_receipt, sizeof(commit_receipt), "sp-commit", reservation_handle + 8, "receipt");
    return sp_copy_out(out_commit_receipt, out_len, commit_receipt);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    (void)hinstDLL;
    (void)fdwReason;
    (void)lpReserved;
    return TRUE;
}
