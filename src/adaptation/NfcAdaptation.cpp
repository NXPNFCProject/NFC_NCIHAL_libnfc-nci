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
/******************************************************************************
 *
 *  The original Work has been changed by NXP Semiconductors.
 *
 *  Copyright (C) 2015 NXP Semiconductors
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
#include "_OverrideLog.h"

#include <android/hardware/nfc/1.0/INfc.h>
#include <android/hardware/nfc/1.0/INfcClientCallback.h>
#include <android/hardware/nfc/1.0/types.h>
#include <hwbinder/ProcessState.h>
#include <pthread.h>
#include "NfcAdaptation.h"
#include "debug_nfcsnoop.h"
#include "nfc_target.h"

extern "C" {
#include "gki.h"
#include "nfa_api.h"
#include "nfc_int.h"
#include "nfc_target.h"
#include "vendor_cfg.h"
}
#include "config.h"
#include "android_logmsg.h"

#undef LOG_TAG
#define LOG_TAG "NfcAdaptation"

using android::OK;
using android::sp;
using android::status_t;

using android::hardware::ProcessState;
using android::hardware::Return;
using android::hardware::Void;
using android::hardware::nfc::V1_0::INfc;
using android::hardware::nfc::V1_0::INfcClientCallback;
using android::hardware::hidl_vec;
using vendor::nxp::nxpnfc::V1_0::INxpNfc;

extern "C" void GKI_shutdown();
extern void resetConfig();
extern "C" void verify_stack_non_volatile_store();
extern "C" void delete_stack_non_volatile_store(bool forceDelete);

NfcAdaptation* NfcAdaptation::mpInstance = NULL;
ThreadMutex NfcAdaptation::sLock;
ThreadMutex NfcAdaptation::sIoctlLock;
sp<INxpNfc> NfcAdaptation::mHalNxpNfc;
sp<INfc> NfcAdaptation::mHal;
INfcClientCallback* NfcAdaptation::mCallback;
tHAL_NFC_CBACK* NfcAdaptation::mHalCallback = NULL;
tHAL_NFC_DATA_CBACK* NfcAdaptation::mHalDataCallback = NULL;
ThreadCondVar NfcAdaptation::mHalOpenCompletedEvent;
ThreadCondVar NfcAdaptation::mHalCloseCompletedEvent;

#if (NXP_EXTNS == TRUE)
ThreadCondVar NfcAdaptation::mHalCoreResetCompletedEvent;
ThreadCondVar NfcAdaptation::mHalCoreInitCompletedEvent;
ThreadCondVar NfcAdaptation::mHalInitCompletedEvent;
#define SIGNAL_NONE 0
#define SIGNAL_SIGNALED 1
static uint8_t isSignaled = SIGNAL_NONE;
static uint8_t evt_status;
#endif

uint32_t ScrProtocolTraceFlag = SCR_PROTO_TRACE_ALL;  // 0x017F00;
uint8_t appl_trace_level = 0xff;
uint8_t appl_dta_mode_flag = 0x00;
char bcm_nfc_location[120];

static uint8_t nfa_dm_cfg[sizeof(tNFA_DM_CFG)];
static uint8_t nfa_proprietary_cfg[sizeof(tNFA_PROPRIETARY_CFG)];
extern tNFA_DM_CFG* p_nfa_dm_cfg;
extern tNFA_PROPRIETARY_CFG* p_nfa_proprietary_cfg;
extern uint8_t nfa_ee_max_ee_cfg;
extern const uint8_t nfca_version_string[];
extern const uint8_t nfa_version_string[];
static uint8_t deviceHostWhiteList[NFA_HCI_MAX_HOST_IN_NETWORK];
static tNFA_HCI_CFG jni_nfa_hci_cfg;
extern tNFA_HCI_CFG* p_nfa_hci_cfg;
extern bool nfa_poll_bail_out_mode;

class NfcClientCallback : public INfcClientCallback {
 public:
  NfcClientCallback(tHAL_NFC_CBACK* eventCallback,
                    tHAL_NFC_DATA_CBACK dataCallback) {
    mEventCallback = eventCallback;
    mDataCallback = dataCallback;
  };
  virtual ~NfcClientCallback() = default;
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

  if (!mpInstance) mpInstance = new NfcAdaptation;
  return *mpInstance;
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
  ALOGD("%s: enter", func);
  ALOGE("%s: ver=%s nfa=%s", func, nfca_version_string, nfa_version_string);
  unsigned long num;

  if (GetNumValue(NAME_USE_RAW_NCI_TRACE, &num, sizeof(num))) {
    if (num == 1) {
      // display protocol traces in raw format
      ProtoDispAdapterUseRawOutput(true);
      ALOGD("%s: logging protocol in raw format", func);
    }
  }
  if (!GetStrValue(NAME_NFA_STORAGE, bcm_nfc_location,
                   sizeof(bcm_nfc_location))) {
    strlcpy(bcm_nfc_location, "/data/vendor/nfc", sizeof(bcm_nfc_location));
  }

  initializeProtocolLogLevel();

  if (GetStrValue(NAME_NFA_DM_CFG, (char*)nfa_dm_cfg, sizeof(nfa_dm_cfg)))
    p_nfa_dm_cfg = (tNFA_DM_CFG*)((void*)&nfa_dm_cfg[0]);

  if (GetNumValue(NAME_NFA_POLL_BAIL_OUT_MODE, &num, sizeof(num))) {
    nfa_poll_bail_out_mode = num;
    ALOGD("%s: Overriding NFA_POLL_BAIL_OUT_MODE to use %d", func,
          nfa_poll_bail_out_mode);
  }

  if (GetStrValue(NAME_NFA_PROPRIETARY_CFG, (char*)nfa_proprietary_cfg,
                  sizeof(tNFA_PROPRIETARY_CFG))) {
    p_nfa_proprietary_cfg =
        (tNFA_PROPRIETARY_CFG*)(void*)(&nfa_proprietary_cfg[0]);
  }
  // configure device host whitelist of HCI host ID's; see specification ETSI TS
  // 102 622 V11.1.10
  //(2012-10), section 6.1.3.1
  num = GetStrValue(NAME_DEVICE_HOST_WHITE_LIST, (char*)deviceHostWhiteList,
                    sizeof(deviceHostWhiteList));
  if (num) {
    memmove(&jni_nfa_hci_cfg, p_nfa_hci_cfg, sizeof(jni_nfa_hci_cfg));
    jni_nfa_hci_cfg.num_whitelist_host =
        (uint8_t)num;  // number of HCI host ID's in the whitelist
    jni_nfa_hci_cfg.p_whitelist = deviceHostWhiteList;  // array of HCI host
                                                        // ID's
    p_nfa_hci_cfg = &jni_nfa_hci_cfg;
  }

  initializeGlobalAppLogLevel();

  verify_stack_non_volatile_store();
  if (GetNumValue(NAME_PRESERVE_STORAGE, (char*)&num, sizeof(num)) &&
      (num == 1))
    ALOGD("%s: preserve stack NV store", __func__);
  else {
    delete_stack_non_volatile_store(false);
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

  mHalCallback = NULL;
  memset(&mHalEntryFuncs, 0, sizeof(mHalEntryFuncs));
  InitializeHalDeviceContext();
  debug_nfcsnoop_init();
  ALOGD("%s: exit", func);
}
#if (NXP_EXTNS == TRUE)
/*******************************************************************************
**
** Function:    NfcAdaptation::MinInitialize()
**
** Description: class initializer
**
** Returns:     none
**
*******************************************************************************/
void NfcAdaptation::MinInitialize() {
  const char* func = "NfcAdaptation::MinInitialize";
  ALOGD("%s: enter", func);
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

  mHalCallback = NULL;
  memset(&mHalEntryFuncs, 0, sizeof(mHalEntryFuncs));
  InitializeHalDeviceContext();
  ALOGD("%s: exit", func);
}
#endif
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

  ALOGD("%s: enter", func);
  GKI_shutdown();

  resetConfig();

  mCallback = NULL;
  memset(&mHalEntryFuncs, 0, sizeof(mHalEntryFuncs));

  ALOGD("%s: exit", func);
  delete this;
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
uint32_t NfcAdaptation::NFCA_TASK(uint32_t arg) {
  const char* func = "NfcAdaptation::NFCA_TASK";
  ALOGD("%s: enter", func);
  GKI_run(0);
  ALOGD("%s: exit", func);
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
uint32_t NfcAdaptation::Thread(uint32_t arg) {
  const char* func = "NfcAdaptation::Thread";
  ALOGD("%s: enter", func);

  {
    ThreadCondVar CondVar;
    AutoThreadMutex guard(CondVar);
    GKI_create_task((TASKPTR)nfc_task, NFC_TASK, (int8_t*)"NFC_TASK", 0, 0,
                    (pthread_cond_t*)CondVar, (pthread_mutex_t*)CondVar);
    CondVar.wait();
  }

  NfcAdaptation::GetInstance().signal();

  GKI_exit_task(GKI_get_taskid());
  ALOGD("%s: exit", func);
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
  ALOGD("%s: enter", func);
  int ret = 0;  // 0 means success

  const hw_module_t* hw_module = NULL;

  mHalEntryFuncs.initialize = HalInitialize;
  mHalEntryFuncs.terminate = HalTerminate;
  mHalEntryFuncs.open = HalOpen;
  mHalEntryFuncs.close = HalClose;
  mHalEntryFuncs.core_initialized = HalCoreInitialized;
  mHalEntryFuncs.write = HalWrite;
#if (NXP_EXTNS == TRUE)
  mHalEntryFuncs.ioctl = HalIoctl;
#endif
  mHalEntryFuncs.prediscover = HalPrediscover;
  mHalEntryFuncs.control_granted = HalControlGranted;
  mHalEntryFuncs.power_cycle = HalPowerCycle;
  mHalEntryFuncs.get_max_ee = HalGetMaxNfcee;
  ALOGI("%s: INfc::getService()", func);
  mHal = INfc::getService();
  LOG_FATAL_IF(mHal == nullptr, "Failed to retrieve the NFC HAL!");
  ALOGI("%s: INfc::getService() returned %p (%s)", func, mHal.get(),
        (mHal->isRemote() ? "remote" : "local"));
  ALOGI("%s: INxpNfc::getService()", func);
  mHalNxpNfc = INxpNfc::getService();
  LOG_FATAL_IF(mHalNxpNfc == nullptr, "Failed to retrieve the NXP NFC HAL!");
  ALOGI("%s: INxpNfc::getService() returned %p (%s)", func, mHalNxpNfc.get(),
        (mHalNxpNfc->isRemote() ? "remote" : "local"));
  ALOGD("%s: exit", func);
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
  ALOGD("%s", func);
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
  ALOGD("%s", func);
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
  ALOGD("%s", func);
  mCallback = new NfcClientCallback(p_hal_cback, p_data_cback);
  mHal->open(mCallback);
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
  ALOGD("%s", func);
  mHal->close();
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
  ALOGD("%s: event=%u", func, event);
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
  ALOGD("%s: len=%u", func, data_len);
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
  ALOGD("%s", func);
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
  ALOGD("%s Ioctl Type=%llu", func, pOutData->ioctlType);
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
  int status = 0;
  ALOGD("%s arg=%ld", func, arg);
  pInpOutData->inp.context = &NfcAdaptation::GetInstance();
  NfcAdaptation::GetInstance().mCurrentIoctlData = pInpOutData;
  data.setToExternal((uint8_t*)pInpOutData, sizeof(nfc_nci_IoctlInOutData_t));
  if(mHalNxpNfc != nullptr)
      mHalNxpNfc->ioctl(arg, data, IoctlCallback);
  ALOGD("%s Ioctl Completed for Type=%llu", func, pInpOutData->out.ioctlType);
  return (pInpOutData->out.result);
}

/*******************************************************************************
**
** Function:    NfcAdaptation::HalGetFwDwnldFlag
**
** Description: Get FW Download Flag.
**
** Returns:     SUCESS or FAIL.
**
*******************************************************************************/
int NfcAdaptation::HalGetFwDwnldFlag(uint8_t* fwDnldRequest) {
  const char* func = "NfcAdaptation::HalGetFwDwnldFlag";
  int status = NFA_STATUS_FAILED;
  ALOGD("%s : Dummy", func);
  /*FIXME: Additional IOCTL type can be added for this
   * Instead of extra function pointer
   * This is required for a corner case or delayed FW download after SPI session
   * is completed
   * This shall be fixed with a better design later*/
  /*
  if (mHalDeviceContext)
  {
      status = mHalDeviceContext->check_fw_dwnld_flag(mHalDeviceContext,
  fwDnldRequest);
  }*/
  return status;
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
  ALOGD("%s", func);
  hidl_vec<uint8_t> data;
  data.setToExternal(p_core_init_rsp_params, data_len);
  mHal->coreInitialized(data);
}

/*******************************************************************************
**
** Function:    NfcAdaptation::HalPrediscover
**
** Description:     Perform any vendor-specific pre-discovery actions (if
**                  needed) If any actions were performed true will be returned,
**                  and HAL_PRE_DISCOVER_CPLT_EVT will notify when actions are
**                  completed.
**
** Returns:         true if vendor-specific pre-discovery actions initialized
**                  false if no vendor-specific pre-discovery actions are
**                  needed.
**
*******************************************************************************/
bool NfcAdaptation::HalPrediscover() {
  const char* func = "NfcAdaptation::HalPrediscover";
  ALOGD("%s", func);
  bool retval = false;
  if(mHal != nullptr)
  {
      mHal->prediscover();
  }
  /*FIXME: Additional IOCTL type can be added for this
   * Instead of extra function pointer
   * prediscover in HAL is empty dummy function
   */
  /*if (mHalDeviceContext)
  {
      retval = mHalDeviceContext->pre_discover (mHalDeviceContext);
  }*/
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
  ALOGD("%s", func);
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
  ALOGD("%s", func);
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
  uint8_t maxNfcee = 0;
  ALOGD("%s", func);
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
  ALOGD("%s: enter", func);
#if (NXP_EXTNS == TRUE)
  static uint8_t cmd_reset_nci[] = {0x20, 0x00, 0x01, 0x00};
  static uint8_t cmd_init_nci[] = {0x20, 0x01, 0x00};
  static uint8_t cmd_init_nci2_0[] = {0x20,0x01,0x02,0x00,0x00};
  static uint8_t cmd_reset_nci_size = sizeof(cmd_reset_nci) / sizeof(uint8_t);
  static uint8_t cmd_init_nci_size = sizeof(cmd_init_nci) / sizeof(uint8_t);
  if(NFA_GetNCIVersion() == NCI_VERSION_2_0)
  {
      cmd_init_nci_size = sizeof(cmd_init_nci2_0) / sizeof(uint8_t);
  }
  nfc_nci_IoctlInOutData_t inpOutData;
  tNFC_FWUpdate_Info_t fw_update_inf;
  uint8_t p_core_init_rsp_params;
  uint16_t fw_dwnld_status = NFC_STATUS_FAILED;
  evt_status = NFC_STATUS_FAILED;
#endif
  HalInitialize();

  mHalOpenCompletedEvent.lock();
  ALOGD("%s: try open HAL", func);
  HalOpen(HalDownloadFirmwareCallback, HalDownloadFirmwareDataCallback);
  mHalOpenCompletedEvent.wait();
  mHalOpenCompletedEvent.unlock();
#if (NXP_EXTNS == TRUE)
  /* Send a CORE_RESET and CORE_INIT to the NFCC. This is required because when
   * calling
   * HalCoreInitialized, the HAL is going to parse the conf file and send NCI
   * commands
   * to the NFCC. Hence CORE-RESET and CORE-INIT have to be sent prior to this.
   */
  isSignaled = SIGNAL_NONE;
  ALOGD("%s: send CORE_RESET", func);
  HalWrite(cmd_reset_nci_size, cmd_reset_nci);
  mHalCoreResetCompletedEvent.lock();
  if (SIGNAL_NONE == isSignaled) {
    mHalCoreResetCompletedEvent.wait();
  }
  isSignaled = SIGNAL_NONE;
  mHalCoreResetCompletedEvent.unlock();
  if (evt_status == NFC_STATUS_FAILED) {
    goto TheEnd;
  }
  ALOGD("%s: send CORE_INIT", func);
  ALOGD("%s: send CORE_INIT NCI Version : %d", func, NFA_GetNCIVersion());
  if(NFA_GetNCIVersion() == NCI_VERSION_2_0)
    HalWrite(cmd_init_nci_size, cmd_init_nci2_0);
  else
    HalWrite(cmd_init_nci_size, cmd_init_nci);
  mHalCoreInitCompletedEvent.lock();
  if (SIGNAL_NONE == isSignaled) {
    mHalCoreInitCompletedEvent.wait();
  }
  isSignaled = SIGNAL_NONE;
  mHalCoreInitCompletedEvent.unlock();
  if (evt_status == NFC_STATUS_FAILED) {
    goto TheEnd;
  }
  mHalEntryFuncs.ioctl(HAL_NFC_IOCTL_CHECK_FLASH_REQ, &inpOutData);
  fw_update_inf = *(tNFC_FWUpdate_Info_t*)&inpOutData.out.data.fwUpdateInf;
  NFC_TRACE_DEBUG1("fw_update required  -> %d", fw_update_inf.fw_update_reqd);
  if (fw_update_inf.fw_update_reqd == true) {
    mHalEntryFuncs.ioctl(HAL_NFC_IOCTL_FW_DWNLD, &inpOutData);
    fw_dwnld_status = inpOutData.out.data.fwDwnldStatus;
    if (fw_dwnld_status != NFC_STATUS_OK) {
      ALOGD("%s: FW Download failed", func);
    } else {
      isSignaled = SIGNAL_NONE;
      ALOGD("%s: send CORE_RESET", func);
      HalWrite(cmd_reset_nci_size, cmd_reset_nci);
      mHalCoreResetCompletedEvent.lock();
      if (SIGNAL_NONE == isSignaled) {
        mHalCoreResetCompletedEvent.wait();
      }
      isSignaled = SIGNAL_NONE;
      mHalCoreResetCompletedEvent.unlock();
      if (evt_status == NFC_STATUS_FAILED) {
        goto TheEnd;
      }
      ALOGD("%s: send CORE_INIT", func);
      if(NFA_GetNCIVersion() == NCI_VERSION_2_0)
        HalWrite(cmd_init_nci_size, cmd_init_nci2_0);
      else
        HalWrite(cmd_init_nci_size, cmd_init_nci);
      mHalCoreInitCompletedEvent.lock();
      if (SIGNAL_NONE == isSignaled) {
        mHalCoreInitCompletedEvent.wait();
      }
      isSignaled = SIGNAL_NONE;
      mHalCoreInitCompletedEvent.unlock();
      if (evt_status == NFC_STATUS_FAILED) {
        goto TheEnd;
      }
      ALOGD("%s: try init HAL", func);
      HalCoreInitialized(sizeof(uint8_t), &p_core_init_rsp_params);
      mHalInitCompletedEvent.lock();
      mHalInitCompletedEvent.wait();
      mHalInitCompletedEvent.unlock();
    }
  }

TheEnd:
  isSignaled = SIGNAL_NONE;
#endif
  ALOGD("%s: try close HAL", func);
  HalClose();
#if (NXP_EXTNS == TRUE)
  mHalCloseCompletedEvent.lock();
  if (SIGNAL_NONE == isSignaled) {
    mHalCloseCompletedEvent.wait();
  }
  isSignaled = SIGNAL_NONE;
  mHalCloseCompletedEvent.unlock();
#endif
  HalTerminate();
  ALOGD("%s: exit", func);
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
                                                nfc_status_t event_status) {
  const char* func = "NfcAdaptation::HalDownloadFirmwareCallback";
  (void)(event_status);
  ALOGD("%s: event=0x%X", func, event);
  switch (event) {
    case HAL_NFC_OPEN_CPLT_EVT: {
      ALOGD("%s: HAL_NFC_OPEN_CPLT_EVT", func);
      mHalOpenCompletedEvent.signal();
      break;
    }
    case HAL_NFC_POST_INIT_CPLT_EVT: {
      ALOGD("%s: HAL_NFC_POST_INIT_CPLT_EVT", func);
      mHalInitCompletedEvent.signal();
      break;
    }
    case HAL_NFC_CLOSE_CPLT_EVT: {
      ALOGD("%s: HAL_NFC_CLOSE_CPLT_EVT", func);
#if (NXP_EXTNS == TRUE)
      isSignaled = SIGNAL_SIGNALED;
#endif
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
void NfcAdaptation::HalDownloadFirmwareDataCallback(uint16_t data_len,
                                                    uint8_t* p_data) {
#if (NXP_EXTNS == TRUE)
  isSignaled = SIGNAL_SIGNALED;
  if (data_len > 3) {
    evt_status = NFC_STATUS_OK;
    if (p_data[0] == 0x40 && p_data[1] == 0x00) {
      mHalCoreResetCompletedEvent.signal();
    } else if (p_data[0] == 0x40 && p_data[1] == 0x01) {
      mHalCoreInitCompletedEvent.signal();
    }
  } else {
    evt_status = NFC_STATUS_FAILED;
    mHalCoreResetCompletedEvent.signal();
    mHalCoreInitCompletedEvent.signal();
  }
#endif
}

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
