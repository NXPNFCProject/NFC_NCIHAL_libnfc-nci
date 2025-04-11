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

#include "NciStateMonitor.h"
#include <NfcFirmwareInfo.h>
#include <phNxpLog.h>

NfcFirmwareInfo *NfcFirmwareInfo::instance = nullptr;

NfcFirmwareInfo::NfcFirmwareInfo() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter", __func__);
}

NfcFirmwareInfo *NfcFirmwareInfo::getInstance() {
  if (instance == nullptr) {
    instance = new NfcFirmwareInfo();
  }
  return instance;
}

void NfcFirmwareInfo::fetchandSendFwVersionNtf() {
  vector<uint8_t> FW_VER_NCI_VENDOR_RSP = {
      NCI_PROP_RSP_VAL, NCI_ROW_PROP_OID_VAL, PAYLOAD_TWO_LEN,
      FW_VERSION_SUB_GIDOID, RESPONSE_STATUS_OK};
  uint8_t FwVerNciVendorRspLen = FW_VER_NCI_VENDOR_RSP.size();

  vector<uint8_t> fwVersion = NciStateMonitor::getInstance()->getFwVersion();

  if (fwVersion.size() == 0) {
    FW_VER_NCI_VENDOR_RSP[FwVerNciVendorRspLen - 1] = RESPONSE_STATUS_FAILED;
  } else {
    FW_VER_NCI_VENDOR_RSP[2] += fwVersion.size();
    for (size_t i = 0; i < fwVersion.size(); i++) {
      FW_VER_NCI_VENDOR_RSP.push_back(fwVersion[i] & 0xFF);
    }
  }
  PlatformAbstractionLayer::getInstance()->palSendNfcDataCallback(
      FW_VER_NCI_VENDOR_RSP.size(), FW_VER_NCI_VENDOR_RSP.data());
}

NFCSTATUS NfcFirmwareInfo::handleVendorNciMessage(uint16_t dataLen,
                                            const uint8_t *pData) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "NfcFirmwareInfo %s Enter dataLen:%d",
                 __func__, dataLen);
  static constexpr uint8_t FW_VERSION_OID = 0x0F;
  uint8_t subOid = pData[SUB_GID_OID_INDEX] & 0x0F;

  if (subOid == FW_VERSION_OID) {
    fetchandSendFwVersionNtf();
    return NFCSTATUS_EXTN_FEATURE_SUCCESS;
  }
  return NFCSTATUS_EXTN_FEATURE_FAILURE;
}

NFCSTATUS NfcFirmwareInfo::handleVendorNciRspNtf(uint16_t dataLen, uint8_t *pData) {
  return NFCSTATUS_EXTN_FEATURE_FAILURE;
}
