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

int PlatformAbstractionLayer::palMemcmp(const void *pSrc1, const void *pSrc2,
                                        size_t dataSize) {
  return memcmp(pSrc1, pSrc2, dataSize);
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
  phNxpHal_NfcDataCallback(dataLen, pData);
}

bool PlatformAbstractionLayer::palRunInThread(void* func(void*)) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s:enter", __func__);
  pthread_attr_t sPThreadAttr;
  pthread_attr_init(&sPThreadAttr);
  pthread_attr_setdetachstate(&sPThreadAttr, PTHREAD_CREATE_DETACHED);
  int creation_result = pthread_create(&sThread, &sPThreadAttr,
                        threadStartRoutine, reinterpret_cast<void*>(func));
  if (creation_result >= 0) {
    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "PalThread creation success!");
  } else {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN, "PalThread creation failed!");
  }
  pthread_attr_destroy(&sPThreadAttr);
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s:exit", __func__);
  return creation_result >= 0;
}

void* PlatformAbstractionLayer::threadStartRoutine(void* arg) {
  auto funcPtr = reinterpret_cast<void*(*)(void*)>(arg);
  void* result = funcPtr(nullptr);
  pthread_exit(result);
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s : pthread_exit done", __func__);
  return nullptr;
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
  return phNxpLog_EnableDisableLogLevel(enable);
}

tNFC_chipType PlatformAbstractionLayer::palGetChipType() {
  return phNxpHal_GetChipType();
}
