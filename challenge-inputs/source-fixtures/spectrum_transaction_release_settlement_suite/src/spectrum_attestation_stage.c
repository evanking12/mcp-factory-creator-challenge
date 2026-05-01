#include "..\include\spectrum_suite.h"

SP_EXPORT int SP_CALL SP_EmitReleaseAttestation(
    const char* preview_ticket,
    const char* fee_quote_id,
    char* out_attestation_packet,
    DWORD packet_len,
    uint32_t* out_attestation_nonce,
    uint32_t* out_attestation_epoch
) {
    char attestation_packet[96];
    int dep_preview = sp_require_dependency("spectrum_dispatch_preview.dll", "SP_PrepareReleasePreview");
    int dep_fee = sp_require_dependency("spectrum_fee_quote.dll", "SP_StageReleaseFeeQuote");
    if (dep_preview != SP_OK || dep_fee != SP_OK) {
        return SP_ERR_DEPENDENCY;
    }
    if (!sp_has_prefix(preview_ticket, "sp-preview-") || !sp_has_prefix(fee_quote_id, "sp-fee-")) {
        return SP_ERR_BADTOKEN;
    }
    if (!out_attestation_packet || packet_len < 24 || !out_attestation_nonce || !out_attestation_epoch) {
        return SP_ERR_BADPARAM;
    }
    sp_make_token(attestation_packet, sizeof(attestation_packet), "sp-att", preview_ticket + 11, "packet");
    *out_attestation_nonce = 0xA7700000UL | ((uint32_t)strlen(fee_quote_id) & 0xFFFFU);
    *out_attestation_epoch = 20260408U;
    return sp_copy_out(out_attestation_packet, packet_len, attestation_packet);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    (void)hinstDLL;
    (void)fdwReason;
    (void)lpReserved;
    return TRUE;
}
