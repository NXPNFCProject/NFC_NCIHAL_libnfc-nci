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

bool TransitConfigHandler::updateVendorConfig(vector<uint8_t> configCmd) {
  std::string vendorNciConfigPath = "/data/vendor/nfc/libnfc-nci-update.conf";
  bool status = false;
  if (configCmd[4] > 0) {
    vector<uint8_t> configVect(configCmd.begin() + 5, configCmd.end());
    std::string configStr(configVect.begin(), configVect.end());

    if (WriteStringToFile(configStr, vendorNciConfigPath)) {
      status = true;
      NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN,
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
      NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN,
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
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN,
                 "TransitConfigHandler::%s Enter dataLen:%d", __func__,
                 dataLen);
  uint8_t subOid = pData[SUB_GID_OID_INDEX] & 0x0F;
  vector<uint8_t> configCmd;
  configCmd.assign(pData, pData + (dataLen));

  if (subOid == TRANSIT_OID) {
    if (updateVendorConfig(configCmd)) {
      PlatformAbstractionLayer::getInstance()->palSendNfcDataCallback(
          sizeof(TRANSIT_NCI_VENDOR_RSP_SUCCESS),
          TRANSIT_NCI_VENDOR_RSP_SUCCESS);
    } else {
      PlatformAbstractionLayer::getInstance()->palSendNfcDataCallback(
          sizeof(TRANSIT_NCI_VENDOR_RSP_FAILURE),
          TRANSIT_NCI_VENDOR_RSP_FAILURE);
    }
  }
  return NFCSTATUS_EXTN_FEATURE_SUCCESS;
}

NFCSTATUS TransitConfigHandler::handleVendorNciRspNtf(uint16_t dataLen,
                                                      uint8_t *pData) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN,
                 "TransitConfigHandler::%s Enter dataLen:%d", __func__,
                 dataLen);
  return NFCSTATUS_EXTN_FEATURE_FAILURE;
}
