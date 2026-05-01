#include "..\include\spectrum_suite.h"

SP_EXPORT int SP_CALL SP_StageReleaseInvoice(
    const char* attestation_packet,
    char* out_invoice_stage_id,
    DWORD out_len
) {
    char invoice_stage_id[96];
    int dep_status = sp_require_dependency("spectrum_attestation_stage.dll", "SP_EmitReleaseAttestation");
    if (dep_status != SP_OK) {
        return dep_status;
    }
    if (!sp_has_prefix(attestation_packet, "sp-att-")) {
        return SP_ERR_BADTOKEN;
    }
    if (!out_invoice_stage_id || out_len < 24) {
        return SP_ERR_BADPARAM;
    }
    sp_make_token(invoice_stage_id, sizeof(invoice_stage_id), "sp-invoice-stage", attestation_packet + 7, "bill");
    return sp_copy_out(out_invoice_stage_id, out_len, invoice_stage_id);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    (void)hinstDLL;
    (void)fdwReason;
    (void)lpReserved;
    return TRUE;
}
