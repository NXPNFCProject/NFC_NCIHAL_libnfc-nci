#ifndef HAL_FAKES_H_
#define HAL_FAKES_H_

#include <android-base/logging.h>

#include "nfc_hal_api.h"

class FakeHal;

extern FakeHal* g_fake_hal;

// Captures HAL callbacks from entry_funcs and allows a client
// to simulate HAL and data packet events.
class FakeHal {
 public:
  FakeHal();
  ~FakeHal();

  void FuzzedOpen(tHAL_NFC_CBACK* p_hal_cback,
                  tHAL_NFC_DATA_CBACK* p_data_cback);
  void FuzzedClose();

  void SimulateHALEvent(uint8_t event, tHAL_NFC_STATUS status);
  void SimulatePacketArrival(uint8_t mt, uint8_t pbf, uint8_t gid,
                             uint8_t opcode, uint8_t* data, size_t size);

 private:
  tHAL_NFC_CBACK* hal_callback_;
  tHAL_NFC_DATA_CBACK* data_callback_;
};

extern tHAL_NFC_ENTRY fuzzed_hal_entry;

#endif  // HAL_FAKES_H_
