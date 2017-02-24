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
 *  Copyright (C) 2015 NXP Semiconductors
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
#ifndef NXP_NFCC_FEATURES_H
#include "NXP_NFCC_Features.h"
#endif
#ifdef __cplusplus
extern "C"
{
#endif

int GetNxpStrValue(const char* name, char* p_value, unsigned long len);
int GetNxpNumValue(const char* name, void* p_value, unsigned long len);
int GetNxpByteArrayValue(const char* name, char* pValue,long bufflen, long *len);
void resetNxpConfig(void);
int isNxpRFConfigModified();
int isNxpConfigModified();
int updateNxpConfigTimestamp();

#ifdef __cplusplus
};
#endif

#define NAME_NXPLOG_EXTNS_LOGLEVEL   "NXPLOG_EXTNS_LOGLEVEL"
#define NAME_NXPLOG_NCIHAL_LOGLEVEL  "NXPLOG_NCIHAL_LOGLEVEL"
#define NAME_NXPLOG_NCIX_LOGLEVEL    "NXPLOG_NCIX_LOGLEVEL"
#define NAME_NXPLOG_NCIR_LOGLEVEL    "NXPLOG_NCIR_LOGLEVEL"
#define NAME_NXPLOG_FWDNLD_LOGLEVEL  "NXPLOG_FWDNLD_LOGLEVEL"
#define NAME_NXPLOG_TML_LOGLEVEL     "NXPLOG_TML_LOGLEVEL"

#define NAME_MIFARE_READER_ENABLE    "MIFARE_READER_ENABLE"
#define NAME_FW_STORAGE              "FW_STORAGE"
#define NAME_NXP_NFC_DEV_NODE        "NXP_NFC_DEV_NODE"
#define NAME_NXP_FW_NAME             "NXP_FW_NAME"
#define NAME_NXP_FW_PROTECION_OVERRIDE "NXP_FW_PROTECION_OVERRIDE"
#define NAME_NXP_SYS_CLK_SRC_SEL     "NXP_SYS_CLK_SRC_SEL"
#define NAME_NXP_SYS_CLK_FREQ_SEL    "NXP_SYS_CLK_FREQ_SEL"
#define NAME_NXP_SYS_CLOCK_TO_CFG    "NXP_SYS_CLOCK_TO_CFG"
#define NAME_NXP_ACT_PROP_EXTN       "NXP_ACT_PROP_EXTN"
#define NAME_NXP_EXT_TVDD_CFG        "NXP_EXT_TVDD_CFG"
#define NAME_NXP_EXT_TVDD_CFG_1      "NXP_EXT_TVDD_CFG_1"
#define NAME_NXP_EXT_TVDD_CFG_2      "NXP_EXT_TVDD_CFG_2"
#define NAME_NXP_EXT_TVDD_CFG_3      "NXP_EXT_TVDD_CFG_3"
#define NAME_NXP_RF_CONF_BLK_1       "NXP_RF_CONF_BLK_1"
#define NAME_NXP_RF_CONF_BLK_2       "NXP_RF_CONF_BLK_2"
#define NAME_NXP_RF_CONF_BLK_3       "NXP_RF_CONF_BLK_3"
#define NAME_NXP_RF_CONF_BLK_4       "NXP_RF_CONF_BLK_4"
#define NAME_NXP_RF_CONF_BLK_5       "NXP_RF_CONF_BLK_5"
#define NAME_NXP_RF_CONF_BLK_6       "NXP_RF_CONF_BLK_6"
#define NAME_NXP_CORE_CONF_EXTN      "NXP_CORE_CONF_EXTN"
#define NAME_NXP_CORE_CONF           "NXP_CORE_CONF"
#define NAME_NXP_CORE_MFCKEY_SETTING "NXP_CORE_MFCKEY_SETTING"
#define NAME_NXP_CORE_STANDBY        "NXP_CORE_STANDBY"
#define NAME_NXP_NFC_PROFILE_EXTN    "NXP_NFC_PROFILE_EXTN"
#define NAME_NXP_CHINA_TIANJIN_RF_ENABLED  "NXP_CHINA_TIANJIN_RF_ENABLED"
#define NAME_NXP_CHINA_BLK_NUM_CHK_ENABLE  "NXP_CN_TRANSIT_BLK_NUM_CHECK_ENABLE"
#if(NXP_ESE_POWER_MODE==TRUE)
#define NAME_NXP_ESE_POWER_DH_CONTROL         "NXP_ESE_POWER_DH_CONTROL"
#define NAME_NXP_ESE_POWER_EXT_PMU            "NXP_ESE_POWER_EXT_PMU"
#define NAME_NXP_ESE_POWER_DH_CONTROL_CFG_1   "NXP_ESE_POWER_DH_CONTROL_CFG_1"
#endif
#define NAME_NXP_SWP_SWITCH_TIMEOUT  "NXP_SWP_SWITCH_TIMEOUT"
#define NAME_NXP_SWP_FULL_PWR_ON     "NXP_SWP_FULL_PWR_ON"
#define NAME_NXP_CORE_RF_FIELD       "NXP_CORE_RF_FIELD"
#define NAME_NXP_NFC_MERGE_RF_PARAMS "NXP_NFC_MERGE_RF_PARAMS"
#define NAME_NXP_I2C_FRAGMENTATION_ENABLED "NXP_I2C_FRAGMENTATION_ENABLED"
#define NAME_AID_MATCHING_PLATFORM "AID_MATCHING_PLATFORM"
#define NAME_NXP_TYPEA_UICC_BAUD_RATE "NXP_TYPEA_UICC_BAUD_RATE"
#define NAME_NXP_TYPEB_UICC_BAUD_RATE "NXP_TYPEB_UICC_BAUD_RATE"
#define NAME_NXP_SET_CONFIG_ALWAYS "NXP_SET_CONFIG_ALWAYS"
#if((NFC_NXP_CHIP_TYPE!=PN547C2) && (NXP_NFCC_ROUTING_BLOCK_BIT_PROP==TRUE))
#define NAME_NXP_PROP_BLACKLIST_ROUTING "NXP_PROP_BLACKLIST_ROUTING"
#endif
#define NAME_NXP_WIREDMODE_RESUME_TIMEOUT  "NXP_WIREDMODE_RESUME_TIMEOUT"
#define NAME_NXP_UICC_LISTEN_TECH_MASK      "UICC_LISTEN_TECH_MASK"
#define NAME_NXP_HOST_LISTEN_TECH_MASK      "HOST_LISTEN_TECH_MASK"
#if(NXP_ESE_SVDD_SYNC == TRUE)
#define NAME_NXP_SVDD_SYNC_OFF_DELAY "NXP_SVDD_SYNC_OFF_DELAY"
#endif
/* default configuration */
#define default_storage_location "/data/nfc"

#endif
