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

#include "QTagHandler.h"
#include "NfcExtensionConstants.h"
#include "PlatformAbstractionLayer.h"
#include "QTag.h"
#include <phNxpLog.h>

QTagHandler::QTagHandler() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter", __func__);
}

void QTagHandler::onFeatureStart() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter mQtagNciPkt.size:%zu",
                 __func__, mQtagNciPkt.size());
  if (mQtagNciPkt.size() >= 3) {
    switch (mQtagNciPkt[NCI_PROP_ACTION_INDEX]) {
    case ENABLE: {
      QTag::getInstance()->enableQTag(ENABLE);
      PlatformAbstractionLayer::getInstance()->palSendNfcDataCallback(
          sizeof(QTAG_NCI_VENDOR_RSP_SUCCESS), QTAG_NCI_VENDOR_RSP_SUCCESS);
      break;
    }
    case DISABLE: {
      QTag::getInstance()->enableQTag(DISABLE);
      PlatformAbstractionLayer::getInstance()->palSendNfcDataCallback(
          sizeof(QTAG_NCI_VENDOR_RSP_SUCCESS), QTAG_NCI_VENDOR_RSP_SUCCESS);
      NfcExtensionController::getInstance()->switchEventHandler(
          HandlerType::DEFAULT);
      break;
    }
    default: {
      PlatformAbstractionLayer::getInstance()->palSendNfcDataCallback(
          sizeof(QTAG_NCI_VENDOR_RSP_FAILURE), QTAG_NCI_VENDOR_RSP_FAILURE);
      NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN, "QTagHandler %s Invalid action.",
                     __func__);
      break;
    }
    }
  } else {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter Invalid NCI packet");
  }
}

void QTagHandler::onFeatureEnd() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "QTagHandler %s Enter", __func__);
  QTag::getInstance()->enableQTag(DISABLE);
  mQtagNciPkt.clear();
}

NFCSTATUS QTagHandler::handleVendorNciMessage(uint16_t dataLen,
                                              const uint8_t *pData) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "QTagHandler %s Enter ", __func__);
  mQtagNciPkt.clear();
  mQtagNciPkt.assign(pData, pData + dataLen);
  NfcExtensionController::getInstance()->switchEventHandler(HandlerType::QTAG);
  return NFCSTATUS_EXTN_FEATURE_SUCCESS;
}

NFCSTATUS QTagHandler::handleVendorNciRspNtf(uint16_t dataLen,
                                             const uint8_t *pData) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "QTagHandler::%s Enter dataLen:%d",
                 __func__, dataLen);
  vector<uint8_t> rfIntfNtf(pData, pData + dataLen);
  return QTag::getInstance()->processIntActivatedNtf(std::move(rfIntfNtf));
}

NFCSTATUS QTagHandler::processExtnWrite(uint16_t *dataLen, uint8_t *pData) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "QTagHandler %s Enter", __func__);
  vector<uint8_t> rfDiscCmd;
  rfDiscCmd.assign(pData, pData + (*dataLen));

  if (QTag::getInstance()->processRfDiscCmd(rfDiscCmd) ==
      NFCSTATUS_EXTN_FEATURE_SUCCESS) {
    if (*dataLen == rfDiscCmd.size()) {
      copy(rfDiscCmd.begin(), rfDiscCmd.end(), pData);
      return NFCSTATUS_EXTN_FEATURE_SUCCESS;
    }
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN, "QTagHandler %s dataLen mismatch",
                   __func__);
    return NFCSTATUS_EXTN_FEATURE_FAILURE;
  }
  return NFCSTATUS_EXTN_FEATURE_FAILURE;
}
