/******************************************************************************
 *
 *  Copyright (C) 2011-2012 Broadcom Corporation
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
/******************************************************************************
 *
 *  The original Work has been changed by NXP Semiconductors.
 *
 *  Copyright (C) 2015-2019 NXP Semiconductors
 *  Copyright 2020 NXP
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
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
#pragma once
#include <pthread.h>

#include "config.h"
#include "nfc_target.h"
#include "nfc_hal_api.h"
#include <hardware/nfc.h>
#include <utils/RefBase.h>
#include <android/hardware/nfc/1.0/INfc.h>
#include <android/hardware/nfc/1.0/INfcClientCallback.h>
#include <android/hardware/nfc/1.0/types.h>
#include <vendor/nxp/nxpnfc/2.0/INxpNfc.h>
#include <vendor/nxp/nxpnfclegacy/1.0/INxpNfcLegacy.h>
#include <vendor/nxp/nxpnfclegacy/1.0/types.h>

using vendor::nxp::nxpnfclegacy::V1_0::INxpNfcLegacy;
using vendor::nxp::nxpnfc::V2_0::INxpNfc;
using ::android::sp;
using ::vendor::nxp::nxpnfclegacy::V1_0::NxpNfcHalEseState;
using ::vendor::nxp::nxpnfclegacy::V1_0::NfcHciInitStatus;
using ::vendor::nxp::nxpnfclegacy::V1_0::NxpNciCfgInfo;
using vendor::nxp::nxpnfclegacy::V1_0::NxpNfcHalConfig;
using ::vendor::nxp::nxpnfclegacy::V1_0::NxpNciExtnCmd;
using ::vendor::nxp::nxpnfclegacy::V1_0::NxpNciExtnResp;

namespace android {
namespace hardware {
namespace nfc {
namespace V1_0 {
struct INfc;
struct INfcClientCallback;
}
namespace V1_1 {
struct INfc;
struct INfcClientCallback;
}
namespace V1_2 {
struct INfc;
}
}
}
}

class ThreadMutex {
 public:
  ThreadMutex();
  virtual ~ThreadMutex();
  void lock();
  void unlock();
  explicit operator pthread_mutex_t*() { return &mMutex; }

 private:
  pthread_mutex_t mMutex;
};

class ThreadCondVar : public ThreadMutex {
 public:
  ThreadCondVar();
  virtual ~ThreadCondVar();
  void signal();
  void wait();
  explicit operator pthread_cond_t*() { return &mCondVar; }
  // NOLINTNEXTLINE(google-explicit-constructor)
  operator pthread_mutex_t*() {
    return ThreadMutex::operator pthread_mutex_t*();
  }

 private:
  pthread_cond_t mCondVar;
};

class AutoThreadMutex {
 public:
  explicit AutoThreadMutex(ThreadMutex& m);
  virtual ~AutoThreadMutex();
  explicit operator ThreadMutex&() { return mm; }
  explicit operator pthread_mutex_t*() { return (pthread_mutex_t*)mm; }

 private:
  ThreadMutex& mm;
};

enum NxpNfcAdaptationEseState  : uint64_t {
    NFC_ESE_IDLE_MODE = 0,
    NFC_ESE_WIRED_MODE
};

enum {
    HAL_NFC_ENABLE_I2C_FRAGMENTATION_EVT = 0x08,
    HAL_NFC_POST_MIN_INIT_CPLT_EVT       = 0x09,
    HAL_NFC_WRITE_COMPLETE = 0x0A
};

enum NxpNfcHalStatus {
    /** In case of an error, HCI network needs to be re-initialized */
    HAL_NFC_STATUS_RESTART = 0x30,
    HAL_NFC_HCI_NV_RESET = 0x40,
    HAL_NFC_CONFIG_ESE_LINK_COMPLETE = 0x50
};

struct NxpAdaptationScrResetEmvcoCmd{
  uint64_t len;
  uint8_t cmd[10];
};

struct NxpAdaptationConfig {
  uint8_t ese_listen_tech_mask;
  uint8_t default_nfcee_disc_timeout;
  uint8_t default_nfcee_timeout;
  uint8_t ese_wired_prt_mask;
  uint8_t uicc_wired_prt_mask;
  uint8_t wired_mode_rf_field_enable;
  uint8_t aid_block_route;
  uint8_t esePowerDhControl;
  uint8_t tagOpTimeout;
  uint8_t loaderServiceVersion;
  uint8_t defaultNfceeDiscTimeout;
  uint8_t dualUiccEnable;
  uint8_t ceRouteStrictDisable;
  uint32_t osDownloadTimeoutValue;
  uint8_t defaultAidRoute;
  uint8_t defaultAidPwrState;
  uint8_t defaultRoutePwrState;
  uint8_t defaultOffHostPwrState;
  uint8_t jcopDlAtBootEnable;
  uint8_t defaultNfceeTimeout;
  uint8_t nxpNfcChip;
  uint8_t coreScrnOffAutonomousEnable;
  uint8_t p61LsDefaultInterface;
  uint8_t p61JcopDefaultInterface;
  uint8_t agcDebugEnable;
  uint8_t felicaCltPowerState;
  uint32_t cmdRspTimeoutValue;
  uint8_t checkDefaultProtoSeId;
  uint8_t nfccPassiveListenTimeout;
  uint32_t nfccStandbyTimeout;
  uint32_t wmMaxWtxCount;
  uint32_t nfccRfFieldEventTimeout;
  uint8_t allowWiredInMifareDesfireClt;
  uint8_t dwpIntfResetEnable;
  uint8_t nxpLogHalLoglevel;
  uint8_t nxpLogExtnsLogLevel;
  uint8_t nxpLogTmlLogLevel;
  uint8_t nxpLogFwDnldLogLevel;
  uint8_t nxpLogNcixLogLevel;
  uint8_t nxpLogNcirLogLevel;
  uint8_t scrCfgFormat;
  uint8_t etsiReaderEnable;
  uint8_t techAbfRoute;
  uint8_t techAbfPwrState;
  uint8_t wTagSupport;
  uint8_t t4tNfceePwrState;
  NxpAdaptationScrResetEmvcoCmd scrResetEmvco;
};

class NfcDeathRecipient ;

class NfcAdaptation {
 public:

  virtual ~NfcAdaptation();
  void Initialize();
  void Finalize();
  void FactoryReset();
  void DeviceShutdown();
  static uint32_t HalsetEseState(tNxpEseState ESEstate);
  static uint8_t HalgetchipType();
  static uint16_t HalsetNfcServicePid(uint64_t NfcNxpServicePid);
  static uint32_t HalgetEseState();
  static uint16_t HalSpiDwpSync(uint32_t level);
  static uint16_t HalRelForceDwpOnOffWait(uint32_t level);
  static int32_t HalHciInitUpdateState(tNFC_HCI_INIT_STATUS HciStatus);
  static void HalGetCachedNfccConfig(tNxpNci_getCfg_info_t *nxpNciAtrInfo);
  static void HalGetNxpConfig(NxpAdaptationConfig& NfcConfigData);
  static uint32_t HalNciTransceive(phNxpNci_Extn_Cmd_t* in,phNxpNci_Extn_Resp_t* out);
  NxpNfcHalConfig mNxpNfcHalConfig;
  static NfcAdaptation& GetInstance();
  tHAL_NFC_ENTRY* GetHalEntryFuncs();
  bool DownloadFirmware();
  void GetNxpConfigs(std::map<std::string, ConfigValue>& configMap);
  void GetVendorConfigs(std::map<std::string, ConfigValue>& configMap);
  void Dump(int fd);
#if (NXP_EXTNS == TRUE)
  void MinInitialize();
  int HalGetFwDwnldFlag(uint8_t* fwDnldRequest);
#endif
  NxpNciCfgInfo AdapCfgInfo;
  NxpNciExtnResp mNciResp;
 private:
  NfcAdaptation();
  void signal();
  static NfcAdaptation* mpInstance;
  static ThreadMutex sLock;

  ThreadCondVar mCondVar;
  tHAL_NFC_ENTRY mHalEntryFuncs;  // function pointers for HAL entry points
  static tHAL_NFC_CBACK* mHalCallback;
  static tHAL_NFC_DATA_CBACK* mHalDataCallback;
  static ThreadCondVar mHalOpenCompletedEvent;
  static ThreadCondVar mHalCloseCompletedEvent;
  static ThreadCondVar mHalIoctlEvent;
  static android::sp<android::hardware::nfc::V1_0::INfc> mHal;
  static android::sp<android::hardware::nfc::V1_1::INfc> mHal_1_1;
  static android::sp<android::hardware::nfc::V1_2::INfc> mHal_1_2;
  static android::sp<vendor::nxp::nxpnfc::V2_0::INxpNfc> mHalNxpNfc;
  static android::sp<vendor::nxp::nxpnfclegacy::V1_0::INxpNfcLegacy> mHalNxpNfcLegacy;
  static android::hardware::nfc::V1_1::INfcClientCallback* mCallback;
  sp<NfcDeathRecipient> mNfcHalDeathRecipient;
#if (NXP_EXTNS == TRUE)
  static ThreadCondVar mHalCoreResetCompletedEvent;
  static ThreadCondVar mHalCoreInitCompletedEvent;
  static ThreadCondVar mHalInitCompletedEvent;
#endif
  static uint32_t NFCA_TASK(uint32_t arg);
  static uint32_t Thread(uint32_t arg);
  void InitializeHalDeviceContext();
  static void HalDeviceContextCallback(nfc_event_t event,
                                       nfc_status_t event_status);
  static void HalDeviceContextDataCallback(uint16_t data_len, uint8_t* p_data);

  static void HalInitialize();
  static void HalTerminate();
  static void HalOpen(tHAL_NFC_CBACK* p_hal_cback,
                      tHAL_NFC_DATA_CBACK* p_data_cback);
  static void HalClose();
  static void HalCoreInitialized(uint16_t data_len,
                                 uint8_t* p_core_init_rsp_params);
  static void HalWrite(uint16_t data_len, uint8_t* p_data);
  static bool HalPrediscover();
  static void HalControlGranted();
  static void HalPowerCycle();
  static uint8_t HalGetMaxNfcee();
  static void HalDownloadFirmwareCallback(nfc_event_t event,
                                          nfc_status_t event_status);
  static void HalDownloadFirmwareDataCallback(uint16_t data_len,
                                              uint8_t* p_data);
#if (NXP_EXTNS == TRUE)
  static bool HalSetTransitConfig(char * strval);
  static int getVendorNumConfig(const char* configName);
#endif
};
