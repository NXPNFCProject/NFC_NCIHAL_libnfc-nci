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
  static const uint8_t CONFIG_LEN_INDEX = 4;
  static const uint8_t PBF_INDEX = 5;
  static const uint8_t MIN_TRANSIT_HEADER_SIZE = 6;

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
   * @brief on Feature start, Current Handler have to be registered with
   * controller through switchEventHandler interface
   * @return void
   *
   */
  void onFeatureStart() override;

  /**
   * @brief on Feature End, Default Handler have to be registered with
   * controller through switchEventHandler interface
   * @return void
   *
   */
  void onFeatureEnd() override;

  /**
   * @brief stores libnfc-nci-update.conf to /data/vendor/nfc/.
   *
   * @param configPkt
   * @return true/false
   */
  bool updateVendorConfig(vector<uint8_t> configPkt);

  /**
   * @brief stores config to vector.
   *
   * @param configPkt
   * @return true/false
   */
  bool storeVendorConfig(vector<uint8_t> configPkt);

  static TransitConfigHandler *instance;

  TransitConfigHandler();
  ~TransitConfigHandler();
  /**
   * @brief Releases all the resources
   * @return None
   *
   */
  static inline void finalize() {
    if (instance != nullptr) {
      delete (instance);
      instance = nullptr;
    }
  }

private:
  std::vector<uint8_t> mConfigPkt;
  std::vector<uint8_t> mStoredConfig;
};
#endif // TRANSITCONFIG_HANDLER_H
