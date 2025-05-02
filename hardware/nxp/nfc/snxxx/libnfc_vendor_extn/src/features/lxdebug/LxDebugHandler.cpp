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

#include "LxDebugHandler.h"
#include "LxDebug.h"
#include "NfcExtensionConstants.h"
#include "NfcExtensionController.h"
#include "NfcExtensionWriter.h"
#include "PlatformAbstractionLayer.h"
#include <phNxpLog.h>

LxDebugHandler::LxDebugHandler() { mLxDebugMngr = LxDebug::getInstance(); }

LxDebugHandler::~LxDebugHandler() { LxDebug::finalize(); }

void LxDebugHandler::onFeatureStart() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "LxDebugHandler::%s Enter",
                 __func__);
}

void LxDebugHandler::onFeatureEnd() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "LxDebugHandler::%s Enter",
                 __func__);
}

NFCSTATUS LxDebugHandler::handleVendorNciMessage(uint16_t dataLen,
                                                 const uint8_t *pData) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN,
                 "LxDebugHandler::%s Enter dataLen:%d", __func__, dataLen);
  HandlerType currentHandleType;
  NFCSTATUS status = NFCSTATUS_EXTN_FEATURE_FAILURE;

  std::vector<uint8_t> response;
  response.push_back(NCI_PROP_RSP_VAL);
  response.push_back(NCI_ROW_PROP_OID_VAL);
  response.push_back(PAYLOAD_TWO_LEN);
  response.push_back(LXDEBUG_SUB_GID);
  response.push_back(RESPONSE_STATUS_OK);

  uint8_t subGid = pData[SUB_GID_OID_INDEX] >> 4;
  if (subGid == LXDEBUG_SUB_GID) {
    uint8_t subOid = pData[SUB_GID_OID_INDEX] & 0x0F;
    status = NFCSTATUS_EXTN_FEATURE_SUCCESS;
    response[SUB_GID_OID_INDEX] = LXDEBUG_SUB_GID << 4 | subOid;
    switch (subOid) {
    case FIELD_DETECT_MODE_SET_SUB_OID: {
      bool mode =
          (pData[SUB_GID_OID_ACTION_INDEX] == EFDM_ENABLED) ? true : false;
      mLxDebugMngr->handleEFDMModeSet(mode);
      currentHandleType =
          NfcExtensionController::getInstance()->getEventHandlerType();
      if (mode && (currentHandleType != HandlerType::LxDebug)) {
        NfcExtensionController::getInstance()->switchEventHandler(
            HandlerType::LxDebug);
      } else if (currentHandleType == HandlerType::LxDebug) {
        NfcExtensionController::getInstance()->switchEventHandler(
            HandlerType::DEFAULT);
      }
      break;
    }
    case IS_FIELD_DETECT_ENABLED_SUB_OID:
      response[NCI_PAYLOAD_LEN_INDEX] += 1;
      response.push_back(mLxDebugMngr->isFieldDetectEnabledFromConfig());
      break;
    case IS_FIELD_DETECT_MODE_STARTED_SUB_OID:
      response[NCI_PAYLOAD_LEN_INDEX] += 1;
      response.push_back(mLxDebugMngr->isEFDMStarted());
      break;
    default:
      response[response.size() - 1] = RESPONSE_STATUS_OPERATION_NOT_SUPPORTED;
      status = NFCSTATUS_EXTN_FEATURE_FAILURE;
      break;
    }
    PlatformAbstractionLayer::getInstance()->palSendNfcDataCallback(
        response.size(), response.data());
  }
  return status;
}

NFCSTATUS LxDebugHandler::handleVendorNciRspNtf(uint16_t dataLen,
                                                uint8_t *pData) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN,
                 "LxDebugHandler::%s "
                 "Enter dataLen:%d",
                 __func__, dataLen);
  return NFCSTATUS_EXTN_FEATURE_FAILURE;
}

NFCSTATUS LxDebugHandler::processExtnWrite(uint16_t dataLen,
                                           const uint8_t *pData) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "LxDebugHandler %s Enter ",
                 __func__);
  NFCSTATUS status = NFCSTATUS_EXTN_FEATURE_FAILURE;

  if (mLxDebugMngr->isEFDMStarted()) {
    vector<uint8_t> rfDiscCmd;
    rfDiscCmd.assign(pData, pData + (dataLen));
    status = mLxDebugMngr->processRfDiscCmd(rfDiscCmd);
    if (status == NFCSTATUS_EXTN_FEATURE_SUCCESS) {
      PlatformAbstractionLayer::getInstance()->palenQueueWrite(
          rfDiscCmd.data(), rfDiscCmd.size());
    }
  }
  return status;
}
