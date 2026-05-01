#include "..\include\spectrum_suite.h"

SP_EXPORT int SP_CALL SP_RecordReleaseReceipt(
    const char* commit_receipt,
    const char* operator_token,
    char* out_audit_receipt,
    DWORD out_len
) {
    char audit_receipt[96];
    if (sp_require_dependency("spectrum_release_commit.dll", "SP_CommitRelease") != SP_OK) {
        return SP_ERR_DEPENDENCY;
    }
    if (!sp_has_prefix(commit_receipt, "sp-commit-")) {
        return SP_ERR_BADTOKEN;
    }
    if (operator_token && *operator_token && !sp_has_prefix(operator_token, "sp-operator-")) {
        return SP_ERR_BADTOKEN;
    }
    if (!out_audit_receipt || out_len < 24) {
        return SP_ERR_BADPARAM;
    }
    sp_make_token(audit_receipt, sizeof(audit_receipt), "sp-audit-receipt", commit_receipt + 10, "ok");
    return sp_copy_out(out_audit_receipt, out_len, audit_receipt);
}

SP_EXPORT int SP_CALL SP_InvalidateUntrustedBranch(
    const char* trusted_branch_id,
    const char* rejected_branch_id,
    char* out_audit_token,
    DWORD out_len
) {
    char audit_token[96];
    const char* rejected_suffix = NULL;
    if (!sp_has_prefix(trusted_branch_id, "sp-branch-") || !sp_has_prefix(rejected_branch_id, "sp-branch-")) {
        return SP_ERR_BADTOKEN;
    }
    if (!out_audit_token || out_len < 48) {
        return SP_ERR_BADPARAM;
    }
    rejected_suffix = rejected_branch_id + strlen("sp-branch-");
    sp_make_token(audit_token, sizeof(audit_token), "sp-audit", rejected_suffix, "branch-invalidated");
    return sp_copy_out(out_audit_token, out_len, audit_token);
}

SP_EXPORT int SP_CALL SP_RecordStaleAttestationInvalidation(
    const char* stale_attestation_token,
    const char* stale_reason,
    char* out_audit_token,
    DWORD out_len
) {
    if (!sp_has_prefix(stale_attestation_token, "sp-att-stale-") || !stale_reason || !*stale_reason) {
        return SP_ERR_BADTOKEN;
    }
    if (!out_audit_token || out_len < 24) {
        return SP_ERR_BADPARAM;
    }
    return sp_copy_out(out_audit_token, out_len, "sp-audit-stale-attestation");
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    (void)hinstDLL;
    (void)fdwReason;
    (void)lpReserved;
    return TRUE;
}
