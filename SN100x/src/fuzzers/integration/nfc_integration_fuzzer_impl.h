#ifndef NFC_INTEGRATION_FUZZER_IMPL_H_
#define NFC_INTEGRATION_FUZZER_IMPL_H_

#include <fuzzer/FuzzedDataProvider.h>

#include "fuzzers/integration/nfc_integration_fuzzer.pb.h"
#include "hal_fakes.h"
#include "nfa_api.h"

class NfcIntegrationFuzzer {
 public:
  NfcIntegrationFuzzer(const Session* session);
  ~NfcIntegrationFuzzer() { TearDown(); }

  bool Setup();
  void RunCommands();

 private:
  void DoOneCommand(std::vector<std::vector<uint8_t>>& bytes_container,
                    const Command& command);

  void TearDown();

  const Session* session_;
  FuzzedDataProvider fuzzed_data_;
  FakeHal fake_hal_;
  std::vector<std::vector<uint8_t>> bytes_container_;
};

#endif  // NFC_INTEGRATION_FUZZER_IMPL_H_
