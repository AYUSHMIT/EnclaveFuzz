#include "Poison.h"
#include "arch.h"
#include "rts_cmd.h"
#include "sgx_eid.h"
#include "sgx_error.h"
#include "sgx_key.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>

/// Birdge Sticker
typedef struct {
  const void *ecall_addr;
  uint8_t is_priv;
  uint8_t is_switchless;
} ecall_addr_t;

typedef struct {
  size_t nr_ecall;
  ecall_addr_t ecall_table[1];
} ecall_table_t;

typedef sgx_status_t (*ecall_func_t)(void *ms);
extern const ecall_table_t g_ecall_table;

/// Global Data

/*SECS data structure*/
typedef struct _global_data_sim_t {
  secs_t *secs_ptr;
  sgx_cpu_svn_t cpusvn_sim;
  uint64_t seed; /* to initialize the PRNG */
} global_data_sim_t;
extern global_data_sim_t g_global_data_sim;
secs_t g_secs;

static void SGXInitInternal() {
  // Prepare necessary Enclave's state
  g_global_data_sim.secs_ptr = &g_secs;
  PoisonShadow((uptr)&g_secs, sizeof(g_secs), kAsanNotPoisonedMagic);
}

extern "C" sgx_status_t tsticker_ecall(const sgx_enclave_id_t eid,
                                       const int index, const void *ocall_table,
                                       void *ms) {
  sgx_status_t result = SGX_ERROR_UNEXPECTED;
  if (index == ECMD_INIT_ENCLAVE) {
    SGXInitInternal();
    result = SGX_SUCCESS;
  } else {
    assert(index < (int)g_ecall_table.nr_ecall);
    result = ((ecall_func_t)g_ecall_table.ecall_table[index].ecall_addr)(ms);
  }
  return result;
}

extern "C" int hook_enclave();
extern "C" uint8_t *getCovMapAddr();
extern "C" void PoisonEnclaveDSOCodeSegment();
// gAlreadyAsanInited should reside in Enclave image, since we should set it to
// false whenever we load Enclave image and call __asan_init
bool gAlreadyAsanInited = false;
/// @brief Must called before SanitizerCoverage's ctors, since in this function
/// I hook callbacks in these ctors.
extern "C" void __asan_init() {
  if (gAlreadyAsanInited == false) {
    // We already initialized shadow memory in host ctor
    if (hook_enclave() != 0) {
      abort();
    }
    PoisonEnclaveDSOCodeSegment();
    gAlreadyAsanInited = true;
  }
}
