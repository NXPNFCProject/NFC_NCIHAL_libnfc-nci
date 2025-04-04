/**
 *
 *  Copyright 2025 NXP
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

#include "TransitConfigHandler.h"
#include "NfcExtensionConstants.h"
#include "NfcExtensionController.h"
#include "NfcExtensionWriter.h"
#include "PlatformAbstractionLayer.h"
#include "RfConfigManager.h"
#include <android-base/file.h>
#include <fstream>
#include <phNxpLog.h>

using android::base::WriteStringToFile;

TransitConfigHandler *TransitConfigHandler::instance = nullptr;

TransitConfigHandler::TransitConfigHandler() {
  NXPLOG_EXTNS_D("%s Enter", __func__);
}

TransitConfigHandler::~TransitConfigHandler() {}

TransitConfigHandler *TransitConfigHandler::getInstance() {
  if (instance == nullptr) {
    instance = new TransitConfigHandler();
  }
  return instance;
}

void TransitConfigHandler::onFeatureStart() {
  NXPLOG_EXTNS_I(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter mConfigPkt.size:%zu",
                 __func__, mConfigPkt.size());
}

void TransitConfigHandler::onFeatureEnd() {
  NXPLOG_EXTNS_I(NXPLOG_ITEM_NXP_GEN_EXTN, "TransitConfigHandler %s Enter",
                 __func__);
  mConfigPkt.clear();
}

bool TransitConfigHandler::updateVendorConfig(vector<uint8_t> mConfigPkt) {
  std::string vendorNciConfigPath = "/data/vendor/nfc/libnfc-nci-update.conf";
  bool status = false;
  if (mConfigPkt[4] > 0) {
    vector<uint8_t> configVect(mConfigPkt.begin() + 5, mConfigPkt.end());
    std::string configStr(configVect.begin(), configVect.end());

    if (WriteStringToFile(configStr, vendorNciConfigPath)) {
      status = true;
      NXPLOG_EXTNS_I(NXPLOG_ITEM_NXP_GEN_EXTN,
                     "TransitConfigHandler::%s libnfc-nci-update.conf updated",
                     __func__);
    } else {
      NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN,
                     "TransitConfigHandler::%s Error while writing to %s",
                     __func__, vendorNciConfigPath.c_str());
    }
  } else {
    if (!remove(vendorNciConfigPath.c_str())) {
      status = true;
      NXPLOG_EXTNS_I(NXPLOG_ITEM_NXP_GEN_EXTN,
                     "TransitConfigHandler::%s libnfc-nci-update.conf removed",
                     __func__);
    } else {
      NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN,
                     "TransitConfigHandler::%s Error while removing %s",
                     __func__, vendorNciConfigPath.c_str());
    }
  }
  return status;
}

NFCSTATUS TransitConfigHandler::handleVendorNciMessage(uint16_t dataLen,
                                                       const uint8_t *pData) {
  mConfigPkt.clear();
  mConfigPkt.assign(pData, pData + dataLen);
  uint8_t subOid = pData[SUB_GID_OID_INDEX] & 0x0F;
  NfcExtensionController::getInstance()->switchEventHandler(
      HandlerType::TRANSIT);

  static constexpr uint8_t TRANSIT_OID = 0x01;
  static constexpr uint8_t RF_REGISTER_OID = 0x02;
  static constexpr uint8_t TRANSIT_NCI_VENDOR_RSP_SUCCESS[] = {
      NCI_PROP_RSP_VAL, NCI_ROW_PROP_OID_VAL, PAYLOAD_TWO_LEN,
      TRANSIT_SUB_GIDOID, RESPONSE_STATUS_OK};
  static constexpr uint8_t TRANSIT_NCI_VENDOR_RSP_FAILURE[] = {
      NCI_PROP_RSP_VAL, NCI_ROW_PROP_OID_VAL, PAYLOAD_TWO_LEN,
      TRANSIT_SUB_GIDOID, RESPONSE_STATUS_FAILED};
  static constexpr uint8_t RF_REGISTER_NCI_VENDOR_RSP_SUCCESS[] = {
      NCI_PROP_RSP_VAL, NCI_ROW_PROP_OID_VAL, PAYLOAD_TWO_LEN,
      RF_REGISTER_SUB_GIDOID, RESPONSE_STATUS_OK};
  static constexpr uint8_t RF_REGISTER_NCI_VENDOR_RSP_FAILURE[] = {
      NCI_PROP_RSP_VAL, NCI_ROW_PROP_OID_VAL, PAYLOAD_TWO_LEN,
      RF_REGISTER_SUB_GIDOID, RESPONSE_STATUS_FAILED};

  if (subOid == TRANSIT_OID) {
    if (updateVendorConfig(mConfigPkt)) {
      PlatformAbstractionLayer::getInstance()->palSendNfcDataCallback(
          sizeof(TRANSIT_NCI_VENDOR_RSP_SUCCESS),
          TRANSIT_NCI_VENDOR_RSP_SUCCESS);
    } else {
      PlatformAbstractionLayer::getInstance()->palSendNfcDataCallback(
          sizeof(TRANSIT_NCI_VENDOR_RSP_FAILURE),
          TRANSIT_NCI_VENDOR_RSP_FAILURE);
    }
  } else if (subOid == RF_REGISTER_OID) {
    if (RfConfigManager::getInstance()->checkUpdateRfRegisterConfig(
            mConfigPkt)) {
      PlatformAbstractionLayer::getInstance()->palSendNfcDataCallback(
          sizeof(RF_REGISTER_NCI_VENDOR_RSP_SUCCESS),
          RF_REGISTER_NCI_VENDOR_RSP_SUCCESS);
    } else {
      PlatformAbstractionLayer::getInstance()->palSendNfcDataCallback(
          sizeof(RF_REGISTER_NCI_VENDOR_RSP_FAILURE),
          RF_REGISTER_NCI_VENDOR_RSP_FAILURE);
    }
  }
  return NFCSTATUS_EXTN_FEATURE_SUCCESS;
}

NFCSTATUS TransitConfigHandler::handleVendorNciRspNtf(uint16_t dataLen,
                                                      uint8_t *pData) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN,
                 "TransitConfigHandler::%s Enter dataLen:%d", __func__,
                 dataLen);
  std::vector<uint8_t> pRspVec(pData, pData + dataLen);
  return RfConfigManager::getInstance()->processRsp(pRspVec);
}
