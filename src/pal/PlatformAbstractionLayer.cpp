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

#include "PlatformAbstractionLayer.h"
#include "NfcExtensionConstants.h"
#include "NfcExtensionController.h"
#include <NfcExtension.h>
#include <phNxpLog.h>

PlatformAbstractionLayer *PlatformAbstractionLayer::sPlatformAbstractionLayer;

PlatformAbstractionLayer::PlatformAbstractionLayer() {}

PlatformAbstractionLayer::~PlatformAbstractionLayer() {}

PlatformAbstractionLayer *PlatformAbstractionLayer::getInstance() {
  if (sPlatformAbstractionLayer == nullptr) {
    sPlatformAbstractionLayer = new PlatformAbstractionLayer();
  }
  return sPlatformAbstractionLayer;
}

void PlatformAbstractionLayer::palMemcpy(void *pDest, size_t destSize,
                                         const void *pSrc, size_t srcSize) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter srcSize:%zu, destSize:%zu",
                 __func__, srcSize, destSize);
  if (srcSize > destSize) {
    srcSize = destSize; // Truncate to avoid over flow
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "%s Truncated length to avoid over flow ", __func__);
  }
  memcpy(pDest, pSrc, srcSize);
}
NFCSTATUS PlatformAbstractionLayer::palenQueueWrite(const uint8_t *pBuffer,
                                                    uint16_t wLength) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter wLength:%d", __func__,
                 wLength);
  uint8_t pBufferCpy[wLength];
  palMemcpy(pBufferCpy, wLength, pBuffer, wLength);
  return phNxpHal_EnqueueWrite(pBufferCpy, wLength);
}

uint32_t PlatformAbstractionLayer::palTimerCreate(void) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter", __func__);
  return phOsalNfc_Timer_Create();
}

NFCSTATUS PlatformAbstractionLayer::palTimerStart(
    uint32_t dwTimerId, uint32_t dwRegTimeCnt,
    pPal_TimerCallbck_t pApplication_callback, void *pContext) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter dwTimerId:%d", __func__,
                 dwTimerId);
  return phOsalNfc_Timer_Start(dwTimerId, dwRegTimeCnt,
                               (pphOsalNfc_TimerCallbck_t)pApplication_callback,
                               pContext);
}

NFCSTATUS PlatformAbstractionLayer::palTimerStop(uint32_t dwTimerId) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter dwTimerId:%d", __func__,
                 dwTimerId);
  return phOsalNfc_Timer_Stop(dwTimerId);
}

void PlatformAbstractionLayer::palRequestHALcontrol(void) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter", __func__);
  phNxpHal_RequestControl();
}

void PlatformAbstractionLayer::palReleaseHALcontrol(void) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter", __func__);
  phNxpHal_ReleaseControl();
}

void PlatformAbstractionLayer::palSendNfcDataCallback(uint16_t dataLen,
                                                      const uint8_t *pData) {
  phNxpHal_NfcDataCallback(dataLen, pData);
}