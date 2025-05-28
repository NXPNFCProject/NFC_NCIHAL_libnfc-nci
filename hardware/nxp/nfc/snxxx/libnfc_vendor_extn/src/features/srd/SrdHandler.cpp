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

#include "SrdHandler.h"
#include "NfcExtensionConstants.h"
#include "NfcExtensionController.h"
#include "NfcExtensionWriter.h"
#include "PlatformAbstractionLayer.h"
#include "Srd.h"
#include <phNxpLog.h>

Srd *mSrdMngr = Srd::getInstance();

SrdHandler::SrdHandler() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter", __func__);
}

SrdHandler::~SrdHandler() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter", __func__);
  mSrdMngr->finalize();
}

void SrdHandler::onFeatureStart() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "SrdHandler::%s Enter", __func__);
  mSrdMngr->startSrdSeq();
}

void SrdHandler::onFeatureEnd() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "SrdHandler::%s Enter", __func__);
  NfcExtensionWriter::getInstance()->releaseHALcontrol();
}

NFCSTATUS SrdHandler::handleVendorNciMessage(uint16_t dataLen,
                                             const uint8_t *pData) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "SrdHandler::%s Enter dataLen:%d",
                 __func__, dataLen);
  uint8_t resp[] = {NCI_PROP_RSP_VAL, NCI_ROW_PROP_OID_VAL, PAYLOAD_TWO_LEN,
                    SRD_INIT_MODE, RESPONSE_STATUS_OK};
  HandlerType currentHandleType;
  NFCSTATUS status = NFCSTATUS_EXTN_FEATURE_FAILURE;

  if (pData[SUB_GID_OID_INDEX] == SRD_INIT_MODE) {
    NXPLOG_EXTNS_I(NXPLOG_ITEM_NXP_GEN_EXTN, "SRd Init");
    resp[SUB_GID_OID_RSP_STATUS_INDEX] = mSrdMngr->intSrd();
    PlatformAbstractionLayer::getInstance()->palSendNfcDataCallback(
        sizeof(resp), resp);
    return NFCSTATUS_EXTN_FEATURE_SUCCESS;
  } else if ((pData[SUB_GID_OID_INDEX] == SRD_ENABLE_MODE)) {
    NXPLOG_EXTNS_I(NXPLOG_ITEM_NXP_GEN_EXTN, "Start the SRD Mode");
    resp[3] = SRD_ENABLE_MODE;
    currentHandleType =
        NfcExtensionController::getInstance()->getEventHandlerType();
    if (currentHandleType != HandlerType::SRD) {
      NfcExtensionController::getInstance()->switchEventHandler(
          HandlerType::SRD);
    }
    return NFCSTATUS_EXTN_FEATURE_SUCCESS;
  } else if ((pData[SUB_GID_OID_INDEX] == SRD_DISABLE_MODE)) {
    NXPLOG_EXTNS_I(NXPLOG_ITEM_NXP_GEN_EXTN, "Stop the SRD Mode");
    resp[3] = SRD_DISABLE_MODE;
    currentHandleType =
        NfcExtensionController::getInstance()->getEventHandlerType();
    if (currentHandleType == HandlerType::SRD) {
      mSrdMngr->stopSrd();
    }
    PlatformAbstractionLayer::getInstance()->palSendNfcDataCallback(
        sizeof(resp), resp);
    return NFCSTATUS_EXTN_FEATURE_SUCCESS;
  } else {
    return NFCSTATUS_EXTN_FEATURE_FAILURE;
  }
  return NFCSTATUS_EXTN_FEATURE_FAILURE;
}

NFCSTATUS SrdHandler::handleVendorNciRspNtf(uint16_t dataLen, uint8_t *pData) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN,
                 "SrdHandler::%s "
                 "Enter dataLen:%d",
                 __func__, dataLen);
  // Convert the raw pointer to a vector
  std::vector<uint8_t> pDataVec(pData, pData + dataLen);

  NfcExtensionWriter::getInstance()->stopWriteRspTimer(pData, dataLen);
  return mSrdMngr->processSrdNciRspNtf(pDataVec);
}

void SrdHandler::onWriteComplete(uint8_t status) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter status:%d", __func__,
                 status);
  NfcExtensionWriter::getInstance()->onWriteComplete(status);
}

void SrdHandler::onWriteRspTimeout() {
  NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter ", __func__);
  HandlerType currentHandleType =
      NfcExtensionController::getInstance()->getEventHandlerType();
  if (currentHandleType == HandlerType::SRD) {
    mSrdMngr->stopSrd();
    NfcExtensionController::getInstance()
        ->getCurrentEventHandler()
        ->notifyGenericErrEvt(NCI_UN_RECOVERABLE_ERR);
  }
}

NFCSTATUS SrdHandler::processExtnWrite(uint16_t *dataLen, uint8_t *pData) {
  return mSrdMngr->processPowerLinkCmd(dataLen, pData);
}
