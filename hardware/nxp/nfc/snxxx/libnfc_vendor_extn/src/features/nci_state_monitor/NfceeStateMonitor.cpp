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
#include "NfceeStateMonitor.h"
#include <cstring>
#include <phNxpLog.h>
#include <stdlib.h>

NfceeStateMonitor *NfceeStateMonitor::instance = nullptr;

NfceeStateMonitor::NfceeStateMonitor() {
  currentEE = NCI_ROUTE_HOST;
  eseRecoveryCount = 0;
}

NfceeStateMonitor::~NfceeStateMonitor() {}

NfceeStateMonitor *NfceeStateMonitor::getInstance() {
  if (instance == nullptr) {
    instance = new NfceeStateMonitor();
  }
  return instance;
}

void NfceeStateMonitor::setCurrentEE(uint8_t ee) { currentEE = ee; }

bool NfceeStateMonitor::isEeRecoveryRequired(uint8_t eeErrorCode) {
  switch (eeErrorCode) {
  case RESPONSE_STATUS_FAILED: {
    if (currentEE != NCI_ROUTE_ESE_ID)
      return false;
  }
  case NCI_NFCEE_TRANSMISSION_ERROR: {
    return true;
  }
  default: {
    return false;
  }
  }
}

NFCSTATUS
NfceeStateMonitor::processNfceeModeSetNtf(vector<uint8_t> &nfceeModeSetNtf) {
  NXPLOG_EXTNS_I(NXPLOG_ITEM_NXP_GEN_EXTN,
                 "NfceeStateMonitor %s Enter, eseRecoveryCount: %d", __func__,
                 eseRecoveryCount);
  uint8_t nfceeModeSetNtf_len = nfceeModeSetNtf.size();
  if (nfceeModeSetNtf_len != 0x04) {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN,
      "NfceeStateMonitor %s Nfcee message invalid length", __func__);
    return NFCSTATUS_EXTN_FEATURE_FAILURE;
  }

  uint8_t errorCode = nfceeModeSetNtf[NCI_MODE_SET_NTF_REASON_CODE_INDEX];
  if (isEeRecoveryRequired(errorCode)) {
    if (eseRecoveryCount >= MAX_ESE_RECOVERY_COUNT) {
      nfceeModeSetNtf[nfceeModeSetNtf_len - 1] = RESPONSE_STATUS_FAILED;
      NXPLOG_EXTNS_I(NXPLOG_ITEM_NXP_GEN_EXTN,
                     "NfceeStateMonitor %s Max recovery count reached for 0x%x",
                     __func__, currentEE);
      return NFCSTATUS_EXTN_FEATURE_FAILURE;
    }

    vector<uint8_t> eeUnrecoverabelNtf = {0x62, 0x02, 0x02};
    eseRecoveryCount++;
    nfceeModeSetNtf[nfceeModeSetNtf_len - 1] = RESPONSE_STATUS_OK;
    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "NfceeStateMonitor %s Sending UNRECOVERABLE ERROR for 0x%x",
                   __func__, currentEE);
    eeUnrecoverabelNtf.push_back(currentEE);
    eeUnrecoverabelNtf.push_back(NCI_NFCEE_STS_UNRECOVERABLE_ERROR);
    PlatformAbstractionLayer::getInstance()->palenQueueRspNtf(
        eeUnrecoverabelNtf.data(), eeUnrecoverabelNtf.size());
    return NFCSTATUS_EXTN_FEATURE_FAILURE;
  }
  return NFCSTATUS_EXTN_FEATURE_FAILURE;
}

NFCSTATUS
NfceeStateMonitor::processNfceeStatusNtf(vector<uint8_t> &nfceeStatusNtf) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "NfceeStateMonitor %s Enter", __func__);
  if (nfceeStatusNtf.size() == 5) {
    uint8_t nfcee_id = nfceeStatusNtf[NCI_EE_STATUS_NTF_EE_INDEX];
    uint8_t errorCode = nfceeStatusNtf[NCI_EE_STATUS_NTF_REASON_CODE_INDEX];
    if ((nfcee_id == NCI_ROUTE_ESE_ID) || (nfcee_id == NCI_ROUTE_EUICC1_ID) ||
        (nfcee_id == NCI_ROUTE_EUICC2_ID)) {
      if (errorCode == NCI_NFCEE_STS_PMUVCC_OFF ||
          (errorCode & 0xF0) == NCI_NFCEE_STS_PROP_UNRECOVERABLE_ERROR) {
        nfceeStatusNtf[NCI_EE_STATUS_NTF_REASON_CODE_INDEX] =
            NCI_NFCEE_STS_UNRECOVERABLE_ERROR;
      }
    }
  } else {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "NfceeStateMonitor %s Nfcee message invalid format",
                   __func__);
  }
  return NFCSTATUS_EXTN_FEATURE_FAILURE;
}
