/*
 * Copyright 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/******************************************************************************
 *
 *  The original Work has been changed by NXP.
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
 *  Copyright 2018-2019 NXP
 *
 ******************************************************************************/
#pragma once

#include <string>
#include <vector>

#include <config.h>

/* Configs from libnfc-nci.conf */
#define NAME_NFC_DEBUG_ENABLED "NFC_DEBUG_ENABLED"
#define NAME_NFA_STORAGE "NFA_STORAGE"
#define NAME_PRESERVE_STORAGE "PRESERVE_STORAGE"
#define NAME_POLLING_TECH_MASK "POLLING_TECH_MASK"
#define NAME_P2P_LISTEN_TECH_MASK "P2P_LISTEN_TECH_MASK"
#define NAME_UICC_LISTEN_TECH_MASK "UICC_LISTEN_TECH_MASK"
#define NAME_NFA_DM_CFG "NFA_DM_CFG"
#define NAME_SCREEN_OFF_POWER_STATE "SCREEN_OFF_POWER_STATE"
#define NAME_NFA_MAX_EE_SUPPORTED "NFA_MAX_EE_SUPPORTED"
#define NAME_NFA_DM_DISC_DURATION_POLL "NFA_DM_DISC_DURATION_POLL"
#define NAME_POLL_FREQUENCY "POLL_FREQUENCY"
#define NAME_NFA_AID_BLOCK_ROUTE "NFA_AID_BLOCK_ROUTE"
#define NAME_AID_FOR_EMPTY_SELECT "AID_FOR_EMPTY_SELECT"
#define NAME_AID_MATCHING_MODE "AID_MATCHING_MODE"

/* Configs from vendor interface */
#define NAME_NFA_POLL_BAIL_OUT_MODE "NFA_POLL_BAIL_OUT_MODE"
#define NAME_PRESENCE_CHECK_ALGORITHM "PRESENCE_CHECK_ALGORITHM"
#define NAME_NFA_PROPRIETARY_CFG "NFA_PROPRIETARY_CFG"
#define NAME_DEFAULT_OFFHOST_ROUTE "DEFAULT_OFFHOST_ROUTE"
#define NAME_DEFAULT_NFCF_ROUTE "DEFAULT_NFCF_ROUTE"
#define NAME_DEFAULT_SYS_CODE "DEFAULT_SYS_CODE"
#define NAME_DEFAULT_SYS_CODE_ROUTE "DEFAULT_SYS_CODE_ROUTE"
#define NAME_DEFAULT_SYS_CODE_PWR_STATE "DEFAULT_SYS_CODE_PWR_STATE"
#define NAME_DEFAULT_ROUTE "DEFAULT_ROUTE"
#define NAME_OFF_HOST_ESE_PIPE_ID "OFF_HOST_ESE_PIPE_ID"
#define NAME_OFF_HOST_SIM_PIPE_ID "OFF_HOST_SIM_PIPE_ID"
#define NAME_ISO_DEP_MAX_TRANSCEIVE "ISO_DEP_MAX_TRANSCEIVE"
#define NAME_DEVICE_HOST_WHITE_LIST "DEVICE_HOST_WHITE_LIST"
#define NAME_OFFHOST_ROUTE_ESE "OFFHOST_ROUTE_ESE"
#define NAME_OFFHOST_ROUTE_UICC "OFFHOST_ROUTE_UICC"
#define NAME_DEFAULT_ISODEP_ROUTE "DEFAULT_ISODEP_ROUTE"
#define NAME_OFFHOST_AID_ROUTE_PWR_STATE "OFFHOST_AID_ROUTE_PWR_STATE"
#define NAME_LEGACY_MIFARE_READER "LEGACY_MIFARE_READER"
#if (NXP_EXTNS == TRUE)
#define NAME_NFA_DM_DISC_NTF_TIMEOUT "NFA_DM_DISC_NTF_TIMEOUT"
#define NAME_DEFAULT_AID_ROUTE "DEFAULT_AID_ROUTE"
#define NAME_DEFAULT_MIFARE_CLT_ROUTE "DEFAULT_MIFARE_CLT_ROUTE"
#define NAME_DEFAULT_FELICA_CLT_ROUTE "DEFAULT_FELICA_CLT_ROUTE"
#define NAME_DEFAULT_AID_PWR_STATE "DEFAULT_AID_PWR_STATE"
#define NAME_DEFAULT_DESFIRE_PWR_STATE "DEFAULT_DESFIRE_PWR_STATE"
#define NAME_DEFAULT_MIFARE_CLT_PWR_STATE "DEFAULT_MIFARE_CLT_PWR_STATE"
#define NAME_DEFAULT_FELICA_CLT_PWR_STATE "DEFAULT_FELICA_CLT_PWR_STATE"
#define NAME_HOST_LISTEN_TECH_MASK "HOST_LISTEN_TECH_MASK"
#define NAME_FORWARD_FUNCTIONALITY_ENABLE "FORWARD_FUNCTIONALITY_ENABLE"
#define NAME_NXP_DEFAULT_UICC2_SELECT "NXP_DEFAULT_UICC2_SELECT"
#define NAME_NXP_WM_MAX_WTX_COUNT "NXP_WM_MAX_WTX_COUNT"
#define NAME_NXP_NCI_CREDIT_NTF_TIMEOUT "NXP_NCI_CREDIT_NTF_TIMEOUT"
#define NAME_NXP_SMB_TRANSCEIVE_TIMEOUT "NXP_SMB_TRANSCEIVE_TIMEOUT"
#define NAME_NXP_NFA_DM_DISC_NTF_TIMEOUT "NXP_NFA_DM_DISC_NTF_TIMEOUT"
#define NAME_CHECK_DEFAULT_PROTO_SE_ID "NXP_CHECK_DEFAULT_PROTO_SE_ID"
#define NAME_NXP_DUAL_UICC_ENABLE "NXP_DUAL_UICC_ENABLE"
#define NAME_NXP_SE_COLD_TEMP_ERROR_DELAY "NXP_SE_COLD_TEMP_ERROR_DELAY"
#define NAME_NXP_SMB_ERROR_RETRY "NXP_SMB_ERROR_RETRY"
#define NAME_NXPLOG_EXTNS_LOGLEVEL "NXPLOG_EXTNS_LOGLEVEL"
#define NAME_NXPLOG_NCIX_LOGLEVEL "NXPLOG_NCIX_LOGLEVEL"
#define NAME_NXPLOG_NCIR_LOGLEVEL "NXPLOG_NCIR_LOGLEVEL"
#define NAME_NXPLOG_FWDNLD_LOGLEVEL "NXPLOG_FWDNLD_LOGLEVEL"
#define NAME_NXPLOG_TML_LOGLEVEL "NXPLOG_TML_LOGLEVEL"
#define NAME_NXP_DEFAULT_SE "NXP_DEFAULT_SE"
#define NAME_NXP_SWP_RD_TAG_OP_TIMEOUT "NXP_SWP_RD_TAG_OP_TIMEOUT"
#define NAME_DEFUALT_GSMA_PWR_STATE "DEFUALT_GSMA_PWR_STATE"
#define NAME_NFA_CONFIG_FORMAT "NFA_CONFIG_FORMAT"
#define NAME_NXPLOG_EXTNS_LOGLEVEL "NXPLOG_EXTNS_LOGLEVEL"
#define NAME_NXPLOG_NCIHAL_LOGLEVEL "NXPLOG_NCIHAL_LOGLEVEL"
#define NAME_NXPLOG_NCIX_LOGLEVEL "NXPLOG_NCIX_LOGLEVEL"
#define NAME_NXPLOG_NCIR_LOGLEVEL "NXPLOG_NCIR_LOGLEVEL"
#define NAME_NXPLOG_FWDNLD_LOGLEVEL "NXPLOG_FWDNLD_LOGLEVEL"
#define NAME_NXPLOG_TML_LOGLEVEL "NXPLOG_TML_LOGLEVEL"
#define NAME_NXP_SE_APDU_GATE_SUPPORT "NXP_SE_APDU_GATE_SUPPORT"
#define NAME_NXP_POLL_FOR_EFD_TIMEDELAY "NXP_POLL_FOR_EFD_TIMEDELAY"
#define NAME_NXP_NFCC_MERGE_SAK_ENABLE "NXP_NFCC_MERGE_SAK_ENABLE"
#define NAME_NXP_STAG_TIMEOUT_CFG "NXP_STAG_TIMEOUT_CFG"
#define NAME_NXP_RF_FILE_VERSION_INFO "NXP_RF_FILE_VERSION_INFO"
#define NAME_RF_STORAGE "RF_STORAGE"
#define NAME_FW_STORAGE "FW_STORAGE"
#define NAME_NXP_CORE_CONF "NXP_CORE_CONF"
#endif

class NfcConfig {
 public:
  static bool hasKey(const std::string& key);
  static std::string getString(const std::string& key);
  static std::string getString(const std::string& key,
                               std::string default_value);
  static unsigned getUnsigned(const std::string& key);
  static unsigned getUnsigned(const std::string& key, unsigned default_value);
  static std::vector<uint8_t> getBytes(const std::string& key);
  static void clear();

 private:
  void loadConfig();
  static NfcConfig& getInstance();
  NfcConfig();

  ConfigFile config_;
};
