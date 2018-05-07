/******************************************************************************
 *
 *  Copyright (C) 1999-2012 Broadcom Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/
/******************************************************************************
 *
 *  The original Work has been changed by NXP Semiconductors.
 *
 *  Copyright (C) 2015-2018 NXP Semiconductors
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
 ******************************************************************************/
#ifndef __CONFIG_H
#define __CONFIG_H


int GetStrValue(const char* name, char* p_value, unsigned long len);
int GetNumValue(const char* name, void* p_value, unsigned long len);

#define NAME_POLLING_TECH_MASK "POLLING_TECH_MASK"
#define NAME_APPL_TRACE_LEVEL "APPL_TRACE_LEVEL"
#define NAME_USE_RAW_NCI_TRACE "USE_RAW_NCI_TRACE"
#define NAME_NXP_NFC_CHIP "NXP_NFC_CHIP"
#define NAME_PROTOCOL_TRACE_LEVEL "PROTOCOL_TRACE_LEVEL"
#define NAME_NFA_DM_CFG "NFA_DM_CFG"
#define NAME_SCREEN_OFF_POWER_STATE "SCREEN_OFF_POWER_STATE"
#define NAME_NFA_STORAGE "NFA_STORAGE"
#define NAME_UICC_LISTEN_TECH_MASK "UICC_LISTEN_TECH_MASK"
#define NAME_P2P_LISTEN_TECH_MASK "P2P_LISTEN_TECH_MASK"
#if (NXP_EXTNS == TRUE)
#define NAME_DEFAULT_AID_ROUTE "DEFAULT_AID_ROUTE"
#define NAME_DEFAULT_DESFIRE_ROUTE "DEFAULT_DESFIRE_ROUTE"
#define NAME_DEFAULT_MIFARE_CLT_ROUTE "DEFAULT_MIFARE_CLT_ROUTE"
#define NAME_DEFAULT_AID_PWR_STATE "DEFAULT_AID_PWR_STATE"
#define NAME_DEFAULT_DESFIRE_PWR_STATE "DEFAULT_DESFIRE_PWR_STATE"
#define NAME_DEFAULT_MIFARE_CLT_PWR_STATE "DEFAULT_MIFARE_CLT_PWR_STATE"
#define NAME_CHECK_DEFAULT_PROTO_SE_ID "NXP_CHECK_DEFAULT_PROTO_SE_ID"
#define NAME_NFA_DM_DISC_NTF_TIMEOUT "NFA_DM_DISC_NTF_TIMEOUT"
#define NAME_NXP_FWD_FUNCTIONALITY_ENABLE "NXP_FWD_FUNCTIONALITY_ENABLE"
#define NAME_HOST_LISTEN_TECH_MASK "HOST_LISTEN_TECH_MASK"
#define NAME_DEFAULT_FELICA_CLT_ROUTE "DEFAULT_FELICA_CLT_ROUTE"
#define NAME_DEFAULT_FELICA_CLT_PWR_STATE "DEFAULT_FELICA_CLT_PWR_STATE"
#define NAME_NXP_ESE_LISTEN_TECH_MASK "NXP_ESE_LISTEN_TECH_MASK"
#define NAME_NXP_ESE_PWR_MGMT_PROP "NXP_ESE_PWR_MGMT_PROP"
#endif
#define NAME_DEFAULT_OFFHOST_ROUTE "DEFAULT_OFFHOST_ROUTE"
#define NAME_NFA_DM_DISC_DURATION_POLL "NFA_DM_DISC_DURATION_POLL"
#define NAME_AID_FOR_EMPTY_SELECT "AID_FOR_EMPTY_SELECT"
#define NAME_PRESERVE_STORAGE "PRESERVE_STORAGE"
#define NAME_NFA_MAX_EE_SUPPORTED "NFA_MAX_EE_SUPPORTED"
#define NAME_POLL_FREQUENCY "POLL_FREQUENCY"
#define NAME_PRESENCE_CHECK_ALGORITHM "PRESENCE_CHECK_ALGORITHM"
#define NAME_DEVICE_HOST_WHITE_LIST "DEVICE_HOST_WHITE_LIST"
#define NAME_NFA_POLL_BAIL_OUT_MODE "NFA_POLL_BAIL_OUT_MODE"
#define NAME_NFA_PROPRIETARY_CFG "NFA_PROPRIETARY_CFG"
#define NAME_NFA_AID_BLOCK_ROUTE "NFA_AID_BLOCK_ROUTE"
#if (NXP_EXTNS == TRUE)
#define NAME_NXP_ESE_LISTEN_TECH_MASK   "NXP_ESE_LISTEN_TECH_MASK"
#define NAME_NXP_WM_MAX_WTX_COUNT       "NXP_WM_MAX_WTX_COUNT"
#define NAME_NXP_NFCC_STANDBY_TIMEOUT "NXP_NFCC_STANDBY_TIMEOUT"
#define NAME_NXP_CP_TIMEOUT "NXP_CP_TIMEOUT"
#define NAME_NXP_CORE_SCRN_OFF_AUTONOMOUS_ENABLE \
  "NXP_CORE_SCRN_OFF_AUTONOMOUS_ENABLE"
#define NAME_NXP_ALLOW_WIRED_IN_MIFARE_DESFIRE_CLT \
  "NXP_ALLOW_WIRED_IN_MIFARE_DESFIRE_CLT"
#define NAME_NXP_NFCC_RF_FIELD_EVENT_TIMEOUT "NXP_NFCC_RF_FIELD_EVENT_TIMEOUT"
#define NAME_NFA_BLOCK_ROUTE "NFA_BLOCK_ROUTE"
#define NAME_NXP_MIFARE_DESFIRE_DISABLE "NXP_MIFARE_DESFIRE_DISABLE"
#define NAME_NXP_DUAL_UICC_ENABLE "NXP_DUAL_UICC_ENABLE"
#define NAME_NXP_DEFAULT_NFCEE_TIMEOUT "NXP_DEFAULT_NFCEE_TIMEOUT"
#define NAME_NXP_DEFAULT_NFCEE_DISC_TIMEOUT "NXP_DEFAULT_NFCEE_DISC_TIMEOUT"
#define NAME_NXP_NFCC_PASSIVE_LISTEN_TIMEOUT "NXP_NFCC_PASSIVE_LISTEN_TIMEOUT"
#define NAME_OS_DOWNLOAD_TIMEOUT_VALUE "OS_DOWNLOAD_TIMEOUT_VALUE"
#define NAME_NXP_HCEF_CMD_RSP_TIMEOUT_VALUE "NXP_HCEF_CMD_RSP_TIMEOUT_VALUE"
#endif

struct tUART_CONFIG {
  int m_iBaudrate;  // 115200
  int m_iDatabits;  // 8
  int m_iParity;    // 0 - none, 1 = odd, 2 = even
  int m_iStopbits;
};

#endif
