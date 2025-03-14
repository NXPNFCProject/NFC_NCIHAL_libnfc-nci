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

#ifndef TRANSITCONFIG_HANDLER_H
#define TRANSITCONFIG_HANDLER_H

#include "IEventHandler.h"
#include "NfcExtensionController.h"
#include "NfcExtensionWriter.h"

class TransitConfigHandler : public IEventHandler {
public:
  static constexpr uint8_t TRANSIT_OID = 0x01;
  static constexpr uint8_t RF_REGISTER_OID = 0x02;
  static constexpr uint8_t TRANSIT_NCI_VENDOR_RSP_SUCCESS[] = {
      NCI_PROP_RSP_VAL, NCI_ROW_PROP_OID_VAL, PAYLOAD_TWO_LEN,
      TRANSIT_SUB_GIDOID, RESPONSE_STATUS_OK};
  static constexpr uint8_t TRANSIT_NCI_VENDOR_RSP_FAILURE[] = {
      NCI_PROP_RSP_VAL, NCI_ROW_PROP_OID_VAL, PAYLOAD_TWO_LEN,
      TRANSIT_SUB_GIDOID, RESPONSE_STATUS_FAILED};

  static TransitConfigHandler *getInstance();
  /**
   * @brief handles the vendor NCI message
   * @return returns NFCSTATUS_EXTN_FEATURE_SUCCESS, if it is vendor specific
   * feature and handled by extension library otherwise
   * NFCSTATUS_EXTN_FEATURE_FAILURE.
   *
   */
  NFCSTATUS handleVendorNciMessage(uint16_t dataLen, const uint8_t *pData);

  /**
   * @brief handles the vendor NCI response and notification
   * @return returns NFCSTATUS_EXTN_FEATURE_SUCCESS, if it is vendor specific
   * feature and handled by extension library otherwise
   * NFCSTATUS_EXTN_FEATURE_FAILURE.
   *
   */
  NFCSTATUS handleVendorNciRspNtf(uint16_t dataLen, uint8_t *pData);

  /**
   * @brief stores libnfc-nci-update.conf to /data/vendor/nfc/.
   *
   * @param configCmd
   * @return true/false
   */
  bool updateVendorConfig(vector<uint8_t> configCmd);

  static TransitConfigHandler *instance;

  TransitConfigHandler();
  ~TransitConfigHandler();
};
#endif // TRANSITCONFIG_HANDLER_H
