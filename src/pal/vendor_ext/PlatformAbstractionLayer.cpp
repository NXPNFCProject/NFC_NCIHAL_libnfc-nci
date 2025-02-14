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

#include "PlatformAbstractionLayer.h"
#include "NfcExtensionConstants.h"
#include "NfcExtensionController.h"
#include <NfcExtension.h>
#include <ProprietaryExtn.h>
#include <phNxpLog.h>
#include <phNxpNciHal_IoctlOperations.h>

extern uint8_t phNxpLog_EnableDisableLogLevel(uint8_t enable);

PlatformAbstractionLayer *PlatformAbstractionLayer::sPlatformAbstractionLayer;

PlatformAbstractionLayer::PlatformAbstractionLayer() {}

PlatformAbstractionLayer::~PlatformAbstractionLayer() {}

PlatformAbstractionLayer *PlatformAbstractionLayer::getInstance() {
  if (sPlatformAbstractionLayer == nullptr) {
    sPlatformAbstractionLayer = new PlatformAbstractionLayer();
  }
  return sPlatformAbstractionLayer;
}

NFCSTATUS PlatformAbstractionLayer::palenQueueWrite(const uint8_t *pBuffer,
                                                    uint16_t wLength) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter wLength:%d", __func__,
                 wLength);
  uint8_t pBufferCpy[wLength];
  palMemcpy(pBufferCpy, wLength, pBuffer, wLength);
  return phNxpHal_EnqueueWrite(pBufferCpy, wLength);
}

NFCSTATUS PlatformAbstractionLayer::palenQueueRspNtf(const uint8_t *pBuffer,
                                                     uint16_t wLength) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter wLength:%d", __func__,
                 wLength);
  uint8_t pBufferCpy[wLength];
  palMemcpy(pBufferCpy, wLength, pBuffer, wLength);
  return phNxpHal_EnqueueRsp(pBufferCpy, wLength);
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
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter", __func__);
  ProprietaryExtn::getInstance()->mapGidOidToPropRspNtf(dataLen,
                                                        (uint8_t *)pData);
  phNxpHal_NfcDataCallback(dataLen, pData);
}

uint8_t PlatformAbstractionLayer::palGetNxpByteArrayValue(const char *name,
                                                          char *pValue,
                                                          long bufflen,
                                                          long *len) {
  return phNxpHal_GetNxpByteArrayValue(name, pValue, bufflen, len);
}

uint8_t PlatformAbstractionLayer::palGetNxpNumValue(const char *name,
                                                    void *pValue,
                                                    unsigned long len) {
  return phNxpHal_GetNxpNumValue(name, pValue, len);
}

uint8_t PlatformAbstractionLayer::palEnableDisableDebugLog(uint8_t enable) {
  phNxpExtLog_EnableDisableLogLevel(enable);
  return phNxpLog_EnableDisableLogLevel(enable);
}

void PlatformAbstractionLayer::coverAttached(string state, string type) {
  phNxpNciHal_setSystemProperty(COVER_ID_PROP, std::move(type));
  phNxpNciHal_setSystemProperty(COVER_STATE_PROP, std::move(state));
}
