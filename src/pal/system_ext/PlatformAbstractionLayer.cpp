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
#include <ProprietaryExtn.h>
#include <phNxpConfig.h>
#include <phNxpLog.h>

#include <aidl/vendor/nxp/nxpnfc_aidl/INxpNfc.h>
#include <android/binder_manager.h>
#include <vendor/nxp/nxpnfc/2.0/INxpNfc.h>

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
NxpNfcHal mNxpNfcHal;

PlatformAbstractionLayer::PlatformAbstractionLayer() {
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
         move(pBufferCpy), wLength)
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

NFCSTATUS PlatformAbstractionLayer::palenQueueRspNtf(const uint8_t *pBuffer,
                                                     uint16_t wLength) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter wLength:%d", __func__,
                 wLength);
  std::vector<uint8_t> bufferCopy(pBuffer, pBuffer + wLength);
  thread(&PlatformAbstractionLayer::palSendNfcDataCallback, this, wLength,
         bufferCopy.data())
      .detach();
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
  bool status = false;
  if (mNxpNfcHal.aidlHalNxpNfc != nullptr) {
    mNxpNfcHal.aidlHalNxpNfc->setVendorParam(DEBUG_ENABLE_PROP_NAME,
                                             enable ? "1" : "0", &status);
  } else if (mNxpNfcHal.halNxpNfc != nullptr) {
    status = mNxpNfcHal.halNxpNfc->setVendorParam(DEBUG_ENABLE_PROP_NAME,
                                                  enable ? "1" : "0");
  } else {
      NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN, "%s No aidl/hidl hal!", __func__);
  }
  phNxpExtLog_EnableDisableLogLevel(enable);
  return status;
}

void PlatformAbstractionLayer::coverAttached(string state, string type) {
  if (mNxpNfcHal.aidlHalNxpNfc != nullptr) {
    bool status = false;
    mNxpNfcHal.aidlHalNxpNfc->setVendorParam(COVER_ID_PROP, std::move(type),
                                             &status);
    mNxpNfcHal.aidlHalNxpNfc->setVendorParam(COVER_STATE_PROP, std::move(state),
                                             &status);
  } else if (mNxpNfcHal.halNxpNfc != nullptr) {
    mNxpNfcHal.halNxpNfc->setVendorParam(COVER_ID_PROP, std::move(type));
    mNxpNfcHal.halNxpNfc->setVendorParam(COVER_STATE_PROP, std::move(state));
  } else {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Failed to call coverAttached!!!", __func__);
  }
}

