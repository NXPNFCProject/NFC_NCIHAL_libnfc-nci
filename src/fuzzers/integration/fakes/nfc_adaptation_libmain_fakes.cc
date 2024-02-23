#include <fuzzer/FuzzedDataProvider.h>

#include "nfa_nv_ci.h"
#include "nfa_nv_co.h"

extern FuzzedDataProvider* g_fuzzed_data;

void nfa_nv_co_read(uint8_t* pBuffer, uint16_t nbytes, uint8_t block) {
  if (g_fuzzed_data->ConsumeBool()) {
    nfa_nv_ci_read(0, NFA_NV_CO_FAIL, block);
    return;
  }

  std::vector<uint8_t> bytes = g_fuzzed_data->ConsumeBytes<uint8_t>(nbytes);
  memcpy(pBuffer, bytes.data(), bytes.size());
  nfa_nv_ci_read(bytes.size(), NFA_NV_CO_OK, block);
}

void nfa_nv_co_write(const uint8_t* pBuffer, uint16_t nbytes,
                     uint8_t /* block */) {
  // Copy to detect invalid pBuffer/nbytes parameters
  std::vector<uint8_t> bytes(pBuffer, pBuffer + nbytes);

  if (g_fuzzed_data->ConsumeBool()) {
    nfa_nv_ci_write(NFA_NV_CO_FAIL);
    return;
  }

  nfa_nv_ci_write(NFA_NV_CO_OK);
}

extern void* nfa_mem_co_alloc(uint32_t num_bytes) {
  // Avoid large allocations that harm fuzzer performance
  if (num_bytes > 100000) {
    return nullptr;
  }
  return malloc(num_bytes);
}

extern void nfa_mem_co_free(void* pBuffer) { free(pBuffer); }

void delete_stack_non_volatile_store(bool) {}
