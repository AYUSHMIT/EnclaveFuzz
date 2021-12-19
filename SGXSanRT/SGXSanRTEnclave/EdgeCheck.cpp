#include "EdgeCheck.hpp"
#include "WhitelistCheck.hpp"
#include "SGXSanCommonPoisonCheck.hpp"
#include "SGXSanCommonErrorReport.hpp"
#include "SGXSanPrintf.hpp"

void sgxsan_edge_check(uint64_t ptr, uint64_t len, int cnt)
{
    uint64_t min_size = len * (cnt <= 1 ? 1 : cnt);
    if (min_size < len)
    {
        // int overflow
        min_size = len;
    }
    SGXSAN_ELRANGE_CHECK_BEG(ptr, 0, min_size)
    if (__asan_region_is_poisoned(ptr, min_size, true))
    {
        // PrintErrorAndAbort("[sgxsan_edge_check] 0x%lx point to sensitive area\n", ptr);
        GET_CALLER_PC_BP_SP;
        ReportGenericError(pc, bp, sp, ptr, 0, min_size, true, true);
    }
    SGXSAN_ELRANGE_CHECK_MID
    //totally outside enclave, add to whitelist
    WhitelistOfAddrOutEnclave_add(ptr, (cnt == -1) ? (1 << 12) : (len * cnt));
    SGXSAN_ELRANGE_CHECK_END;
    return;
}
