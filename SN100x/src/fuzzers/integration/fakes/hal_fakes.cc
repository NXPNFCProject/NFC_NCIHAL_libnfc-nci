#include "hal_fakes.h"

#include <android-base/logging.h>
#include <android-base/stringprintf.h>

#include "nci_defs.h"

FakeHal* g_fake_hal;

FakeHal::FakeHal() : hal_callback_(nullptr), data_callback_(nullptr) {
  CHECK(!g_fake_hal);
  g_fake_hal = this;
}

FakeHal::~FakeHal() { g_fake_hal = nullptr; }

void FakeHal::FuzzedOpen(tHAL_NFC_CBACK* p_hal_cback,
                         tHAL_NFC_DATA_CBACK* p_data_cback) {
  hal_callback_ = p_hal_cback;
  data_callback_ = p_data_cback;
}

void FakeHal::FuzzedClose() {
  hal_callback_ = nullptr;
  data_callback_ = nullptr;
}

void FakeHal::SimulateHALEvent(uint8_t event, tHAL_NFC_STATUS status) {
  if (!hal_callback_) {
    return;
  }

  hal_callback_(event, status);
}

void FakeHal::SimulatePacketArrival(uint8_t mt, uint8_t pbf, uint8_t gid,
                                    uint8_t opcode, uint8_t* data,
                                    size_t size) {
  if (!data_callback_) {
    return;
  }

  if (size > 255) {
    return;
  }

  static uint8_t buffer[255 + 3];

  buffer[0] = (mt << NCI_MT_SHIFT) | (pbf << NCI_PBF_SHIFT) | gid;
  buffer[1] = (mt == NCI_MT_DATA) ? 0 : opcode;
  buffer[2] = static_cast<uint8_t>(size);
  memcpy(&buffer[3], data, size);

  data_callback_(size + 3, buffer);
}

void FuzzedOpen(tHAL_NFC_CBACK* p_hal_cback,
                tHAL_NFC_DATA_CBACK* p_data_cback) {
  g_fake_hal->FuzzedOpen(p_hal_cback, p_data_cback);
}

void FuzzedClose() { g_fake_hal->FuzzedClose(); }

void FuzzedCoreInitialized(uint16_t, uint8_t*) {}

void FuzzedWrite(uint16_t size, uint8_t*) {
  // Note: compromised firmware can observe writes to the HAL
  LOG(DEBUG) << android::base::StringPrintf("Got a write of %d bytes", size);
}

bool FuzzedPrediscover() { return false; }

void FuzzedControlGranted() {}

tHAL_NFC_ENTRY fuzzed_hal_entry = {
    .open = FuzzedOpen,
    .close = FuzzedClose,
    .core_initialized = FuzzedCoreInitialized,
    .write = FuzzedWrite,
    .prediscover = FuzzedPrediscover,
    .control_granted = FuzzedControlGranted,
};
