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

RfStateMonitor::RfStateMonitor() {}

RfStateMonitor::~RfStateMonitor() {}

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
    NXPLOG_EXTNS_D(
        NXPLOG_ITEM_NXP_GEN_EXTN,
        "RfStateMonitor %s: Updated protocol to ISO_DEP & SAK value to 0x53",
        __func__);
  }
  return NFCSTATUS_EXTN_FEATURE_FAILURE;
}
