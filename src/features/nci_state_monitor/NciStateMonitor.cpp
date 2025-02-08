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
#include "NfceeStateMonitor.h"
#include <phNxpLog.h>
#include <stdio.h>

NciStateMonitor *NciStateMonitor::instance = nullptr;

NciStateMonitor::NciStateMonitor() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter", __func__);
}

NciStateMonitor::~NciStateMonitor() {}

NciStateMonitor *NciStateMonitor::getInstance() {
  if (instance == nullptr) {
    instance = new NciStateMonitor();
  }
  return instance;
}

NFCSTATUS NciStateMonitor::handleVendorNciMessage(uint16_t dataLen,
                                               const uint8_t *pData) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "NciStateMonitor %s Enter dataLen:%d",
                 __func__, dataLen);
  return NFCSTATUS_EXTN_FEATURE_FAILURE;
}

NFCSTATUS NciStateMonitor::handleVendorNciRspNtf(uint16_t dataLen,
                                              uint8_t *pData) {
  vector<uint8_t> nciMsg(pData, pData + dataLen);
  uint16_t mGidOid = ((nciMsg[0] << 8) | nciMsg[1]);
  NFCSTATUS status = NFCSTATUS_EXTN_FEATURE_FAILURE;
  constexpr uint16_t NCI_EE_STATUS_NTF_GID_OID =
      (((NCI_MT_NTF | NCI_GID_EE_MANAGE) << 8) | NCI_EE_STATUS_OID);
  constexpr uint16_t NCI_EE_MODE_SET_NTF_GID_OID =
      (((NCI_MT_NTF | NCI_GID_EE_MANAGE) << 8) | NCI_EE_MODE_SET_OID);
  switch (mGidOid) {
  case NCI_EE_STATUS_NTF_GID_OID: {
    status = NfceeStateMonitor::getInstance()->processNfceeStatusNtf(nciMsg);
  }
  case NCI_EE_MODE_SET_NTF_GID_OID: {
    status = NfceeStateMonitor::getInstance()->processNfceeModeSetNtf(nciMsg);
    break;
  }
  default: {
    return status;
  }
  }
  if (status == NFCSTATUS_EXTN_FEATURE_SUCCESS) {
    if (nciMsg.size() == dataLen) {
      copy(nciMsg.begin(), nciMsg.end(), pData);
    } else {
      status = NFCSTATUS_EXTN_FEATURE_FAILURE;
      NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN,
                     "NciStateMonitor %s dataLen mismatch", __func__);
    }
  }
  return status;
}

NFCSTATUS NciStateMonitor::processExtnWrite(uint16_t dataLen, const uint8_t *pData) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "NciStateMonitor %s Enter", __func__);
  vector<uint8_t> nciCmd;
  constexpr uint16_t NCI_EE_MODE_SET_CMD_GID_OID =
      (((NCI_MT_CMD | NCI_GID_EE_MANAGE) << 8) | NCI_EE_MODE_SET_OID);
  nciCmd.assign(pData, pData + (dataLen));
  uint16_t mGidOid = ((nciCmd[0] << 8) | nciCmd[1]);
  if ((mGidOid == NCI_EE_MODE_SET_CMD_GID_OID) &&
      (nciCmd.size() > NCI_MODE_SET_CMD_EE_INDEX)) {
    NfceeStateMonitor::getInstance()->setCurrentEE(
        nciCmd[NCI_MODE_SET_CMD_EE_INDEX]);
  }
  return NFCSTATUS_EXTN_FEATURE_FAILURE;
}
