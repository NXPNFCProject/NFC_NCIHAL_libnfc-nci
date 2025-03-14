/**
 *
 *  Copyright 2024-2025 NXP
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
 **/

#include <thread>
#include "PlatformAbstractionLayer.h"
#include "NfcExtensionConstants.h"
#include "NfcExtensionController.h"
#include "NfcExtensionApi.h"
#include "ConfigHandler.h"
#include <ProprietaryExtn.h>
#include <phNxpConfig.h>
#include <phNxpLog.h>

#include <android/binder_manager.h>
#include <vendor/nxp/nxpnfc/2.0/INxpNfc.h>

#if __has_include("aidl/vendor/nxp/nxpnfc_aidl/INxpNfc.h")
#include <aidl/vendor/nxp/nxpnfc_aidl/INxpNfc.h>
#else
#include "INxpNfc.h"
#endif

using android::sp;
using INxpNfc = vendor::nxp::nxpnfc::V2_0::INxpNfc;
using INxpNfcAidl = ::aidl::vendor::nxp::nxpnfc_aidl::INxpNfc;

struct NxpNfcHal {
  sp<INxpNfc> halNxpNfc;
  shared_ptr<INxpNfcAidl> aidlHalNxpNfc;
};

string DEBUG_ENABLE_PROP_NAME = "nfc.debug_enabled";
string NXPNFC_AIDL_HAL_SERVICE_NAME = "vendor.nxp.nxpnfc_aidl.INxpNfc/default";

PlatformAbstractionLayer *PlatformAbstractionLayer::sPlatformAbstractionLayer;
string propVal;
NxpNfcHal mNxpNfcHal;
ConfigHandler *mConfigHandler;

PlatformAbstractionLayer::PlatformAbstractionLayer() {
  mConfigHandler = ConfigHandler::getInstance();
  getNxpNfcHal();
}

PlatformAbstractionLayer::~PlatformAbstractionLayer() {}

PlatformAbstractionLayer *PlatformAbstractionLayer::getInstance() {
  if (sPlatformAbstractionLayer == nullptr) {
    sPlatformAbstractionLayer = new PlatformAbstractionLayer();
  }
  return sPlatformAbstractionLayer;
}

void PlatformAbstractionLayer::getNxpNfcHal() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s: enter", __func__);
  ::ndk::SpAIBinder binder(
      AServiceManager_checkService(NXPNFC_AIDL_HAL_SERVICE_NAME.c_str()));
  mNxpNfcHal.aidlHalNxpNfc = INxpNfcAidl::fromBinder(binder);
  if (mNxpNfcHal.aidlHalNxpNfc == nullptr) {
    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "%s: HIDL INxpNfc::tryGetService()", __func__);
    mNxpNfcHal.halNxpNfc = INxpNfc::tryGetService();
    if (mNxpNfcHal.halNxpNfc != nullptr) {
      NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN,
                     "%s: INxpNfc::getService() returned %p (%s)", __func__,
                     mNxpNfcHal.halNxpNfc.get(),
                     (mNxpNfcHal.halNxpNfc->isRemote() ? "remote" : "local"));
    }
  } else {
    mNxpNfcHal.halNxpNfc = nullptr;
    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "%s: INxpNfcAidl::fromBinder returned", __func__);
  }
}

NFCSTATUS PlatformAbstractionLayer::palenQueueWrite(const uint8_t *pBuffer,
                                                    uint16_t wLength) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter wLength:%d", __func__,
                 wLength);
  vector<uint8_t> pBufferCpy(wLength);
  memcpy(pBufferCpy.data(), pBuffer, wLength);
  thread(&PlatformAbstractionLayer::enQueueWriteInternal, this,
         std::move(pBufferCpy), wLength)
      .detach();
  return NFCSTATUS_SUCCESS;
}

void PlatformAbstractionLayer::enQueueWriteInternal(vector<uint8_t> buffer,
                                                    uint16_t wLength) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter wLength:%d", __func__,
                 wLength);
  if (getNfcVendorExtnCb() != nullptr) {
    if (getNfcVendorExtnCb()->aidlHal != nullptr) {
      int ret;
      getNfcVendorExtnCb()->aidlHal->write(buffer, &ret);
    } else if (getNfcVendorExtnCb()->hidlHal != nullptr) {
      ::android::hardware::nfc::V1_0::NfcData data;
      data.setToExternal(buffer.data(), wLength);
      getNfcVendorExtnCb()->hidlHal->write(data);
    } else {
      NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN, "%s No HAL to Write!", __func__);
    }
  } else {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN, "%s No Vendor Extension CB!",
                   __func__);
  }
}

NFCSTATUS PlatformAbstractionLayer::palenQueueRspNtf(const uint8_t *pBuffer, uint16_t wLength) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter wLength:%d", __func__, wLength);

  auto bufferCopy = std::make_shared<std::vector<uint8_t>>(pBuffer, pBuffer + wLength);

  std::thread([this, wLength, bufferCopy]() {
      palSendNfcDataCallback(wLength, bufferCopy->data());
  }).detach();

  return NFCSTATUS_SUCCESS;
}

void PlatformAbstractionLayer::palRequestHALcontrol(void) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter", __func__);
  /* Grant the control as this is system(libnfc_nci) context */
  NfcExtensionController::getInstance()->halControlGranted();
}

void PlatformAbstractionLayer::palReleaseHALcontrol(void) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter", __func__);
}

void PlatformAbstractionLayer::palSendNfcDataCallback(uint16_t dataLen,
                                                      const uint8_t *pData) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter", __func__);
  ProprietaryExtn::getInstance()->mapGidOidToPropRspNtf(dataLen,
                                                        (uint8_t *)pData);
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter mapGidOid Exit", __func__);
  if (getNfcVendorExtnCb() != nullptr) {
    if (getNfcVendorExtnCb()->pDataCback != nullptr) {
      getNfcVendorExtnCb()->pDataCback(dataLen, (uint8_t *)pData);
    } else {
      NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN, "%s No Data Callback!",
                     __func__);
    }
  } else {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN, "%s No Vendor Extension CB!",
                   __func__);
  }
}

void PlatformAbstractionLayer::palEnQueueEvt(uint8_t evt, uint8_t evtStatus) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter evt: %d", __func__, evt);
  std::thread([this, evt, evtStatus]() {
    palSendNfcEventCallback(evt, evtStatus);
  }).detach();
}

void PlatformAbstractionLayer::palSendNfcEventCallback(uint8_t evt, uint8_t evtStatus) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter evt: %d", __func__, evt);
  if (getNfcVendorExtnCb() != nullptr) {
    if (getNfcVendorExtnCb()->pHalCback != nullptr) {
      getNfcVendorExtnCb()->pHalCback(evt, evtStatus);
    } else {
      NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN, "%s No Hal Callback!",
                     __func__);
    }
  } else {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN, "%s No Vendor Extension CB!",
                   __func__);
  }
}


uint8_t PlatformAbstractionLayer::palGetNxpByteArrayValue(const char *name,
                                                          char *pValue,
                                                          long bufflen,
                                                          long *len) {
  return GetNxpByteArrayValue(name, pValue, bufflen, len);
}

uint8_t PlatformAbstractionLayer::palGetNxpNumValue(const char *name,
                                                    void *pValue,
                                                    unsigned long len) {
  return GetNxpNumValue(name, pValue, len);
}

uint8_t PlatformAbstractionLayer::palEnableDisableDebugLog(uint8_t enable) {
  bool status = setVendorParam(DEBUG_ENABLE_PROP_NAME,
                                                   enable ? "1" : "0");
  status = phNxpExtLog_EnableDisableLogLevel(enable);
  return status;
}

bool PlatformAbstractionLayer::setVendorParam(const std::string& paramKey,
                                                 const std::string& paramValue) {
  bool status = false;
  if (mNxpNfcHal.aidlHalNxpNfc != nullptr) {
    mNxpNfcHal.aidlHalNxpNfc->setVendorParam(paramKey, std::move(paramValue), &status);
  } else if (mNxpNfcHal.halNxpNfc != nullptr) {
    status = mNxpNfcHal.halNxpNfc->setVendorParam(paramKey, std::move(paramValue));
  } else {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "%s Failed to setVendorParam!!! '%s' ", __func__,
                   paramKey.c_str());
  }
  return status;
}

static void HalGetProperty_cb(::android::hardware::hidl_string value) {
  propVal = value;
  if (propVal.size()) {
    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s: received value -> %s",
                   __func__, propVal.c_str());
  } else {
    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s: No Key found in HAL",
                   __func__);
  }
  return;
}

string PlatformAbstractionLayer::getVendorParam(const std::string& paramKey) {
  string paramValue;
  if (mNxpNfcHal.aidlHalNxpNfc != nullptr) {
    mNxpNfcHal.aidlHalNxpNfc->getVendorParam(paramKey, &paramValue);
  } else if (mNxpNfcHal.halNxpNfc != nullptr) {
    mNxpNfcHal.halNxpNfc->getVendorParam(paramKey, HalGetProperty_cb);
    paramValue = propVal;
    propVal.assign("");
  } else {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "%s Failed to getVendorParam!!! '%s' ", __func__,
                   paramKey.c_str());
  }
  return paramValue;
}

void PlatformAbstractionLayer::coverAttached(string state, string type) {
  setVendorParam(COVER_ID_PROP, std::move(type));
  setVendorParam(COVER_STATE_PROP, std::move(state));
}

void PlatformAbstractionLayer::updateHalConfig(
    map<string, ConfigValue>* pConfigMap) {
  mConfigHandler->updateConfig(pConfigMap);
}
