# contoso_legacy — Build Instructions

## What this is
A deliberately undocumented Win32 DLL simulating a 2004-era Contoso customer service backend.
No header file is distributed. No documentation. Source is NOT included in the demo — only the compiled DLL.

## Exported functions (what Ghidra must recover)
| Function | Calling conv | Description |
|---|---|---|
| `CS_Initialize` | cdecl | Must call first; seeds in-memory DB |
| `CS_GetVersion` | cdecl | Returns encoded version DWORD |
| `CS_LookupCustomer` | cdecl | Returns pipe-delimited customer record |
| `CS_GetAccountBalance` | **stdcall** | Returns balance in cents via out-param |
| `CS_GetLoyaltyPoints` | cdecl | Returns point balance via out-param |
| `CS_RedeemLoyaltyPoints` | **stdcall** | Deducts points, credits account |
| `CS_ProcessPayment` | cdecl | Debits balance, awards loyalty points |
| `CS_ProcessRefund` | cdecl | Credits balance, returns refund ID |
| `CS_UnlockAccount` | **stdcall** | Clears ACCT_LOCKED flag if token valid |
| `CS_CalculateInterest` | cdecl | Amortization formula, monthly payment |
| `CS_GetOrderStatus` | cdecl | Returns pipe-delimited order record |
| `CS_GetDiagnostics` | cdecl | Internal call counter + build info |

Mixed cdecl / stdcall is intentional — realistic legacy mess.

## Internal symbols Ghidra also recovers (not exported)
- `_xor_buf` — XOR encoder used internally
- `_calc_checksum` — record integrity check
- `_validate_checksum` — tamper detection
- `_find_customer` — linear scan of global customer array
- `_compute_tier` — tier logic from lifetime spend
- `_seed_database` — populates the 4 demo customers

## Seeded demo data
| ID | Name | Balance | Points | Tier | Status |
|---|---|---|---|---|---|
| CUST-001 | Alice Contoso | $250.00 | 1450 | Gold | ACTIVE/VIP |
| CUST-002 | Bob Builder | $50.00 | 320 | Standard | ACTIVE |
| CUST-003 | Carol Danvers | $0.00 | 75 | Standard | **LOCKED** |
| CUST-004 | David Nakamura | $1200.00 | 18750 | Platinum | ACTIVE/VIP |

## Compile — MSVC (from Developer Command Prompt)
```cmd
cl /O2 /GS- /LD /W3 contoso_cs.c /Fe:contoso_cs.dll /link /NODEFAULTLIB:MSVCRT user32.lib kernel32.lib
```

## Strip debug info (remove PDB reference)
```cmd
editbin /NOLOGO contoso_cs.dll
```
Or compile with `/Zi` suppressed (default with `/O2`). The `/GS-` disables security cookies, more realistic for old code.

## Verify it's stripped (no symbols)
```cmd
dumpbin /symbols contoso_cs.dll
```
Should show 0 COFF symbols. Exports are still visible (that's normal — export table is required for LoadLibrary). Ghidra goes beyond exports and recovers the internal `_xor_buf`, `_find_customer` etc. from the disassembly.

## Demo sequence
1. Show the DLL with no source, no header, no documentation
2. Upload to MCP Factory
3. Show Ghidra recovering all 12 exports + 5 internal helpers
4. Show AI calling `CS_LookupCustomer("CUST-001", ...)` → returns Alice's record
5. Show `CS_ProcessPayment("CUST-001", 1000, "ORD-TEST")` → deducts $10, awards points
6. Show `CS_GetDiagnostics` → call counter incremented

## "Proof it's not hardcoded" binary
Upload `kernel32.dll` or `zstd.dll` immediately after.
Same Ghidra pipeline runs. Completely different functions extracted.
This proves the system is generic, not trained on contoso_cs.dll specifically.
