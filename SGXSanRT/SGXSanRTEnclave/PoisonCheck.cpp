#include "PoisonCheck.hpp"
#include <sgx_trts.h>
#include <cstdlib>

uptr sgxsan_region_is_poisoned(uptr beg, uptr size, uint8_t mask, bool ret_addr)
{
    if (!beg)
        return 1;
    if (!size)
        return 0;
    uptr end = beg + size;
    if (!AddrIsInMem(beg))
        return beg;
    // end is offset by one, so there is a offset-by-one bug in original ASan
    if (!AddrIsInMem(end - 1))
        return end - 1;

    CHECK_LT(beg, end);
    uptr aligned_b = RoundUpTo(beg, SHADOW_GRANULARITY);
    uptr aligned_e = RoundDownTo(end - 1, SHADOW_GRANULARITY);
    uptr shadow_beg = MemToShadow(aligned_b);
    uptr shadow_end = MemToShadow(aligned_e);
    // First check the first and the last application bytes,
    // then check the SHADOW_GRANULARITY-aligned region by calling
    // mem_is_zero on the corresponding shadow.
    if (!AddressIsPoisoned(beg, mask) && !AddressIsPoisoned(end - 1, mask) &&
        (shadow_end <= shadow_beg || mem_is_zero((uint8_t *)shadow_beg,
                                                 shadow_end - shadow_beg, mask)))
    {
        return 0;
    }
    if (ret_addr)
    {
        // The fast check failed, so we have a poisoned byte somewhere.
        // Find it slowly.
        for (; beg < end; beg++)
            if (AddressIsPoisoned(beg, mask))
                return beg;
        // UNREACHABLE("mem_is_zero returned false, but poisoned byte was not found");
        abort();
        return 0;
    }
    else
    {
        return 1;
    }
}

bool sgxsan_region_is_in_elrange_and_poisoned(uint64_t beg, uint64_t size, uint8_t mask)
{
    if (sgx_is_within_enclave((void *)beg, size))
    {
        return sgxsan_region_is_poisoned(beg, size, mask);
    }
    return false;
}
