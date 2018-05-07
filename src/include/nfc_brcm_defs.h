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
 *  This file contains the Broadcom-specific defintions that are shared
 *  between HAL, nfc stack, adaptation layer and applications.
 *
 ******************************************************************************/

#ifndef NFC_BRCM_DEFS_H
#define NFC_BRCM_DEFS_H

/**********************************************
 * NCI Message Proprietary  Group       - F
 **********************************************/
#define NCI_MSG_GET_BUILD_INFO 0x04
#define NCI_MSG_HCI_NETWK 0x05
#define NCI_MSG_POWER_LEVEL 0x08
#define NCI_MSG_UICC_READER_ACTION 0x0A
/* reset HCI network/close all pipes (S,D) register */
#define NCI_MSG_GET_NV_DEVICE 0x24
#define NCI_MSG_LPTD 0x25
#define NCI_MSG_EEPROM_RW 0x29
#define NCI_MSG_GET_PATCH_VERSION 0x2D
#define NCI_MSG_SECURE_PATCH_DOWNLOAD 0x2E

/* Secure Patch Download definitions (patch type definitions) */
#define NCI_SPD_TYPE_HEADER 0x00

/**********************************************
 * NCI Interface Types
 **********************************************/
#define NCI_INTERFACE_VS_MIFARE 0x80
#define NCI_INTERFACE_VS_T2T_CE 0x82 /* for Card Emulation side */

/**********************************************
 * NCI Proprietary Parameter IDs
 **********************************************/
#define NCI_PARAM_ID_HOST_LISTEN_MASK 0xA2
#define NCI_PARAM_ID_TAGSNIFF_CFG 0xB9
#define NCI_PARAM_ID_ACT_ORDER 0xC5
#define NFC_SNOOZE_MODE_UART 0x01    /* Snooze mode for UART    */

#define NFC_SNOOZE_ACTIVE_LOW 0x00  /* high to low voltage is asserting */

/**********************************************
 * HCI definitions
 **********************************************/
#define NFC_HAL_HCI_SESSION_ID_LEN 8
#define NFC_HAL_HCI_SYNC_ID_LEN 2

/* Card emulation RF Gate A definitions */
#define NFC_HAL_HCI_CE_RF_A_UID_REG_LEN 10
#define NFC_HAL_HCI_CE_RF_A_ATQA_RSP_CODE_LEN 2
#define NFC_HAL_HCI_CE_RF_A_MAX_HIST_DATA_LEN 15
#define NFC_HAL_HCI_CE_RF_A_MAX_DATA_RATE_LEN 3

/* Card emulation RF Gate B definitions */
#define NFC_HAL_HCI_CE_RF_B_PUPI_LEN 4
#define NFC_HAL_HCI_CE_RF_B_ATQB_LEN 4
#define NFC_HAL_HCI_CE_RF_B_HIGHER_LAYER_RSP_LEN 61
#define NFC_HAL_HCI_CE_RF_B_MAX_DATA_RATE_LEN 3

/* Card emulation RF Gate BP definitions */
#define NFC_HAL_HCI_CE_RF_BP_MAX_PAT_IN_LEN 8
#define NFC_HAL_HCI_CE_RF_BP_DATA_OUT_LEN 40

/* Reader RF Gate A definitions */
#define NFC_HAL_HCI_RD_RF_B_HIGHER_LAYER_DATA_LEN 61

/* DH HCI Network command definitions */
#define NFC_HAL_HCI_DH_MAX_DYN_PIPES 20

/* Card emulation RF Gate A registry information */
typedef struct {
  uint8_t pipe_id; /* if MSB is set then valid, 7 bits for Pipe ID */
  uint8_t mode; /* Type A card emulation enabled indicator, 0x02:enabled    */
  uint8_t sak;
  uint8_t uid_reg_len;
  uint8_t uid_reg[NFC_HAL_HCI_CE_RF_A_UID_REG_LEN];
  uint8_t atqa[NFC_HAL_HCI_CE_RF_A_ATQA_RSP_CODE_LEN]; /* ATQA response code */
  uint8_t app_data_len;
  uint8_t
      app_data[NFC_HAL_HCI_CE_RF_A_MAX_HIST_DATA_LEN]; /* 15 bytes optional
                                                          storage for historic
                                                          data, use 2 slots */
  uint8_t fwi_sfgi; /* FRAME WAITING TIME, START-UP FRAME GUARD TIME */
  uint8_t cid_support;
  uint8_t datarate_max[NFC_HAL_HCI_CE_RF_A_MAX_DATA_RATE_LEN];
  uint8_t clt_support;
} tNCI_HCI_CE_RF_A;

/* Card emulation RF Gate B registry information */
typedef struct {
  uint8_t pipe_id; /* if MSB is set then valid, 7 bits for Pipe ID */
  uint8_t mode; /* Type B card emulation enabled indicator, 0x02:enabled    */
  uint8_t pupi_len;
  uint8_t pupi_reg[NFC_HAL_HCI_CE_RF_B_PUPI_LEN];
  uint8_t afi;
  uint8_t
      atqb[NFC_HAL_HCI_CE_RF_B_ATQB_LEN]; /* 4 bytes ATQB application data */
  uint8_t higherlayer_resp
      [NFC_HAL_HCI_CE_RF_B_HIGHER_LAYER_RSP_LEN]; /* 0~ 61 bytes ATRB_INF use
                                                     1~4 personality slots */
  uint8_t datarate_max[NFC_HAL_HCI_CE_RF_B_MAX_DATA_RATE_LEN];
  uint8_t natrb;
} tNCI_HCI_CE_RF_B;

/* Card emulation RF Gate BP registry information */
typedef struct {
  uint8_t pipe_id; /* if MSB is set then valid, 7 bits for Pipe ID */
  uint8_t
      mode; /* Type B prime card emulation enabled indicator, 0x02:enabled */
  uint8_t pat_in_len;
  uint8_t pat_in[NFC_HAL_HCI_CE_RF_BP_MAX_PAT_IN_LEN];
  uint8_t dat_out_len;
  uint8_t
      dat_out[NFC_HAL_HCI_CE_RF_BP_DATA_OUT_LEN]; /* ISO7816-3 <=64 byte, and
                                                     other fields are 9 bytes */
  uint8_t natr;
} tNCI_HCI_CE_RF_BP;

/* Card emulation RF Gate F registry information */
typedef struct {
  uint8_t pipe_id; /* if MSB is set then valid, 7 bits for Pipe ID */
  uint8_t mode; /* Type F card emulation enabled indicator, 0x02:enabled    */
  uint8_t speed_cap;
  uint8_t clt_support;
} tNCI_HCI_CE_RF_F;

/* Reader RF Gate A registry information */
typedef struct {
  uint8_t pipe_id; /* if MSB is set then valid, 7 bits for Pipe ID */
  uint8_t datarate_max;
} tNCI_HCI_RD_RF_A;

/* Reader RF Gate B registry information */
typedef struct {
  uint8_t pipe_id; /* if MSB is set then valid, 7 bits for Pipe ID */
  uint8_t afi;
  uint8_t hldata_len;
  uint8_t
      high_layer_data[NFC_HAL_HCI_RD_RF_B_HIGHER_LAYER_DATA_LEN]; /* INF field
                                                                     in ATTRIB
                                                                     command */
} tNCI_HCI_RD_RF_B;

/* Dynamic pipe information */
typedef struct {
  uint8_t source_host;
  uint8_t dest_host;
  uint8_t source_gate;
  uint8_t dest_gate;
  uint8_t pipe_id; /* if MSB is set then valid, 7 bits for Pipe ID */
} tNCI_HCI_DYN_PIPE_INFO;

/*************************************************************
 * HCI Network CMD/NTF structure for UICC host in the network
 *************************************************************/
typedef struct {
  uint8_t target_handle;
  uint8_t session_id[NFC_HAL_HCI_SESSION_ID_LEN];
  uint8_t sync_id[NFC_HAL_HCI_SYNC_ID_LEN];
  uint8_t static_pipe_info;
  tNCI_HCI_CE_RF_A ce_rf_a;
  tNCI_HCI_CE_RF_B ce_rf_b;
  tNCI_HCI_CE_RF_BP ce_rf_bp;
  tNCI_HCI_CE_RF_F ce_rf_f;
  tNCI_HCI_RD_RF_A rw_rf_a;
  tNCI_HCI_RD_RF_B rw_rf_b;
} tNCI_HCI_NETWK;

/************************************************
 * HCI Network CMD/NTF structure for Device host
 ************************************************/
typedef struct {
  uint8_t target_handle;
  uint8_t session_id[NFC_HAL_HCI_SESSION_ID_LEN];
  uint8_t static_pipe_info;
  uint8_t num_dyn_pipes;
  tNCI_HCI_DYN_PIPE_INFO dyn_pipe_info[NFC_HAL_HCI_DH_MAX_DYN_PIPES];
} tNCI_HCI_NETWK_DH;

#endif /* NFC_BRCM_DEFS_H */
