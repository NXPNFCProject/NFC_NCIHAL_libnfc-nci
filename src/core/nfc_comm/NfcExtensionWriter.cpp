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
  writeRspTimeoutTimerId =
      PlatformAbstractionLayer::getInstance()->palTimerCreate();
  halCtrlTimeoutTimerId =
      PlatformAbstractionLayer::getInstance()->palTimerCreate();
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN,
                 "%s Enter writeRspTimeoutTimerId:%d, halCtrlTimeoutTimerId:%d",
                 __func__, writeRspTimeoutTimerId, halCtrlTimeoutTimerId);
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
  NFCSTATUS status = PlatformAbstractionLayer::getInstance()->palTimerStop(
      halCtrlTimeoutTimerId);
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter status:%d", __func__,
                 status);
}

static void halRequestControlTimeoutCbk(uint32_t timerId, void *pContext) {
  UNUSED_PROP(pContext);
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter timerId:%d", __func__,
                 timerId);
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
  NFCSTATUS wStatus = NFCSTATUS_FAILED;
  wStatus = PlatformAbstractionLayer::getInstance()->palTimerStart(
      halCtrlTimeoutTimerId, NXP_EXTNS_HAL_REQUEST_CTRL_TIMEOUT_IN_MS,
      (PlatformAbstractionLayer::pPal_TimerCallbck_t)
          halRequestControlTimeoutCbk,
      NULL);
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter wStatus:%d", __func__,
                 wStatus);
  PlatformAbstractionLayer::getInstance()->palRequestHALcontrol();
}

static void writeRspTimeoutCbk(uint32_t timerId, void *pContext) {
  UNUSED_PROP(timerId);
  UNUSED_PROP(pContext);
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter", __func__);
  NfcExtensionController::getInstance()->writeRspTimedout();
  return;
}

NFCSTATUS NfcExtensionWriter::write(const uint8_t *pBuffer, uint16_t wLength) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter wLength:%d", __func__,
                 wLength);
  NFCSTATUS wStatus = NFCSTATUS_FAILED;
  wStatus = PlatformAbstractionLayer::getInstance()->palTimerStart(
      writeRspTimeoutTimerId, NXP_EXTNS_WRITE_RSP_TIMEOUT_IN_MS,
      (PlatformAbstractionLayer::pPal_TimerCallbck_t)writeRspTimeoutCbk, NULL);
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter wStatus:%d", __func__,
                 wStatus);
  if (NFCSTATUS_SUCCESS == wStatus) {
    PlatformAbstractionLayer::getInstance()->palenQueueWrite(pBuffer, wLength);
  } else {
    NXPLOG_EXTNS_E(
        NXPLOG_ITEM_NXP_GEN_EXTN,
        "Failed to write the NCI packet to controller. Response timer not "
        "started!!!");
    // TODO: Error handling should be added. caller must be notified for error
  }

  return wStatus;
}

void NfcExtensionWriter::onWriteComplete(uint8_t status) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter status = 0x%x", __func__,
                 status);
}

void NfcExtensionWriter::stopWriteRspTimer() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter", __func__);
  NFCSTATUS status = PlatformAbstractionLayer::getInstance()->palTimerStop(
      writeRspTimeoutTimerId);
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter status:%d", __func__,
                 status);
}
