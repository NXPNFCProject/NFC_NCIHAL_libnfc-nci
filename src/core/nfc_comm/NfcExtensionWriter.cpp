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

#include "NfcExtensionWriter.h"
#include "NfcExtensionConstants.h"
#include "NfcExtensionController.h"
#include "PlatformAbstractionLayer.h"
#include <phNxpLog.h>

NfcExtensionWriter::NfcExtensionWriter() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter", __func__);
}

NfcExtensionWriter::~NfcExtensionWriter() {}

NfcExtensionWriter &NfcExtensionWriter::getInstance() {
  static NfcExtensionWriter instance;
  return instance;
}

void NfcExtensionWriter::onhalControlGrant() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter", __func__);
  stopHalCtrlTimer();
}

void NfcExtensionWriter::stopHalCtrlTimer() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter", __func__);
  mHalCtrlTimer.kill();
}

static void halRequestControlTimeoutCbk(union sigval val) {
  UNUSED_PROP(val);
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter ", __func__);
  NfcExtensionController::getInstance()->halRequestControlTimedout();
  return;
}

void NfcExtensionWriter::releaseHALcontrol() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter", __func__);
  stopHalCtrlTimer();
  PlatformAbstractionLayer::getInstance()->palReleaseHALcontrol();
}

void NfcExtensionWriter::requestHALcontrol() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter ", __func__);
  if (mHalCtrlTimer.set(NXP_EXTNS_HAL_REQUEST_CTRL_TIMEOUT_IN_MS, NULL,
                        halRequestControlTimeoutCbk)) {
    PlatformAbstractionLayer::getInstance()->palRequestHALcontrol();
  } else {
    NfcExtensionController::getInstance()
        ->getCurrentEventHandler()
        ->notifyGenericErrEvt(NCI_UN_RECOVERABLE_ERR);
    NfcExtensionController::getInstance()->switchEventHandler(
        HandlerType::DEFAULT);
  }
}

static void writeRspTimeoutCbk(union sigval val) {
  UNUSED_PROP(val);
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter", __func__);
  NfcExtensionController::getInstance()->writeRspTimedout();
  return;
}

NFCSTATUS NfcExtensionWriter::write(const uint8_t *pBuffer, uint16_t wLength,
                                    int timeout) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter wLength:%d", __func__,
                 wLength);
  if (mWriteRspTimer.set(timeout, NULL, writeRspTimeoutCbk)) {
    cmdData.clear();
    cmdData.assign(pBuffer, pBuffer + wLength);
    PlatformAbstractionLayer::getInstance()->palenQueueWrite(pBuffer, wLength);
    return NFCSTATUS_SUCCESS;
  } else {
    NfcExtensionController::getInstance()
        ->getCurrentEventHandler()
        ->notifyGenericErrEvt(NCI_UN_RECOVERABLE_ERR);
    NfcExtensionController::getInstance()->switchEventHandler(
        HandlerType::DEFAULT);
    return NFCSTATUS_FAILED;
  }
}

void NfcExtensionWriter::onWriteComplete(uint8_t status) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter status = 0x%x", __func__,
                 status);
}

void NfcExtensionWriter::stopWriteRspTimer(const uint8_t *pRspBuffer,
                                           uint16_t rspLength) {
  if (cmdData.size() >= NCI_PAYLOAD_LEN_INDEX &&
      rspLength >= NCI_PAYLOAD_LEN_INDEX) {
    uint8_t cmdGid = (cmdData[NCI_GID_INDEX] & NCI_GID_MASK);
    uint8_t cmdOid = (cmdData[NCI_OID_INDEX] & NCI_OID_MASK);
    uint8_t rspGid = (pRspBuffer[NCI_GID_INDEX] & NCI_GID_MASK);
    uint8_t rspOid = (pRspBuffer[NCI_OID_INDEX] & NCI_OID_MASK);

    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "%s Enter cmdGid:%d, cmdOid:%d, rspGid:%d, rspOid:%d,",
                   __func__, cmdGid, cmdOid, rspGid, rspOid);
    if (cmdGid == rspGid && cmdOid == rspOid) {
      mWriteRspTimer.kill();
    }
  }
}
