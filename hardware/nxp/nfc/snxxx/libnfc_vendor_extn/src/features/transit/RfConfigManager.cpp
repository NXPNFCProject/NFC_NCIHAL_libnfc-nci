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

#include "RfConfigManager.h"
#include <android-base/parseint.h>
#include <phNxpLog.h>

using namespace ::android::base;
RfConfigManager *RfConfigManager::instance = nullptr;

RfConfigManager::RfConfigManager() {}

RfConfigManager::~RfConfigManager() {}

RfConfigManager *RfConfigManager::getInstance() {
  if (instance == nullptr) {
    instance = new RfConfigManager();
  }
  return instance;
}

NFCSTATUS RfConfigManager::processRsp(vector<uint8_t> rfConfigRsp) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "RfConfigManager::%s: enter",
                 __func__);
  NFCSTATUS rspStatus = NFCSTATUS_EXTN_FEATURE_FAILURE;

  if (mCmdBuffer.size() >= NCI_PAYLOAD_LEN_INDEX &&
      rfConfigRsp.size() >= NCI_PAYLOAD_LEN_INDEX) {
    uint8_t cmdGid = (mCmdBuffer[NCI_GID_INDEX] & EXT_NCI_GID_MASK);
    uint8_t cmdOid = (mCmdBuffer[NCI_OID_INDEX] & EXT_NCI_OID_MASK);
    uint8_t rspGid = (rfConfigRsp[NCI_GID_INDEX] & EXT_NCI_GID_MASK);
    uint8_t rspOid = (rfConfigRsp[NCI_OID_INDEX] & EXT_NCI_OID_MASK);

    NXPLOG_EXTNS_D(
        NXPLOG_ITEM_NXP_GEN_EXTN,
        "RfConfigManager %s Enter cmdGid:%d, cmdOid:%d, rspGid:%d, rspOid:%d,",
        __func__, cmdGid, cmdOid, rspGid, rspOid);
    if (cmdGid == rspGid && cmdOid == rspOid) {
      mRspBuffer.insert(mRspBuffer.begin(), rfConfigRsp.begin(),
                        rfConfigRsp.end());
      mCmdRspCv.signal();
      rspStatus = NFCSTATUS_EXTN_FEATURE_SUCCESS;
    }
  }
  return rspStatus;
}

uint8_t RfConfigManager::sendNciCmd(vector<uint8_t> rfConfigCmd) {
  NXPLOG_EXTNS_I(NXPLOG_ITEM_NXP_GEN_EXTN, "RfConfigManager::%s Enter ",
                 __func__);
  uint8_t status = 0xFF;
  mCmdBuffer.clear();
  mCmdBuffer.assign(rfConfigCmd.begin(), rfConfigCmd.end());
  mRspBuffer.clear();
  NfcExtensionWriter::getInstance().write(rfConfigCmd.data(),
                                          rfConfigCmd.size());
  mCmdRspCv.timedWait(NCI_CMD_TIMEOUT_IN_SEC);
  mCmdRspCv.unlock();
  if (mRspBuffer.size() < MIN_PCK_MSG_LEN)
    return DEFAULT_RESP;
  return mRspBuffer.at(EXT_NFC_STATUS_INDEX);
}

bool RfConfigManager::isRFConfigUpdateRequired(
    unsigned newValue, vector<uint8_t> &propCmdresData) {
  vector<uint8_t> propRfGetConfigCmd{0x2F, 0x14, 0x02, 0x62, 0x32};
  uint8_t getPropRfCount = 0;
  uint8_t index = 10; // Index for RF register 6232
  do {
    if (EXT_NFC_STATUS_OK != sendNciCmd(propRfGetConfigCmd)) {
      NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN,
                     "RfConfigManager::%s : Get config failed for A00D",
                     __func__);
      return false;
    }
    if (GET_RES_STATUS_CHECK(mRspBuffer.size(), mRspBuffer.data())) {
      NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN,
                     "RfConfigManager::%s : Get reponse failed", __func__);
      return false;
    }
    if (mRspBuffer[RF_CM_TX_UNDERSHOOT_INDEX] == newValue)
      return true;

    // Mapping Prop command response to NCI command response.
    propCmdresData[index] = (uint8_t)(newValue & BYTE0_SHIFT_MASK);
    propCmdresData[index + 1] = mRspBuffer[RF_CM_TX_UNDERSHOOT_INDEX + 1];
    propCmdresData[index + 2] = mRspBuffer[RF_CM_TX_UNDERSHOOT_INDEX + 2];
    propCmdresData[index + 3] = mRspBuffer[RF_CM_TX_UNDERSHOOT_INDEX + 3];

    getPropRfCount++;
    if (getPropRfCount == 1) {
      index = 19; // Index for RF register 6732
      propRfGetConfigCmd[NCI_PACKET_TLV_INDEX] = 0x67;
    }
  } while (getPropRfCount < 2);

  return true;
}

bool RfConfigManager::updateRfSetConfig(vector<uint8_t> &setConfCmd,
                                        vector<uint8_t> &getResData) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "RfConfigManager::%s : Enter",
                 __func__);
  uint8_t res_data_packet_len = 0;
  if ((getResData.size() <= 5) ||
      (getResData.size() !=
       (getResData[NCI_PACKET_LEN_INDEX] + NCI_HEADER_LEN))) {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "RfConfigManager::%s : Invalid res data length", __func__);
    return false;
  }
  /* Updating the actual TLV packet length by excluding the status & tlv bytes
   */
  res_data_packet_len = getResData[NCI_PACKET_LEN_INDEX] - 2;
  /* Copying the TLV packet and excluding  NCI header, status & tlv bytes */
  setConfCmd.insert(setConfCmd.end(), getResData.begin() + 5, getResData.end());
  if (setConfCmd.size() >= 0xFF) {
    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "RfConfigManager::%s : set rf command", __func__);
    vector<uint8_t> setConfCmdVect(
        setConfCmd.begin(),
        setConfCmd.begin() + (setConfCmd.size() - res_data_packet_len));
    if (EXT_NFC_STATUS_OK != sendNciCmd(setConfCmdVect)) {
      NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN,
                     "RfConfigManager::%s : Set confing failed", __func__);
      return false;
    }
    // Clear setConf Data expect the last command response.
    setConfCmd.erase(setConfCmd.begin() + 4,
                     setConfCmd.end() - res_data_packet_len);
    // Clear the length and TLV after sending the packet.
    setConfCmd[NCI_PACKET_LEN_INDEX] = 0x01;
    setConfCmd[NCI_PACKET_TLV_INDEX] = 0x00;
  }
  setConfCmd[NCI_PACKET_LEN_INDEX] += res_data_packet_len;
  setConfCmd[NCI_PACKET_TLV_INDEX] += getResData[NCI_GET_RES_TLV_INDEX];

  return true;
}

void RfConfigManager::send_sram_config_to_flash() {
  NXPLOG_EXTNS_I(NXPLOG_ITEM_NXP_GEN_EXTN, "RfConfigManager::%s  Enter",
                 __func__);
  vector<uint8_t> send_sram_flash = {NCI_PROP_CMD_VAL,
                                     NXP_FLUSH_SRAM_AO_TO_FLASH_OID, 0x00};
  NfcExtensionWriter::getInstance().write(send_sram_flash.data(),
                                          send_sram_flash.size());
}

bool RfConfigManager::isUpdateThreshold(uint8_t index, unsigned &new_value,
                                        vector<uint8_t> &lpdet_cmd_response) {
  unsigned read_value = lpdet_cmd_response[index];
  bool isThresholdUpdateRequired = false;
  read_value |= (lpdet_cmd_response[index + 1] << 8);
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN,
                 "RfConfigManager::%s : read_value = %02x Value: %02x",
                 __func__, read_value, new_value);
  if (read_value != new_value) {
    lpdet_cmd_response[index] = (uint8_t)(new_value & BYTE0_SHIFT_MASK);
    lpdet_cmd_response[index + 1] =
        (uint8_t)((new_value & BYTE1_SHIFT_MASK) >> 8);
    isThresholdUpdateRequired = true;
  }
  return isThresholdUpdateRequired;
}

bool RfConfigManager::checkUpdateRfRegisterConfig(vector<uint8_t> rfConfigCmd) {
  NXPLOG_EXTNS_I(NXPLOG_ITEM_NXP_GEN_EXTN, "RfConfigManager::%s Enter ",
                 __func__);

  vector<uint8_t> rfConfigPayload(rfConfigCmd.begin() + 5, rfConfigCmd.end());

  if (rfConfigPayload.size() == 0) {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "RfConfigManager::%s :  RF register config is invalid",
                   __func__);
    return false;
  }

  vector<uint8_t> cmd_get_rfconfval{0x20, 0x03, 0x03, 0x01, 0xA0, 0x85};
  vector<uint8_t> cmd_response{};
  vector<uint8_t> lpdet_cmd_response{};
  vector<uint8_t> get_cmd_response{};
  vector<uint8_t> cmd_set_rfconfval{0x20, 0x02, 0x01, 0x00};
  vector<uint8_t> prop_Cmd_Response{
      /* Preset get config response for A00D register */
      0x40, 0x03, 0x14, 0x00, 0x02, 0xA0, 0x0D, 0x06, 0x62, 0x32, 0xAE, 0x00,
      0x7F, 0x00, 0xA0, 0x0D, 0x06, 0x67, 0x32, 0xAE, 0x00, 0x1F, 0x00};
  bool is_feature_update_required = false;
  bool is_lpdet_threshold_required = false;
  uint8_t index_to_value = 0;
  uint8_t update_mode = BITWISE;
  unsigned b_position = 0;
  unsigned new_value = 0;
  unsigned read_value = 0;
  unsigned rf_reg_A085_value = 0;

  static const std::unordered_map<uint8_t, uint8_t> rfRegA085Map = {
      {UPDATE_MIFARE_NACK_TO_RATS_ENABLE, MIFARE_NACK_TO_RATS_ENABLE_BIT_POS},
      {UPDATE_MIFARE_MUTE_TO_RATS_ENABLE, MIFARE_MUTE_TO_RATS_ENABLE_BIT_POS},
      {UPDATE_CHINA_TIANJIN_RF_ENABLED, CHINA_TIANJIN_RF_ENABLE_BIT_POS},
      {UPDATE_CN_TRANSIT_CMA_BYPASSMODE_ENABLE,
       CN_TRANSIT_CMA_BYPASSMODE_ENABLE_BIT_POS},
      {UPDATE_CN_TRANSIT_BLK_NUM_CHECK_ENABLE,
       CN_TRANSIT_BLK_NUM_CHECK_ENABLE_BIT_POS}};

  if (EXT_NFC_STATUS_OK != sendNciCmd(cmd_get_rfconfval)) {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "RfConfigManager::%s : Get config failed for A085",
                   __func__);
    return false;
  }
  if (GET_RES_STATUS_CHECK(mRspBuffer.size(), mRspBuffer.data())) {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "RfConfigManager::%s :  Get config failed", __func__);
    return false;
  }
  // Updating the A085 get config command response to vector.
  cmd_response.insert(
      cmd_response.end(), &mRspBuffer[0],
      (&mRspBuffer[0] + (mRspBuffer[NCI_PACKET_LEN_INDEX] + NCI_HEADER_LEN)));
  rf_reg_A085_value = (unsigned)((cmd_response[REG_A085_DATA_INDEX + 3] << 24) |
                                 (cmd_response[REG_A085_DATA_INDEX + 2] << 16) |
                                 (cmd_response[REG_A085_DATA_INDEX + 1] << 8) |
                                 (cmd_response[REG_A085_DATA_INDEX]));

  cmd_get_rfconfval[NCI_GET_CMD_TLV_INDEX2] = 0x9E;
  if (EXT_NFC_STATUS_OK != sendNciCmd(cmd_get_rfconfval)) {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "RfConfigManager::%s : Get config failed for A09E",
                   __func__);
    return false;
  }
  if (GET_RES_STATUS_CHECK(mRspBuffer.size(), mRspBuffer.data())) {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "RfConfigManager::%s : Get config failed", __func__);
    return false;
  }
  // Updating the A09E get config command response to vector.
  lpdet_cmd_response.insert(
      lpdet_cmd_response.end(), &mRspBuffer[0],
      (&mRspBuffer[0] + (mRspBuffer[NCI_PACKET_LEN_INDEX] + NCI_HEADER_LEN)));

  size_t cnt = 0;
  while (cnt < rfConfigPayload.size()) {
    uint8_t configKey = rfConfigPayload[cnt];
    if (!((cnt + 1) < rfConfigPayload.size())) {
      NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN,
                     "RfConfigManager::%s : Invalid input payload", __func__);
      return false;
    }
    uint8_t configLen = rfConfigPayload[cnt + 1];
    if (!((cnt + configLen + 1) < rfConfigPayload.size())) {
      NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN,
                     "RfConfigManager::%s : Invalid input payload", __func__);
      return false;
    }
    uint8_t configValStart = cnt + 2;
    uint8_t configValEnd = configValStart + configLen;
    if (configLen == 2) {
      new_value = (rfConfigPayload[configValStart] << 8) |
                  rfConfigPayload[configValStart + 1];
    } else {
      new_value = rfConfigPayload[configValStart];
    }
    cnt = configValEnd;
    update_mode = BITWISE;
    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "RfConfigManager::%s : configKey:%d ConfigVal:%02x",
                   __func__, configKey, new_value);

    switch (configKey) {
    case UPDATE_DLMA_ID_TX_ENTRY:
      cmd_get_rfconfval[NCI_GET_CMD_TLV_INDEX1] = 0xA0;
      cmd_get_rfconfval[NCI_GET_CMD_TLV_INDEX2] = 0x34;
      index_to_value = DLMA_ID_TX_ENTRY_INDEX;
      break;
    case UPDATE_RF_CM_TX_UNDERSHOOT_CONFIG:
      if (!isRFConfigUpdateRequired(new_value, prop_Cmd_Response))
        return false;

      if ((mRspBuffer.size() > RF_CM_TX_UNDERSHOOT_INDEX) &&
          (mRspBuffer[RF_CM_TX_UNDERSHOOT_INDEX] != new_value)) {
        if (!updateRfSetConfig(cmd_set_rfconfval, prop_Cmd_Response))
          return false;
      }
      break;
    case UPDATE_INITIAL_TX_PHASE:
      cmd_get_rfconfval[NCI_GET_CMD_TLV_INDEX1] = 0xA0;
      cmd_get_rfconfval[NCI_GET_CMD_TLV_INDEX2] = 0x6A;
      index_to_value = INITIAL_TX_PHASE_INDEX;
      update_mode = BYTEWISE;
      break;
    case UPDATE_LPDET_THRESHOLD:
      is_lpdet_threshold_required = isUpdateThreshold(
          LPDET_THRESHOLD_INDEX, new_value, lpdet_cmd_response);
      break;
    case UPDATE_NFCLD_THRESHOLD:
      is_lpdet_threshold_required = isUpdateThreshold(
          NFCLD_THRESHOLD_INDEX, new_value, lpdet_cmd_response);
      break;
    case UPDATE_GUARD_TIMEOUT_TX2RX:
      cmd_get_rfconfval[NCI_GET_CMD_TLV_INDEX1] = 0xA1;
      cmd_get_rfconfval[NCI_GET_CMD_TLV_INDEX2] = 0x0E;
      index_to_value = GUARD_TIMEOUT_TX2RX_INDEX;
      break;
    case UPDATE_RF_PATTERN_CHK:
      cmd_get_rfconfval[NCI_GET_CMD_TLV_INDEX1] = 0xA1;
      cmd_get_rfconfval[NCI_GET_CMD_TLV_INDEX2] = 0x48;
      index_to_value = RF_PATTERN_CHK_INDEX;
      break;
    case UPDATE_MIFARE_NACK_TO_RATS_ENABLE:
    case UPDATE_MIFARE_MUTE_TO_RATS_ENABLE:
    case UPDATE_CHINA_TIANJIN_RF_ENABLED:
    case UPDATE_CN_TRANSIT_CMA_BYPASSMODE_ENABLE:
    case UPDATE_CN_TRANSIT_BLK_NUM_CHECK_ENABLE: {
      auto itReg = rfRegA085Map.find(configKey);
      if (itReg == rfRegA085Map.end())
        continue;
      if ((rf_reg_A085_value & b_position) !=
          ((new_value & 0x01) << itReg->second)) {
        rf_reg_A085_value ^= (1 << itReg->second);
        is_feature_update_required = true;
      }
    } break;
    case UPDATE_PHONEOFF_TECH_DISABLE:
      cmd_get_rfconfval[NCI_GET_CMD_TLV_INDEX1] = 0xA1;
      cmd_get_rfconfval[NCI_GET_CMD_TLV_INDEX2] = 0x1A;
      index_to_value = PHONEOFF_TECH_DISABLE_INDEX;
      break;
    case UPDATE_ISO_DEP_MERGE_SAK:
      cmd_get_rfconfval[NCI_GET_CMD_TLV_INDEX1] = 0xA1;
      cmd_get_rfconfval[NCI_GET_CMD_TLV_INDEX2] = 0x1B;
      index_to_value = ISO_DEP_MERGE_SAK_INDEX;
      break;
    default:
      NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN,
                     "RfConfigManager::%s : default = %x", __func__, new_value);
      break;
    }
    if (index_to_value) {
      if (EXT_NFC_STATUS_OK != sendNciCmd(cmd_get_rfconfval)) {
        NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN,
                       "RfConfigManager::%s : Get config failed for %d",
                       __func__, configKey);
        return false;
      }
      if (GET_RES_STATUS_CHECK(mRspBuffer.size(), mRspBuffer)) {
        NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN,
                       "RfConfigManager::%s : Get confing response failed ",
                       __func__);
        return false;
      }
      read_value = 0;
      read_value = mRspBuffer[index_to_value];
      if (update_mode == BYTEWISE)
        read_value |= (mRspBuffer[index_to_value + 1] << 8);
      if (read_value == new_value) {
        index_to_value = 0;
        continue;
      }
      mRspBuffer[index_to_value] = (uint8_t)(new_value & BYTE0_SHIFT_MASK);
      if (update_mode == BYTEWISE) {
        mRspBuffer[index_to_value + 1] =
            (uint8_t)((new_value & BYTE1_SHIFT_MASK) >> 8);
      }

      // Updating the get config command response to vector.
      get_cmd_response.insert(
          get_cmd_response.end(), &mRspBuffer[0],
          (&mRspBuffer[0] +
           (mRspBuffer[NCI_PACKET_LEN_INDEX] + NCI_HEADER_LEN)));

      if (!updateRfSetConfig(cmd_set_rfconfval, get_cmd_response))
        return false;

      get_cmd_response.clear();
      index_to_value = 0;
    }
  }

  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN,
                 "RfConfigManager::%s : is_feature_update_required = %d",
                 __func__, is_feature_update_required);
  if (is_feature_update_required) {
    // Updating the A085 response to set config command.
    cmd_response[REG_A085_DATA_INDEX + 3] =
        (uint8_t)((rf_reg_A085_value & BYTE3_SHIFT_MASK) >> 24);
    cmd_response[REG_A085_DATA_INDEX + 2] =
        (uint8_t)((rf_reg_A085_value & BYTE2_SHIFT_MASK) >> 16);
    cmd_response[REG_A085_DATA_INDEX + 1] =
        (uint8_t)((rf_reg_A085_value & BYTE1_SHIFT_MASK) >> 8);
    cmd_response[REG_A085_DATA_INDEX] =
        (uint8_t)(rf_reg_A085_value & BYTE0_SHIFT_MASK);
    if (!updateRfSetConfig(cmd_set_rfconfval, cmd_response))
      return false;
  }

  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN,
                 "RfConfigManager::%s : is_lpdet_threshold_required = %d",
                 __func__, is_lpdet_threshold_required);
  if (is_lpdet_threshold_required) {
    // Updating the A09E response to set config command.
    if (!updateRfSetConfig(cmd_set_rfconfval, lpdet_cmd_response))
      return false;
  }
  if (cmd_set_rfconfval.size() <= NCI_HEADER_LEN) {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "RfConfigManager::%s : Invalid NCI Command length = %zu",
                   __func__, cmd_set_rfconfval.size());
    return false;
  }

  if (cmd_set_rfconfval[NCI_PACKET_TLV_INDEX] != 0x00) {
    /*If update require do set-config in NFCC otherwise skip */
    if (EXT_NFC_STATUS_OK == sendNciCmd(cmd_set_rfconfval)) {
      if (is_feature_update_required) {
        send_sram_config_to_flash();
      }
    } else {
      if (GET_RES_STATUS_CHECK(mRspBuffer.size(), mRspBuffer)) {
        NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN,
                       "RfConfigManager::%s Set RF update cmd  is failed..",
                       __func__);
      }
      return false;
    }
  }
  return true;
}
