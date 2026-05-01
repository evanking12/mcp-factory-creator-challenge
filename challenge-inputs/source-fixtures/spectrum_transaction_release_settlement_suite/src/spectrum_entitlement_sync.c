#include "..\include\spectrum_suite.h"

SP_EXPORT int SP_CALL SP_StageCustomerEntitlementUpdate(
    const char* attestation_packet,
    char* out_entitlement_stage_id,
    DWORD out_len
) {
    char entitlement_stage_id[96];
    int dep_status = sp_require_dependency("spectrum_attestation_stage.dll", "SP_EmitReleaseAttestation");
    if (dep_status != SP_OK) {
        return dep_status;
    }
    if (!sp_has_prefix(attestation_packet, "sp-att-")) {
        return SP_ERR_BADTOKEN;
    }
    if (!out_entitlement_stage_id || out_len < 24) {
        return SP_ERR_BADPARAM;
    }
    sp_make_token(entitlement_stage_id, sizeof(entitlement_stage_id), "sp-entitlement-stage", attestation_packet + 7, "grant");
    return sp_copy_out(out_entitlement_stage_id, out_len, entitlement_stage_id);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    (void)hinstDLL;
    (void)fdwReason;
    (void)lpReserved;
    return TRUE;
}
