/******************************************************************************
 *
 *  Copyright (C) 1999-2012 Broadcom Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/
#include <android-base/stringprintf.h>
#include <android/hardware/nfc/1.1/INfc.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <cutils/properties.h>
#include <hwbinder/ProcessState.h>

#include "NfcAdaptation.h"
#include "android_logmsg.h"
#include "debug_nfcsnoop.h"
#if (NXP_EXTNS == TRUE)
#include <vendor/nxp/nxpnfc/1.0/INxpNfc.h>
#include "hal_nxpese.h"
#endif
#include "nfa_api.h"
#include "nfa_rw_api.h"
#include "nfc_config.h"
#include "nfc_int.h"
#include <hidl/LegacySupport.h>

using android::OK;
using android::sp;
using android::status_t;

using android::base::StringPrintf;
using android::hardware::ProcessState;
using android::hardware::Return;
using android::hardware::Void;
using android::hardware::nfc::V1_0::INfc;
using android::hardware::nfc::V1_1::PresenceCheckAlgorithm;
using INfcV1_1 = android::hardware::nfc::V1_1::INfc;
using NfcVendorConfig = android::hardware::nfc::V1_1::NfcConfig;
using android::hardware::nfc::V1_1::INfcClientCallback;
using android::hardware::hidl_vec;
using android::hardware::hidl_death_recipient;
using android::hardware::configureRpcThreadpool;
#if (NXP_EXTNS == TRUE)
using vendor::nxp::nxpnfc::V1_0::INxpNfc;

ThreadMutex NfcAdaptation::sIoctlLock;
sp<INxpNfc> NfcAdaptation::mHalNxpNfc;
#endif
extern bool nfc_debug_enabled;

extern void GKI_shutdown();
extern void verify_stack_non_volatile_store();
extern void delete_stack_non_volatile_store(bool forceDelete);

NfcAdaptation* NfcAdaptation::mpInstance = NULL;
ThreadMutex NfcAdaptation::sLock;
tHAL_NFC_CBACK* NfcAdaptation::mHalCallback = NULL;
tHAL_NFC_DATA_CBACK* NfcAdaptation::mHalDataCallback = NULL;
ThreadCondVar NfcAdaptation::mHalOpenCompletedEvent;
ThreadCondVar NfcAdaptation::mHalCloseCompletedEvent;
sp<INfc> NfcAdaptation::mHal;
sp<INfcV1_1> NfcAdaptation::mHal_1_1;
INfcClientCallback* NfcAdaptation::mCallback;
sp<NfcDeathRecipient> NfcAdaptation::mDeathRecipient = NULL;

bool nfc_debug_enabled = false;
std::string nfc_storage_path;
uint8_t appl_dta_mode_flag = 0x00;

extern tNFA_DM_CFG nfa_dm_cfg;
extern tNFA_PROPRIETARY_CFG nfa_proprietary_cfg;
extern tNFA_HCI_CFG nfa_hci_cfg;
extern uint8_t nfa_ee_max_ee_cfg;
extern bool nfa_poll_bail_out_mode;

// Whitelist for hosts allowed to create a pipe
// See ADM_CREATE_PIPE command in the ETSI test specification
// ETSI TS 102 622, section 6.1.3.1
static std::vector<uint8_t> host_whitelist;

namespace {
void initializeGlobalDebugEnabledFlag() {
  nfc_debug_enabled =
      (NfcConfig::getUnsigned(NAME_NFC_DEBUG_ENABLED, 0) != 0) ? true : false;

  char valueStr[PROPERTY_VALUE_MAX] = {0};
  int len = property_get("nfc.debug_enabled", valueStr, "");
  if (len > 0) {
    // let Android property override .conf variable
    unsigned debug_enabled = 0;
    sscanf(valueStr, "%u", &debug_enabled);
    nfc_debug_enabled = (debug_enabled == 0) ? false : true;
  }

  DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf("%s: level=%u", __func__, nfc_debug_enabled);
}
}  // namespace

class NfcClientCallback : public INfcClientCallback {
 public:
  NfcClientCallback(tHAL_NFC_CBACK* eventCallback,
                    tHAL_NFC_DATA_CBACK dataCallback) {
    mEventCallback = eventCallback;
    mDataCallback = dataCallback;
  };
  virtual ~NfcClientCallback() = default;
  Return<void> sendEvent_1_1(
      ::android::hardware::nfc::V1_1::NfcEvent event,
      ::android::hardware::nfc::V1_0::NfcStatus event_status) override {
    mEventCallback((uint8_t)event, (tHAL_NFC_STATUS)event_status);
    return Void();
  };
  Return<void> sendEvent(
      ::android::hardware::nfc::V1_0::NfcEvent event,
      ::android::hardware::nfc::V1_0::NfcStatus event_status) override {
    mEventCallback((uint8_t)event, (tHAL_NFC_STATUS)event_status);
    return Void();
  };
  Return<void> sendData(
      const ::android::hardware::nfc::V1_0::NfcData& data) override {
    ::android::hardware::nfc::V1_0::NfcData copy = data;
    mDataCallback(copy.size(), &copy[0]);
    return Void();
  };

 private:
  tHAL_NFC_CBACK* mEventCallback;
  tHAL_NFC_DATA_CBACK* mDataCallback;
};

class NfcDeathRecipient : public hidl_death_recipient {
 public:
  NfcDeathRecipient() {}

  virtual void serviceDied(
      uint64_t /* cookie */,
      const android::wp<::android::hidl::base::V1_0::IBase>& /* who */) {
    ALOGE("NfcDeathRecipient::serviceDied - Nfc Hal service died");
    abort();
  }
};

/*******************************************************************************
**
** Function:    NfcAdaptation::NfcAdaptation()
**
** Description: class constructor
**
** Returns:     none
**
*******************************************************************************/
NfcAdaptation::NfcAdaptation() {
  memset(&mHalEntryFuncs, 0, sizeof(mHalEntryFuncs));
}

/*******************************************************************************
**
** Function:    NfcAdaptation::~NfcAdaptation()
**
** Description: class destructor
**
** Returns:     none
**
*******************************************************************************/
NfcAdaptation::~NfcAdaptation() { mpInstance = NULL; }

/*******************************************************************************
**
** Function:    NfcAdaptation::GetInstance()
**
** Description: access class singleton
**
** Returns:     pointer to the singleton object
**
*******************************************************************************/
NfcAdaptation& NfcAdaptation::GetInstance() {
  AutoThreadMutex a(sLock);

  if (!mpInstance) {
    mpInstance = new NfcAdaptation;
    mpInstance->InitializeHalDeviceContext();
  }
  return *mpInstance;
}

void NfcAdaptation::GetVendorConfigs(
    std::map<std::string, ConfigValue>& configMap) {
  if (mHal_1_1) {
    mHal_1_1->getConfig([&configMap](NfcVendorConfig config) {
      std::vector<uint8_t> nfaPropCfg = {
          config.nfaProprietaryCfg.protocol18092Active,
          config.nfaProprietaryCfg.protocolBPrime,
          config.nfaProprietaryCfg.protocolDual,
          config.nfaProprietaryCfg.protocol15693,
          config.nfaProprietaryCfg.protocolKovio,
          config.nfaProprietaryCfg.protocolMifare,
          config.nfaProprietaryCfg.discoveryPollKovio,
          config.nfaProprietaryCfg.discoveryPollBPrime,
          config.nfaProprietaryCfg.discoveryListenBPrime};
      configMap.emplace(NAME_NFA_PROPRIETARY_CFG, ConfigValue(nfaPropCfg));
      configMap.emplace(NAME_NFA_POLL_BAIL_OUT_MODE,
                        ConfigValue(config.nfaPollBailOutMode ? 1 : 0));
      configMap.emplace(NAME_DEFAULT_OFFHOST_ROUTE,
                        ConfigValue(config.defaultOffHostRoute));
      configMap.emplace(NAME_DEFAULT_ROUTE, ConfigValue(config.defaultRoute));
      configMap.emplace(NAME_DEFAULT_NFCF_ROUTE,
                        ConfigValue(config.defaultOffHostRouteFelica));
      configMap.emplace(NAME_DEFAULT_SYS_CODE_ROUTE,
                        ConfigValue(config.defaultSystemCodeRoute));
      configMap.emplace(NAME_DEFAULT_SYS_CODE_PWR_STATE,
                        ConfigValue(config.defaultSystemCodePowerState));
      configMap.emplace(NAME_OFF_HOST_SIM_PIPE_ID,
                        ConfigValue(config.offHostSIMPipeId));
      configMap.emplace(NAME_OFF_HOST_ESE_PIPE_ID,
                        ConfigValue(config.offHostESEPipeId));
      configMap.emplace(NAME_ISO_DEP_MAX_TRANSCEIVE,
                        ConfigValue(config.maxIsoDepTransceiveLength));
      if (config.hostWhitelist.size() != 0) {
        configMap.emplace(NAME_DEVICE_HOST_WHITE_LIST,
                          ConfigValue(config.hostWhitelist));
      }
      /* For Backwards compatibility */
      if (config.presenceCheckAlgorithm ==
          PresenceCheckAlgorithm::ISO_DEP_NAK) {
        configMap.emplace(NAME_PRESENCE_CHECK_ALGORITHM,
                          ConfigValue((uint32_t)NFA_RW_PRES_CHK_ISO_DEP_NAK));
      } else {
        configMap.emplace(NAME_PRESENCE_CHECK_ALGORITHM,
                          ConfigValue((uint32_t)config.presenceCheckAlgorithm));
      }
    });
  }
}
/*******************************************************************************
**
** Function:    NfcAdaptation::Initialize()
**
** Description: class initializer
**
** Returns:     none
**
*******************************************************************************/
void NfcAdaptation::Initialize() {
  const char* func = "NfcAdaptation::Initialize";
  const char* argv[] = {"libnfc_nci"};
  // Init log tag
  base::CommandLine::Init(1, argv);

  // Android already logs thread_id, proc_id, timestamp, so disable those.
  logging::SetLogItems(false, false, false, false);

  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: enter", func);

  nfc_storage_path = NfcConfig::getString(NAME_NFA_STORAGE, "/data/nfc");

  if (NfcConfig::hasKey(NAME_NFA_DM_CFG)) {
    std::vector<uint8_t> dm_config = NfcConfig::getBytes(NAME_NFA_DM_CFG);
    if (dm_config.size() > 0) nfa_dm_cfg.auto_detect_ndef = dm_config[0];
    if (dm_config.size() > 1) nfa_dm_cfg.auto_read_ndef = dm_config[1];
    if (dm_config.size() > 2) nfa_dm_cfg.auto_presence_check = dm_config[2];
    if (dm_config.size() > 3) nfa_dm_cfg.presence_check_option = dm_config[3];
    // NOTE: The timeout value is not configurable here because the endianess
    // of a byte array is ambiguous and needlessly difficult to configure.
    // If this value needs to be configgurable, a numeric config option should
    // be used.
  }

  if (NfcConfig::hasKey(NAME_NFA_MAX_EE_SUPPORTED)) {
    nfa_ee_max_ee_cfg = NfcConfig::getUnsigned(NAME_NFA_MAX_EE_SUPPORTED);
    DLOG_IF(INFO, nfc_debug_enabled)
        << StringPrintf("%s: Overriding NFA_EE_MAX_EE_SUPPORTED to use %d",
                        func, nfa_ee_max_ee_cfg);
  }

  if (NfcConfig::hasKey(NAME_NFA_POLL_BAIL_OUT_MODE)) {
    nfa_poll_bail_out_mode =
        NfcConfig::getUnsigned(NAME_NFA_POLL_BAIL_OUT_MODE);
    DLOG_IF(INFO, nfc_debug_enabled)
        << StringPrintf("%s: Overriding NFA_POLL_BAIL_OUT_MODE to use %d", func,
                        nfa_poll_bail_out_mode);
  }

  if (NfcConfig::hasKey(NAME_NFA_PROPRIETARY_CFG)) {
    std::vector<uint8_t> p_config =
        NfcConfig::getBytes(NAME_NFA_PROPRIETARY_CFG);
    if (p_config.size() > 0)
      nfa_proprietary_cfg.pro_protocol_18092_active = p_config[0];
    if (p_config.size() > 1)
      nfa_proprietary_cfg.pro_protocol_b_prime = p_config[1];
    if (p_config.size() > 2)
      nfa_proprietary_cfg.pro_protocol_dual = p_config[2];
    if (p_config.size() > 3)
      nfa_proprietary_cfg.pro_protocol_15693 = p_config[3];
    if (p_config.size() > 4)
      nfa_proprietary_cfg.pro_protocol_kovio = p_config[4];
    if (p_config.size() > 5) nfa_proprietary_cfg.pro_protocol_mfc = p_config[5];
    if (p_config.size() > 6)
      nfa_proprietary_cfg.pro_discovery_kovio_poll = p_config[6];
    if (p_config.size() > 7)
      nfa_proprietary_cfg.pro_discovery_b_prime_poll = p_config[7];
    if (p_config.size() > 8)
      nfa_proprietary_cfg.pro_discovery_b_prime_listen = p_config[8];
  }

  // Configure whitelist of HCI host ID's
  // See specification: ETSI TS 102 622, section 6.1.3.1
  if (NfcConfig::hasKey(NAME_DEVICE_HOST_WHITE_LIST)) {
    host_whitelist = NfcConfig::getBytes(NAME_DEVICE_HOST_WHITE_LIST);
    nfa_hci_cfg.num_whitelist_host = host_whitelist.size();
    nfa_hci_cfg.p_whitelist = &host_whitelist[0];
  }

  initializeGlobalDebugEnabledFlag();
#if(NXP_EXTNS == TRUE)
  if (NfcConfig::hasKey(NAME_NXP_WM_MAX_WTX_COUNT)) {
    nfa_hci_cfg.max_wtx_count = NfcConfig::getUnsigned(NAME_NXP_WM_MAX_WTX_COUNT);
    DLOG_IF(INFO, nfc_debug_enabled)
        << StringPrintf("%s: MAX_WTX_COUNT to wait for HCI response %d",
                        func, nfa_hci_cfg.max_wtx_count);
  }
#endif
  verify_stack_non_volatile_store();
  if (NfcConfig::hasKey(NAME_PRESERVE_STORAGE) &&
      NfcConfig::getUnsigned(NAME_PRESERVE_STORAGE) == 1) {
    DLOG_IF(INFO, nfc_debug_enabled)
        << StringPrintf("%s: preserve stack NV store", __func__);
  } else {
    delete_stack_non_volatile_store(FALSE);
  }

  GKI_init();
  GKI_enable();
  GKI_create_task((TASKPTR)NFCA_TASK, BTU_TASK, (int8_t*)"NFCA_TASK", 0, 0,
                  (pthread_cond_t*)NULL, NULL);
  {
    AutoThreadMutex guard(mCondVar);
    GKI_create_task((TASKPTR)Thread, MMI_TASK, (int8_t*)"NFCA_THREAD", 0, 0,
                    (pthread_cond_t*)NULL, NULL);
    mCondVar.wait();
  }

  debug_nfcsnoop_init();
  configureRpcThreadpool(2, false);
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: exit", func);
}

/*******************************************************************************
**
** Function:    NfcAdaptation::Finalize()
**
** Description: class finalizer
**
** Returns:     none
**
*******************************************************************************/
void NfcAdaptation::Finalize() {
  const char* func = "NfcAdaptation::Finalize";
  AutoThreadMutex a(sLock);

  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: enter", func);
  GKI_shutdown();

  NfcConfig::clear();

  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: exit", func);
  delete this;
}

/*******************************************************************************
**
** Function:    NfcAdaptation::FactoryReset
**
** Description: Native support for FactoryReset function.
** It will delete the nfaStorage file and invoke the factory reset function
** in HAL level to set the session ID to default value.
**
** Returns:     None.
**
*******************************************************************************/
void NfcAdaptation::FactoryReset() {
#if(NXP_EXTNS == TRUE)
  const char* func = "NfcAdaptation::FactoryReset";
  int status;
  const char config_eseinfo_path[] = "/data/nfc/nfaStorage.bin1";
#endif
  if (mHal_1_1 != nullptr) {
    mHal_1_1->factoryReset();
#if(NXP_EXTNS == TRUE)
    status=remove(config_eseinfo_path);
    if(status==0){
      DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: Storage file deleted... ", func);
    }
    else{
      DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: Storage file delete failed... ", func);
    }
#endif
  }
}

void NfcAdaptation::DeviceShutdown() {
  if (mHal_1_1 != nullptr) {
    mHal_1_1->closeForPowerOffCase();
  }
}

/*******************************************************************************
**
** Function:    NfcAdaptation::Dump
**
** Description: Native support for dumpsys function.
**
** Returns:     None.
**
*******************************************************************************/
void NfcAdaptation::Dump(int fd) { debug_nfcsnoop_dump(fd); }

/*******************************************************************************
**
** Function:    NfcAdaptation::signal()
**
** Description: signal the CondVar to release the thread that is waiting
**
** Returns:     none
**
*******************************************************************************/
void NfcAdaptation::signal() { mCondVar.signal(); }

/*******************************************************************************
**
** Function:    NfcAdaptation::NFCA_TASK()
**
** Description: NFCA_TASK runs the GKI main task
**
** Returns:     none
**
*******************************************************************************/
uint32_t NfcAdaptation::NFCA_TASK(__attribute__((unused)) uint32_t arg) {
  const char* func = "NfcAdaptation::NFCA_TASK";
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: enter", func);
  GKI_run(0);
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: exit", func);
  return 0;
}

/*******************************************************************************
**
** Function:    NfcAdaptation::Thread()
**
** Description: Creates work threads
**
** Returns:     none
**
*******************************************************************************/
uint32_t NfcAdaptation::Thread(__attribute__((unused)) uint32_t arg) {
  const char* func = "NfcAdaptation::Thread";
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: enter", func);

  {
    ThreadCondVar CondVar;
    AutoThreadMutex guard(CondVar);
    GKI_create_task((TASKPTR)nfc_task, NFC_TASK, (int8_t*)"NFC_TASK", 0, 0,
                    (pthread_cond_t*)CondVar, (pthread_mutex_t*)CondVar);
    CondVar.wait();
  }

  NfcAdaptation::GetInstance().signal();

  GKI_exit_task(GKI_get_taskid());
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: exit", func);
  return 0;
}

/*******************************************************************************
**
** Function:    NfcAdaptation::GetHalEntryFuncs()
**
** Description: Get the set of HAL entry points.
**
** Returns:     Functions pointers for HAL entry points.
**
*******************************************************************************/
tHAL_NFC_ENTRY* NfcAdaptation::GetHalEntryFuncs() { return &mHalEntryFuncs; }

/*******************************************************************************
**
** Function:    NfcAdaptation::InitializeHalDeviceContext
**
** Description: Ask the generic Android HAL to find the Broadcom-specific HAL.
**
** Returns:     None.
**
*******************************************************************************/
void NfcAdaptation::InitializeHalDeviceContext() {
  const char* func = "NfcAdaptation::InitializeHalDeviceContext";

  mHalEntryFuncs.initialize = HalInitialize;
  mHalEntryFuncs.terminate = HalTerminate;
  mHalEntryFuncs.open = HalOpen;
  mHalEntryFuncs.close = HalClose;
  mHalEntryFuncs.core_initialized = HalCoreInitialized;
  mHalEntryFuncs.write = HalWrite;
  mHalEntryFuncs.prediscover = HalPrediscover;
  mHalEntryFuncs.control_granted = HalControlGranted;
  mHalEntryFuncs.power_cycle = HalPowerCycle;
  mHalEntryFuncs.get_max_ee = HalGetMaxNfcee;
  LOG(INFO) << StringPrintf("%s: INfc::getService()", func);
  mHal = mHal_1_1 = INfcV1_1::getService();
  if (mHal_1_1 == nullptr) {
    mHal = INfc::getService();
  }
  LOG_FATAL_IF(mHal == nullptr, "Failed to retrieve the NFC HAL!");
  LOG(INFO) << StringPrintf("%s: INfc::getService() returned %p (%s)", func,
                            mHal.get(),
                            (mHal->isRemote() ? "remote" : "local"));
}

/*******************************************************************************
**
** Function:    NfcAdaptation::HalInitialize
**
** Description: Not implemented because this function is only needed
**              within the HAL.
**
** Returns:     None.
**
*******************************************************************************/
void NfcAdaptation::HalInitialize() {
  const char* func = "NfcAdaptation::HalInitialize";
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s", func);
}

/*******************************************************************************
**
** Function:    NfcAdaptation::HalTerminate
**
** Description: Not implemented because this function is only needed
**              within the HAL.
**
** Returns:     None.
**
*******************************************************************************/
void NfcAdaptation::HalTerminate() {
  const char* func = "NfcAdaptation::HalTerminate";
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s", func);
}

/*******************************************************************************
**
** Function:    NfcAdaptation::HalOpen
**
** Description: Turn on controller, download firmware.
**
** Returns:     None.
**
*******************************************************************************/
void NfcAdaptation::HalOpen(tHAL_NFC_CBACK* p_hal_cback,
                            tHAL_NFC_DATA_CBACK* p_data_cback) {
  const char* func = "NfcAdaptation::HalOpen";
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s", func);
  mCallback = new NfcClientCallback(p_hal_cback, p_data_cback);
  mDeathRecipient = new NfcDeathRecipient();
  if (mHal_1_1 != nullptr) {
    mHal_1_1->open_1_1(mCallback);
    mHal_1_1->linkToDeath(mDeathRecipient, 0);
  } else {
    mHal->open(mCallback);
    mHal->linkToDeath(mDeathRecipient, 0);
  }
}

/*******************************************************************************
**
** Function:    NfcAdaptation::HalClose
**
** Description: Turn off controller.
**
** Returns:     None.
**
*******************************************************************************/
void NfcAdaptation::HalClose() {
  const char* func = "NfcAdaptation::HalClose";
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s", func);
  mHal->close();
  mHal->unlinkToDeath(mDeathRecipient);
}

/*******************************************************************************
**
** Function:    NfcAdaptation::HalDeviceContextCallback
**
** Description: Translate generic Android HAL's callback into Broadcom-specific
**              callback function.
**
** Returns:     None.
**
*******************************************************************************/
void NfcAdaptation::HalDeviceContextCallback(nfc_event_t event,
                                             nfc_status_t event_status) {
  const char* func = "NfcAdaptation::HalDeviceContextCallback";
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: event=%u", func, event);
  if (mHalCallback) mHalCallback(event, (tHAL_NFC_STATUS)event_status);
}

/*******************************************************************************
**
** Function:    NfcAdaptation::HalDeviceContextDataCallback
**
** Description: Translate generic Android HAL's callback into Broadcom-specific
**              callback function.
**
** Returns:     None.
**
*******************************************************************************/
void NfcAdaptation::HalDeviceContextDataCallback(uint16_t data_len,
                                                 uint8_t* p_data) {
  const char* func = "NfcAdaptation::HalDeviceContextDataCallback";
  DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf("%s: len=%u", func, data_len);
  if (mHalDataCallback) mHalDataCallback(data_len, p_data);
}

/*******************************************************************************
**
** Function:    NfcAdaptation::HalWrite
**
** Description: Write NCI message to the controller.
**
** Returns:     None.
**
*******************************************************************************/
void NfcAdaptation::HalWrite(uint16_t data_len, uint8_t* p_data) {
  const char* func = "NfcAdaptation::HalWrite";
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s", func);
  ::android::hardware::nfc::V1_0::NfcData data;
  data.setToExternal(p_data, data_len);
  mHal->write(data);
}
#if (NXP_EXTNS == TRUE)
/*******************************************************************************
**
** Function:    IoctlCallback
**
** Description: Callback from HAL stub for IOCTL api invoked.
**              Output data for IOCTL is sent as argument
**
** Returns:     None.
**
*******************************************************************************/

void IoctlCallback(::android::hardware::nfc::V1_0::NfcData outputData) {
  const char* func = "IoctlCallback";
  nfc_nci_ExtnOutputData_t* pOutData =
      (nfc_nci_ExtnOutputData_t*)&outputData[0];
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s Ioctl Type=%llu", func, (unsigned long long)pOutData->ioctlType);
  NfcAdaptation* pAdaptation = (NfcAdaptation*)pOutData->context;
  /*Output Data from stub->Proxy is copied back to output data
   * This data will be sent back to libnfc*/
  memcpy(&pAdaptation->mCurrentIoctlData->out, &outputData[0],
         sizeof(nfc_nci_ExtnOutputData_t));
}
/*******************************************************************************
**
** Function:    NfcAdaptation::HalIoctl
**
** Description: Calls ioctl to the Nfc driver.
**              If called with a arg value of 0x01 than wired access requested,
**              status of the requst would be updated to p_data.
**              If called with a arg value of 0x00 than wired access will be
**              released, status of the requst would be updated to p_data.
**              If called with a arg value of 0x02 than current p61 state would
*be
**              updated to p_data.
**
** Returns:     -1 or 0.
**
*******************************************************************************/

int NfcAdaptation::HalIoctl(long arg, void* p_data) {
  const char* func = "NfcAdaptation::HalIoctl";
  ::android::hardware::nfc::V1_0::NfcData data;
  AutoThreadMutex a(sIoctlLock);
  nfc_nci_IoctlInOutData_t* pInpOutData = (nfc_nci_IoctlInOutData_t*)p_data;
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s arg=%ld", func, arg);
  pInpOutData->inp.context = &NfcAdaptation::GetInstance();
  NfcAdaptation::GetInstance().mCurrentIoctlData = pInpOutData;
  data.setToExternal((uint8_t*)pInpOutData, sizeof(nfc_nci_IoctlInOutData_t));
    /*Insert Transit config at the end of IOCTL data as transit buffer also
    needs to be part of NfcData(hidl_vec)*/
  if (arg == HAL_NFC_IOCTL_SET_TRANSIT_CONFIG) {
    std::vector<uint8_t> tempStdVec(data);
    tempStdVec.insert(
        tempStdVec.end(), pInpOutData->inp.data.transitConfig.val,
        pInpOutData->inp.data.transitConfig.val +
            (pInpOutData->inp.data.transitConfig.len));
    data = tempStdVec;
  }
  if(mHalNxpNfc != nullptr)
      mHalNxpNfc->ioctl(arg, data, IoctlCallback);
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s Ioctl Completed for Type=%llu", func, (unsigned long long)pInpOutData->out.ioctlType);
  return (pInpOutData->out.result);
}
#endif
/*******************************************************************************
**
** Function:    NfcAdaptation::HalCoreInitialized
**
** Description: Adjust the configurable parameters in the controller.
**
** Returns:     None.
**
*******************************************************************************/
void NfcAdaptation::HalCoreInitialized(uint16_t data_len,
                                       uint8_t* p_core_init_rsp_params) {
  const char* func = "NfcAdaptation::HalCoreInitialized";
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s", func);
  hidl_vec<uint8_t> data;
  data.setToExternal(p_core_init_rsp_params, data_len);

  mHal->coreInitialized(data);
}

/*******************************************************************************
**
** Function:    NfcAdaptation::HalPrediscover
**
** Description:     Perform any vendor-specific pre-discovery actions (if
**                  needed) If any actions were performed TRUE will be returned,
**                  and HAL_PRE_DISCOVER_CPLT_EVT will notify when actions are
**                  completed.
**
** Returns:         TRUE if vendor-specific pre-discovery actions initialized
**                  FALSE if no vendor-specific pre-discovery actions are
**                  needed.
**
*******************************************************************************/
bool NfcAdaptation::HalPrediscover() {
  const char* func = "NfcAdaptation::HalPrediscover";
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s", func);
  bool retval = FALSE;
  mHal->prediscover();
  return retval;
}

/*******************************************************************************
**
** Function:        HAL_NfcControlGranted
**
** Description:     Grant control to HAL control for sending NCI commands.
**                  Call in response to HAL_REQUEST_CONTROL_EVT.
**                  Must only be called when there are no NCI commands pending.
**                  HAL_RELEASE_CONTROL_EVT will notify when HAL no longer
**                  needs control of NCI.
**
** Returns:         void
**
*******************************************************************************/
void NfcAdaptation::HalControlGranted() {
  const char* func = "NfcAdaptation::HalControlGranted";
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s", func);
  mHal->controlGranted();
}

/*******************************************************************************
**
** Function:    NfcAdaptation::HalPowerCycle
**
** Description: Turn off and turn on the controller.
**
** Returns:     None.
**
*******************************************************************************/
void NfcAdaptation::HalPowerCycle() {
  const char* func = "NfcAdaptation::HalPowerCycle";
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s", func);
  mHal->powerCycle();
}

/*******************************************************************************
**
** Function:    NfcAdaptation::HalGetMaxNfcee
**
** Description: Turn off and turn on the controller.
**
** Returns:     None.
**
*******************************************************************************/
uint8_t NfcAdaptation::HalGetMaxNfcee() {
  const char* func = "NfcAdaptation::HalPowerCycle";
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s", func);

  return nfa_ee_max_ee_cfg;
}

/*******************************************************************************
**
** Function:    NfcAdaptation::DownloadFirmware
**
** Description: Download firmware patch files.
**
** Returns:     None.
**
*******************************************************************************/
void NfcAdaptation::DownloadFirmware() {
  const char* func = "NfcAdaptation::DownloadFirmware";
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: enter", func);
  HalInitialize();

  mHalOpenCompletedEvent.lock();
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: try open HAL", func);
  HalOpen(HalDownloadFirmwareCallback, HalDownloadFirmwareDataCallback);
  mHalOpenCompletedEvent.wait();

  mHalCloseCompletedEvent.lock();
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: try close HAL", func);
  HalClose();
  mHalCloseCompletedEvent.wait();

  HalTerminate();
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: exit", func);
}

/*******************************************************************************
**
** Function:    NfcAdaptation::HalDownloadFirmwareCallback
**
** Description: Receive events from the HAL.
**
** Returns:     None.
**
*******************************************************************************/
void NfcAdaptation::HalDownloadFirmwareCallback(nfc_event_t event,
                                                __attribute__((unused))
                                                nfc_status_t event_status) {
  const char* func = "NfcAdaptation::HalDownloadFirmwareCallback";
  DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf("%s: event=0x%X", func, event);
  switch (event) {
    case HAL_NFC_OPEN_CPLT_EVT: {
      DLOG_IF(INFO, nfc_debug_enabled)
          << StringPrintf("%s: HAL_NFC_OPEN_CPLT_EVT", func);
      mHalOpenCompletedEvent.signal();
      break;
    }
    case HAL_NFC_CLOSE_CPLT_EVT: {
      DLOG_IF(INFO, nfc_debug_enabled)
          << StringPrintf("%s: HAL_NFC_CLOSE_CPLT_EVT", func);
      mHalCloseCompletedEvent.signal();
      break;
    }
  }
}

/*******************************************************************************
**
** Function:    NfcAdaptation::HalDownloadFirmwareDataCallback
**
** Description: Receive data events from the HAL.
**
** Returns:     None.
**
*******************************************************************************/
void NfcAdaptation::HalDownloadFirmwareDataCallback(__attribute__((unused))
                                                    uint16_t data_len,
                                                    __attribute__((unused))
                                                    uint8_t* p_data) {}

/*******************************************************************************
**
** Function:    ThreadMutex::ThreadMutex()
**
** Description: class constructor
**
** Returns:     none
**
*******************************************************************************/
ThreadMutex::ThreadMutex() {
  pthread_mutexattr_t mutexAttr;

  pthread_mutexattr_init(&mutexAttr);
  pthread_mutex_init(&mMutex, &mutexAttr);
  pthread_mutexattr_destroy(&mutexAttr);
}

/*******************************************************************************
**
** Function:    ThreadMutex::~ThreadMutex()
**
** Description: class destructor
**
** Returns:     none
**
*******************************************************************************/
ThreadMutex::~ThreadMutex() { pthread_mutex_destroy(&mMutex); }

/*******************************************************************************
**
** Function:    ThreadMutex::lock()
**
** Description: lock kthe mutex
**
** Returns:     none
**
*******************************************************************************/
void ThreadMutex::lock() { pthread_mutex_lock(&mMutex); }

/*******************************************************************************
**
** Function:    ThreadMutex::unblock()
**
** Description: unlock the mutex
**
** Returns:     none
**
*******************************************************************************/
void ThreadMutex::unlock() { pthread_mutex_unlock(&mMutex); }

/*******************************************************************************
**
** Function:    ThreadCondVar::ThreadCondVar()
**
** Description: class constructor
**
** Returns:     none
**
*******************************************************************************/
ThreadCondVar::ThreadCondVar() {
  pthread_condattr_t CondAttr;

  pthread_condattr_init(&CondAttr);
  pthread_cond_init(&mCondVar, &CondAttr);

  pthread_condattr_destroy(&CondAttr);
}

/*******************************************************************************
**
** Function:    ThreadCondVar::~ThreadCondVar()
**
** Description: class destructor
**
** Returns:     none
**
*******************************************************************************/
ThreadCondVar::~ThreadCondVar() { pthread_cond_destroy(&mCondVar); }

/*******************************************************************************
**
** Function:    ThreadCondVar::wait()
**
** Description: wait on the mCondVar
**
** Returns:     none
**
*******************************************************************************/
void ThreadCondVar::wait() {
  pthread_cond_wait(&mCondVar, *this);
  pthread_mutex_unlock(*this);
}

/*******************************************************************************
**
** Function:    ThreadCondVar::signal()
**
** Description: signal the mCondVar
**
** Returns:     none
**
*******************************************************************************/
void ThreadCondVar::signal() {
  AutoThreadMutex a(*this);
  pthread_cond_signal(&mCondVar);
}

/*******************************************************************************
**
** Function:    AutoThreadMutex::AutoThreadMutex()
**
** Description: class constructor, automatically lock the mutex
**
** Returns:     none
**
*******************************************************************************/
AutoThreadMutex::AutoThreadMutex(ThreadMutex& m) : mm(m) { mm.lock(); }

/*******************************************************************************
**
** Function:    AutoThreadMutex::~AutoThreadMutex()
**
** Description: class destructor, automatically unlock the mutex
**
** Returns:     none
**
*******************************************************************************/
AutoThreadMutex::~AutoThreadMutex() { mm.unlock(); }
