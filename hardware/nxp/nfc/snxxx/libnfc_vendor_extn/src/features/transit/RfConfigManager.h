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

#ifndef RfCONFIG_MANAGER_H
#define RfCONFIG_MANAGER_H

#include "IEventHandler.h"
#include "NfcExtensionController.h"
#include "NfcExtensionWriter.h"
#include "PalThreadMutex.h"
#include <map>

#define BITWISE (1U)
#define BYTEWISE (2U)
#define BYTE0_SHIFT_MASK 0x000000FF
#define BYTE1_SHIFT_MASK 0x0000FF00
#define BYTE2_SHIFT_MASK 0x00FF0000
#define BYTE3_SHIFT_MASK 0xFF000000

#define GET_RES_STATUS_CHECK(len, data)                                        \
  (((len) < NCI_HEADER_LEN) ||                                                 \
   ((len) != ((data[NCI_PACKET_LEN_INDEX]) + NCI_HEADER_LEN)) ||               \
   (EXT_NFC_STATUS_OK != (data[NCI_GET_RES_STATUS_INDEX])))

class RfConfigManager {
public:
  static RfConfigManager *getInstance();

  RfConfigManager(const RfConfigManager &) = delete;
  RfConfigManager &operator=(const RfConfigManager &) = delete;

  PalThreadCondVar mCmdRspCv;
  std::vector<uint8_t> mCmdBuffer, mRspBuffer;

  NFCSTATUS processRsp(vector<uint8_t> rfConfigRsp);

  uint8_t sendNciCmd(vector<uint8_t> rfConfigCmd);

  bool checkUpdateRfRegisterConfig(vector<uint8_t> rfConfigCmd);

  /**
   * @brief Get the Update Prop Rf Set Config object
   *
   * @param newValue To be update at the index position
   * @param propCmdresData Udpate the prop response buffer based on
   *                 prop getConfig response.
   * @return true/false
   */
  bool isRFConfigUpdateRequired(unsigned newValue,
                                vector<uint8_t> &propCmdresData);

  /**
   * @brief Update the set RF settings.
   *
   * @param setConfCmd Udpate the set config buffer based on getConfig
   * @param getResData Response data.
   * @return true/false
   */
  bool updateRfSetConfig(vector<uint8_t> &setConfCmd,
                         vector<uint8_t> &getResData);

  /**
   * @brief This function is called to update the SRAM contents such as
   *        set config to FLASH for permanent storage.
   *        Note: This function has to be called after set config and
   *        before sending  core_reset command again.
   *
   */
  void send_sram_config_to_flash();

  /**
   * @brief set threshold value as per new_value if new_value & existing
   *        values are different.
   *
   * @param index response index which need to update.
   * @param new_value input value
   * @param lpdet_cmd_response previous proprietary resoponse which will
   *                           be used to compare new & existing value.
   * @return true/false
   */
  bool isUpdateThreshold(uint8_t index, unsigned &new_value,
                         vector<uint8_t> &lpdet_cmd_response);
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
  static RfConfigManager *instance;

  RfConfigManager();
  ~RfConfigManager();

  /* Below are A085 RF register bitpostions */
  static const uint8_t CN_TRANSIT_BLK_NUM_CHECK_ENABLE_BIT_POS = 6;
  static const uint8_t MIFARE_NACK_TO_RATS_ENABLE_BIT_POS = 13;
  static const uint8_t MIFARE_MUTE_TO_RATS_ENABLE_BIT_POS = 9;
  static const uint8_t CN_TRANSIT_CMA_BYPASSMODE_ENABLE_BIT_POS = 23;
  static const uint8_t CHINA_TIANJIN_RF_ENABLE_BIT_POS = 28;

  static const uint8_t NCI_PACKET_TLV_INDEX = 3;
  static const uint8_t NCI_PACKET_LEN_INDEX = 2;
  static const uint8_t NCI_GET_CMD_TLV_INDEX1 = 4;
  static const uint8_t NCI_GET_CMD_TLV_INDEX2 = 5;
  static const uint8_t NCI_GET_RES_STATUS_INDEX = 3;
  static const uint8_t NCI_GET_RES_TLV_INDEX = 4;

  /* Below are NCI get config response index values for each RF register */
  static const uint8_t DLMA_ID_TX_ENTRY_INDEX = 12;
  static const uint8_t RF_CM_TX_UNDERSHOOT_INDEX = 5;
  static const uint8_t PHONEOFF_TECH_DISABLE_INDEX = 8;
  static const uint8_t ISO_DEP_MERGE_SAK_INDEX = 8;
  static const uint8_t INITIAL_TX_PHASE_INDEX = 8;
  static const uint8_t LPDET_THRESHOLD_INDEX = 11;
  static const uint8_t NFCLD_THRESHOLD_INDEX = 13;
  static const uint8_t RF_PATTERN_CHK_INDEX = 8;
  static const uint8_t GUARD_TIMEOUT_TX2RX_INDEX = 8;
  static const uint8_t REG_A085_DATA_INDEX = 8;

  typedef enum {
    UPDATE_DLMA_ID_TX_ENTRY,
    UPDATE_MIFARE_NACK_TO_RATS_ENABLE,
    UPDATE_MIFARE_MUTE_TO_RATS_ENABLE,
    UPDATE_ISO_DEP_MERGE_SAK,
    UPDATE_PHONEOFF_TECH_DISABLE,
    UPDATE_RF_CM_TX_UNDERSHOOT_CONFIG,
    UPDATE_CHINA_TIANJIN_RF_ENABLED,
    UPDATE_CN_TRANSIT_CMA_BYPASSMODE_ENABLE,
    UPDATE_CN_TRANSIT_BLK_NUM_CHECK_ENABLE,
    UPDATE_RF_PATTERN_CHK,
    UPDATE_GUARD_TIMEOUT_TX2RX,
    UPDATE_INITIAL_TX_PHASE,
    UPDATE_LPDET_THRESHOLD,
    UPDATE_NFCLD_THRESHOLD,
    UPDATE_UNKNOWN = 0xFF
  } tSetDynamicRfConfigType;
};

#endif // RfCONFIG_MANAGER_H
