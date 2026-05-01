#include "..\include\spectrum_suite.h"

SP_EXPORT int SP_CALL SP_ReplayReleaseBranch(
    const char* reservation_handle,
    const char* hold_token,
    char* out_replay_receipt,
    DWORD out_len
) {
    char replay_receipt[96];
    int dep_status = sp_require_dependency("spectrum_release_commit.dll", "SP_CommitRelease");
    if (dep_status != SP_OK) {
        return dep_status;
    }
    if (!sp_has_prefix(reservation_handle, "sp-resv-") || !sp_has_prefix(hold_token, "sp-hold-")) {
        return SP_ERR_BADTOKEN;
    }
    if (!out_replay_receipt || out_len < 24) {
        return SP_ERR_BADPARAM;
    }
    sp_make_token(replay_receipt, sizeof(replay_receipt), "sp-branch-replay", reservation_handle + 8, "receipt");
    return sp_copy_out(out_replay_receipt, out_len, replay_receipt);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    (void)hinstDLL;
    (void)fdwReason;
    (void)lpReserved;
    return TRUE;
}
