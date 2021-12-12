#pragma once

#include <stdint.h>
#include <pthread.h>
#include <map>
#include "SGXSanDefs.h"

#if defined(__cplusplus)
extern "C"
{
#endif
    // a list of c wrapper of WhitelistOfAddrOutEnclave, class member function is inlined defaultly
    void WhitelistOfAddrOutEnclave_init();
    void WhitelistOfAddrOutEnclave_destroy();
    void WhitelistOfAddrOutEnclave_add(uint64_t start, uint64_t size);
    void WhitelistOfAddrOutEnclave_query(uint64_t start, uint64_t size);
    void WhitelistOfAddrOutEnclave_global_propagate(uint64_t addr);

    void WhitelistOfAddrOutEnclave_active();
    void WhitelistOfAddrOutEnclave_deactive();
#if defined(__cplusplus)
}
#endif

// Init/Destroy at Enclave Tbridge Side, I didn't want to modify sgxsdk
// Active/Deactive at Enclave Tbridge Side to avoid nested calls, these operations are as close to Customized Enclave Side as possible
// Add at Enclave Tbridge Side to collect whitlist info
// Query at Customized Enclave Side for whitelist checking
// Global Proagate at Customized Enclave Side, which only consider global variables at Customized Enclave Side. This operation will use Add/Query
class WhitelistOfAddrOutEnclave
{
public:
    // add at bridge
    static void init();
    static void destroy();
    static void iter(bool is_global = false);
    static std::pair<std::map<uint64_t, uint64_t>::iterator, bool> add(uint64_t start, uint64_t size);
    static std::pair<std::map<uint64_t, uint64_t>::iterator, bool> add_global(uint64_t start, uint64_t size);
    static std::tuple<uint64_t, uint64_t, bool /* is_at_global? */> query(uint64_t start, uint64_t size);
    static std::pair<uint64_t, uint64_t> query_global(uint64_t start, uint64_t size);
    static bool global_propagate(uint64_t addr);
    static void active();
    static void deactive();

private:
    static __thread std::map<uint64_t, uint64_t> *m_whitelist;
    // used in nested ecall-ocall case
    static __thread int m_whitelist_init_cnt;
    static __thread bool m_whitelist_active;
    static std::map<uint64_t, uint64_t> m_global_whitelist;
    static pthread_rwlock_t m_rwlock_global_whitelist;
};