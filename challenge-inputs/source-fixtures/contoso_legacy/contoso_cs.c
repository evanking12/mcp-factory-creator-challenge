/*
 * CONTOSO CORPORATION
 * Customer Services Core Library
 * Version 2.3.1  (c) 1998-2004 Contoso Corp. All rights reserved.
 *
 * INTERNAL USE ONLY - NOT FOR DISTRIBUTION
 * Compiled with: MSVC 6.0 /O2 /GS-
 * DO NOT MODIFY WITHOUT AUTHORIZATION FROM IT DEPT EXT 4422
 *
 * Revision history:
 *   2004-11-02  jrwilkes   Added ProcessRefund, fixed Y2K holdover in date calc
 *   2003-06-14  dstone     LoyaltyMultiplier now tier-aware
 *   2002-01-08  jrwilkes   Initial stable release
 */

#include <windows.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* -----------------------------------------------------------------------
 * Internal constants (obfuscated at link time via /GS-)
 * ----------------------------------------------------------------------- */
#define CS_MAX_CUST         512
#define CS_ACCT_SEED        0xDEADBEEF
#define CS_LOYALTY_BASE     100
#define CS_TIER_SILVER      1000
#define CS_TIER_GOLD        5000
#define CS_TIER_PLATINUM    15000
#define CS_XOR_KEY          0x5A
#define CS_VERSION_MAJ      2
#define CS_VERSION_MIN      3
#define CS_VERSION_REV      1

/* Error codes */
#define CS_OK               0
#define CS_ERR_NOTFOUND     -1
#define CS_ERR_BADPARAM     -2
#define CS_ERR_OVERFLOW     -3
#define CS_ERR_LOCKED       -4
#define CS_ERR_INSUFFICENT  -5
#define CS_ERR_EXPIRED      -6

/* Account status flags */
#define ACCT_ACTIVE         0x01
#define ACCT_LOCKED         0x02
#define ACCT_SUSPENDED      0x04
#define ACCT_VIP            0x08
#define ACCT_OVERDUE        0x10

/* -----------------------------------------------------------------------
 * Internal data structures (not in any header — Ghidra must infer these)
 * ----------------------------------------------------------------------- */
typedef struct _CUST_RECORD {
    char        szCustId[16];
    char        szFirstName[32];
    char        szLastName[32];
    char        szEmail[64];
    char        szPhone[16];
    DWORD       dwAccountBalance;   /* in cents */
    DWORD       dwLoyaltyPoints;
    DWORD       dwTotalSpend;       /* lifetime, in cents */
    WORD        wTier;              /* 0=standard,1=silver,2=gold,3=platinum */
    BYTE        bStatusFlags;
    BYTE        bFailedLogins;
    DWORD       dwLastActivityUnix;
    DWORD       dwChecksum;         /* XOR checksum of above fields */
} CUST_RECORD, *LPCUST_RECORD;

typedef struct _ORDER_ITEM {
    char        szSku[24];
    WORD        wQty;
    DWORD       dwUnitPrice;        /* cents */
} ORDER_ITEM, *LPORDER_ITEM;

typedef struct _ORDER_HDR {
    char        szOrderId[20];
    char        szCustId[16];
    ORDER_ITEM  arItems[16];
    WORD        wItemCount;
    DWORD       dwSubtotal;
    DWORD       dwDiscount;
    DWORD       dwTotal;
    BYTE        bStatus;            /* 0=pending,1=shipped,2=delivered,3=cancelled */
    DWORD       dwCreatedUnix;
} ORDER_HDR, *LPORDER_HDR;

typedef struct _REFUND_REQ {
    char        szOrderId[20];
    char        szCustId[16];
    DWORD       dwAmount;           /* cents to refund */
    char        szReason[128];
    BYTE        bApproved;
    DWORD       dwRefundId;
} REFUND_REQ, *LPREFUND_REQ;

/* -----------------------------------------------------------------------
 * Global state — simulated in-memory customer database
 * ----------------------------------------------------------------------- */
static CUST_RECORD  g_arCustomers[CS_MAX_CUST];
static DWORD        g_dwCustCount   = 0;
static ORDER_HDR    g_arOrders[2048];
static DWORD        g_dwOrderCount  = 0;
static BOOL         g_bInitialised  = FALSE;
static DWORD        g_dwCallCount   = 0;     /* telemetry */

/* -----------------------------------------------------------------------
 * Internal helpers — NOT exported, but Ghidra recovers these
 * ----------------------------------------------------------------------- */

/* XOR-encode/decode a buffer in-place (used for "security") */
static void _xor_buf(BYTE *pBuf, DWORD dwLen) {
    DWORD i;
    for (i = 0; i < dwLen; i++)
        pBuf[i] ^= CS_XOR_KEY;
}

/* Compute a naive checksum over the fixed fields of a customer record */
static DWORD _calc_checksum(LPCUST_RECORD pRec) {
    DWORD dw = CS_ACCT_SEED;
    BYTE *p  = (BYTE*)pRec;
    DWORD i;
    /* exclude the checksum field itself (last 4 bytes) */
    for (i = 0; i < sizeof(CUST_RECORD) - sizeof(DWORD); i++)
        dw ^= (DWORD)p[i] * (i + 1);
    return dw;
}

/* Validate checksum — returns TRUE if record is untampered */
static BOOL _validate_checksum(LPCUST_RECORD pRec) {
    return (pRec->dwChecksum == _calc_checksum(pRec));
}

/* Look up customer index by ID string; returns -1 if not found */
static int _find_customer(const char *lpszCustId) {
    DWORD i;
    if (!lpszCustId || !*lpszCustId) return -1;
    for (i = 0; i < g_dwCustCount; i++) {
        if (_stricmp(g_arCustomers[i].szCustId, lpszCustId) == 0)
            return (int)i;
    }
    return -1;
}

/* Compute tier from lifetime spend */
static WORD _compute_tier(DWORD dwTotalSpendCents) {
    DWORD dollars = dwTotalSpendCents / 100;
    if (dollars >= CS_TIER_PLATINUM) return 3;
    if (dollars >= CS_TIER_GOLD)     return 2;
    if (dollars >= CS_TIER_SILVER)   return 1;
    return 0;
}

/* Seed the in-memory DB with Contoso demo customers */
static void _seed_database(void) {
    DWORD now = (DWORD)time(NULL);

    /* CUST-001 */
    memset(&g_arCustomers[0], 0, sizeof(CUST_RECORD));
    lstrcpyA(g_arCustomers[0].szCustId,    "CUST-001");
    lstrcpyA(g_arCustomers[0].szFirstName, "Alice");
    lstrcpyA(g_arCustomers[0].szLastName,  "Contoso");
    lstrcpyA(g_arCustomers[0].szEmail,     "alice@contoso.com");
    lstrcpyA(g_arCustomers[0].szPhone,     "555-0101");
    g_arCustomers[0].dwAccountBalance   = 25000;   /* $250.00 */
    g_arCustomers[0].dwLoyaltyPoints    = 1450;
    g_arCustomers[0].dwTotalSpend       = 620000;  /* $6200 lifetime */
    g_arCustomers[0].wTier              = 2;       /* gold */
    g_arCustomers[0].bStatusFlags       = ACCT_ACTIVE | ACCT_VIP;
    g_arCustomers[0].dwLastActivityUnix = now - 86400;
    g_arCustomers[0].dwChecksum         = _calc_checksum(&g_arCustomers[0]);

    /* CUST-002 */
    memset(&g_arCustomers[1], 0, sizeof(CUST_RECORD));
    lstrcpyA(g_arCustomers[1].szCustId,    "CUST-002");
    lstrcpyA(g_arCustomers[1].szFirstName, "Bob");
    lstrcpyA(g_arCustomers[1].szLastName,  "Builder");
    lstrcpyA(g_arCustomers[1].szEmail,     "bob.builder@fabrikam.com");
    lstrcpyA(g_arCustomers[1].szPhone,     "555-0202");
    g_arCustomers[1].dwAccountBalance   = 5000;    /* $50.00 */
    g_arCustomers[1].dwLoyaltyPoints    = 320;
    g_arCustomers[1].dwTotalSpend       = 85000;
    g_arCustomers[1].wTier              = 0;       /* standard */
    g_arCustomers[1].bStatusFlags       = ACCT_ACTIVE;
    g_arCustomers[1].dwLastActivityUnix = now - 604800;
    g_arCustomers[1].dwChecksum         = _calc_checksum(&g_arCustomers[1]);

    /* CUST-003 — locked account */
    memset(&g_arCustomers[2], 0, sizeof(CUST_RECORD));
    lstrcpyA(g_arCustomers[2].szCustId,    "CUST-003");
    lstrcpyA(g_arCustomers[2].szFirstName, "Carol");
    lstrcpyA(g_arCustomers[2].szLastName,  "Danvers");
    lstrcpyA(g_arCustomers[2].szEmail,     "carol.d@northwind.com");
    lstrcpyA(g_arCustomers[2].szPhone,     "555-0303");
    g_arCustomers[2].dwAccountBalance   = 0;
    g_arCustomers[2].dwLoyaltyPoints    = 75;
    g_arCustomers[2].dwTotalSpend       = 22000;
    g_arCustomers[2].wTier              = 0;
    g_arCustomers[2].bStatusFlags       = ACCT_LOCKED;
    g_arCustomers[2].bFailedLogins      = 5;
    g_arCustomers[2].dwLastActivityUnix = now - 2592000;
    g_arCustomers[2].dwChecksum         = _calc_checksum(&g_arCustomers[2]);

    /* CUST-004 — platinum */
    memset(&g_arCustomers[3], 0, sizeof(CUST_RECORD));
    lstrcpyA(g_arCustomers[3].szCustId,    "CUST-004");
    lstrcpyA(g_arCustomers[3].szFirstName, "David");
    lstrcpyA(g_arCustomers[3].szLastName,  "Nakamura");
    lstrcpyA(g_arCustomers[3].szEmail,     "dnakamura@contoso.com");
    lstrcpyA(g_arCustomers[3].szPhone,     "555-0404");
    g_arCustomers[3].dwAccountBalance   = 120000;  /* $1200 */
    g_arCustomers[3].dwLoyaltyPoints    = 18750;
    g_arCustomers[3].dwTotalSpend       = 2450000; /* $24500 lifetime */
    g_arCustomers[3].wTier              = 3;       /* platinum */
    g_arCustomers[3].bStatusFlags       = ACCT_ACTIVE | ACCT_VIP;
    g_arCustomers[3].dwLastActivityUnix = now - 3600;
    g_arCustomers[3].dwChecksum         = _calc_checksum(&g_arCustomers[3]);

    g_dwCustCount = 4;
    g_bInitialised = TRUE;
}

/* -----------------------------------------------------------------------
 * Exported API — these are what the MCP factory must discover and expose
 * ----------------------------------------------------------------------- */

/*
 * CS_Initialize
 * Must be called once before any other function.
 * Returns CS_OK on success.
 */
__declspec(dllexport) int __cdecl CS_Initialize(void) {
    if (g_bInitialised) return CS_OK;
    _seed_database();
    g_dwCallCount = 0;
    return CS_OK;
}

/*
 * CS_GetVersion
 * Returns encoded version integer: (major<<16)|(minor<<8)|rev
 */
__declspec(dllexport) DWORD __cdecl CS_GetVersion(void) {
    return (CS_VERSION_MAJ << 16) | (CS_VERSION_MIN << 8) | CS_VERSION_REV;
}

/*
 * CS_LookupCustomer
 * lpszCustId   : null-terminated customer ID string (e.g. "CUST-001")
 * pOutBuf      : caller-allocated buffer to receive pipe-delimited record
 * dwBufLen     : size of pOutBuf in bytes
 *
 * Returns CS_OK, CS_ERR_NOTFOUND, CS_ERR_BADPARAM, CS_ERR_OVERFLOW
 */
__declspec(dllexport) int __cdecl CS_LookupCustomer(
        const char *lpszCustId,
        char       *pOutBuf,
        DWORD       dwBufLen)
{
    int idx;
    LPCUST_RECORD pRec;
    char szTier[12];

    g_dwCallCount++;
    if (!lpszCustId || !pOutBuf || dwBufLen < 64)
        return CS_ERR_BADPARAM;
    if (!g_bInitialised) CS_Initialize();

    idx = _find_customer(lpszCustId);
    if (idx < 0) return CS_ERR_NOTFOUND;

    pRec = &g_arCustomers[idx];
    if (!_validate_checksum(pRec)) return CS_ERR_LOCKED;  /* tampered record */

    switch (pRec->wTier) {
        case 1: lstrcpyA(szTier, "Silver");   break;
        case 2: lstrcpyA(szTier, "Gold");     break;
        case 3: lstrcpyA(szTier, "Platinum"); break;
        default: lstrcpyA(szTier, "Standard");
    }

    _snprintf(pOutBuf, dwBufLen - 1,
        "id=%s|name=%s %s|email=%s|phone=%s|balance=$%.2f|points=%lu|tier=%s|status=%s",
        pRec->szCustId,
        pRec->szFirstName, pRec->szLastName,
        pRec->szEmail,
        pRec->szPhone,
        pRec->dwAccountBalance / 100.0,
        pRec->dwLoyaltyPoints,
        szTier,
        (pRec->bStatusFlags & ACCT_LOCKED)    ? "LOCKED" :
        (pRec->bStatusFlags & ACCT_SUSPENDED) ? "SUSPENDED" :
        (pRec->bStatusFlags & ACCT_ACTIVE)    ? "ACTIVE" : "INACTIVE"
    );
    pOutBuf[dwBufLen - 1] = '\0';
    return CS_OK;
}

/*
 * CS_GetAccountBalance
 * lpszCustId   : customer ID
 * pdwCents     : receives balance in cents on success
 *
 * Returns CS_OK, CS_ERR_NOTFOUND, CS_ERR_LOCKED
 */
__declspec(dllexport) int __stdcall CS_GetAccountBalance(
        const char *lpszCustId,
        DWORD      *pdwCents)
{
    int idx;
    g_dwCallCount++;
    if (!lpszCustId || !pdwCents) return CS_ERR_BADPARAM;
    if (!g_bInitialised) CS_Initialize();

    idx = _find_customer(lpszCustId);
    if (idx < 0) return CS_ERR_NOTFOUND;

    if (g_arCustomers[idx].bStatusFlags & ACCT_LOCKED)
        return CS_ERR_LOCKED;

    *pdwCents = g_arCustomers[idx].dwAccountBalance;
    return CS_OK;
}

/*
 * CS_GetLoyaltyPoints
 * lpszCustId   : customer ID
 * pdwPoints    : receives point balance
 *
 * Returns CS_OK or error code
 */
__declspec(dllexport) int __cdecl CS_GetLoyaltyPoints(
        const char *lpszCustId,
        DWORD      *pdwPoints)
{
    int idx;
    g_dwCallCount++;
    if (!lpszCustId || !pdwPoints) return CS_ERR_BADPARAM;
    if (!g_bInitialised) CS_Initialize();

    idx = _find_customer(lpszCustId);
    if (idx < 0) return CS_ERR_NOTFOUND;

    *pdwPoints = g_arCustomers[idx].dwLoyaltyPoints;
    return CS_OK;
}

/*
 * CS_RedeemLoyaltyPoints
 * lpszCustId   : customer ID
 * dwPoints     : number of points to redeem
 * pdwCentsAdded: receives the dollar credit added (points * 0.01)
 *
 * Returns CS_OK, CS_ERR_INSUFFICENT, CS_ERR_LOCKED, CS_ERR_NOTFOUND
 */
__declspec(dllexport) int __stdcall CS_RedeemLoyaltyPoints(
        const char *lpszCustId,
        DWORD       dwPoints,
        DWORD      *pdwCentsAdded)
{
    int idx;
    LPCUST_RECORD pRec;
    g_dwCallCount++;
    if (!lpszCustId || !pdwCentsAdded || dwPoints == 0)
        return CS_ERR_BADPARAM;
    if (!g_bInitialised) CS_Initialize();

    idx = _find_customer(lpszCustId);
    if (idx < 0) return CS_ERR_NOTFOUND;

    pRec = &g_arCustomers[idx];
    if (pRec->bStatusFlags & ACCT_LOCKED)    return CS_ERR_LOCKED;
    if (pRec->dwLoyaltyPoints < dwPoints)    return CS_ERR_INSUFFICENT;

    pRec->dwLoyaltyPoints  -= dwPoints;
    *pdwCentsAdded          = dwPoints;  /* 1 point = $0.01 */
    pRec->dwAccountBalance += *pdwCentsAdded;
    pRec->dwChecksum        = _calc_checksum(pRec);
    return CS_OK;
}

/*
 * CS_ProcessPayment
 * lpszCustId   : customer ID
 * dwAmountCents: amount to debit from account balance
 * lpszOrderRef : optional order reference string (may be NULL)
 *
 * Returns CS_OK, CS_ERR_INSUFFICENT, CS_ERR_LOCKED, CS_ERR_NOTFOUND
 */
__declspec(dllexport) int __cdecl CS_ProcessPayment(
        const char *lpszCustId,
        DWORD       dwAmountCents,
        const char *lpszOrderRef)
{
    int idx;
    LPCUST_RECORD pRec;
    DWORD dwBonus;

    g_dwCallCount++;
    if (!lpszCustId || dwAmountCents == 0) return CS_ERR_BADPARAM;
    if (!g_bInitialised) CS_Initialize();

    idx = _find_customer(lpszCustId);
    if (idx < 0) return CS_ERR_NOTFOUND;
    pRec = &g_arCustomers[idx];

    if (pRec->bStatusFlags & ACCT_LOCKED)    return CS_ERR_LOCKED;
    if (pRec->dwAccountBalance < dwAmountCents) return CS_ERR_INSUFFICENT;

    pRec->dwAccountBalance -= dwAmountCents;
    pRec->dwTotalSpend     += dwAmountCents;

    /* Award loyalty points: 1 per dollar, tier multiplier */
    dwBonus = dwAmountCents / 100;
    switch (pRec->wTier) {
        case 1: dwBonus = dwBonus * 3 / 2; break;  /* Silver: 1.5x */
        case 2: dwBonus = dwBonus * 2;     break;  /* Gold:   2x   */
        case 3: dwBonus = dwBonus * 3;     break;  /* Platinum: 3x */
    }
    pRec->dwLoyaltyPoints += dwBonus;

    /* Re-evaluate tier after spend update */
    pRec->wTier      = _compute_tier(pRec->dwTotalSpend);
    pRec->dwChecksum = _calc_checksum(pRec);

    (void)lpszOrderRef;  /* stored in order log — not implemented here */
    return CS_OK;
}

/*
 * CS_ProcessRefund
 * lpszCustId   : customer ID
 * dwAmountCents: amount to credit back to account
 * lpszReason   : free-text reason (logged, may be NULL)
 * pdwRefundId  : receives a generated refund tracking ID
 *
 * Returns CS_OK, CS_ERR_NOTFOUND, CS_ERR_BADPARAM, CS_ERR_OVERFLOW
 */
__declspec(dllexport) int __cdecl CS_ProcessRefund(
        const char *lpszCustId,
        DWORD       dwAmountCents,
        const char *lpszReason,
        DWORD      *pdwRefundId)
{
    int idx;
    LPCUST_RECORD pRec;
    DWORD dwNewBal;
    g_dwCallCount++;

    if (!lpszCustId || dwAmountCents == 0 || !pdwRefundId)
        return CS_ERR_BADPARAM;
    if (!g_bInitialised) CS_Initialize();

    idx = _find_customer(lpszCustId);
    if (idx < 0) return CS_ERR_NOTFOUND;
    pRec = &g_arCustomers[idx];

    dwNewBal = pRec->dwAccountBalance + dwAmountCents;
    if (dwNewBal < pRec->dwAccountBalance) return CS_ERR_OVERFLOW;  /* wraparound */

    pRec->dwAccountBalance = dwNewBal;
    *pdwRefundId           = CS_ACCT_SEED ^ g_dwCallCount ^ dwAmountCents;
    pRec->dwChecksum       = _calc_checksum(pRec);

    (void)lpszReason;
    return CS_OK;
}

/*
 * CS_UnlockAccount
 * lpszCustId       : customer ID to unlock
 * lpszAuthToken    : 8-char auth token (XOR-validated internally)
 *
 * Returns CS_OK, CS_ERR_NOTFOUND, CS_ERR_BADPARAM
 */
__declspec(dllexport) int __stdcall CS_UnlockAccount(
        const char *lpszCustId,
        const char *lpszAuthToken)
{
    int idx;
    LPCUST_RECORD pRec;
    BYTE bCheck;
    int i;
    g_dwCallCount++;

    if (!lpszCustId || !lpszAuthToken || lstrlenA(lpszAuthToken) < 4)
        return CS_ERR_BADPARAM;
    if (!g_bInitialised) CS_Initialize();

    idx = _find_customer(lpszCustId);
    if (idx < 0) return CS_ERR_NOTFOUND;
    pRec = &g_arCustomers[idx];

    /* Validate token: XOR of chars must equal 0xA5 */
    bCheck = 0;
    for (i = 0; lpszAuthToken[i]; i++)
        bCheck ^= (BYTE)lpszAuthToken[i];
    if (bCheck != 0xA5) return CS_ERR_BADPARAM;

    pRec->bStatusFlags  &= ~ACCT_LOCKED;
    pRec->bStatusFlags  |= ACCT_ACTIVE;
    pRec->bFailedLogins  = 0;
    pRec->dwChecksum     = _calc_checksum(pRec);
    return CS_OK;
}

/*
 * CS_CalculateInterest
 * dwPrincipalCents : principal amount in cents
 * dwAnnualRateBps  : annual interest rate in basis points (e.g. 500 = 5.00%)
 * wTermMonths      : loan term in months
 * pdwMonthlyPayment: receives monthly payment in cents
 *
 * Returns CS_OK, CS_ERR_BADPARAM
 * Uses standard amortization formula.
 */
__declspec(dllexport) int __cdecl CS_CalculateInterest(
        DWORD  dwPrincipalCents,
        DWORD  dwAnnualRateBps,
        WORD   wTermMonths,
        DWORD *pdwMonthlyPayment)
{
    double dP, dR, dM, dPow;
    g_dwCallCount++;

    if (!pdwMonthlyPayment || dwPrincipalCents == 0 || wTermMonths == 0)
        return CS_ERR_BADPARAM;

    dP = (double)dwPrincipalCents;
    dR = (dwAnnualRateBps / 10000.0) / 12.0;  /* monthly rate */

    if (dR < 1e-9) {
        /* zero-interest: equal installments */
        *pdwMonthlyPayment = (DWORD)(dP / wTermMonths);
        return CS_OK;
    }

    dPow = 1.0;
    {
        int i;
        for (i = 0; i < wTermMonths; i++) dPow *= (1.0 + dR);
    }
    dM = dP * dR * dPow / (dPow - 1.0);
    *pdwMonthlyPayment = (DWORD)(dM + 0.5);
    return CS_OK;
}

/*
 * CS_GetOrderStatus
 * lpszOrderId  : order ID string (e.g. "ORD-20040301-0042")
 * pOutBuf      : caller buffer, receives pipe-delimited status string
 * dwBufLen     : buffer size
 *
 * Returns CS_OK, CS_ERR_NOTFOUND
 * Note: order database pre-seeded with sample orders for CUST-001 and CUST-004
 */
__declspec(dllexport) int __cdecl CS_GetOrderStatus(
        const char *lpszOrderId,
        char       *pOutBuf,
        DWORD       dwBufLen)
{
    static BOOL bOrdsSeeded = FALSE;
    DWORD i;
    g_dwCallCount++;

    if (!lpszOrderId || !pOutBuf || dwBufLen < 32) return CS_ERR_BADPARAM;
    if (!g_bInitialised) CS_Initialize();

    /* Lazy-seed a few sample orders */
    if (!bOrdsSeeded) {
        memset(g_arOrders, 0, sizeof(ORDER_HDR) * 3);

        lstrcpyA(g_arOrders[0].szOrderId, "ORD-20040301-0042");
        lstrcpyA(g_arOrders[0].szCustId,  "CUST-001");
        lstrcpyA(g_arOrders[0].arItems[0].szSku, "WDG-XL-BLU");
        g_arOrders[0].arItems[0].wQty       = 2;
        g_arOrders[0].arItems[0].dwUnitPrice = 4999;
        g_arOrders[0].wItemCount  = 1;
        g_arOrders[0].dwSubtotal  = 9998;
        g_arOrders[0].dwDiscount  = 500;
        g_arOrders[0].dwTotal     = 9498;
        g_arOrders[0].bStatus     = 2;  /* delivered */
        g_arOrders[0].dwCreatedUnix = 1078012800UL;

        lstrcpyA(g_arOrders[1].szOrderId, "ORD-20040315-0117");
        lstrcpyA(g_arOrders[1].szCustId,  "CUST-004");
        lstrcpyA(g_arOrders[1].arItems[0].szSku, "PRO-SVC-ANNUAL");
        g_arOrders[1].arItems[0].wQty       = 1;
        g_arOrders[1].arItems[0].dwUnitPrice = 59900;
        g_arOrders[1].wItemCount  = 1;
        g_arOrders[1].dwSubtotal  = 59900;
        g_arOrders[1].dwDiscount  = 5990;
        g_arOrders[1].dwTotal     = 53910;
        g_arOrders[1].bStatus     = 1;  /* shipped */
        g_arOrders[1].dwCreatedUnix = 1079308800UL;

        g_dwOrderCount = 2;
        bOrdsSeeded    = TRUE;
    }

    for (i = 0; i < g_dwOrderCount; i++) {
        if (_stricmp(g_arOrders[i].szOrderId, lpszOrderId) == 0) {
            const char *pStatus;
            switch (g_arOrders[i].bStatus) {
                case 0: pStatus = "PENDING";   break;
                case 1: pStatus = "SHIPPED";   break;
                case 2: pStatus = "DELIVERED"; break;
                case 3: pStatus = "CANCELLED"; break;
                default: pStatus = "UNKNOWN";
            }
            _snprintf(pOutBuf, dwBufLen - 1,
                "order=%s|customer=%s|items=%u|subtotal=$%.2f|discount=$%.2f|total=$%.2f|status=%s",
                g_arOrders[i].szOrderId,
                g_arOrders[i].szCustId,
                g_arOrders[i].wItemCount,
                g_arOrders[i].dwSubtotal  / 100.0,
                g_arOrders[i].dwDiscount  / 100.0,
                g_arOrders[i].dwTotal     / 100.0,
                pStatus);
            pOutBuf[dwBufLen - 1] = '\0';
            return CS_OK;
        }
    }
    return CS_ERR_NOTFOUND;
}

/*
 * CS_GetDiagnostics
 * pOutBuf  : caller buffer, receives diagnostic string
 * dwBufLen : buffer size
 *
 * Returns CS_OK always.
 * Reports internal call counter, customer count, build info.
 */
__declspec(dllexport) int __cdecl CS_GetDiagnostics(
        char  *pOutBuf,
        DWORD  dwBufLen)
{
    DWORD dwVer = CS_GetVersion();
    if (!pOutBuf || dwBufLen < 32) return CS_ERR_BADPARAM;
    _snprintf(pOutBuf, dwBufLen - 1,
        "version=%u.%u.%u|customers=%lu|orders=%lu|calls=%lu|initialized=%s",
        (dwVer >> 16) & 0xFF,
        (dwVer >>  8) & 0xFF,
        (dwVer      ) & 0xFF,
        g_dwCustCount,
        g_dwOrderCount,
        g_dwCallCount,
        g_bInitialised ? "yes" : "no");
    pOutBuf[dwBufLen - 1] = '\0';
    return CS_OK;
}

/* -----------------------------------------------------------------------
 * DLL entry point
 * ----------------------------------------------------------------------- */
BOOL WINAPI DllMain(HINSTANCE hInstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hInstDLL);
            break;
        case DLL_PROCESS_DETACH:
            break;
    }
    (void)lpvReserved;
    return TRUE;
}
