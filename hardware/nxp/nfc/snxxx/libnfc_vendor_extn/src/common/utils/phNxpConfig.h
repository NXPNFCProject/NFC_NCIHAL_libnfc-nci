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
 *  The original Work has been changed by NXP.
 *
 *  Copyright 2013-2025 NXP
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

#ifdef __cplusplus
extern "C" {
#endif

int GetNxpStrValue(const char *name, char *p_value, unsigned long len);
int GetNxpNumValue(const char *name, void *p_value, unsigned long len);
int GetNxpByteArrayValue(const char *name, char *pValue, long bufflen,
                         long *len);
void resetNxpConfig(void);
int isNxpRFConfigModified();
int isNxpConfigModified();
int updateNxpConfigTimestamp();
int updateNxpRfConfigTimestamp();
void setNxpRfConfigPath(const char *name);
void setNxpFwConfigPath();

#ifdef __cplusplus
};
#endif

#define NXP_MAX_CONFIG_STRING_LEN 260
#define NAME_NXP_STAG_TIMEOUT_CFG "NXP_STAG_TIMEOUT_CFG"
#define NAME_NXP_SYS_CLK_SRC_SEL "NXP_SYS_CLK_SRC_SEL"
#define NAME_NXP_SYS_CLK_FREQ_SEL "NXP_SYS_CLK_FREQ_SEL"
#define NAME_NXP_SWP_RD_TAG_OP_TIMEOUT "NXP_SWP_RD_TAG_OP_TIMEOUT"
#define NAME_NXP_DM_DISC_NTF_TIMEOUT "NXP_DM_DISC_NTF_TIMEOUT"
#define NAME_NXP_RDR_REQ_GUARD_TIME "NXP_RDR_REQ_GUARD_TIME"
#define NAME_NXP_PROP_RESET_EMVCO_CMD "NXP_PROP_RESET_EMVCO_CMD"
#define NAME_NXP_RF_FILE_VERSION_INFO "NXP_RF_FILE_VERSION_INFO"
#define NAME_NXP_T4T_NFCEE_ENABLE "NXP_T4T_NFCEE_ENABLE"
#define NAME_NXP_SRD_TIMEOUT "NXP_SRD_TIMEOUT"
#define NAME_T4T_NFCEE_ENABLE "T4T_NFCEE_ENABLE"
#define NAME_OFF_HOST_SIM_PIPE_ID "OFF_HOST_SIM_PIPE_ID"
#define NAME_OFF_HOST_SIM2_PIPE_ID "OFF_HOST_SIM2_PIPE_ID"
#define NAME_OFF_HOST_ESIM_PIPE_ID "OFF_HOST_ESIM_PIPE_ID"
#define NAME_OFF_HOST_ESIM2_PIPE_ID "OFF_HOST_ESIM2_PIPE_ID"
#define NAME_OFF_HOST_SIM_PIPE_IDS "OFF_HOST_SIM_PIPE_IDS"
#define NAME_NFA_PROPRIETARY_CFG "NFA_PROPRIETARY_CFG"
#define NAME_DEFAULT_ROUTE "DEFAULT_ROUTE"
#define NAME_DEFAULT_AID_ROUTE "DEFAULT_AID_ROUTE"
#define NAME_DEFAULT_ISODEP_ROUTE "DEFAULT_ISODEP_ROUTE"
#define NAME_DEFAULT_FELICA_CLT_ROUTE "DEFAULT_FELICA_CLT_ROUTE"
#define NAME_DEFAULT_SYS_CODE_ROUTE "DEFAULT_SYS_CODE_ROUTE"
#define NAME_DEFAULT_NFCF_ROUTE "DEFAULT_NFCF_ROUTE"
#define NAME_DEFAULT_OFFHOST_ROUTE "DEFAULT_OFFHOST_ROUTE"
#define NAME_DEFAULT_MIFARE_CLT_ROUTE "DEFAULT_MIFARE_CLT_ROUTE"
#define NAME_OFFHOST_ROUTE_ESE "OFFHOST_ROUTE_ESE"
#define NAME_OFFHOST_ROUTE_UICC "OFFHOST_ROUTE_UICC"
#define NAME_NXP_EXTENDED_FIELD_DETECT_MODE "NXP_EXTENDED_FIELD_DETECT_MODE"
#define NAME_MIFARE_READER_ENABLE "MIFARE_READER_ENABLE"

#endif // __CONFIG_H
