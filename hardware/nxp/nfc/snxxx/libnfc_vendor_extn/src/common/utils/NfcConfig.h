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
#pragma once

#include <config.h>

#include <string>
#include <vector>

/* Configs from libnfc-nci.conf */
#define NAME_NFC_DEBUG_ENABLED "NFC_DEBUG_ENABLED"
#define NAME_NFA_STORAGE "NFA_STORAGE"
#define NAME_PRESERVE_STORAGE "PRESERVE_STORAGE"
#define NAME_POLLING_TECH_MASK "POLLING_TECH_MASK"
#define NAME_UICC_LISTEN_TECH_MASK "UICC_LISTEN_TECH_MASK"
#define NAME_OFFHOST_LISTEN_TECH_MASK "OFFHOST_LISTEN_TECH_MASK"
#define NAME_HOST_LISTEN_TECH_MASK "HOST_LISTEN_TECH_MASK"
#define NAME_NFA_DM_CFG "NFA_DM_CFG"
#define NAME_SCREEN_OFF_POWER_STATE "SCREEN_OFF_POWER_STATE"
#define NAME_NFA_MAX_EE_SUPPORTED "NFA_MAX_EE_SUPPORTED"
#define NAME_NFA_DM_DISC_DURATION_POLL "NFA_DM_DISC_DURATION_POLL"
#define NAME_POLL_FREQUENCY "POLL_FREQUENCY"
#define NAME_NFA_AID_BLOCK_ROUTE "NFA_AID_BLOCK_ROUTE"
#define NAME_AID_FOR_EMPTY_SELECT "AID_FOR_EMPTY_SELECT"
#define NAME_AID_MATCHING_MODE "AID_MATCHING_MODE"
#define NAME_OFFHOST_AID_ROUTE_PWR_STATE "OFFHOST_AID_ROUTE_PWR_STATE"
#define NAME_RECOVERY_OPTION "RECOVERY_OPTION"
#define NAME_ALWAYS_ON_SET_EE_POWER_AND_LINK_CONF \
  "ALWAYS_ON_SET_EE_POWER_AND_LINK_CONF"
#define NAME_DISABLE_ALWAYS_ON_SET_EE_POWER_AND_LINK_CONF \
  "DISABLE_ALWAYS_ON_SET_EE_POWER_AND_LINK_CONF"
#define NAME_NCI_RESET_TYPE "NCI_RESET_TYPE"
#define NAME_MUTE_TECH_ROUTE_OPTION "MUTE_TECH_ROUTE_OPTION"
#define NAME_NFA_DM_LISTEN_ACTIVE_DEACT_NTF_TIMEOUT \
  "NFA_DM_LISTEN_ACTIVE_DEACT_NTF_TIMEOUT"
#define NAME_EUICC_MEP_MODE "EUICC_MEP_MODE"
#define NAME_NFCEE_EVENT_RF_DISCOVERY_OPTION "NFCEE_EVENT_RF_DISCOVERY_OPTION"
/* Configs from vendor interface */
#define NAME_NFA_POLL_BAIL_OUT_MODE "NFA_POLL_BAIL_OUT_MODE"
#define NAME_PRESENCE_CHECK_ALGORITHM "PRESENCE_CHECK_ALGORITHM"
#define NAME_NFA_PROPRIETARY_CFG "NFA_PROPRIETARY_CFG"
#define NAME_DEFAULT_OFFHOST_ROUTE "DEFAULT_OFFHOST_ROUTE"
#define NAME_OFFHOST_ROUTE_ESE "OFFHOST_ROUTE_ESE"
#define NAME_OFFHOST_ROUTE_UICC "OFFHOST_ROUTE_UICC"
#define NAME_DEFAULT_NFCF_ROUTE "DEFAULT_NFCF_ROUTE"
#define NAME_DEFAULT_SYS_CODE "DEFAULT_SYS_CODE"
#define NAME_DEFAULT_SYS_CODE_ROUTE "DEFAULT_SYS_CODE_ROUTE"
#define NAME_DEFAULT_SYS_CODE_PWR_STATE "DEFAULT_SYS_CODE_PWR_STATE"
#define NAME_DEFAULT_ROUTE "DEFAULT_ROUTE"
#define NAME_OFF_HOST_ESE_PIPE_ID "OFF_HOST_ESE_PIPE_ID"
#define NAME_OFF_HOST_SIM_PIPE_ID "OFF_HOST_SIM_PIPE_ID"
#define NAME_OFF_HOST_SIM_PIPE_IDS "OFF_HOST_SIM_PIPE_IDS"
#define NAME_ISO_DEP_MAX_TRANSCEIVE "ISO_DEP_MAX_TRANSCEIVE"
#define NAME_DEVICE_HOST_ALLOW_LIST "DEVICE_HOST_ALLOW_LIST"
#define NAME_DEFAULT_ISODEP_ROUTE "DEFAULT_ISODEP_ROUTE"
#define NAME_PRESENCE_CHECK_RETRY_COUNT "PRESENCE_CHECK_RETRY_COUNT"
#define NAME_ISO15693_SKIP_GET_SYS_INFO_CMD "ISO15693_SKIP_GET_SYS_INFO_CMD"
#define NAME_DEFAULT_T4TNFCEE_AID_POWER_STATE "DEFAULT_T4TNFCEE_AID_POWER_STATE"
#define NAME_T4T_NDEF_NFCEE_AID "T4T_NDEF_NFCEE_AID"
#define NAME_DEFAULT_NDEF_NFCEE_ROUTE "DEFAULT_NDEF_NFCEE_ROUTE"
#define NAME_T4T_NFCEE_ENABLE "T4T_NFCEE_ENABLE"
#define NAME_NFA_EE_ROUTE_DEBOUNCE_TIMER "NFA_EE_ROUTE_DEBOUNCE_TIMER"
#define NAME_NFA_DM_DISC_NTF_TIMEOUT "NFA_DM_DISC_NTF_TIMEOUT"

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
  NfcConfig() {};
  static NfcConfig& getInstance();
  ConfigFile config_;
};
