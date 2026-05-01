/*
 * CONTOSO CORPORATION
 * Payment Processing & Inventory Services Library
 * Version 1.4.0  (c) 2002-2004 Contoso Corp. All rights reserved.
 *
 * INTERNAL USE ONLY - NOT FOR DISTRIBUTION
 * Compiled with: MSVC 6.0 /O2 /GS-
 * DO NOT MODIFY WITHOUT AUTHORIZATION FROM FINANCE DEPT EXT 4488
 *
 * Revision history:
 *   2004-09-22  mhansen    Added CP_GetTransactionHistory, fixed discount cap
 *   2003-11-05  dstone     Inventory reservation system
 *   2003-03-18  mhansen    Initial stable release
 *
 * Interweave notes:
 *   This DLL shares the customer ID namespace with contoso_cs.dll.
 *   Customer IDs like "CUST-001" are valid across both modules.
 *   contoso_cs handles customer records/loyalty; this handles payments/inventory.
 */

#include <windows.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* -----------------------------------------------------------------------
 * Internal constants
 * ----------------------------------------------------------------------- */
#define CP_MAX_PRODUCTS     256
#define CP_MAX_TRANSACTIONS 4096
#define CP_MAX_RESERVATIONS 512
#define CP_MERCHANT_SEED    0xCAFEBABE
#define CP_TAX_RATE_BPS     875     /* 8.75% sales tax in basis points */
#define CP_DISCOUNT_CAP_PCT 25      /* max discount percentage */
#define CP_VERSION_MAJ      1
#define CP_VERSION_MIN      4
#define CP_VERSION_REV      0

/* Error codes — intentionally overlapping semantics with contoso_cs */
#define CP_OK               0
#define CP_ERR_NOTFOUND     -1
#define CP_ERR_BADPARAM     -2
#define CP_ERR_OVERFLOW     -3
#define CP_ERR_NOSTOCK      -4
#define CP_ERR_INSUFFICIENT -5
#define CP_ERR_EXPIRED      -6
#define CP_ERR_DUPLICATE    -7
#define CP_ERR_LIMIT        -8

/* Transaction types */
#define TXN_PURCHASE        0x01
#define TXN_REFUND          0x02
#define TXN_VOID            0x03
#define TXN_ADJUSTMENT      0x04

/* Reservation status */
#define RESV_ACTIVE         0x01
#define RESV_FULFILLED      0x02
#define RESV_EXPIRED        0x04
#define RESV_CANCELLED      0x08

/* -----------------------------------------------------------------------
 * Internal data structures (not in any header — Ghidra must infer these)
 * ----------------------------------------------------------------------- */
typedef struct _PRODUCT_REC {
    char        szSku[24];
    char        szName[64];
    char        szCategory[32];
    DWORD       dwUnitPriceCents;
    DWORD       dwStockQty;
    DWORD       dwReservedQty;
    WORD        wReorderPoint;
    BYTE        bActive;
    BYTE        bTaxable;
    DWORD       dwChecksum;
} PRODUCT_REC, *LPPRODUCT_REC;

typedef struct _TRANSACTION_REC {
    char        szTxnId[24];
    char        szCustId[16];
    char        szSku[24];
    BYTE        bType;
    DWORD       dwAmountCents;
    DWORD       dwTaxCents;
    DWORD       dwDiscountCents;
    WORD        wQty;
    DWORD       dwTimestampUnix;
    DWORD       dwAuthCode;
} TRANSACTION_REC, *LPTRANSACTION_REC;

typedef struct _RESERVATION_REC {
    char        szResvId[24];
    char        szCustId[16];
    char        szSku[24];
    WORD        wQty;
    BYTE        bStatus;
    DWORD       dwCreatedUnix;
    DWORD       dwExpiresUnix;
} RESERVATION_REC, *LPRESERVATION_REC;

/* -----------------------------------------------------------------------
 * Global state — simulated in-memory databases
 * ----------------------------------------------------------------------- */
static PRODUCT_REC       g_arProducts[CP_MAX_PRODUCTS];
static DWORD             g_dwProductCount   = 0;
static TRANSACTION_REC   g_arTransactions[CP_MAX_TRANSACTIONS];
static DWORD             g_dwTxnCount       = 0;
static RESERVATION_REC   g_arReservations[CP_MAX_RESERVATIONS];
static DWORD             g_dwResvCount      = 0;
static BOOL              g_bInitialised     = FALSE;
static DWORD             g_dwCallCount      = 0;

/* -----------------------------------------------------------------------
 * Internal helpers — NOT exported, but Ghidra recovers these
 * ----------------------------------------------------------------------- */

static DWORD _calc_product_checksum(LPPRODUCT_REC pRec) {
    DWORD dw = CP_MERCHANT_SEED;
    BYTE *p  = (BYTE*)pRec;
    DWORD i;
    for (i = 0; i < sizeof(PRODUCT_REC) - sizeof(DWORD); i++)
        dw ^= (DWORD)p[i] * (i + 1);
    return dw;
}

static int _find_product(const char *lpszSku) {
    DWORD i;
    if (!lpszSku || !*lpszSku) return -1;
    for (i = 0; i < g_dwProductCount; i++) {
        if (_stricmp(g_arProducts[i].szSku, lpszSku) == 0)
            return (int)i;
    }
    return -1;
}

static DWORD _generate_auth_code(DWORD dwSeed) {
    return (CP_MERCHANT_SEED ^ dwSeed ^ g_dwCallCount) & 0x7FFFFFFF;
}

static void _generate_txn_id(char *pBuf, DWORD dwBufLen) {
    DWORD now = (DWORD)time(NULL);
    _snprintf(pBuf, dwBufLen - 1, "TXN-%08X-%04X", now, g_dwTxnCount & 0xFFFF);
    pBuf[dwBufLen - 1] = '\0';
}

static DWORD _compute_tax(DWORD dwAmountCents, BOOL bTaxable) {
    if (!bTaxable) return 0;
    return (dwAmountCents * CP_TAX_RATE_BPS + 5000) / 10000;
}

static void _seed_products(void) {
    DWORD now = (DWORD)time(NULL);

    memset(&g_arProducts[0], 0, sizeof(PRODUCT_REC));
    lstrcpyA(g_arProducts[0].szSku,      "WDG-XL-BLU");
    lstrcpyA(g_arProducts[0].szName,     "Widget XL Blue");
    lstrcpyA(g_arProducts[0].szCategory, "Widgets");
    g_arProducts[0].dwUnitPriceCents = 4999;
    g_arProducts[0].dwStockQty       = 150;
    g_arProducts[0].dwReservedQty    = 0;
    g_arProducts[0].wReorderPoint    = 25;
    g_arProducts[0].bActive          = 1;
    g_arProducts[0].bTaxable         = 1;
    g_arProducts[0].dwChecksum       = _calc_product_checksum(&g_arProducts[0]);

    memset(&g_arProducts[1], 0, sizeof(PRODUCT_REC));
    lstrcpyA(g_arProducts[1].szSku,      "PRO-SVC-ANNUAL");
    lstrcpyA(g_arProducts[1].szName,     "Pro Service Annual Plan");
    lstrcpyA(g_arProducts[1].szCategory, "Services");
    g_arProducts[1].dwUnitPriceCents = 59900;
    g_arProducts[1].dwStockQty       = 9999;
    g_arProducts[1].dwReservedQty    = 0;
    g_arProducts[1].wReorderPoint    = 0;
    g_arProducts[1].bActive          = 1;
    g_arProducts[1].bTaxable         = 0;
    g_arProducts[1].dwChecksum       = _calc_product_checksum(&g_arProducts[1]);

    memset(&g_arProducts[2], 0, sizeof(PRODUCT_REC));
    lstrcpyA(g_arProducts[2].szSku,      "GDG-SM-RED");
    lstrcpyA(g_arProducts[2].szName,     "Gadget Small Red");
    lstrcpyA(g_arProducts[2].szCategory, "Gadgets");
    g_arProducts[2].dwUnitPriceCents = 1299;
    g_arProducts[2].dwStockQty       = 500;
    g_arProducts[2].dwReservedQty    = 12;
    g_arProducts[2].wReorderPoint    = 50;
    g_arProducts[2].bActive          = 1;
    g_arProducts[2].bTaxable         = 1;
    g_arProducts[2].dwChecksum       = _calc_product_checksum(&g_arProducts[2]);

    memset(&g_arProducts[3], 0, sizeof(PRODUCT_REC));
    lstrcpyA(g_arProducts[3].szSku,      "ACC-USB-C");
    lstrcpyA(g_arProducts[3].szName,     "USB-C Charging Cable");
    lstrcpyA(g_arProducts[3].szCategory, "Accessories");
    g_arProducts[3].dwUnitPriceCents = 799;
    g_arProducts[3].dwStockQty       = 2000;
    g_arProducts[3].dwReservedQty    = 0;
    g_arProducts[3].wReorderPoint    = 200;
    g_arProducts[3].bActive          = 1;
    g_arProducts[3].bTaxable         = 1;
    g_arProducts[3].dwChecksum       = _calc_product_checksum(&g_arProducts[3]);

    memset(&g_arProducts[4], 0, sizeof(PRODUCT_REC));
    lstrcpyA(g_arProducts[4].szSku,      "SW-LIC-ENT");
    lstrcpyA(g_arProducts[4].szName,     "Enterprise License Key");
    lstrcpyA(g_arProducts[4].szCategory, "Software");
    g_arProducts[4].dwUnitPriceCents = 249900;
    g_arProducts[4].dwStockQty       = 50;
    g_arProducts[4].dwReservedQty    = 3;
    g_arProducts[4].wReorderPoint    = 5;
    g_arProducts[4].bActive          = 1;
    g_arProducts[4].bTaxable         = 0;
    g_arProducts[4].dwChecksum       = _calc_product_checksum(&g_arProducts[4]);

    /* Discontinued product — tests inactive handling */
    memset(&g_arProducts[5], 0, sizeof(PRODUCT_REC));
    lstrcpyA(g_arProducts[5].szSku,      "WDG-SM-GRN");
    lstrcpyA(g_arProducts[5].szName,     "Widget Small Green (DISC)");
    lstrcpyA(g_arProducts[5].szCategory, "Widgets");
    g_arProducts[5].dwUnitPriceCents = 2499;
    g_arProducts[5].dwStockQty       = 0;
    g_arProducts[5].dwReservedQty    = 0;
    g_arProducts[5].wReorderPoint    = 0;
    g_arProducts[5].bActive          = 0;
    g_arProducts[5].bTaxable         = 1;
    g_arProducts[5].dwChecksum       = _calc_product_checksum(&g_arProducts[5]);

    g_dwProductCount = 6;

    /* Seed a couple of historical transactions */
    memset(&g_arTransactions[0], 0, sizeof(TRANSACTION_REC));
    lstrcpyA(g_arTransactions[0].szTxnId,  "TXN-00000001-0000");
    lstrcpyA(g_arTransactions[0].szCustId, "CUST-001");
    lstrcpyA(g_arTransactions[0].szSku,    "WDG-XL-BLU");
    g_arTransactions[0].bType           = TXN_PURCHASE;
    g_arTransactions[0].dwAmountCents   = 9998;
    g_arTransactions[0].dwTaxCents      = 875;
    g_arTransactions[0].dwDiscountCents = 0;
    g_arTransactions[0].wQty            = 2;
    g_arTransactions[0].dwTimestampUnix = now - 2592000;
    g_arTransactions[0].dwAuthCode      = 0x12345678;

    memset(&g_arTransactions[1], 0, sizeof(TRANSACTION_REC));
    lstrcpyA(g_arTransactions[1].szTxnId,  "TXN-00000002-0001");
    lstrcpyA(g_arTransactions[1].szCustId, "CUST-004");
    lstrcpyA(g_arTransactions[1].szSku,    "SW-LIC-ENT");
    g_arTransactions[1].bType           = TXN_PURCHASE;
    g_arTransactions[1].dwAmountCents   = 249900;
    g_arTransactions[1].dwTaxCents      = 0;
    g_arTransactions[1].dwDiscountCents = 24990;
    g_arTransactions[1].wQty            = 1;
    g_arTransactions[1].dwTimestampUnix = now - 604800;
    g_arTransactions[1].dwAuthCode      = 0x87654321;

    g_dwTxnCount = 2;
    g_bInitialised = TRUE;
}

/* -----------------------------------------------------------------------
 * Exported API — 12 functions for MCP Factory to discover
 * ----------------------------------------------------------------------- */

/*
 * CP_Initialize
 * Must be called once before any other function.
 * Returns CP_OK on success.
 */
__declspec(dllexport) int __cdecl CP_Initialize(void) {
    if (g_bInitialised) return CP_OK;
    _seed_products();
    g_dwCallCount = 0;
    return CP_OK;
}

/*
 * CP_GetVersion
 * Returns encoded version: (major<<16)|(minor<<8)|rev
 */
__declspec(dllexport) DWORD __cdecl CP_GetVersion(void) {
    return (CP_VERSION_MAJ << 16) | (CP_VERSION_MIN << 8) | CP_VERSION_REV;
}

/*
 * CP_LookupProduct
 * lpszSku    : SKU string (e.g. "WDG-XL-BLU")
 * pOutBuf    : caller-allocated buffer for pipe-delimited product info
 * dwBufLen   : buffer size in bytes
 *
 * Returns CP_OK, CP_ERR_NOTFOUND, CP_ERR_BADPARAM
 */
__declspec(dllexport) int __cdecl CP_LookupProduct(
        const char *lpszSku,
        char       *pOutBuf,
        DWORD       dwBufLen)
{
    int idx;
    LPPRODUCT_REC pRec;
    g_dwCallCount++;

    if (!lpszSku || !pOutBuf || dwBufLen < 64)
        return CP_ERR_BADPARAM;
    if (!g_bInitialised) CP_Initialize();

    idx = _find_product(lpszSku);
    if (idx < 0) return CP_ERR_NOTFOUND;

    pRec = &g_arProducts[idx];
    _snprintf(pOutBuf, dwBufLen - 1,
        "sku=%s|name=%s|cat=%s|price=$%.2f|stock=%lu|reserved=%lu|active=%s|taxable=%s",
        pRec->szSku,
        pRec->szName,
        pRec->szCategory,
        pRec->dwUnitPriceCents / 100.0,
        pRec->dwStockQty,
        pRec->dwReservedQty,
        pRec->bActive ? "yes" : "no",
        pRec->bTaxable ? "yes" : "no");
    pOutBuf[dwBufLen - 1] = '\0';
    return CP_OK;
}

/*
 * CP_CheckStock
 * lpszSku     : SKU string
 * pdwAvailable: receives available quantity (stock - reserved)
 *
 * Returns CP_OK, CP_ERR_NOTFOUND, CP_ERR_BADPARAM
 */
__declspec(dllexport) int __stdcall CP_CheckStock(
        const char *lpszSku,
        DWORD      *pdwAvailable)
{
    int idx;
    g_dwCallCount++;
    if (!lpszSku || !pdwAvailable) return CP_ERR_BADPARAM;
    if (!g_bInitialised) CP_Initialize();

    idx = _find_product(lpszSku);
    if (idx < 0) return CP_ERR_NOTFOUND;

    *pdwAvailable = g_arProducts[idx].dwStockQty - g_arProducts[idx].dwReservedQty;
    return CP_OK;
}

/*
 * CP_CalculateTotal
 * lpszSku         : SKU string
 * wQty            : quantity to purchase
 * dwDiscountBps   : discount in basis points (e.g. 1000 = 10%)
 * pdwSubtotal     : receives subtotal in cents
 * pdwTax          : receives tax in cents
 * pdwTotal        : receives final total in cents
 *
 * Returns CP_OK, CP_ERR_NOTFOUND, CP_ERR_BADPARAM, CP_ERR_LIMIT
 */
__declspec(dllexport) int __cdecl CP_CalculateTotal(
        const char *lpszSku,
        WORD        wQty,
        DWORD       dwDiscountBps,
        DWORD      *pdwSubtotal,
        DWORD      *pdwTax,
        DWORD      *pdwTotal)
{
    int idx;
    DWORD dwSub, dwDisc, dwTax;
    g_dwCallCount++;

    if (!lpszSku || wQty == 0 || !pdwSubtotal || !pdwTax || !pdwTotal)
        return CP_ERR_BADPARAM;
    if (dwDiscountBps > CP_DISCOUNT_CAP_PCT * 100)
        return CP_ERR_LIMIT;
    if (!g_bInitialised) CP_Initialize();

    idx = _find_product(lpszSku);
    if (idx < 0) return CP_ERR_NOTFOUND;

    dwSub  = g_arProducts[idx].dwUnitPriceCents * wQty;
    dwDisc = (dwSub * dwDiscountBps + 5000) / 10000;
    dwSub -= dwDisc;
    dwTax  = _compute_tax(dwSub, g_arProducts[idx].bTaxable);

    *pdwSubtotal = dwSub;
    *pdwTax      = dwTax;
    *pdwTotal    = dwSub + dwTax;
    return CP_OK;
}

/*
 * CP_ProcessPurchase
 * lpszCustId      : customer ID (shared with contoso_cs namespace)
 * lpszSku         : SKU string
 * wQty            : quantity
 * dwDiscountBps   : discount in basis points
 * pdwAuthCode     : receives authorization code on success
 *
 * Returns CP_OK, CP_ERR_NOTFOUND, CP_ERR_NOSTOCK, CP_ERR_BADPARAM, CP_ERR_LIMIT
 */
__declspec(dllexport) int __stdcall CP_ProcessPurchase(
        const char *lpszCustId,
        const char *lpszSku,
        WORD        wQty,
        DWORD       dwDiscountBps,
        DWORD      *pdwAuthCode)
{
    int idx;
    LPPRODUCT_REC pProd;
    DWORD dwSub, dwDisc, dwTax, dwTotal;
    g_dwCallCount++;

    if (!lpszCustId || !lpszSku || wQty == 0 || !pdwAuthCode)
        return CP_ERR_BADPARAM;
    if (dwDiscountBps > CP_DISCOUNT_CAP_PCT * 100)
        return CP_ERR_LIMIT;
    if (!g_bInitialised) CP_Initialize();

    idx = _find_product(lpszSku);
    if (idx < 0) return CP_ERR_NOTFOUND;
    pProd = &g_arProducts[idx];

    if (!pProd->bActive) return CP_ERR_EXPIRED;
    if ((pProd->dwStockQty - pProd->dwReservedQty) < wQty)
        return CP_ERR_NOSTOCK;

    dwSub  = pProd->dwUnitPriceCents * wQty;
    dwDisc = (dwSub * dwDiscountBps + 5000) / 10000;
    dwSub -= dwDisc;
    dwTax  = _compute_tax(dwSub, pProd->bTaxable);
    dwTotal = dwSub + dwTax;

    pProd->dwStockQty -= wQty;
    pProd->dwChecksum  = _calc_product_checksum(pProd);

    if (g_dwTxnCount < CP_MAX_TRANSACTIONS) {
        LPTRANSACTION_REC pTxn = &g_arTransactions[g_dwTxnCount];
        memset(pTxn, 0, sizeof(TRANSACTION_REC));
        _generate_txn_id(pTxn->szTxnId, sizeof(pTxn->szTxnId));
        lstrcpynA(pTxn->szCustId, lpszCustId, sizeof(pTxn->szCustId));
        lstrcpynA(pTxn->szSku, lpszSku, sizeof(pTxn->szSku));
        pTxn->bType           = TXN_PURCHASE;
        pTxn->dwAmountCents   = dwTotal;
        pTxn->dwTaxCents      = dwTax;
        pTxn->dwDiscountCents = dwDisc;
        pTxn->wQty            = wQty;
        pTxn->dwTimestampUnix = (DWORD)time(NULL);
        pTxn->dwAuthCode      = _generate_auth_code(dwTotal);
        *pdwAuthCode = pTxn->dwAuthCode;
        g_dwTxnCount++;
    }
    return CP_OK;
}

/*
 * CP_VoidTransaction
 * lpszTxnId   : transaction ID to void
 * pdwRefunded : receives refunded amount in cents
 *
 * Returns CP_OK, CP_ERR_NOTFOUND, CP_ERR_BADPARAM, CP_ERR_DUPLICATE
 */
__declspec(dllexport) int __cdecl CP_VoidTransaction(
        const char *lpszTxnId,
        DWORD      *pdwRefunded)
{
    DWORD i;
    g_dwCallCount++;

    if (!lpszTxnId || !pdwRefunded) return CP_ERR_BADPARAM;
    if (!g_bInitialised) CP_Initialize();

    for (i = 0; i < g_dwTxnCount; i++) {
        if (_stricmp(g_arTransactions[i].szTxnId, lpszTxnId) == 0) {
            if (g_arTransactions[i].bType == TXN_VOID)
                return CP_ERR_DUPLICATE;

            *pdwRefunded = g_arTransactions[i].dwAmountCents;

            /* Restore stock */
            {
                int pidx = _find_product(g_arTransactions[i].szSku);
                if (pidx >= 0) {
                    g_arProducts[pidx].dwStockQty += g_arTransactions[i].wQty;
                    g_arProducts[pidx].dwChecksum = _calc_product_checksum(&g_arProducts[pidx]);
                }
            }

            g_arTransactions[i].bType = TXN_VOID;
            return CP_OK;
        }
    }
    return CP_ERR_NOTFOUND;
}

/*
 * CP_ReserveStock
 * lpszCustId   : customer ID
 * lpszSku      : SKU to reserve
 * wQty         : quantity
 * dwHoldMinutes: how long to hold the reservation
 * pOutResvId   : caller buffer for reservation ID
 * dwBufLen     : buffer size
 *
 * Returns CP_OK, CP_ERR_NOSTOCK, CP_ERR_NOTFOUND, CP_ERR_BADPARAM, CP_ERR_OVERFLOW
 */
__declspec(dllexport) int __stdcall CP_ReserveStock(
        const char *lpszCustId,
        const char *lpszSku,
        WORD        wQty,
        DWORD       dwHoldMinutes,
        char       *pOutResvId,
        DWORD       dwBufLen)
{
    int idx;
    LPPRODUCT_REC pProd;
    DWORD now;
    g_dwCallCount++;

    if (!lpszCustId || !lpszSku || wQty == 0 || !pOutResvId || dwBufLen < 24)
        return CP_ERR_BADPARAM;
    if (dwHoldMinutes == 0 || dwHoldMinutes > 1440)
        return CP_ERR_BADPARAM;
    if (!g_bInitialised) CP_Initialize();

    idx = _find_product(lpszSku);
    if (idx < 0) return CP_ERR_NOTFOUND;
    pProd = &g_arProducts[idx];

    if ((pProd->dwStockQty - pProd->dwReservedQty) < wQty)
        return CP_ERR_NOSTOCK;

    if (g_dwResvCount >= CP_MAX_RESERVATIONS)
        return CP_ERR_OVERFLOW;

    now = (DWORD)time(NULL);
    {
        LPRESERVATION_REC pResv = &g_arReservations[g_dwResvCount];
        memset(pResv, 0, sizeof(RESERVATION_REC));
        _snprintf(pResv->szResvId, sizeof(pResv->szResvId) - 1,
            "RESV-%08X-%04X", now, g_dwResvCount & 0xFFFF);
        lstrcpynA(pResv->szCustId, lpszCustId, sizeof(pResv->szCustId));
        lstrcpynA(pResv->szSku, lpszSku, sizeof(pResv->szSku));
        pResv->wQty         = wQty;
        pResv->bStatus      = RESV_ACTIVE;
        pResv->dwCreatedUnix = now;
        pResv->dwExpiresUnix = now + (dwHoldMinutes * 60);

        lstrcpynA(pOutResvId, pResv->szResvId, dwBufLen);
        pOutResvId[dwBufLen - 1] = '\0';
    }

    pProd->dwReservedQty += wQty;
    pProd->dwChecksum     = _calc_product_checksum(pProd);
    g_dwResvCount++;
    return CP_OK;
}

/*
 * CP_CancelReservation
 * lpszResvId  : reservation ID to cancel
 *
 * Returns CP_OK, CP_ERR_NOTFOUND, CP_ERR_BADPARAM, CP_ERR_EXPIRED
 */
__declspec(dllexport) int __cdecl CP_CancelReservation(
        const char *lpszResvId)
{
    DWORD i;
    g_dwCallCount++;

    if (!lpszResvId) return CP_ERR_BADPARAM;
    if (!g_bInitialised) CP_Initialize();

    for (i = 0; i < g_dwResvCount; i++) {
        if (_stricmp(g_arReservations[i].szResvId, lpszResvId) == 0) {
            if (g_arReservations[i].bStatus != RESV_ACTIVE)
                return CP_ERR_EXPIRED;

            g_arReservations[i].bStatus = RESV_CANCELLED;

            /* Release reserved stock */
            {
                int pidx = _find_product(g_arReservations[i].szSku);
                if (pidx >= 0 && g_arProducts[pidx].dwReservedQty >= g_arReservations[i].wQty) {
                    g_arProducts[pidx].dwReservedQty -= g_arReservations[i].wQty;
                    g_arProducts[pidx].dwChecksum = _calc_product_checksum(&g_arProducts[pidx]);
                }
            }
            return CP_OK;
        }
    }
    return CP_ERR_NOTFOUND;
}

/*
 * CP_GetTransactionHistory
 * lpszCustId  : customer ID
 * pOutBuf     : caller buffer for pipe-delimited transaction list
 * dwBufLen    : buffer size
 * pdwCount    : receives number of transactions found
 *
 * Returns CP_OK, CP_ERR_BADPARAM, CP_ERR_NOTFOUND (if zero transactions)
 */
__declspec(dllexport) int __cdecl CP_GetTransactionHistory(
        const char *lpszCustId,
        char       *pOutBuf,
        DWORD       dwBufLen,
        DWORD      *pdwCount)
{
    DWORD i, count = 0;
    int written = 0;
    g_dwCallCount++;

    if (!lpszCustId || !pOutBuf || dwBufLen < 64 || !pdwCount)
        return CP_ERR_BADPARAM;
    if (!g_bInitialised) CP_Initialize();

    pOutBuf[0] = '\0';
    for (i = 0; i < g_dwTxnCount; i++) {
        if (_stricmp(g_arTransactions[i].szCustId, lpszCustId) == 0) {
            int remaining = (int)(dwBufLen - written - 1);
            if (remaining < 40) break;
            if (count > 0) {
                pOutBuf[written++] = ';';
            }
            written += _snprintf(pOutBuf + written, remaining,
                "txn=%s|sku=%s|type=%u|amount=$%.2f",
                g_arTransactions[i].szTxnId,
                g_arTransactions[i].szSku,
                g_arTransactions[i].bType,
                g_arTransactions[i].dwAmountCents / 100.0);
            count++;
        }
    }
    pOutBuf[dwBufLen - 1] = '\0';
    *pdwCount = count;
    return (count > 0) ? CP_OK : CP_ERR_NOTFOUND;
}

/*
 * CP_ApplyBulkDiscount
 * lpszSku         : SKU to discount
 * dwNewPriceCents : new unit price in cents
 * lpszAuthToken   : 8-char auth token (XOR validation, same as contoso_cs)
 *
 * Returns CP_OK, CP_ERR_NOTFOUND, CP_ERR_BADPARAM, CP_ERR_LIMIT
 */
__declspec(dllexport) int __stdcall CP_ApplyBulkDiscount(
        const char *lpszSku,
        DWORD       dwNewPriceCents,
        const char *lpszAuthToken)
{
    int idx;
    BYTE bCheck;
    int i;
    g_dwCallCount++;

    if (!lpszSku || !lpszAuthToken || lstrlenA(lpszAuthToken) < 4)
        return CP_ERR_BADPARAM;
    if (dwNewPriceCents == 0) return CP_ERR_BADPARAM;
    if (!g_bInitialised) CP_Initialize();

    /* XOR token validation — same algorithm as CS_UnlockAccount */
    bCheck = 0;
    for (i = 0; lpszAuthToken[i]; i++)
        bCheck ^= (BYTE)lpszAuthToken[i];
    if (bCheck != 0xA5) return CP_ERR_BADPARAM;

    idx = _find_product(lpszSku);
    if (idx < 0) return CP_ERR_NOTFOUND;

    /* Price floor: cannot discount below 10% of original */
    if (dwNewPriceCents < g_arProducts[idx].dwUnitPriceCents / 10)
        return CP_ERR_LIMIT;

    g_arProducts[idx].dwUnitPriceCents = dwNewPriceCents;
    g_arProducts[idx].dwChecksum = _calc_product_checksum(&g_arProducts[idx]);
    return CP_OK;
}

/*
 * CP_GetDiagnostics
 * pOutBuf  : caller buffer for diagnostic string
 * dwBufLen : buffer size
 *
 * Returns CP_OK always.
 */
__declspec(dllexport) int __cdecl CP_GetDiagnostics(
        char  *pOutBuf,
        DWORD  dwBufLen)
{
    DWORD dwVer = CP_GetVersion();
    if (!pOutBuf || dwBufLen < 32) return CP_ERR_BADPARAM;
    _snprintf(pOutBuf, dwBufLen - 1,
        "version=%u.%u.%u|products=%lu|transactions=%lu|reservations=%lu|calls=%lu|initialized=%s",
        (dwVer >> 16) & 0xFF,
        (dwVer >>  8) & 0xFF,
        (dwVer      ) & 0xFF,
        g_dwProductCount,
        g_dwTxnCount,
        g_dwResvCount,
        g_dwCallCount,
        g_bInitialised ? "yes" : "no");
    pOutBuf[dwBufLen - 1] = '\0';
    return CP_OK;
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
