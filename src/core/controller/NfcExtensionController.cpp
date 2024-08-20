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

#include "NfcExtensionController.h"
#include "NfcExtensionConstants.h"
#include "PlatformAbstractionLayer.h"
#include <phNxpLog.h>

NfcExtensionController *NfcExtensionController::sNfcExtensionController;

NfcExtensionController::NfcExtensionController() {}

NfcExtensionController::~NfcExtensionController() {}

NfcExtensionController *NfcExtensionController::getInstance() {
  if (sNfcExtensionController == nullptr) {
    sNfcExtensionController = new NfcExtensionController();
  }
  return sNfcExtensionController;
}

std::shared_ptr<IEventHandler> NfcExtensionController::getEventHandler() {
  return mIEventHandler;
}

void NfcExtensionController::addEventHandler(
    HandlerType handlerType, std::shared_ptr<IEventHandler> eventHandler) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter", __func__);
  if (HandlerType::DEFAULT == handlerType) {
    mDefaultEventHandler = eventHandler;
    mIEventHandler = eventHandler;
    mHandlers[handlerType] = eventHandler;
  } else {
    mHandlers[handlerType] = std::move(eventHandler);
  }
}

void NfcExtensionController::revertToDefaultHandler() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter", __func__);
  mIEventHandler = mDefaultEventHandler;
}

// TODO:Add state diagram for usage for switchEventHandler
void NfcExtensionController::switchEventHandler(HandlerType handlerType) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter handlerType:%d", __func__,
                 static_cast<int>(handlerType));
  if (handlerType == mCurrentHandlerType) {
    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "%s current handler and incoming handler are same. directly "
                   "call feature start",
                   __func__);
    // TODO: After Nfc FW download, not able to give callback to concrete class
    mIEventHandler->onFeatureStart();
    return;
  }
  auto it = mHandlers.find(handlerType);
  if (it != mHandlers.end()) {
    if (mIEventHandler) {
      mCurrentHandlerState = HandlerState::STOPPED;
      mIEventHandler->onFeatureEnd();
    }
    mIEventHandler = it->second;
    if (mIEventHandler) {
      mIEventHandler->onFeatureStart();
      mCurrentHandlerType = handlerType;
      mCurrentHandlerState = HandlerState::STARTED;
    }
  } else {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Handler not found", __func__);
  }
}

void NfcExtensionController::init() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter", __func__);
}

NFCSTATUS NfcExtensionController::handleVendorNciMessage(uint16_t dataLen,
                                                         const uint8_t *pData) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter dataLen:%d", __func__,
                 dataLen);
  auto subGid = HandlerType((pData[SUB_GID_OID_INDEX] & SUB_GID_MASK) >> 4);
  uint8_t subOid = (pData[SUB_GID_OID_INDEX] & SUB_OID_MASK);
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s subGid:%d subOid:%d", __func__,
                 static_cast<int>(subGid), subOid);

  auto it = mHandlers.find(subGid);
  if (it != mHandlers.end()) {
    std::shared_ptr<IEventHandler> eventHandler = it->second;
    return eventHandler->handleVendorNciMessage(dataLen, pData);
  } else {
    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Handler not found", __func__);
    return NFCSTATUS_EXTN_FEATURE_FAILURE;
  }
}

NFCSTATUS NfcExtensionController::handleVendorNciRspNtf(uint16_t dataLen,
                                                        const uint8_t *pData) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter dataLen:%d", __func__,
                 dataLen);
  return mIEventHandler->handleVendorNciRspNtf(dataLen, pData);
}

void NfcExtensionController::updateRfState(int state) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter state:%d", __func__,
                 state);
  // TODO: RF state definition and mapping to be done
}

void NfcExtensionController::onWriteCompletion(uint8_t status) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter status:%d", __func__,
                 status);
  mIEventHandler->onWriteComplete(status);
}

void NfcExtensionController::updateNfcHalState(int state) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter state:%d", __func__,
                 state);
  auto it = mNfcHalStateMap.find(state);
  if (it != mNfcHalStateMap.end()) {
    mNfcHalState = it->second;
  } else {
    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Invalid state", __func__);
  }
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter mNfcHalState:%d", __func__,
                 static_cast<int>(mNfcHalState));
  // TODO: NFC state handling to be done
}

void NfcExtensionController::halControlGranted() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter ", __func__);
  mIEventHandler->halControlGranted();
}

void NfcExtensionController::halRequestControlTimedout() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter", __func__);
  mIEventHandler->onHalRequestControlTimeout();
}

void NfcExtensionController::writeRspTimedout() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter", __func__);
  mIEventHandler->onWriteRspTimeout();
}
