#include <android-base/logging.h>
#include <android-base/stringprintf.h>

#include "debug_nfcsnoop.h"
#include "fuzzers/integration/nfc_integration_fuzzer.pb.h"
#include "gki_common.h"
#include "gki_int.h"
#include "nfa_ee_api.h"
#include "nfc_integration_fuzzer_impl.h"
#include "src/libfuzzer/libfuzzer_macro.h"

extern bool nfa_poll_bail_out_mode;

DEFINE_BINARY_PROTO_FUZZER(const Session& session) {
  static bool init = false;
  if (!init) {
    nfa_poll_bail_out_mode = true;
    // 0x90 is the protocol value on Pixel 4a. The default value (0xFF)
    // will prevent Mifare from being discoverable.
    p_nfa_proprietary_cfg->pro_protocol_mfc = 0x90;
    debug_nfcsnoop_init();
    gki_buffer_init();
    gki_cb.com.OSRdyTbl[BTU_TASK] = TASK_READY;
    gki_cb.com.OSRdyTbl[MMI_TASK] = TASK_READY;
    gki_cb.com.OSRdyTbl[NFC_TASK] = TASK_READY;
    init = true;
  }

  // Print the testcase in debug mode
  if (android::base::GetMinimumLogSeverity() <= android::base::DEBUG) {
    std::string str;
    google::protobuf::TextFormat::PrintToString(session, &str);
    LOG(INFO) << str;
  }

  NfcIntegrationFuzzer nfc_integration_fuzzer(&session);
  if (!nfc_integration_fuzzer.Setup()) {
    return;
  }

  nfc_integration_fuzzer.RunCommands();
}
