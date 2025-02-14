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

#include "MposHandler.h"
#include "Mpos.h"
#include "NfcExtensionConstants.h"
#include "NfcExtensionController.h"
#include "NfcExtensionWriter.h"
#include "PlatformAbstractionLayer.h"
#include <phNxpLog.h>

Mpos *mPosMngr = Mpos::getInstance();

MposHandler::MposHandler() { NXPLOG_EXTNS_D("%s Enter", __func__); }

static void CreateAndReadTimerValFromConfig() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter", __func__);
  unsigned long scrTimerVal = 0;

  if (PlatformAbstractionLayer::getInstance()->palGetNxpNumValue(
          NAME_NXP_SWP_RD_TAG_OP_TIMEOUT, &scrTimerVal, sizeof(scrTimerVal))) {
    mPosMngr->tagOperationTimeout = scrTimerVal;
  } else {
    mPosMngr->tagOperationTimeout = READER_MODE_TAG_OP_DEFAULT_TIMEOUT_IN_MS;
  }

  scrTimerVal = 0;
  if (PlatformAbstractionLayer::getInstance()->palGetNxpNumValue(
          NAME_NXP_DM_DISC_NTF_TIMEOUT, &scrTimerVal, sizeof(scrTimerVal))) {
    mPosMngr->mTimeoutMaxCount = scrTimerVal;
  } else {
    mPosMngr->mTimeoutMaxCount = READER_MODE_DISC_NTF_DEFAULT_TIMEOUT_IN_MS;
  }

  scrTimerVal = 0;
  if (PlatformAbstractionLayer::getInstance()->palGetNxpNumValue(
          NAME_NXP_RDR_REQ_GUARD_TIME, &scrTimerVal, sizeof(scrTimerVal))) {
    mPosMngr->RdrReqGuardTimer = scrTimerVal;
  } else {
    mPosMngr->RdrReqGuardTimer = 0x00;
  }
}

void MposHandler::onFeatureStart() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "MposHandler::%s Enter", __func__);
  mPosMngr->updateState(MPOS_STATE_START_IN_PROGRESS);
  CreateAndReadTimerValFromConfig();
}

void MposHandler::handleHalEvent(uint8_t errorCode) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter errorCode:%d", __func__,
                 errorCode);
}

void MposHandler::onFeatureEnd() {
  mPosMngr->isTimerStarted = false;
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "MposHandler::%s Enter", __func__);
  mPosMngr->updateState(MPOS_STATE_IDLE);
  NfcExtensionWriter::getInstance().releaseHALcontrol();
  mPosMngr->tagOperationTimer.kill();
  mPosMngr->tagRemovalTimer.kill();
  mPosMngr->startStopGuardTimer.kill();
}

NFCSTATUS MposHandler::handleVendorNciMessage(uint16_t dataLen,
                                              const uint8_t *pData) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "MposHandler::%s Enter dataLen:%d",
                 __func__, dataLen);
  uint8_t resp[] = {0x4F, 0x3E, 0x02, 0x51, 00};
  HandlerType currentHandleType;
  NFCSTATUS status = NFCSTATUS_EXTN_FEATURE_FAILURE;

  if ((pData[SUB_GID_OID_INDEX] == SE_READER_DEDICATED_MODE_ID) &&
      (pData[SUB_GID_OID_ACTION_INDEX] == SE_READER_DEDICATED_MODE_ON)) {
    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "Start the SE Reader mode");
    currentHandleType =
        NfcExtensionController::getInstance()->getEventHandlerType();
    if (currentHandleType != HandlerType::MPOS) {
      NfcExtensionController::getInstance()->switchEventHandler(
          HandlerType::MPOS);
    }
    PlatformAbstractionLayer::getInstance()->palSendNfcDataCallback(
        sizeof(resp), resp);
    return NFCSTATUS_EXTN_FEATURE_SUCCESS;
  } else if ((pData[SUB_GID_OID_INDEX] == SE_READER_DEDICATED_MODE_ID) &&
             (pData[SUB_GID_OID_ACTION_INDEX] ==
              SE_READER_DEDICATED_MODE_OFF)) {
    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "Stop the SE Reader mode");
    currentHandleType =
        NfcExtensionController::getInstance()->getEventHandlerType();
    if (currentHandleType == HandlerType::MPOS) {
      mPosMngr->tagOperationTimer.kill();
      mPosMngr->tagRemovalTimer.kill();
      mPosMngr->startStopGuardTimer.kill();
      mPosMngr->updateState(MPOS_STATE_SEND_PROFILE_DESELECT_CONFIG_CMD);
      NfcExtensionWriter::getInstance().requestHALcontrol();
    }
    PlatformAbstractionLayer::getInstance()->palSendNfcDataCallback(
        sizeof(resp), resp);
    return NFCSTATUS_EXTN_FEATURE_SUCCESS;
  } else {
    return NFCSTATUS_EXTN_FEATURE_FAILURE;
  }
}

NFCSTATUS MposHandler::handleVendorNciRspNtf(uint16_t dataLen,
                                             const uint8_t *pData) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN,
                 "MposHandler::%s "
                 "Enter dataLen:%d",
                 __func__, dataLen);
  // Convert the raw pointer to a vector
  std::vector<uint8_t> pDataVec(pData, pData + dataLen);

  // Must to call the stopWriteRspTimer to cancel the rsp timer
  NfcExtensionWriter::getInstance().stopWriteRspTimer(pData, dataLen);

  return mPosMngr->processMposNciRspNtf(pDataVec);
}

void MposHandler::onWriteComplete(uint8_t status) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter status:%d", __func__,
                 status);
  NfcExtensionWriter::getInstance().onWriteComplete(status);
}

void MposHandler::halControlGranted() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter ", __func__);
  NfcExtensionWriter::getInstance().onhalControlGrant();

  if (mPosMngr->getState() == MPOS_STATE_SEND_PROFILE_DESELECT_CONFIG_CMD) {
    mPosMngr->processMposEvent(MPOS_STATE_SEND_PROFILE_DESELECT_CONFIG_CMD);
  } else {
    mPosMngr->updateState(MPOS_STATE_SEND_PROFILE_SELECT_CONFIG_CMD);
    mPosMngr->processMposEvent(MPOS_STATE_SEND_PROFILE_SELECT_CONFIG_CMD);
  }
}

void MposHandler::onHalRequestControlTimeout() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter ", __func__);
  NfcExtensionController::getInstance()
      ->getCurrentEventHandler()
      ->notifyGenericErrEvt(NCI_HAL_CONTROL_BUSY);
  NfcExtensionController::getInstance()->switchEventHandler(
      HandlerType::DEFAULT);
}

void MposHandler::onWriteRspTimeout() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter ", __func__);
  NfcExtensionController::getInstance()
      ->getCurrentEventHandler()
      ->notifyGenericErrEvt(NCI_UN_RECOVERABLE_ERR);
  NfcExtensionController::getInstance()->switchEventHandler(
      HandlerType::DEFAULT);
}
