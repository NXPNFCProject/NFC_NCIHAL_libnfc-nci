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
#include "NfcExtensionConstants.h"
#include "NfcExtensionController.h"
#include "NfcExtensionWriter.h"
#include "PlatformAbstractionLayer.h"
#include <phNxpLog.h>

MposHandler::MposHandler() { NXPLOG_EXTNS_D("%s Enter", __func__); }

void MposHandler::onFeatureStart() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter mPosNciPkt.size:%zu",
                 __func__, mPosNciPkt.size());
  if (mPosNciPkt.size() >= 3) {
    NfcExtensionWriter::getInstance().requestHALcontrol();
  } else {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter Invalid NCI packet");
  }
}
void MposHandler::handleHalEvent(int errorCode) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter errorCode:%d", __func__,
                 errorCode);
}

void MposHandler::onFeatureEnd() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter", __func__);
  mPosNciPkt.clear();
  NfcExtensionWriter::getInstance().releaseHALcontrol();
  NfcExtensionController::getInstance()->revertToDefaultHandler();
}

NFCSTATUS MposHandler::handleVendorNciMessage(uint16_t dataLen,
                                              const uint8_t *pData) {
  // Return true, only you will handle it
  NFCSTATUS status = NFCSTATUS_FAILED;
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "MposHandler::%s Enter dataLen:%d",
                 __func__, dataLen);
  // Sample code to demonstrate the priority handling
  /*if (HandlerType::T4T ==
          NfcExtensionController::getInstance()->getEventHandlerType() &&
      HandlerState::STARTED ==
          NfcExtensionController::getInstance()->getEventHandlerState()) {
    // TODO: Return error callback, if request can not be processed due to other
  on going priority feature. return true; } else {*/
  // TODO state diagram to be added
  mPosNciPkt.clear();
  mPosNciPkt.assign(pData, pData + dataLen);
  NfcExtensionController::getInstance()->switchEventHandler(HandlerType::MPOS);
  return NFCSTATUS_EXTN_FEATURE_SUCCESS;
  /*}*/
}

NFCSTATUS MposHandler::handleVendorNciRspNtf(uint16_t dataLen,
                                             const uint8_t *pData) {
  NXPLOG_NCIHAL_D(NXPLOG_ITEM_NXP_GEN_EXTN, "MposHandler::%s Enter dataLen:%d",
                  __func__, dataLen);
  // Must to call the stopWriteRspTimer to cancel the rsp timer
  NfcExtensionWriter::getInstance().stopWriteRspTimer(
      mPosNciPkt.data(), mPosNciPkt.size(), pData, dataLen);

  /* Currently sending the Rsp/Ntf to upper layer for testing purpose.
   To be updated as part of MPOS implementation */
  PlatformAbstractionLayer::getInstance()->palSendNfcDataCallback(dataLen,
                                                                  pData);

  // At the end of mPOS processing call releaseHALcontrol.
  NfcExtensionWriter::getInstance().releaseHALcontrol();
  return NFCSTATUS_EXTN_FEATURE_SUCCESS;
}

void MposHandler::onWriteComplete(uint8_t status) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter status:%d", __func__,
                 status);
  NfcExtensionWriter::getInstance().onWriteComplete(status);
}

void MposHandler::halControlGranted() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter nciPkt.size():%zu",
                 __func__, mPosNciPkt.size());
  NfcExtensionWriter::getInstance().onhalControlGrant();
  if (mPosNciPkt.size() >= 3) {
    NfcExtensionWriter::getInstance().write(mPosNciPkt.data(),
                                            mPosNciPkt.size());
  } else {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter Invalid NCI packet");
  }
}

void MposHandler::onHalRequestControlTimeout() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter ", __func__);
}

void MposHandler::onWriteRspTimeout() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter ", __func__);
}