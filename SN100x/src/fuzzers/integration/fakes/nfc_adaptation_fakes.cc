#include "NfcAdaptation.h"

bool nfc_debug_enabled = false;
uint8_t appl_dta_mode_flag = 0;
bool nfc_nci_reset_keep_cfg_enabled = false;
uint8_t nfc_nci_reset_type = 0x00;

NfcAdaptation* NfcAdaptation::mpInstance = nullptr;

NfcAdaptation& NfcAdaptation::GetInstance() {
  if (!mpInstance) {
    mpInstance = new NfcAdaptation;
    mpInstance->InitializeHalDeviceContext();
  }
  return *mpInstance;
}

void NfcAdaptation::GetVendorConfigs(std::map<std::string, ConfigValue>&) {}

NfcAdaptation::NfcAdaptation() {}
NfcAdaptation::~NfcAdaptation() { mpInstance = nullptr; }

void NfcAdaptation::InitializeHalDeviceContext() {}

class NfcHalDeathRecipient : public virtual android::RefBase {};

ThreadCondVar::ThreadCondVar() {}
ThreadCondVar::~ThreadCondVar() {}
ThreadMutex::~ThreadMutex() {}
ThreadMutex::ThreadMutex() {}
