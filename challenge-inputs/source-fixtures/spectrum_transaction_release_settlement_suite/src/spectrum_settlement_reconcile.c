#include "..\include\spectrum_suite.h"

SP_EXPORT int SP_CALL SP_VerifyReservedRelease(
    const char* reservation_handle,
    const char* preview_ticket,
    const char* fee_quote_id,
    char* out_verification_ticket,
    DWORD out_len
) {
    char verification_ticket[96];
    if (sp_require_dependency("spectrum_dispatch_preview.dll", "SP_PrepareReleasePreview") != SP_OK) {
        return SP_ERR_DEPENDENCY;
    }
    if (sp_require_dependency("spectrum_fee_quote.dll", "SP_StageReleaseFeeQuote") != SP_OK) {
        return SP_ERR_DEPENDENCY;
    }
    if (!sp_has_prefix(reservation_handle, "sp-resv-") || !sp_has_prefix(preview_ticket, "sp-preview-") || !sp_has_prefix(fee_quote_id, "sp-fee-")) {
        return SP_ERR_BADTOKEN;
    }
    if (!out_verification_ticket || out_len < 24) {
        return SP_ERR_BADPARAM;
    }
    sp_make_token(verification_ticket, sizeof(verification_ticket), "sp-verify", reservation_handle + 8, "reserved");
    return sp_copy_out(out_verification_ticket, out_len, verification_ticket);
}

SP_EXPORT int SP_CALL SP_ConfirmAttestedRelease(
    const char* attestation_packet,
    const char* invoice_stage_id,
    const char* entitlement_stage_id,
    char* out_confirmation_id,
    DWORD out_len
) {
    char confirmation_id[96];
    if (sp_require_dependency("spectrum_invoice_sync.dll", "SP_StageReleaseInvoice") != SP_OK) {
        return SP_ERR_DEPENDENCY;
    }
    if (sp_require_dependency("spectrum_entitlement_sync.dll", "SP_StageCustomerEntitlementUpdate") != SP_OK) {
        return SP_ERR_DEPENDENCY;
    }
    if (!sp_has_prefix(attestation_packet, "sp-att-") ||
        !sp_has_prefix(invoice_stage_id, "sp-invoice-stage-") ||
        !sp_has_prefix(entitlement_stage_id, "sp-entitlement-stage-")) {
        return SP_ERR_BADTOKEN;
    }
    if (!out_confirmation_id || out_len < 24) {
        return SP_ERR_BADPARAM;
    }
    sp_make_token(confirmation_id, sizeof(confirmation_id), "sp-confirm", attestation_packet + 7, "attested");
    return sp_copy_out(out_confirmation_id, out_len, confirmation_id);
}

SP_EXPORT int SP_CALL SP_InvalidateStaleAttestedRelease(
    const char* attestation_packet,
    uint32_t attestation_nonce,
    uint32_t attestation_epoch,
    char* out_stale_attestation_token,
    DWORD token_len,
    char* out_stale_reason,
    DWORD reason_len
) {
    char stale_token[96];
    char stale_reason[128];
    if (!sp_has_prefix(attestation_packet, "sp-att-")) {
        return SP_ERR_BADTOKEN;
    }
    if (!out_stale_attestation_token || token_len < 24 || !out_stale_reason || reason_len < 32) {
        return SP_ERR_BADPARAM;
    }
    if (attestation_nonce == 0 || attestation_epoch < 20250000U) {
        return SP_ERR_STALE;
    }
    _snprintf(stale_token, sizeof(stale_token) - 1, "sp-att-stale-%lu", (unsigned long)(attestation_nonce & 0xFFFFUL));
    stale_token[sizeof(stale_token) - 1] = '\0';
    _snprintf(stale_reason, sizeof(stale_reason) - 1, "freshness_gap|epoch=%lu|packet=%s", (unsigned long)attestation_epoch, attestation_packet);
    stale_reason[sizeof(stale_reason) - 1] = '\0';
    if (sp_copy_out(out_stale_attestation_token, token_len, stale_token) != SP_OK) {
        return SP_ERR_BADPARAM;
    }
    return sp_copy_out(out_stale_reason, reason_len, stale_reason);
}

SP_EXPORT int SP_CALL SP_SelectTrustedBranch(
    const char* primary_commit_receipt,
    const char* replay_commit_receipt,
    char* out_trusted_branch_id,
    DWORD trusted_len,
    char* out_rejected_branch_id,
    DWORD rejected_len
) {
    if (!sp_has_prefix(primary_commit_receipt, "sp-commit-") || !sp_has_prefix(replay_commit_receipt, "sp-branch-replay-")) {
        return SP_ERR_BADTOKEN;
    }
    if (!out_trusted_branch_id || trusted_len < 16 || !out_rejected_branch_id || rejected_len < 16) {
        return SP_ERR_BADPARAM;
    }
    if (sp_copy_out(out_trusted_branch_id, trusted_len, "sp-branch-primary") != SP_OK) {
        return SP_ERR_BADPARAM;
    }
    return sp_copy_out(out_rejected_branch_id, rejected_len, "sp-branch-replay");
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    (void)hinstDLL;
    (void)fdwReason;
    (void)lpReserved;
    return TRUE;
}
