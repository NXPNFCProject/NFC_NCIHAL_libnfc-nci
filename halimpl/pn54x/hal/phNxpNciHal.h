/*
 * Copyright (C) 2015 NXP Semiconductors
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
#ifndef _PHNXPNCIHAL_H_
#define _PHNXPNCIHAL_H_

#include <hardware/nfc.h>
#include <phNxpNciHal_utils.h>
#include "NxpNfcCapability.h"
#include "nfc_hal_api.h"

/********************* Definitions and structures *****************************/
#define MAX_RETRY_COUNT 5
#define NCI_MAX_DATA_LEN 300
#define NCI_POLL_DURATION 500
#define HAL_NFC_ENABLE_I2C_FRAGMENTATION_EVT 0x07
#define NXP_STAG_TIMEOUT_BUF_LEN 0x04 /*FIXME:TODO:remove*/
#define NXP_WIREDMODE_RESUME_TIMEOUT_LEN 0x04
#define NFCC_DECIDES 00
#define POWER_ALWAYS_ON 01
#define LINK_ALWAYS_ON 02
#undef P2P_PRIO_LOGIC_HAL_IMP

#define NCI_VERSION_2_0 0x20
#define NCI_VERSION_1_1 0x11
#define NCI_VERSION_1_0 0x10
#define NCI_VERSION_UNKNOWN 0x00

/*Mem alloc with 8 byte alignment*/
#define size_align(sz) ((((sz)-1) | 7) + 1)
#define nxp_malloc(size) malloc(size_align((size)))

typedef void(phNxpNciHal_control_granted_callback_t)();

/*ROM CODE VERSION FW*/
#define FW_MOBILE_ROM_VERSION_PN551 0x10
#define FW_MOBILE_ROM_VERSION_PN553 0x11
#define FW_MOBILE_ROM_VERSION_PN548AD 0x10
#define FW_MOBILE_ROM_VERSION_PN547C2 0x08
#define FW_MOBILE_ROM_VERSION_PN557 0x12

/* NCI Data */
#define NCI_MT_CMD  0x20
#define NCI_MT_RSP  0x40
#define NCI_MT_NTF  0x60

#define CORE_RESET_TRIGGER_TYPE_CORE_RESET_CMD_RECEIVED 0x02
#define CORE_RESET_TRIGGER_TYPE_POWERED_ON              0x01
#define NCI_MSG_CORE_RESET           0x00
#define NCI_MSG_CORE_INIT            0x01
#define NCI_MT_MASK                  0xE0
#define NCI_OID_MASK                 0x3F
typedef struct nci_data {
  uint16_t len;
  uint8_t p_data[NCI_MAX_DATA_LEN];
} nci_data_t;

typedef enum { HAL_STATUS_CLOSE = 0, HAL_STATUS_OPEN } phNxpNci_HalStatus;

/* Macros to enable and disable extensions */
#define HAL_ENABLE_EXT() (nxpncihal_ctrl.hal_ext_enabled = 1)
#define HAL_DISABLE_EXT() (nxpncihal_ctrl.hal_ext_enabled = 0)

typedef struct phNxpNciInfo {
  uint8_t   nci_version;
  bool_t    wait_for_ntf;
}phNxpNciInfo_t;
/* NCI Control structure */
typedef struct phNxpNciHal_Control {
  phNxpNci_HalStatus halStatus; /* Indicate if hal is open or closed */
  pthread_t client_thread;      /* Integration thread handle */
  uint8_t thread_running;       /* Thread running if set to 1, else set to 0 */
  phLibNfc_sConfig_t gDrvCfg;   /* Driver config data */

  /* Rx data */
  uint8_t* p_rx_data;
  uint16_t rx_data_len;

  /* libnfc-nci callbacks */
  nfc_stack_callback_t* p_nfc_stack_cback;
  nfc_stack_data_callback_t* p_nfc_stack_data_cback;

  /* control granted callback */
  phNxpNciHal_control_granted_callback_t* p_control_granted_cback;

  /* HAL open status */
  bool_t hal_open_status;

  bool_t is_wait_for_ce_ntf;

  /* HAL extensions */
  uint8_t hal_ext_enabled;

  /* Waiting semaphore */
  phNxpNciHal_Sem_t ext_cb_data;

  uint16_t cmd_len;
  uint8_t p_cmd_data[NCI_MAX_DATA_LEN];
  uint16_t rsp_len;
  uint8_t p_rsp_data[NCI_MAX_DATA_LEN];

  /* retry count used to force download */
  uint16_t retry_cnt;
  uint8_t read_retry_cnt;
  phNxpNciInfo_t nci_info;
  uint8_t hal_boot_mode;
} phNxpNciHal_Control_t;

typedef struct {
  uint8_t fw_update_reqd;
  uint8_t rf_update_reqd;
} phNxpNciHal_FwRfupdateInfo_t;

typedef struct phNxpNciClock {
  bool_t isClockSet;
  uint8_t p_rx_data[20];
  bool_t issetConfig;
} phNxpNciClock_t;

typedef struct phNxpNciRfSetting {
  bool_t isGetRfSetting;
  uint8_t p_rx_data[20];
} phNxpNciRfSetting_t;

/*set config management*/

#define TOTAL_DURATION 0x00
#define ATR_REQ_GEN_BYTES_POLL 0x29
#define ATR_REQ_GEN_BYTES_LIS 0x61
#define LEN_WT 0x60

/*Whenever a new get cfg need to be sent,
 * array must be updated with defined config type*/
static const uint8_t get_cfg_arr[] = {TOTAL_DURATION, ATR_REQ_GEN_BYTES_POLL,
                                      ATR_REQ_GEN_BYTES_LIS, LEN_WT};

typedef enum {
  EEPROM_RF_CFG,
  EEPROM_FW_DWNLD,
  EEPROM_WIREDMODE_RESUME_TIMEOUT,
  EEPROM_ESE_SVDD_POWER,
  EEPROM_ESE_POWER_EXT_PMU,
  EEPROM_PROP_ROUTING,
  EEPROM_ESE_SESSION_ID,
  EEPROM_SWP1_INTF,
  EEPROM_SWP1A_INTF,
  EEPROM_SWP2_INTF
} phNxpNci_EEPROM_request_type_t;

typedef struct phNxpNci_EEPROM_info {
  uint8_t request_mode;
  phNxpNci_EEPROM_request_type_t request_type;
  uint8_t update_mode;
  uint8_t* buffer;
  uint8_t bufflen;
} phNxpNci_EEPROM_info_t;

typedef struct phNxpNci_getCfg_info {
  bool_t isGetcfg;
  uint8_t total_duration[4];
  uint8_t total_duration_len;
  uint8_t atr_req_gen_bytes[48];
  uint8_t atr_req_gen_bytes_len;
  uint8_t atr_res_gen_bytes[48];
  uint8_t atr_res_gen_bytes_len;
  uint8_t pmid_wt[3];
  uint8_t pmid_wt_len;
} phNxpNci_getCfg_info_t;

typedef enum {
  NFC_FORUM_PROFILE,
  EMV_CO_PROFILE,
  INVALID_PROFILe
} phNxpNciProfile_t;

typedef enum {
  NFC_NORMAL_BOOT_MODE,
  NFC_FAST_BOOT_MODE,
  NFC_OSU_BOOT_MODE
} phNxpNciBootMode;
/* NXP Poll Profile control structure */
typedef struct phNxpNciProfile_Control {
  phNxpNciProfile_t profile_type;
  uint8_t bClkSrcVal; /* Holds the System clock source read from config file */
  uint8_t
      bClkFreqVal;  /* Holds the System clock frequency read from config file */
  uint8_t bTimeout; /* Holds the Timeout Value */
} phNxpNciProfile_Control_t;

/* Internal messages to handle callbacks */
#define NCI_HAL_OPEN_CPLT_MSG 0x411
#define NCI_HAL_CLOSE_CPLT_MSG 0x412
#define NCI_HAL_POST_INIT_CPLT_MSG 0x413
#define NCI_HAL_PRE_DISCOVER_CPLT_MSG 0x414
#define NCI_HAL_ERROR_MSG 0x415
#define NCI_HAL_RX_MSG 0xF01
#define NCI_HAL_POST_MIN_INIT_CPLT_MSG 0xF02
#define NCIHAL_CMD_CODE_LEN_BYTE_OFFSET (2U)
#define NCIHAL_CMD_CODE_BYTE_LEN (3U)

/******************** NCI HAL exposed functions *******************************/

void phNxpNciHal_request_control(void);
void phNxpNciHal_release_control(void);
NFCSTATUS phNxpNciHal_send_get_cfgs();
int phNxpNciHal_write_unlocked(uint16_t data_len, const uint8_t* p_data);
static int phNxpNciHal_fw_mw_ver_check();
NFCSTATUS request_EEPROM(phNxpNci_EEPROM_info_t* mEEPROM_info);
NFCSTATUS phNxpNciHal_send_nfcee_pwr_cntl_cmd(uint8_t type);

/*******************************************************************************
**
** Function         phNxpNciHal_configFeatureList
**
** Description      Configures the featureList based on chip type
**                  HW Version information number will provide chipType.
**                  HW Version can be obtained from CORE_INIT_RESPONSE(NCI 1.0)
**                  or CORE_RST_NTF(NCI 2.0)
**
** Parameters       CORE_INIT_RESPONSE/CORE_RST_NTF, len
**
** Returns          none
*******************************************************************************/
void phNxpNciHal_configFeatureList(uint8_t* init_rsp, uint16_t rsp_len);

/*******************************************************************************
**
** Function         phNxpNciHal_getChipType
**
** Description      Gets the chipType which is configured during bootup
**
** Parameters       none
**
** Returns          chipType
*******************************************************************************/
tNFC_chipType phNxpNciHal_getChipType();
#endif /* _PHNXPNCIHAL_H_ */
