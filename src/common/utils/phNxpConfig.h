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

#define NAME_NXP_STAG_TIMEOUT_CFG "NXP_STAG_TIMEOUT_CFG"
#define NAME_NXP_POLL_FOR_EFD_TIMEDELAY "NXP_POLL_FOR_EFD_TIMEDELAY"
#define NAME_NXP_SYS_CLK_SRC_SEL "NXP_SYS_CLK_SRC_SEL"
#define NAME_NXP_SYS_CLK_FREQ_SEL "NXP_SYS_CLK_FREQ_SEL"
#define NAME_NXP_SWP_RD_TAG_OP_TIMEOUT "NXP_SWP_RD_TAG_OP_TIMEOUT"
#define NAME_NXP_DM_DISC_NTF_TIMEOUT "NXP_DM_DISC_NTF_TIMEOUT"
#define NAME_NXP_RDR_REQ_GUARD_TIME "NXP_RDR_REQ_GUARD_TIME"
#define NAME_NXP_PROP_RESET_EMVCO_CMD "NXP_PROP_RESET_EMVCO_CMD"
#define NAME_NXP_RF_FILE_VERSION_INFO "NXP_RF_FILE_VERSION_INFO"

#endif // __CONFIG_H
