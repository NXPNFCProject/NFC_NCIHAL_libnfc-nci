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

#ifndef NFC_FIRMWARE_INFO_H
#define NFC_FIRMWARE_INFO_H

#include "IEventHandler.h"

/** \addtogroup FW_EVENT_HANDLER_API_INTERFACE
 *  @brief  interface to perform the FW extract feature functionality.
 *  @{
 */
class NfcFirmwareInfo : public IEventHandler {
public:
  static NfcFirmwareInfo *getInstance();
  /**
   * @brief handles the vendor NCI message by delegating the message to
   *        current active event handler
   * @return returns NFCSTATUS_EXTN_FEATURE_SUCCESS, if it is vendor specific
   * feature and handled by extension library otherwise
   * NFCSTATUS_EXTN_FEATURE_FAILURE.
   *
   */
  NFCSTATUS handleVendorNciMessage(uint16_t dataLen, const uint8_t *pData);

  /**
   * @brief handles the vendor NCI response/Notification by delegating the
   * message to current active event handler
   * @return returns NFCSTATUS_EXTN_FEATURE_SUCCESS, if it is vendor specific
   * feature and handled by extension library otherwise
   * NFCSTATUS_EXTN_FEATURE_FAILURE.
   *
   */
  NFCSTATUS handleVendorNciRspNtf(uint16_t dataLen, uint8_t *pData);

  /**
   * @brief get FW version from NciStateMonitor & enque vendor specific
   * response.
   */
  void fetchandSendFwVersionNtf();

  NfcFirmwareInfo();

private:
  static NfcFirmwareInfo *instance;
};

#endif // NFC_FIRMWARE_INFO_H
