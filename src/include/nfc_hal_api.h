/******************************************************************************
 *
 *  Copyright (C) 2012-2014 Broadcom Corporation
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
 *  Copyright 2015-2020 NXP
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

/******************************************************************************
 *
 *  NFC Hardware Abstraction Layer API
 *
 ******************************************************************************/
#ifndef NFC_HAL_API_H
#define NFC_HAL_API_H
#include <hardware/nfc.h>
#include "data_types.h"
#include "nfc_hal_target.h"

typedef struct {
  bool isGetcfg;
  uint8_t total_duration[4];
  uint8_t total_duration_len;
  uint8_t atr_req_gen_bytes[48];
  uint8_t atr_req_gen_bytes_len;
  uint8_t atr_res_gen_bytes[48];
  uint8_t atr_res_gen_bytes_len;
  uint8_t pmid_wt[3];
  uint8_t pmid_wt_len;
} tNxpNci_getCfg_info_t;

typedef enum {
  NFC_HCI_INIT_COMPLETE = 0x00,/* Status of HCI initialization     */
  NFC_HCI_INIT_START = 0x01
} tNFC_HCI_INIT_STATUS;

enum tNxpEseState  : uint64_t {
    tNFC_ESE_IDLE_MODE = 0,
    tNFC_ESE_WIRED_MODE
};

typedef struct phNxpNci_Extn_Cmd{
  uint16_t cmd_len;
  uint8_t p_cmd[256];
}phNxpNci_Extn_Cmd_t;

typedef struct phNxpNci_Extn_Resp{
  uint32_t status;
  uint16_t rsp_len;
  uint8_t p_rsp[256];
}phNxpNci_Extn_Resp_t;

typedef uint8_t tHAL_NFC_STATUS;
typedef void(tHAL_NFC_STATUS_CBACK)(tHAL_NFC_STATUS status);
typedef void(tHAL_NFC_CBACK)(uint8_t event, tHAL_NFC_STATUS status);
typedef void(tHAL_NFC_DATA_CBACK)(uint16_t data_len, uint8_t* p_data);

/*******************************************************************************
** tHAL_NFC_ENTRY HAL entry-point lookup table
*******************************************************************************/

typedef void(tHAL_API_INITIALIZE)(void);
typedef void(tHAL_API_TERMINATE)(void);
typedef void(tHAL_API_OPEN)(tHAL_NFC_CBACK* p_hal_cback,
                            tHAL_NFC_DATA_CBACK* p_data_cback);
typedef void(tHAL_API_CLOSE)(void);
typedef void(tHAL_API_CORE_INITIALIZED)(uint16_t data_len,
                                        uint8_t* p_core_init_rsp_params);
typedef void(tHAL_API_WRITE)(uint16_t data_len, uint8_t* p_data);
typedef bool(tHAL_API_PREDISCOVER)(void);
typedef void(tHAL_API_CONTROL_GRANTED)(void);
typedef void(tHAL_API_POWER_CYCLE)(void);
typedef uint8_t(tHAL_API_GET_MAX_NFCEE)(void);
#if (NXP_EXTNS == TRUE)
typedef int(tHAL_API_IOCTL)(long arg, void* p_data);
typedef int(tHAL_API_GET_FW_DWNLD_FLAG)(uint8_t* fwDnldRequest);
typedef bool(tHAL_API_SET_NXP_TRANSIT_CONFIG)(char* strval);
#endif
typedef uint16_t(tHAL_API_spiDwpSync)(uint32_t level);
typedef uint16_t(tHAL_API_RelForceDwpOnOffWait)(uint32_t level);
typedef int32_t(tHAL_API_HciInitUpdateState) (tNFC_HCI_INIT_STATUS HciStatus);
typedef uint32_t(tHAL_API_setEseState)(tNxpEseState tESEstate);
typedef uint8_t(tHAL_API_getchipType)(void);
typedef uint16_t(tHAL_API_setNfcServicePid)(uint64_t NfcNxpServicePid);
typedef uint32_t(tHAL_API_getEseState)(void);
typedef void (tHAL_API_GetCachedNfccConfig)(tNxpNci_getCfg_info_t *nxpNciAtrInfo);
typedef uint32_t (tHAL_API_nciTransceive)(phNxpNci_Extn_Cmd_t* in,phNxpNci_Extn_Resp_t* out);

typedef struct {
  tHAL_API_INITIALIZE* initialize;
  tHAL_API_TERMINATE* terminate;
  tHAL_API_OPEN* open;
  tHAL_API_CLOSE* close;
  tHAL_API_CORE_INITIALIZED* core_initialized;
  tHAL_API_WRITE* write;
  tHAL_API_PREDISCOVER* prediscover;
  tHAL_API_CONTROL_GRANTED* control_granted;
  tHAL_API_POWER_CYCLE* power_cycle;
  tHAL_API_GET_MAX_NFCEE* get_max_ee;
#if (NXP_EXTNS == TRUE)
  tHAL_API_IOCTL* ioctl;
  tHAL_API_GET_FW_DWNLD_FLAG* check_fw_dwnld_flag;
  tHAL_API_SET_NXP_TRANSIT_CONFIG* set_transit_config;
#endif
  tHAL_API_spiDwpSync* spiDwpSync;
  tHAL_API_RelForceDwpOnOffWait* RelForceDwpOnOffWait;
  tHAL_API_HciInitUpdateState* HciInitUpdateState;
  tHAL_API_setEseState* setEseState;
  tHAL_API_getchipType* getchipType;
  tHAL_API_setNfcServicePid* setNfcServicePid;
  tHAL_API_getEseState* getEseState;
  tHAL_API_GetCachedNfccConfig* GetCachedNfccConfig;
  tHAL_API_nciTransceive* nciTransceive;
} tHAL_NFC_ENTRY;

#if (NXP_EXTNS == TRUE)
typedef struct {
  tHAL_NFC_ENTRY* hal_entry_func;
  uint8_t boot_mode;
  bool    isLowRam;
} tHAL_NFC_CONTEXT;
#endif
/*******************************************************************************
** HAL API Function Prototypes
*******************************************************************************/

#endif /* NFC_HAL_API_H  */
