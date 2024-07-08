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
*  Copyright 2020-2022 NXP
*
******************************************************************************/
#pragma once
#include <pthread.h>
#include <utils/RefBase.h>

#include "config.h"
#include "nfc_hal_api.h"
#include "nfc_target.h"

#if (NXP_EXTNS == TRUE)
#include <semaphore.h>
#endif

using ::android::sp;

namespace android {
namespace hardware {
namespace nfc {
namespace V1_0 {
struct INfc;
struct INfcClientCallback;
}  // namespace V1_0
namespace V1_1 {
struct INfc;
struct INfcClientCallback;
}  // namespace V1_1
namespace V1_2 {
struct INfc;
}
}  // namespace nfc
}  // namespace hardware
}  // namespace android
#if (NXP_EXTNS == TRUE)
namespace vendor {
namespace nxp {
namespace nxpnfc {
namespace V2_0 {
struct INxpNfc;
}
}
}
}
namespace aidl {
namespace vendor {
namespace nxp {
namespace nxpnfc_aidl {
class INxpNfc;
}
}
}
}
typedef void(tNFC_JNI_FWSTATUS_CBACK)(uint8_t status);
#endif

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

class NfcHalDeathRecipient;

class NfcAdaptation {
 public:
  virtual ~NfcAdaptation();
  void Initialize();
  void Finalize();
  void FactoryReset();
  void DeviceShutdown();
  static NfcAdaptation& GetInstance();
  tHAL_NFC_ENTRY* GetHalEntryFuncs();
#if (NXP_EXTNS == TRUE)
  bool DownloadFirmware(tNFC_JNI_FWSTATUS_CBACK* p_cback, bool isNfcOn);
#else
  bool DownloadFirmware();
#endif
  void GetVendorConfigs(std::map<std::string, ConfigValue>& configMap);
#if (NXP_EXTNS == TRUE)
  void NFA_SetBootMode(uint8_t boot_mode);
  uint8_t NFA_GetBootMode();
  string HalGetProperty(string key);
  bool HalSetProperty(string key, string value);
  string propVal;
  static bool resetEse(uint64_t level);
#endif

  void Dump(int fd);
#if (NXP_EXTNS == TRUE)

  tNFC_JNI_FWSTATUS_CBACK* p_fwupdate_status_cback;
#endif
 private:
  NfcAdaptation();
  void signal();
  static NfcAdaptation* mpInstance;
  static ThreadMutex sLock;
#if (NXP_EXTNS == TRUE)
  static ThreadMutex sIoctlLock;
  uint8_t nfcBootMode;
#endif
  ThreadCondVar mCondVar;
  tHAL_NFC_ENTRY mHalEntryFuncs;  // function pointers for HAL entry points
  static android::sp<android::hardware::nfc::V1_0::INfc> mHal;
  static android::sp<android::hardware::nfc::V1_1::INfc> mHal_1_1;
  static android::sp<android::hardware::nfc::V1_2::INfc> mHal_1_2;
#if (NXP_EXTNS == TRUE)
  static android::sp<vendor::nxp::nxpnfc::V2_0::INxpNfc> mHalNxpNfc;
  static std::shared_ptr<::aidl::vendor::nxp::nxpnfc_aidl::INxpNfc> mAidlHalNxpNfc;
#endif
  static android::hardware::nfc::V1_1::INfcClientCallback* mCallback;
  sp<NfcHalDeathRecipient> mNfcHalDeathRecipient;
  static tHAL_NFC_CBACK* mHalCallback;
  static tHAL_NFC_DATA_CBACK* mHalDataCallback;
  static ThreadCondVar mHalOpenCompletedEvent;
#if (NXP_EXTNS == TRUE)
  static sem_t mSemHalDataCallBackEvent;
#endif
  static ThreadCondVar mHalCloseCompletedEvent;

  static uint32_t NFCA_TASK(uint32_t arg);
  static uint32_t Thread(uint32_t arg);
  void InitializeHalDeviceContext();
  static void InitializeAidlHalContext();
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
#if (NXP_EXTNS == TRUE)
  static void HalWriteIntf(uint16_t data_len, uint8_t* p_data);
  static bool HalSetTransitConfig(char * strval);
#endif
  static bool HalPrediscover();
  static void HalControlGranted();
  static void HalPowerCycle();
  static uint8_t HalGetMaxNfcee();
  static void HalDownloadFirmwareCallback(nfc_event_t event,
                                          nfc_status_t event_status);
  static void HalDownloadFirmwareDataCallback(uint16_t data_len,
                                              uint8_t* p_data);

  // Death recipient callback that is called when INfcAidl dies.
  // The cookie is a pointer to a NfcAdaptation object.
  static void HalAidlBinderDied(void* cookie);
  void HalAidlBinderDiedImpl();
};
