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

enum class OemRouteLocation {
  HOST = 0x00,
  ESE = 0x01,
  UICC = 0x02,
  UICC2 = 0x03,
  EUICC = 0x04,
  EUICC2 = 0x05
};

enum class GenRouteLocation {
  HOST = 0,
  ESE = 192,
  UICC = 128,
  UICC2 = 129,
  EUICC = 193,
  EUICC2 = 194
};

map<OemRouteLocation, uint8_t> routeMap;

struct NxpNfcHal {
  sp<INxpNfc> halNxpNfc;
  shared_ptr<INxpNfcAidl> aidlHalNxpNfc;
};

string DEBUG_ENABLE_PROP_NAME = "nfc.debug_enabled";
string NXPNFC_AIDL_HAL_SERVICE_NAME = "vendor.nxp.nxpnfc_aidl.INxpNfc/default";

PlatformAbstractionLayer *PlatformAbstractionLayer::sPlatformAbstractionLayer;
NxpNfcHal mNxpNfcHal;

void initRouteMap() {
  routeMap[OemRouteLocation::HOST] = static_cast<uint8_t> (GenRouteLocation::HOST);
  routeMap[OemRouteLocation::ESE] = static_cast<uint8_t> (GenRouteLocation::ESE);
  routeMap[OemRouteLocation::UICC] = static_cast<uint8_t> (GenRouteLocation::UICC);
  routeMap[OemRouteLocation::UICC2] = static_cast<uint8_t> (GenRouteLocation::UICC2);
  routeMap[OemRouteLocation::EUICC] = static_cast<uint8_t> (GenRouteLocation::EUICC);
  routeMap[OemRouteLocation::EUICC2] = static_cast<uint8_t> (GenRouteLocation::EUICC2);
}

PlatformAbstractionLayer::PlatformAbstractionLayer() {
  initRouteMap();
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

bool PlatformAbstractionLayer::setVendorParam(string key, string value) {
  bool status = false;
  if (mNxpNfcHal.aidlHalNxpNfc != nullptr) {
    mNxpNfcHal.aidlHalNxpNfc->setVendorParam(key, std::move(value), &status);
  } else if (mNxpNfcHal.halNxpNfc != nullptr) {
    status = mNxpNfcHal.halNxpNfc->setVendorParam(key, std::move(value));
  } else {
      NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN, "%s No aidl/hidl hal!", __func__);
  }
  return status;
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

OemRouteLocation getOemRouteLocation(int value) {
  switch (value) {
  case 0:
    return OemRouteLocation::HOST;
  case 1:
    return OemRouteLocation::ESE;
  case 2:
    return OemRouteLocation::UICC;
  case 3:
    return OemRouteLocation::UICC2;
  case 4:
    return OemRouteLocation::EUICC;
  case 5:
    return OemRouteLocation::EUICC2;
  default:
    return OemRouteLocation::HOST;
  }
}

uint8_t getUnsignedRouteValue(string key, uint8_t value) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s: key:%s,value:%d", __func__,
                 key.c_str(), value);
  if (key == NAME_DEFAULT_ROUTE || key == NAME_DEFAULT_AID_ROUTE ||
      key == NAME_DEFAULT_ISODEP_ROUTE ||
      key == NAME_DEFAULT_FELICA_CLT_ROUTE ||
      key == NAME_DEFAULT_SYS_CODE_ROUTE || key == NAME_DEFAULT_OFFHOST_ROUTE) {
    OemRouteLocation oemRouteLocation = getOemRouteLocation(value);
    auto it = routeMap.find(static_cast<OemRouteLocation>(oemRouteLocation));
    if (it != routeMap.end()) {
      NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN,
                     "%s: DEFAULT_OFFHOST_ROUTE static cast value:%d", __func__,
                     static_cast<uint8_t>(it->second));
      return static_cast<uint8_t>(it->second);
    } else {
      NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN,
                     "%s key not found, returned error!", __func__);
      return 0;
    }
  } else {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN, "%s No need of conversion!",
                   __func__);
    return -1;
  }
}

void updateConfigInternal(string oldKey, string newKey,
                          map<string, ConfigValue>* pConfigMap) {
  auto oldPair = pConfigMap->find(oldKey);
  if (oldPair != pConfigMap->end()) {
    auto newPair = pConfigMap->find(newKey);
    ConfigValue newConfigVal;
    // new key already exists in map
    if (newPair != pConfigMap->end()) {
      switch (newPair->second.getType()) {
        // if existing value is single UNSIGNED, Create a vector
        // to hold multiple values for single key
        case ConfigValue::UNSIGNED: {
          /* code */
          uint8_t newVal = newPair->second.getUnsigned();
          uint8_t tempVal = getUnsignedRouteValue(oldKey, newVal);
          if (-1 != tempVal) {
            newVal = tempVal;
          } else {
            NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN,
                          "%s: key: %s value:%d", __func__, newKey.c_str(), newVal);
          }
          newConfigVal = ConfigValue(static_cast<unsigned>(newVal));
          break;
        }
        // if existing value is in multiple in number i.e, vector
        // add newValue to the vector
        case ConfigValue::BYTES: {
          // prev new value is array just do push_back
          vector<uint8_t> oldVal = newPair->second.getBytes();
          oldVal.push_back(oldPair->second.getUnsigned());
          newConfigVal = ConfigValue(oldVal);
        } break;
        default:
          NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN,
                         "%s %s ConfigValue Type (%d) Not handled!", __func__,
                         oldKey.c_str(), newPair->second.getType());
          return;
      }
      // update the value
      newPair->second = std::move(newConfigVal);
    } else {
      // Adding value for new key in config map.
      pConfigMap->emplace(newKey, oldPair->second);
    }
  } else {
    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s %s Not found!", __func__,
                   oldKey.c_str());
  }
}

void PlatformAbstractionLayer::updateHalConfig(
    map<string, ConfigValue>* pConfigMap) {
  if (pConfigMap != nullptr) {
    // TODO: Read DEFAULT_FELICA_CLT_ROUTE value and update the
    // NAME_DEFAULT_NFCF_ROUTE value dynamically.
    // DEFAULT_FELICA_CLT_ROUTE to be updated as well
    pConfigMap->erase(NAME_DEFAULT_NFCF_ROUTE);
    pConfigMap->emplace(NAME_DEFAULT_NFCF_ROUTE, ConfigValue(192));
    updateConfigInternal(NAME_NXP_T4T_NFCEE_ENABLE, NAME_T4T_NFCEE_ENABLE,
                         pConfigMap);
    //updateConfigInternal(NAME_OFF_HOST_SIM_PIPE_ID, NAME_OFF_HOST_SIM_PIPE_IDS,
    //                     pConfigMap);
    //pConfigMap->erase(NAME_OFF_HOST_SIM_PIPE_ID);
    //updateConfigInternal(NAME_OFF_HOST_SIM2_PIPE_ID, NAME_OFF_HOST_SIM_PIPE_IDS,
    //                     pConfigMap);
    //pConfigMap->erase(NAME_OFF_HOST_SIM2_PIPE_ID);
    updateConfigInternal(NAME_DEFAULT_ROUTE, NAME_DEFAULT_ROUTE, pConfigMap);
    updateConfigInternal(NAME_DEFAULT_ISODEP_ROUTE, NAME_DEFAULT_ISODEP_ROUTE, pConfigMap);
    updateConfigInternal(NAME_DEFAULT_OFFHOST_ROUTE, NAME_DEFAULT_OFFHOST_ROUTE, pConfigMap);
    updateConfigInternal(NAME_DEFAULT_FELICA_CLT_ROUTE, NAME_DEFAULT_NFCF_ROUTE, pConfigMap);
    updateConfigInternal(NAME_DEFAULT_SYS_CODE_ROUTE, NAME_DEFAULT_SYS_CODE_ROUTE, pConfigMap);

    auto t3tPair = pConfigMap->find(NAME_NFA_PROPRIETARY_CFG);
    if (t3tPair != pConfigMap->end()) {
      vector<uint8_t> oldVal = t3tPair->second.getBytes();
      //As per .conf byte[9] is T3T ID
      oldVal.push_back(0x8B);
      t3tPair->second = ConfigValue(oldVal);
    }
    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s mHostSimPipId erased & Added!", __func__);
    pConfigMap->erase(NAME_OFF_HOST_SIM_PIPE_IDS);
    vector<uint8_t> offHostValues = {0x0A, 0x23, 0x2B, 0x2F};
    pConfigMap->emplace(NAME_OFF_HOST_SIM_PIPE_IDS, ConfigValue(offHostValues));
    pConfigMap->erase(NAME_T4T_NFCEE_ENABLE);
    pConfigMap->emplace(NAME_T4T_NFCEE_ENABLE, ConfigValue(1));
    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s NAME_T4T_NFCEE_ENABLE(1) erased & Added!", __func__);
  }
}

