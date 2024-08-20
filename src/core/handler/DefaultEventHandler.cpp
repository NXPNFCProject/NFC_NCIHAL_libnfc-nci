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

#include "DefaultEventHandler.h"
#include "NfcExtensionConstants.h"
#include "NfcExtensionController.h"
#include "PlatformAbstractionLayer.h"
#include <phNxpLog.h>

NFCSTATUS DefaultEventHandler::handleVendorNciMessage(uint16_t dataLen,
                                                      const uint8_t *pData) {
  NXPLOG_EXTNS_D(
      NXPLOG_ITEM_NXP_GEN_EXTN,
      "DefaultEventHandler::%s Enter dataLen:%d, EventHandlerType:%d, "
      "EventHandlerState:%d",
      __func__, dataLen,
      static_cast<int>(
          NfcExtensionController::getInstance()->getEventHandlerType()),
      static_cast<int>(
          NfcExtensionController::getInstance()->getEventHandlerState()));
  return NFCSTATUS_EXTN_FEATURE_FAILURE;
}

NFCSTATUS DefaultEventHandler::handleVendorNciRspNtf(uint16_t dataLen,
                                                     const uint8_t *pData) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN,
                 "DefaultEventHandler::%s Enter dataLen:%d", __func__, dataLen);
  // call false, if it is not to be processed by Nfc Extension library
  return NFCSTATUS_EXTN_FEATURE_FAILURE;
}
