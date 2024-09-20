/**
 *
 *  Copyright 2024 NXP
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
#include "QTag.h"
#include "QTagHandler.h"
#include <cstring>
#include <phNxpLog.h>
#include <stdlib.h>
#include <string.h>

QTag *QTag::instance = nullptr;

QTag::QTag() { mIsQTagEnabled = 0; }

QTag::~QTag() {}

QTag *QTag::getInstance() {
  if (instance == nullptr) {
    instance = new QTag();
  }
  return instance;
}

void QTag::enableQTag(uint8_t flag) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter flag:%d", __func__, flag);
  mIsQTagEnabled = flag;
}

uint8_t QTag::isQTagEnabled() { return mIsQTagEnabled; }

bool QTag::isQTagRfIntfNtf(vector<uint8_t> rfIntfNtf) {
  if ((rfIntfNtf.size() > NCI_LEN_RF_CMD) && isQTagEnabled() &&
      rfIntfNtf[NCI_MSG_TYPE_INDEX] == NCI_RF_INTF_ACT_NTF_GID_VAL &&
      rfIntfNtf[NCI_OID_TYPE_INDEX] == NCI_RF_INTF_ACT_NTF_OID_VAL &&
      rfIntfNtf[NCI_RF_INTF_ACT_TECH_TYPE_INDEX] == NCI_TECH_Q_POLL_VAL)
    return true;
  return false;
}

NFCSTATUS QTag::processIntActivatedNtf(vector<uint8_t> rfIntfNtf) {
  if (!QTag::getInstance()->isQTagRfIntfNtf(rfIntfNtf)) {
    return NFCSTATUS_EXTN_FEATURE_FAILURE;
  }

  uint16_t ntfLen = rfIntfNtf.size();
  uint8_t p_ntf[ntfLen];
  copy(rfIntfNtf.begin(), rfIntfNtf.end(), p_ntf);
  p_ntf[NCI_RF_INTF_ACT_TECH_TYPE_INDEX] = NCI_TECH_A_POLL_VAL;

  PlatformAbstractionLayer::getInstance()->palSendNfcDataCallback(ntfLen,
                                                                  p_ntf);
  return NFCSTATUS_EXTN_FEATURE_SUCCESS;
}

NFCSTATUS QTag::processRfDiscCmd(vector<uint8_t> &rfDiscCmd) {
  if ((QTag::getInstance()->isQTagEnabled()) &&
      (rfDiscCmd.size() == NCI_LEN_RF_CMD) &&
      (rfDiscCmd[NCI_MSG_TYPE_INDEX] == NCI_RF_DISC_GID_VAL) &&
      (rfDiscCmd[NCI_OID_TYPE_INDEX] == NCI_RF_DISC_OID_VAL) &&
      (rfDiscCmd[NCI_RF_DISC_TECH_TYPE_INDEX] == NCI_TECH_A_POLL_VAL)) {
    rfDiscCmd[NCI_RF_DISC_TECH_TYPE_INDEX] = NCI_TECH_Q_POLL_VAL;
    return NFCSTATUS_EXTN_FEATURE_SUCCESS;
  }
  return NFCSTATUS_EXTN_FEATURE_FAILURE;
}
