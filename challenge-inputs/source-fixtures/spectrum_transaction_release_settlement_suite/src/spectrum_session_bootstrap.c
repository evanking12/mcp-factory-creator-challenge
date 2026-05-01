#include "..\include\spectrum_suite.h"

SP_EXPORT int SP_CALL SP_OpenOperatorSession(const char* operator_id, char* out_operator_token, DWORD out_len) {
    char token[96];
    const char* effective_operator_id = (operator_id && *operator_id) ? operator_id : "easy01-operator";
    if (!out_operator_token || out_len < 24) {
        return SP_ERR_BADPARAM;
    }
    sp_make_token(token, sizeof(token), "sp-operator", effective_operator_id, "session");
    return sp_copy_out(out_operator_token, out_len, token);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    (void)hinstDLL;
    (void)fdwReason;
    (void)lpReserved;
    return TRUE;
}
