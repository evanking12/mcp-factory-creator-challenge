#ifndef SPECTRUM_SUITE_H
#define SPECTRUM_SUITE_H

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SP_EXPORT __declspec(dllexport)
#define SP_CALL __cdecl

#define SP_OK                0
#define SP_ERR_BADPARAM   -7101
#define SP_ERR_PRECONDITION -7102
#define SP_ERR_DEPENDENCY -7103
#define SP_ERR_BADTOKEN   -7104
#define SP_ERR_CONFLICT   -7105
#define SP_ERR_STALE      -7106

static __inline int sp_copy_out(char* out_buf, DWORD out_len, const char* text) {
    if (!out_buf || out_len == 0) {
        return SP_ERR_BADPARAM;
    }
    if (!text) {
        text = "";
    }
    _snprintf(out_buf, out_len - 1, "%s", text);
    out_buf[out_len - 1] = '\0';
    return SP_OK;
}

static __inline int sp_has_prefix(const char* value, const char* prefix) {
    size_t prefix_len = 0;
    if (!value || !prefix) {
        return 0;
    }
    prefix_len = strlen(prefix);
    if (strlen(value) < prefix_len) {
        return 0;
    }
    return strncmp(value, prefix, prefix_len) == 0;
}

static __inline void sp_make_token(
    char* out_buf,
    size_t out_len,
    const char* prefix,
    const char* part_a,
    const char* part_b
) {
    const char* safe_a = (part_a && *part_a) ? part_a : "none";
    const char* safe_b = (part_b && *part_b) ? part_b : "none";
    _snprintf(out_buf, out_len - 1, "%s-%s-%s", prefix, safe_a, safe_b);
    out_buf[out_len - 1] = '\0';
}

static __inline int sp_require_dependency(const char* dll_name, const char* symbol_name) {
    HMODULE module_handle = NULL;
    FARPROC symbol = NULL;
    if (!dll_name || !symbol_name) {
        return SP_ERR_BADPARAM;
    }
    module_handle = LoadLibraryA(dll_name);
    if (!module_handle) {
        return SP_ERR_DEPENDENCY;
    }
    symbol = GetProcAddress(module_handle, symbol_name);
    if (!symbol) {
        return SP_ERR_DEPENDENCY;
    }
    return SP_OK;
}

SP_EXPORT int SP_CALL SP_OpenOperatorSession(const char* operator_id, char* out_operator_token, DWORD out_len);
SP_EXPORT int SP_CALL SP_IssueReleaseHold(
    const char* operator_token,
    const char* order_id,
    uint32_t amount_cents,
    char* out_reservation_handle,
    DWORD reservation_len,
    char* out_hold_token,
    DWORD hold_len
);
SP_EXPORT int SP_CALL SP_PrepareReleasePreview(
    const char* reservation_handle,
    const char* operator_token,
    char* out_preview_ticket,
    DWORD out_len
);
SP_EXPORT int SP_CALL SP_StageReleaseFeeQuote(
    const char* reservation_handle,
    const char* operator_token,
    char* out_fee_quote_id,
    DWORD out_len
);
SP_EXPORT int SP_CALL SP_EmitReleaseAttestation(
    const char* preview_ticket,
    const char* fee_quote_id,
    char* out_attestation_packet,
    DWORD packet_len,
    uint32_t* out_attestation_nonce,
    uint32_t* out_attestation_epoch
);
SP_EXPORT int SP_CALL SP_StageReleaseInvoice(
    const char* attestation_packet,
    char* out_invoice_stage_id,
    DWORD out_len
);
SP_EXPORT int SP_CALL SP_StageCustomerEntitlementUpdate(
    const char* attestation_packet,
    char* out_entitlement_stage_id,
    DWORD out_len
);
SP_EXPORT int SP_CALL SP_CommitRelease(
    const char* reservation_handle,
    const char* hold_token,
    char* out_commit_receipt,
    DWORD out_len
);
SP_EXPORT int SP_CALL SP_ReplayReleaseBranch(
    const char* reservation_handle,
    const char* hold_token,
    char* out_replay_receipt,
    DWORD out_len
);
SP_EXPORT int SP_CALL SP_VerifyReservedRelease(
    const char* reservation_handle,
    const char* preview_ticket,
    const char* fee_quote_id,
    char* out_verification_ticket,
    DWORD out_len
);
SP_EXPORT int SP_CALL SP_ConfirmAttestedRelease(
    const char* attestation_packet,
    const char* invoice_stage_id,
    const char* entitlement_stage_id,
    char* out_confirmation_id,
    DWORD out_len
);
SP_EXPORT int SP_CALL SP_InvalidateStaleAttestedRelease(
    const char* attestation_packet,
    uint32_t attestation_nonce,
    uint32_t attestation_epoch,
    char* out_stale_attestation_token,
    DWORD token_len,
    char* out_stale_reason,
    DWORD reason_len
);
SP_EXPORT int SP_CALL SP_SelectTrustedBranch(
    const char* primary_commit_receipt,
    const char* replay_commit_receipt,
    char* out_trusted_branch_id,
    DWORD trusted_len,
    char* out_rejected_branch_id,
    DWORD rejected_len
);
SP_EXPORT int SP_CALL SP_RecordReleaseReceipt(
    const char* commit_receipt,
    const char* operator_token,
    char* out_audit_receipt,
    DWORD out_len
);
SP_EXPORT int SP_CALL SP_InvalidateUntrustedBranch(
    const char* trusted_branch_id,
    const char* rejected_branch_id,
    char* out_audit_token,
    DWORD out_len
);
SP_EXPORT int SP_CALL SP_RecordStaleAttestationInvalidation(
    const char* stale_attestation_token,
    const char* stale_reason,
    char* out_audit_token,
    DWORD out_len
);
SP_EXPORT int SP_CALL SP_QuarantineUnsupportedTarget(
    const char* operator_token,
    const char* reservation_handle,
    const char* unsupported_target_id,
    char* out_quarantine_token,
    DWORD out_len
);
SP_EXPORT int SP_CALL SP_RecordUnsupportedTargetQuarantine(
    const char* unsupported_target_id,
    const char* quarantine_token,
    char* out_quarantine_audit_token,
    DWORD out_len
);

#ifdef __cplusplus
}
#endif

#endif
