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
#include "QTag.h"
#include "QTagHandler.h"
#include <cstring>
#include <phNxpLog.h>
#include <stdlib.h>
#include <string.h>

QTag *QTag::instance = nullptr;

QTag::QTag() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter", __func__);
  mIsQTagEnabled = 0;
}

QTag::~QTag() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter", __func__);
}

QTag *QTag::getInstance() {
  if (instance == nullptr) {
    instance = new QTag();
  }
  return instance;
}

void QTag::enableQTag(uint8_t flag) {
  NXPLOG_EXTNS_I(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter flag:%d", __func__, flag);
  mIsQTagEnabled = flag;
}

uint8_t QTag::getQTagStatus() { return mIsQTagEnabled; }

bool QTag::isQTagRfIntfNtf(vector<uint8_t> rfIntfNtf) {
  uint16_t mGidOid = ((rfIntfNtf[0] << 8) | rfIntfNtf[1]);
  if ((rfIntfNtf.size() > NCI_LEN_RF_CMD) &&
      (mGidOid == NCI_RF_INTF_ACTD_NTF_GID_OID)) {
    if ((getQTagStatus() != QTAG_DISABLE_OID) &&
        (rfIntfNtf[NCI_RF_INTF_ACT_TECH_TYPE_INDEX] == NCI_TECH_Q_POLL_VAL)) {
      return true;
    }
    PlatformAbstractionLayer::getInstance()->palSendNfcDataCallback(
        sizeof(QTAG_DETECTION_NTF_FAILURE), QTAG_DETECTION_NTF_FAILURE);
  }
  return false;
}

bool QTag::checkDiscCmd(vector<uint8_t> rfDiscCmd) {
  for (size_t i = 0; i < rfDiscCmd.size(); i++) {
    if ((rfDiscCmd[i] == NCI_TYPE_A_LISTEN) ||
        (rfDiscCmd[i] == NCI_TYPE_B_LISTEN) ||
        (rfDiscCmd[i] == NCI_TYPE_F_LISTEN)) {
      return false;
    }
  }
  return true;
}

NFCSTATUS QTag::processIntActivatedNtf(vector<uint8_t> rfIntfNtf) {
  if (!QTag::getInstance()->isQTagRfIntfNtf(rfIntfNtf)) {
    return NFCSTATUS_EXTN_FEATURE_FAILURE;
  }

  rfIntfNtf[NCI_RF_INTF_ACT_TECH_TYPE_INDEX] = NCI_TECH_A_POLL_VAL;
  PlatformAbstractionLayer::getInstance()->palSendNfcDataCallback(
      sizeof(QTAG_DETECTION_NTF_SUCCESS), QTAG_DETECTION_NTF_SUCCESS);
  PlatformAbstractionLayer::getInstance()->palSendNfcDataCallback(
      rfIntfNtf.size(), rfIntfNtf.data());
  return NFCSTATUS_EXTN_FEATURE_SUCCESS;
}

NFCSTATUS QTag::processRfDiscCmd(vector<uint8_t> &rfDiscCmd) {
  vector<uint8_t> QTAG_RF_DISC_CMD = {0x21, 0x03, 0x03, 0x01, 0x71, 0x01};
  uint8_t NCI_QTAG_PAYLOAD_LEN = 2;
  uint8_t NCI_RF_DISC_PAYLOAD_LEN_INDEX = 2;
  uint8_t NCI_RF_DISC_NUM_OF_CONFIG_INDEX = 3;
  uint16_t mGidOid = ((rfDiscCmd[0] << 8) | rfDiscCmd[1]);

  if (mGidOid == NCI_RF_DISC_CMD_GID_OID) {
    if (checkDiscCmd(rfDiscCmd)) {
      if (QTag::getInstance()->getQTagStatus() == QTAG_ENABLE_OID) {
        if (QTag::getInstance()->isObserveModeEnabled(rfDiscCmd)) {
          QTAG_RF_DISC_CMD[NCI_RF_DISC_NUM_OF_CONFIG_INDEX]++;
          QTAG_RF_DISC_CMD[NCI_RF_DISC_PAYLOAD_LEN_INDEX] +=
              NCI_QTAG_PAYLOAD_LEN;
          QTAG_RF_DISC_CMD.push_back(0xFF);
          QTAG_RF_DISC_CMD.push_back(0x01);
        }

        rfDiscCmd.assign(QTAG_RF_DISC_CMD.begin(), QTAG_RF_DISC_CMD.end());
        return NFCSTATUS_EXTN_FEATURE_SUCCESS;
      } else if (QTag::getInstance()->getQTagStatus() == QTAG_APPEND_OID) {
        rfDiscCmd[NCI_RF_DISC_NUM_OF_CONFIG_INDEX]++;
        rfDiscCmd[NCI_RF_DISC_PAYLOAD_LEN_INDEX] += NCI_QTAG_PAYLOAD_LEN;
        rfDiscCmd.push_back(NCI_TECH_Q_POLL_VAL);
        rfDiscCmd.push_back(QTAG_ENABLE_OID);
        return NFCSTATUS_EXTN_FEATURE_SUCCESS;
      }
    } else {
      enableQTag(QTAG_DISABLE_OID);
      NXPLOG_EXTNS_I(NXPLOG_ITEM_NXP_GEN_EXTN,
                     "%s Listen tech also enabled, disabling QPoll", __func__);
      PlatformAbstractionLayer::getInstance()->palSendNfcDataCallback(
          sizeof(QTAG_NCI_VENDOR_NTF_FAILURE), QTAG_NCI_VENDOR_NTF_FAILURE);
      return NFCSTATUS_EXTN_FEATURE_FAILURE;
    }
    return NFCSTATUS_EXTN_FEATURE_FAILURE;
  }
  return NFCSTATUS_EXTN_FEATURE_FAILURE;
}

bool QTag::isObserveModeEnabled(vector<uint8_t> rfDiscCmd) {
  return rfDiscCmd.size() > 2 && rfDiscCmd[rfDiscCmd.size() - 2] == 0xFF &&
         rfDiscCmd[rfDiscCmd.size() - 1] == 0x01;
}
