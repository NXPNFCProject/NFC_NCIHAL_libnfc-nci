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
#include "RfStateMonitor.h"

RfStateMonitor *RfStateMonitor::instance = nullptr;

RfStateMonitor::RfStateMonitor() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter", __func__);
  nfcRfState = NfcRfState::IDLE;
}

RfStateMonitor::~RfStateMonitor() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter", __func__);
}

RfStateMonitor *RfStateMonitor::getInstance() {
  if (instance == nullptr) {
    instance = new RfStateMonitor();
  }
  return instance;
}

NFCSTATUS
RfStateMonitor::processRfIntfActdNtf(vector<uint8_t> &rfIntfActdNtf) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "RfStateMonitor %s Enter", __func__);
  if ((rfIntfActdNtf.size() > 15) && (rfIntfActdNtf[4] == T2T_RF_PROTOCOL) &&
      (rfIntfActdNtf[15] == NXP_NON_STD_SAK_VALUE)) {
    /*When RF DISCOVERY NTF contains T2T protocol & 0x13 as SAK value
      then updating to ISO_DEP protocol & 0x53 SAK value respectively*/
    rfIntfActdNtf[4] = ISO_DEP_RF_PROTOCOL;
    rfIntfActdNtf[15] = NXP_STD_SAK_VALUE;
    NXPLOG_EXTNS_I(
        NXPLOG_ITEM_NXP_GEN_EXTN,
        "RfStateMonitor %s: Updated protocol to ISO_DEP & SAK value to 0x53",
        __func__);
  }
  return NFCSTATUS_EXTN_FEATURE_FAILURE;
}

NFCSTATUS RfStateMonitor::processRfDiscRspNtf(vector<uint8_t> &rfResNtf) {

  uint16_t mGidOid = ((rfResNtf[0] << 8) | rfResNtf[1]);
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN,
                 "RfStateMonitor %s: message type: %02x", __func__, mGidOid);
  switch (mGidOid) {
  case NCI_RF_DEACTD_NTF_GID_OID:
    switch (rfResNtf[3]) {
    case static_cast<int>(RfDeactivateType::IDLE):
      updateNfcRfState(NfcRfState::IDLE);
      break;
    case static_cast<int>(RfDeactivateType::SLEEP):
      updateNfcRfState(NfcRfState::LISTEN_SLEEP);
      break;
    case static_cast<int>(RfDeactivateType::DISCOVER):
      updateNfcRfState(NfcRfState::DISCOVER);
      break;
    }
    break;
  case NCI_RF_DEACTD_RSP_GID_OID:
    if (!rfResNtf[3]) {
      updateNfcRfState(NfcRfState::IDLE);
    }
    break;
  case NCI_RF_DISC_RSP_GID_OID:
    if (!rfResNtf[3]) {
      updateNfcRfState(NfcRfState::DISCOVER);
    }
    break;
  default:
    break;
  }

  return NFCSTATUS_EXTN_FEATURE_FAILURE;
}

void RfStateMonitor::updateNfcRfState(NfcRfState state) {
  NfcRfState old_state = nfcRfState;
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN,
                 "RfStateMonitor %s: RF state old:%d new:%d", __func__,
                 old_state, state);
  nfcRfState = state;
}

NfcRfState RfStateMonitor::getNfcRfState() { return nfcRfState; }
