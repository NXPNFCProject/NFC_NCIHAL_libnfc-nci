/******************************************************************************
 *
 *  Copyright (C) 1999-2014 Broadcom Corporation
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
 *  This file contains functions that interface with the NFC NCI transport.
 *  On the receive side, it routes events to the appropriate handler
 *  (callback). On the transmit side, it manages the command transmission.
 *
 ******************************************************************************/
#include <android-base/stringprintf.h>
#include <base/logging.h>
#include <log/log.h>

#include "nfc_target.h"

#include <sys/stat.h>
#include "include/debug_nfcsnoop.h"
#include "nci_defs.h"
#include "nci_hmsgs.h"
#include "nfc_api.h"
#include "nfc_hal_api.h"
#include "nfc_int.h"
#include "rw_api.h"
#include "rw_int.h"
#if (NXP_EXTNS == TRUE)
#include "nfa_ce_int.h"
#include "nfa_sys.h"
#include "nfa_dm_int.h"
#include "nfa_hci_int.h"
#include <nfc_config.h>
#endif

#include <statslog.h>
#include "metrics.h"

using android::base::StringPrintf;

tNFC_CONN_CB* p_cb_stored = nullptr;
#if (NFC_RW_ONLY == FALSE)
static const uint8_t nfc_mpl_code_to_size[] = {64, 128, 192, 254};

#endif /* NFC_RW_ONLY */
#if (NXP_EXTNS == TRUE || APPL_DTA_MODE == TRUE)
// Global Structure varibale for FW Version
static tNFC_FW_VERSION nfc_fw_version;
#endif
#if (NXP_EXTNS == TRUE)
#define NFC_NCI_WAIT_DATA_NTF_TOUT 2
#define NFC_NCI_RFFIELD_EVT_TIMEOUT 2
uint8_t temp_buff[660];
#endif
#define NFC_PB_ATTRIB_REQ_FIXED_BYTES 1
#define NFC_LB_ATTRIB_REQ_FIXED_BYTES 8

extern unsigned char appl_dta_mode_flag;
extern bool nfc_debug_enabled;

#if (NXP_EXTNS == TRUE)
// Global Structure varibale for FW Version
static uint8_t gScreenState = 0x0;  // SCREEN ON UNLOCKED
static uint16_t maxRoutingTableSize;
uint8_t sListenActivated;
extern tNFA_CE_CB nfa_ce_cb;
bool core_reset_init_num_buff = false;
uint8_t nfcc_dh_conn_id = 0xFF;
extern void nfa_hci_rsp_timeout();
void disc_deact_ntf_timeout_handler(tNFC_RESPONSE_EVT event);
extern bool etsi_reader_in_progress;
void uicc_eeprom_get_config(uint8_t* config_resp);
void uicc_eeprom_set_config(uint8_t* config_resp);
bool isRedundantEndOfApdu(uint8_t* data, uint8_t len);
#endif

static struct timeval timer_start;
static struct timeval timer_end;

/*******************************************************************************
**
** Function         nfc_hal_nfcc_get_reset_info
**
** Description      Register dump when CORE_RESET_NTF happens
**
** Returns          status of command transceive
**
*******************************************************************************/
uint8_t nfc_hal_nfcc_get_reset_info(void) {

  phNxpNci_Extn_Cmd_t inData;
  phNxpNci_Extn_Resp_t outData;
  /*NCI_RESET_CMD*/
  uint8_t nfc_hal_nfcc_get_reset_info[6][6] = {
      {
       0x20, 0x00, 0x01, 0x00,
      },
      {
       0x20, 0x01, 0x00,
      },
      {0x20, 0x03, 0x03, 0x01, 0xA0, 0x1A},
      {0x20, 0x03, 0x03, 0x01, 0xA0, 0x1B},
      {0x20, 0x03, 0x03, 0x01, 0xA0, 0x1C},
      {0x20, 0x03, 0x03, 0x01, 0xA0, 0x27}};
  uint8_t core_status = NCI_STATUS_FAILED;
  uint8_t retry_count = 0;
  uint8_t i = 0, j = 0;
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s Enter", __func__);

  for (i = 0; i < 6; i++) {
    memset(&inData, 0x00, sizeof(phNxpNci_Extn_Cmd_t));
    memset(&outData, 0x00, sizeof(phNxpNci_Extn_Resp_t));
    inData.cmd_len = nfc_hal_nfcc_get_reset_info[i][2] + 3;
    memcpy(inData.p_cmd,
           (uint8_t*)&nfc_hal_nfcc_get_reset_info[i],
           nfc_hal_nfcc_get_reset_info[i][2] + 3);
    do {
      core_status =
            nfc_cb.p_hal->nciTransceive(&inData,&outData);
      retry_count++;
    } while (NCI_STATUS_OK != core_status &&
             retry_count < (NFC_NFCC_INIT_MAX_RETRY + 1));
    if (core_status == NCI_STATUS_OK) {
      DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf(
          "%s No.%d response for nfc_hal_nfcc_get_reset_info", __func__, i);
      for (j = 0; j < outData.rsp_len; j++) {
        DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf(
            " %s 0x%02x", __func__, outData.p_rsp[j]);
      }
    }
  }
  return core_status;
}

/*******************************************************************************
**
** Function         nfc_ncif_update_window
**
** Description      Update tx cmd window to indicate that NFCC can received
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_update_window(void) {
  /* Sanity check - see if we were expecting a update_window */
  if (nfc_cb.nci_cmd_window == NCI_MAX_CMD_WINDOW) {
    if (nfc_cb.nfc_state != NFC_STATE_W4_HAL_CLOSE) {
      LOG(ERROR) << StringPrintf("nfc_ncif_update_window: Unexpected call");
    }
    return;
  }

  /* Stop command-pending timer */
  nfc_stop_timer(&nfc_cb.nci_wait_rsp_timer);

  nfc_cb.p_vsc_cback = nullptr;
  nfc_cb.nci_cmd_window++;

  /* Check if there were any commands waiting to be sent */
  nfc_ncif_check_cmd_queue(nullptr);
}
/*******************************************************************************
**
** Function         nfc_ncif_update_data_queue
**
** Description      Update tx cmd window  to indicate that NFCC can received and
*core credit ntf received
**
** Returns          void
**
*********************************************************************************/
void nfc_ncif_update_data_queue(void) {
  nfc_cb.nci_cmd_window++;
  LOG(ERROR) << StringPrintf("nfc_ncif_update_data_queue- incrementing window");
  /* Check if there were any commands waiting to be sent */
  nfc_ncif_check_cmd_queue(nullptr);
}
/*******************************************************************************
**
** Function         nfc_ncif_cmd_timeout
**
** Description      Handle a command timeout
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_cmd_timeout(void) {
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("nfc_ncif_cmd_timeout(): enter");


  /* report an error */
  nfc_ncif_event_status(NFC_GEN_ERROR_REVT, NFC_STATUS_HW_TIMEOUT);
  nfc_ncif_event_status(NFC_NFCC_TIMEOUT_REVT, NFC_STATUS_HW_TIMEOUT);

  /* if enabling NFC, notify upper layer of failure */
  if (nfc_cb.nfc_state == NFC_STATE_CORE_INIT) {
    nfc_enabled(NFC_STATUS_FAILED, nullptr);
  }
  /* XXX maco since this failure is unrecoverable, abort the process */
  abort();
}

/*******************************************************************************
**
** Function         nfc_wait_2_deactivate_timeout
**
** Description      Handle a command timeout
**
** Returns          void
**
*******************************************************************************/
void nfc_wait_2_deactivate_timeout(void) {
  LOG(ERROR) << StringPrintf("nfc_wait_2_deactivate_timeout");
  nfc_cb.flags &= ~NFC_FL_DEACTIVATING;
  nci_snd_deactivate_cmd((uint8_t)nfc_cb.deactivate_timer.param);
}
#if (NXP_EXTNS == TRUE)
/*******************************************************************************
**
** Function         nfc_ncif_retransmit_data
**
** Description      Retransmission of lost packet
**
** Returns          void
**
*******************************************************************************/
uint8_t nfc_ncif_retransmit_data(tNFC_CONN_CB* p_cb, NFC_HDR* p_data) {
  LOG(ERROR) << StringPrintf("nfc_ncif_retransmit_data");
  if (p_data == nullptr) {
    LOG(ERROR) << StringPrintf("nfc_ncif_retransmit_data: p_data is NULL");
    return NCI_STATUS_FAILED;
  }
  if (p_cb->num_buff != NFC_CONN_NO_FC) p_cb->num_buff--;

  HAL_RE_WRITE(p_data);
  if (p_cb->conn_id == NFC_NFCEE_CONN_ID) {
    // Start waiting for credit ntf
    nfc_cb.bIsCreditNtfRcvd = false;
    nfc_start_timer(&nfc_cb.nci_wait_data_ntf_timer,
                    (uint16_t)(NFC_TTYPE_NCI_WAIT_DATA_NTF),
                    NFC_NCI_WAIT_DATA_NTF_TOUT);
  }
  return NCI_STATUS_OK;
}
#endif
/*******************************************************************************
**
** Function         nfc_ncif_send_data
**
** Description      This function is called to add the NCI data header
**                  and send it to NCIT task for sending it to transport
**                  as credits are available.
**
** Returns          void
**
*******************************************************************************/
uint8_t nfc_ncif_send_data(tNFC_CONN_CB* p_cb, NFC_HDR* p_data) {
  uint8_t* pp;
  uint8_t* ps;
  uint8_t ulen = NCI_MAX_PAYLOAD_SIZE;
  NFC_HDR* p;
  uint8_t pbf = 1;
  uint8_t buffer_size = p_cb->buff_size;
  uint8_t hdr0 = p_cb->conn_id;
  bool fragmented = false;
#if (NXP_EXTNS == TRUE)
  uint8_t* pTemp;
  if (core_reset_init_num_buff == true) {
    LOG(ERROR) << StringPrintf("Reinitializing the num_buff");

    if (!p_cb->num_buff) p_cb->num_buff++;
    core_reset_init_num_buff = false;
  }
#endif
  if (get_i2c_fragmentation_enabled() == I2C_FRAGMENATATION_ENABLED) {
    if (nfc_cb.i2c_data_t.nci_cmd_channel_busy == 1 && p_data) {
      DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("NxpNci : avoiding data packet sending data packet");
      nfc_cb.i2c_data_t.conn_id = p_cb->conn_id;
      GKI_enqueue(&p_cb->tx_q, p_data);
      nfc_cb.i2c_data_t.data_stored = 1;
      return NCI_STATUS_OK;
    }
  }
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("nfc_ncif_send_data :%d, num_buff:%d qc:%d", p_cb->conn_id,
                   p_cb->num_buff, p_cb->tx_q.count);
  if (p_cb->id == NFC_RF_CONN_ID) {
    if (nfc_cb.nfc_state != NFC_STATE_OPEN) {
      if (nfc_cb.nfc_state == NFC_STATE_CLOSING) {
        if ((p_data == nullptr) && /* called because credit from NFCC */
            (nfc_cb.flags & NFC_FL_DEACTIVATING)) {
          if (p_cb->init_credits == p_cb->num_buff) {
            /* all the credits are back */
            nfc_cb.flags &= ~NFC_FL_DEACTIVATING;
            DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf(
                "deactivating NFC-DEP init_credits:%d, num_buff:%d",
                p_cb->init_credits, p_cb->num_buff);
            nfc_stop_timer(&nfc_cb.deactivate_timer);
            nci_snd_deactivate_cmd((uint8_t)nfc_cb.deactivate_timer.param);
          }
        }
      }
      return NCI_STATUS_FAILED;
    }
  }

  if (p_data) {
    /* always enqueue the data to the tx queue */
    GKI_enqueue(&p_cb->tx_q, p_data);
  }

#if (NXP_EXTNS == TRUE)
  if ((nfcFL.eseFL._ESE_DUAL_MODE_PRIO_SCHEME ==
          nfcFL.eseFL._ESE_WIRED_MODE_RESUME) &&
          ((nfc_cb.bBlockWiredMode) && (p_cb->conn_id == NFC_NFCEE_CONN_ID))) {
      /* CE is activated on UICC hence return simply without sedning data */
      return (NCI_STATUS_OK);
  }
#endif

  /* try to send the first data packet in the tx queue  */
  p_data = (NFC_HDR*)GKI_getfirst(&p_cb->tx_q);

  /* post data fragment to NCIT task as credits are available */
  while (p_data && (p_cb->num_buff > 0)) {
    if (p_data->len <= buffer_size) {
      pbf = 0; /* last fragment */
      ulen = (uint8_t)(p_data->len);
      fragmented = false;
    } else {
      fragmented = true;
      ulen = buffer_size;
    }

    if (!fragmented) {
      /* if data packet is not fragmented, use the original buffer */
      p = p_data;
      p_data = (NFC_HDR*)GKI_dequeue(&p_cb->tx_q);
    } else {
      /* the data packet is too big and need to be fragmented
       * prepare a new GKI buffer
       * (even the last fragment to avoid issues) */
      p = NCI_GET_CMD_BUF(ulen);
      if (p == nullptr)
        return (NCI_STATUS_BUFFER_FULL);

      p->len = ulen;
      p->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE + 1;

      if (p->len) {
        pp = (uint8_t*)(p + 1) + p->offset;
        ps = (uint8_t*)(p_data + 1) + p_data->offset;
        memcpy(pp, ps, ulen);
      }
      /* adjust the NFC_HDR on the old fragment */
      p_data->len -= ulen;
      p_data->offset += ulen;
    }

    p->event = BT_EVT_TO_NFC_NCI;
    p->layer_specific = pbf;
    p->len += NCI_DATA_HDR_SIZE;
    p->offset -= NCI_DATA_HDR_SIZE;
    pp = (uint8_t*)(p + 1) + p->offset;
    /* build NCI Data packet header */
    NCI_DATA_PBLD_HDR(pp, pbf, hdr0, ulen);
#if (NXP_EXTNS == TRUE)
    if (isRedundantEndOfApdu(pp, ulen)) {
      GKI_freebuf(p_data);
      return (NCI_STATUS_OK);
    }
#endif
    if (p_cb->num_buff != NFC_CONN_NO_FC) p_cb->num_buff--;

#if (NXP_EXTNS == TRUE)
    if ((nfcFL.eseFL._ESE_DUAL_MODE_PRIO_SCHEME ==
            nfcFL.eseFL._ESE_WIRED_MODE_RESUME) &&
            (p_cb->conn_id == NFC_NFCEE_CONN_ID)) {
        nfc_cb.temp_data = (NFC_HDR*)temp_buff;
        nfc_cb.temp_data->offset = p->offset;
        pTemp = (uint8_t*)(nfc_cb.temp_data + 1) + nfc_cb.temp_data->offset;
        nfc_cb.temp_data->len = p->len;
        memcpy(pTemp, ((uint8_t*)(p + 1) + p->offset), p->len);
    }
#endif

    /* send to HAL */
    HAL_WRITE(p);
    nfcsnoop_capture(p, false);
#if (NXP_EXTNS == TRUE)
    /* start NFC data ntf timeout timer */
    if (get_i2c_fragmentation_enabled() == I2C_FRAGMENATATION_ENABLED) {
      nfc_cb.nci_cmd_window--;
    }
    if (p_cb->conn_id != NFC_RF_CONN_ID) {
      // Start waiting for credit ntf
      if (p_cb->conn_id == NFC_NFCEE_CONN_ID) {
        nfc_cb.bIsCreditNtfRcvd = false;
      }
      nfc_start_timer(&nfc_cb.nci_wait_data_ntf_timer,
                      (uint16_t)(NFC_TTYPE_NCI_WAIT_DATA_NTF),
                      NFC_NCI_WAIT_DATA_NTF_TOUT);
    }
    if (!fragmented) {
      /* check if there are more data to send */
      p_data = (NFC_HDR*)GKI_getfirst(&p_cb->tx_q);
    }
#endif
  }

  // log duration for the first hce data response
  if (timer_start.tv_sec != 0 || timer_start.tv_usec != 0) {
    gettimeofday(&timer_end, nullptr);
    uint32_t delta_time_ms = (timer_end.tv_sec - timer_start.tv_sec) * 1000 +
                             (timer_end.tv_usec - timer_start.tv_usec) / 1000;
    memset(&timer_start, 0, sizeof(timer_start));
    memset(&timer_end, 0, sizeof(timer_end));
    android::util::stats_write(android::util::NFC_HCE_TRANSACTION_OCCURRED,
                               (int32_t)delta_time_ms);
  }
  return (NCI_STATUS_OK);
}

#if (NXP_EXTNS == TRUE)
/*Function to empty cmd queue.*/
void nfc_ncif_empty_cmd_queue() {
  NFC_HDR* p_buf = (NFC_HDR*)GKI_dequeue(&nfc_cb.nci_cmd_xmit_q);

  while (p_buf) {
    GKI_freebuf(p_buf);
    p_buf = (NFC_HDR*)GKI_dequeue(&nfc_cb.nci_cmd_xmit_q);
  }
}

/*Function to empty data queue.*/
void nfc_ncif_empty_data_queue() {
  NFC_HDR* p_data = (NFC_HDR*)GKI_dequeue(&p_cb_stored->tx_q);

  while (p_data) {
    GKI_freebuf(p_data);
    p_data = (NFC_HDR*)GKI_dequeue(&p_cb_stored->tx_q);
  }
}
#endif
/*******************************************************************************
**
** Function         nfc_ncif_check_cmd_queue
**
** Description      Send NCI command to the transport
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_check_cmd_queue(NFC_HDR* p_buf) {
  uint8_t* ps;
  /* If there are commands waiting in the xmit queue, or if the controller
   * cannot accept any more commands, */
  /* then enqueue this command */
  if (p_buf) {
    if ((nfc_cb.nci_cmd_xmit_q.count) || (nfc_cb.nci_cmd_window == 0)) {
      GKI_enqueue(&nfc_cb.nci_cmd_xmit_q, p_buf);
#if (NXP_EXTNS == TRUE)
      if (p_buf != nullptr) {
        DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("nfc_ncif_check_cmd_queue : making p_buf NULL.");
        p_buf = nullptr;
      }
#else
      p_buf = nullptr;
#endif
    }
  }
  /* If controller can accept another command, then send the next command */
  if (nfc_cb.nci_cmd_window > 0) {
    /* If no command was provided, or if older commands were in the queue, then
     * get cmd from the queue */
    if (!p_buf) p_buf = (NFC_HDR*)GKI_dequeue(&nfc_cb.nci_cmd_xmit_q);

    if (p_buf) {
#if (NXP_EXTNS == TRUE)
      DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("nfc_ncif_check_cmd_queue : Writing to HAL...");
      /*save the message header to double check the response */
      ps = (uint8_t*)(p_buf + 1) + p_buf->offset;
      /*save command HEADER(GID+OID) only*/
      memcpy(nfc_cb.last_hdr, ps, NFC_SAVED_HDR_SIZE);
      /*save command length only*/
      nfc_cb.cmd_size = *(ps + NFC_SAVED_HDR_SIZE);
      DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s : cmd_size:%d", __func__, nfc_cb.cmd_size);
      if ((nfc_cb.last_hdr[0] == 0x20 &&
           (nfc_cb.last_hdr[1] == 0x02 || nfc_cb.last_hdr[1] == 0x03)) ||
          (nfc_cb.last_hdr[0] == 0x2F && nfc_cb.last_hdr[1] == 0x15) ||
          (nfc_cb.last_hdr[0] == 0x21 && nfc_cb.last_hdr[1] == 0x01) ||
          (nfc_cb.last_hdr[0] == 0x21 && nfc_cb.last_hdr[1] == 0x06)) {
        if (nfc_cb.last_cmd_buf != nullptr) {
          GKI_freebuf(nfc_cb.last_cmd_buf);  // ======> Free before allocation
        }
        nfc_cb.last_cmd_buf = (uint8_t*)GKI_getbuf(nfc_cb.cmd_size + 1);
        if (nfc_cb.last_cmd_buf != nullptr) {
          /*save command data including length and excluding header*/
          memcpy(nfc_cb.last_cmd_buf, ps + NFC_SAVED_HDR_SIZE,
                 (nfc_cb.cmd_size + 1));
          memcpy(nfc_cb.last_cmd, ps + NCI_MSG_HDR_SIZE, NFC_SAVED_CMD_SIZE);
        } else {
          DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("NULL buffer nfc_cb.last_cmd_buf");
        }
      } else {
        memcpy(nfc_cb.last_cmd, ps + NCI_MSG_HDR_SIZE, NFC_SAVED_CMD_SIZE);
      }
#else
      /* save the message header to double check the response */
      ps = (uint8_t*)(p_buf + 1) + p_buf->offset;
      memcpy(nfc_cb.last_hdr, ps, NFC_SAVED_HDR_SIZE);
      memcpy(nfc_cb.last_cmd, ps + NCI_MSG_HDR_SIZE, NFC_SAVED_CMD_SIZE);
#endif
      if (p_buf->layer_specific == NFC_WAIT_RSP_VSC) {
        /* save the callback for NCI VSCs)  */
        nfc_cb.p_vsc_cback = (void*)((tNFC_NCI_VS_MSG*)p_buf)->p_cback;
      } else if (p_buf->layer_specific == NFC_WAIT_RSP_RAW_VS) {
        /* save the callback for RAW VS */
        nfc_cb.p_vsc_cback = (void*)((tNFC_NCI_VS_MSG*)p_buf)->p_cback;
        nfc_cb.rawVsCbflag = true;
      }

      /* Indicate command is pending */
      nfc_cb.nci_cmd_window--;

      /* send to HAL */
      HAL_WRITE(p_buf);
#if (NXP_EXTNS != TRUE)
      /* Indicate command is pending */
      nfc_cb.nci_cmd_window--;
#endif
      if (get_i2c_fragmentation_enabled() == I2C_FRAGMENATATION_ENABLED) {
        nfc_cb.i2c_data_t.nci_cmd_channel_busy = 1;
        DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("setting channel busy flag");
      }

      /* start NFC command-timeout timer */
      nfc_start_timer(&nfc_cb.nci_wait_rsp_timer,
                      (uint16_t)(NFC_TTYPE_NCI_WAIT_RSP),
                      nfc_cb.nci_wait_rsp_tout);
    }
  }

  if (nfc_cb.nci_cmd_window == NCI_MAX_CMD_WINDOW) {
    /* the command queue must be empty now */
    if (nfc_cb.flags & NFC_FL_CONTROL_REQUESTED) {
      /* HAL requested control or stack needs to handle pre-discover */
      nfc_cb.flags &= ~NFC_FL_CONTROL_REQUESTED;
      if (nfc_cb.flags & NFC_FL_DISCOVER_PENDING) {
        if (nfc_cb.p_hal->prediscover()) {
          /* HAL has the command window now */
          nfc_cb.flags |= NFC_FL_CONTROL_GRANTED;
          nfc_cb.nci_cmd_window = 0;
        } else {
          /* HAL does not need to send command,
           * - restore the command window and issue the discovery command now */
          nfc_cb.flags &= ~NFC_FL_DISCOVER_PENDING;
          ps = (uint8_t*)nfc_cb.p_disc_pending;
          nci_snd_discover_cmd(*ps, (tNFC_DISCOVER_PARAMS*)(ps + 1));
#if (NXP_EXTNS == TRUE)
          if (nfc_cb.p_last_disc) {
            GKI_freebuf(nfc_cb.p_last_disc);
            nfc_cb.p_last_disc = nullptr;
          }
          nfc_cb.p_last_disc = nfc_cb.p_disc_pending;
#else
          GKI_freebuf(nfc_cb.p_disc_pending);
#endif
          nfc_cb.p_disc_pending = nullptr;
        }
      } else if (nfc_cb.flags & NFC_FL_HAL_REQUESTED) {
        /* grant the control to HAL */
        nfc_cb.flags &= ~NFC_FL_HAL_REQUESTED;
        nfc_cb.flags |= NFC_FL_CONTROL_GRANTED;
        nfc_cb.nci_cmd_window = 0;
        nfc_cb.p_hal->control_granted();
      }
    }
  }
}

#if (NXP_EXTNS == TRUE || APPL_DTA_MODE == TRUE)
/*******************************************************************************
**
** Function         nfc_ncif_getFWVersion
**
** Description      This function is called to fet the FW Version
**
** Returns          tNFC_FW_VERSION
**
*******************************************************************************/
tNFC_FW_VERSION nfc_ncif_getFWVersion() { return nfc_fw_version; }
#endif

/*******************************************************************************
**
** Function         nfc_ncif_send_cmd
**
** Description      Send NCI command to the NCIT task
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_send_cmd(NFC_HDR* p_buf) {
#if (NXP_EXTNS == TRUE)
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("nfc_ncif_send_cmd()");
  if (p_buf == nullptr) {
    DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("p_buf is NULL.");
    return;
  }
  uint8_t* cmd = nullptr;
  cmd = (uint8_t*)(p_buf + 1) + p_buf->offset;

  if (sListenActivated == true) {
    nfc_stop_timer(&nfc_cb.listen_activation_timer_list);
    sListenActivated = false;
  }
#endif
  /* post the p_buf to NCIT task */
  p_buf->event = BT_EVT_TO_NFC_NCI;
  p_buf->layer_specific = 0;
  nfcsnoop_capture(p_buf, false);
  nfc_ncif_check_cmd_queue(p_buf);
}

/*******************************************************************************
**
** Function         nfc_ncif_process_event
**
** Description      This function is called to process the
**                  data/response/notification from NFCC
**
** Returns          true if need to free buffer
**
*******************************************************************************/
bool nfc_ncif_process_event(NFC_HDR* p_msg) {
  uint8_t mt, pbf, gid, *p;
  bool free = true;
  uint8_t oid;
  uint16_t len;
  uint8_t* p_old, old_gid, old_oid, old_mt;
  p = (uint8_t*)(p_msg + 1) + p_msg->offset;

  if (p_msg->len < 3) {
    // Per NCI spec, every packets should have at least 3 bytes: HDR0, HDR1, and
    // LEN field.
    LOG(ERROR) << StringPrintf("Invalid NCI packet: p_msg->len: %d",
                               p_msg->len);
    return free;
  }

  // LEN field contains the size of the payload, not including the 3-byte packet
  // header.
  len = p[2] + 3;
  if (p_msg->len < len) {
    // Making sure the packet holds enough data than it claims.
    LOG(ERROR) << StringPrintf("Invalid NCI packet: p_msg->len (%d) < len (%d)",
                               p_msg->len, len);
    return free;
  }

  NCI_MSG_PRS_HDR0(p, mt, pbf, gid);
  oid = ((*p) & NCI_OID_MASK);
#if (NXP_EXTNS == TRUE)
  if (sListenActivated == true) {
    nfc_stop_timer(&nfc_cb.listen_activation_timer_list);
    sListenActivated = false;
  }
#endif
  if (nfc_cb.rawVsCbflag == true &&
      nfc_ncif_proc_proprietary_rsp(mt, gid, oid) == true) {
#if (NXP_EXTNS == TRUE)
    if (mt == NCI_MT_RSP) nci_proc_prop_raw_vs_rsp(p_msg);
#else
    nci_proc_prop_raw_vs_rsp(p_msg);
    nfc_cb.rawVsCbflag = false;
#endif
    return free;
  }

  nfcsnoop_capture(p_msg, true);
  switch (mt) {
    case NCI_MT_DATA:
      DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("NFC received data");
      nfc_ncif_proc_data(p_msg);
      free = false;
      break;

    case NCI_MT_RSP:
      DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("NFC received rsp gid:%d", gid);
      oid = ((*p) & NCI_OID_MASK);
      p_old = nfc_cb.last_hdr;
      NCI_MSG_PRS_HDR0(p_old, old_mt, pbf, old_gid);
      old_oid = ((*p_old) & NCI_OID_MASK);
      /* make sure this is the RSP we are waiting for before updating the
       * command window */
      if ((old_gid != gid) || (old_oid != oid)) {
          if ((nfcFL.nfcNxpEse && nfcFL.eseFL._ESE_ETSI_READER_ENABLE) &&
                  ((gid == NCI_GID_RF_MANAGE) && (oid == NCI_MSG_RF_DISCOVER)) &&
                  (etsi_reader_in_progress == true)) {
              DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("Changing disc_state and disc_flags");
              nfa_dm_cb.disc_cb.disc_state = NFA_DM_RFST_IDLE;
              nfa_dm_cb.disc_cb.disc_flags &=
                      ~(NFA_DM_DISC_FLAGS_W4_NTF | NFA_DM_DISC_FLAGS_STOPPING);
              nfa_dm_cb.disc_cb.disc_flags |=
                      (NFA_DM_DISC_FLAGS_W4_RSP | NFA_DM_DISC_FLAGS_NOTIFY |
                              NFA_DM_DISC_FLAGS_ENABLED);
              disc_deact_ntf_timeout_handler(NFC_NFCC_TIMEOUT_REVT);
          } else {
              /*no response after the deactivate command, handling the error
               * scenario after the recovery*/
              if ((gid == NCI_GID_RF_MANAGE) && (oid == NCI_MSG_RF_DISCOVER) &&
                      (nfc_cb.nci_cmd_window == 0)) {
                  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("resetting the nci_cmd_window");
                  nfc_cb.nci_cmd_window++;
              } else {
                  LOG(ERROR) << StringPrintf(
                          "nfc_ncif_process_event unexpected rsp: gid:0x%x, oid:0x%x",
                          gid, oid);
                  return true;
              }
          }
      }

      switch (gid) {
        case NCI_GID_CORE: /* 0000b NCI Core group */
          free = nci_proc_core_rsp(p_msg);
          break;
        case NCI_GID_RF_MANAGE: /* 0001b NCI Discovery group */
          nci_proc_rf_management_rsp(p_msg);
          break;
#if (NFC_NFCEE_INCLUDED == true)
#if (NFC_RW_ONLY == FALSE)
        case NCI_GID_EE_MANAGE: /* 0x02 0010b NFCEE Discovery group */
          nci_proc_ee_management_rsp(p_msg);
          break;
#endif
#endif
        case NCI_GID_PROP: /* 1111b Proprietary */
          nci_proc_prop_rsp(p_msg);
          break;
        default:
          LOG(ERROR) << StringPrintf("NFC: Unknown gid:%d", gid);
          break;
      }
#if (NXP_EXTNS == TRUE)
      // received command response
      // set the saved header and command  buffer to zero
      nfc_cb.cmd_size = 0x00;
      memset(nfc_cb.last_hdr, 0, NFC_SAVED_HDR_SIZE);
      memset(nfc_cb.last_cmd, 0, NFC_SAVED_CMD_SIZE);
#endif
      if (get_i2c_fragmentation_enabled() == I2C_FRAGMENATATION_ENABLED) {
        nfc_cb.i2c_data_t.nci_cmd_channel_busy = 0;
        DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s,updating window", __func__);
        p_cb_stored = nfc_find_conn_cb_by_conn_id(nfc_cb.i2c_data_t.conn_id);
        nfc_ncif_update_window();
        if (p_cb_stored && (nfc_cb.i2c_data_t.data_stored == 1) &&
            (nfc_cb.i2c_data_t.nci_cmd_channel_busy == 0x00)) {
          LOG(ERROR) << StringPrintf("resending the stored data  packet");
          nfc_ncif_send_data(p_cb_stored, nullptr);
          nfc_cb.i2c_data_t.data_stored = 0;
        }
      } else {
        nfc_ncif_update_window();
      }
      break;
    case NCI_MT_NTF:
      DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("NFC received ntf gid:%d", gid);
      switch (gid) {
        case NCI_GID_CORE: /* 0000b NCI Core group */
          nci_proc_core_ntf(p_msg);
          break;
        case NCI_GID_RF_MANAGE: /* 0001b NCI Discovery group */
          nci_proc_rf_management_ntf(p_msg);
          break;
#if (NFC_NFCEE_INCLUDED == true)
#if (NFC_RW_ONLY == FALSE)
        case NCI_GID_EE_MANAGE: /* 0x02 0010b NFCEE Discovery group */
          nci_proc_ee_management_ntf(p_msg);
          break;
#endif
#endif
        case NCI_GID_PROP: /* 1111b Proprietary */
          nci_proc_prop_ntf(p_msg);
          break;
        default:
          LOG(ERROR) << StringPrintf("NFC: Unknown gid:%d", gid);
          break;
      }
      break;

    default:
      DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("NFC received unknown mt:0x%x, gid:%d", mt, gid);
  }

  return (free);
}

/*******************************************************************************
**
** Function         nfc_ncif_rf_management_status
**
** Description      This function is called to report an event
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_rf_management_status(tNFC_DISCOVER_EVT event, uint8_t status) {
  tNFC_DISCOVER evt_data;
  if (nfc_cb.p_discv_cback) {
    evt_data.status = (tNFC_STATUS)status;
    (*nfc_cb.p_discv_cback)(event, &evt_data);
  }
}

/*******************************************************************************
**
** Function         nfc_ncif_set_config_status
**
** Description      This function is called to report NFC_SET_CONFIG_REVT
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_set_config_status(uint8_t* p, uint8_t len) {
  tNFC_RESPONSE evt_data;

  if (nfc_cb.p_resp_cback) {
    evt_data.set_config.num_param_id = 0;
    if (len == 0) {
      LOG(ERROR) << StringPrintf("Insufficient RSP length");
      evt_data.set_config.status = NFC_STATUS_SYNTAX_ERROR;
      (*nfc_cb.p_resp_cback)(NFC_SET_CONFIG_REVT, &evt_data);
      return;
    }
    evt_data.set_config.status = (tNFC_STATUS)*p++;
    if (evt_data.set_config.status != NFC_STATUS_OK && len > 1) {
      evt_data.set_config.num_param_id = *p++;
      if (evt_data.set_config.num_param_id > NFC_MAX_NUM_IDS) {
        android_errorWriteLog(0x534e4554, "114047681");
        LOG(ERROR) << StringPrintf("OOB write num_param_id %d",
                                   evt_data.set_config.num_param_id);
        evt_data.set_config.num_param_id = 0;
      } else if (evt_data.set_config.num_param_id <= len - 2) {
        STREAM_TO_ARRAY(evt_data.set_config.param_ids, p,
                        evt_data.set_config.num_param_id);
      } else {
        LOG(ERROR) << StringPrintf("Insufficient RSP length %d,num_param_id %d",
                                   len, evt_data.set_config.num_param_id);
        evt_data.set_config.num_param_id = 0;
      }
    }

    (*nfc_cb.p_resp_cback)(NFC_SET_CONFIG_REVT, &evt_data);
  }
}

/*******************************************************************************
**
** Function         nfc_ncif_event_status
**
** Description      This function is called to report an event
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_event_status(tNFC_RESPONSE_EVT event, uint8_t status) {
  tNFC_RESPONSE evt_data;
  if (event == NFC_NFCC_TIMEOUT_REVT && status == NFC_STATUS_HW_TIMEOUT) {
    uint32_t cmd_hdr = (nfc_cb.last_hdr[0] << 8) | nfc_cb.last_hdr[1];
    android::util::stats_write(android::util::NFC_ERROR_OCCURRED,
                               (int32_t)NCI_TIMEOUT, (int32_t)cmd_hdr,
                               (int32_t)status);
  }
#if (NXP_EXTNS == TRUE)
  if ((nfcFL.eseFL._ESE_DUAL_MODE_PRIO_SCHEME ==
          nfcFL.eseFL._ESE_WIRED_MODE_RESUME) &&
          ((status == NFC_STATUS_WIRED_SESSION_ABORTED) ||
                  (status == NFC_STATUS_DWP_APDU_DROPPPED))) {
    tNFC_CONN_CB* p_cb;
    nfc_cb.bBlockWiredMode = true;
    nfc_cb.bBlkPwrlinkAndModeSetCmd = true;
    p_cb = nfc_find_conn_cb_by_conn_id(nfa_hci_cb.conn_id);
    /*Stop rf_filed timer only if SPI is not open*/
    if(NFC_INTF_REQ_SRC_DWP == nfc_cb.pwr_link_cmd.reqSrc)
    nfc_stop_timer(&nfc_cb.rf_filed_event_timeout_timer);
    if (!nfc_cb.bIsCreditNtfRcvd) {
      nfc_stop_timer(&nfc_cb.nci_wait_data_ntf_timer);
      nfc_cb.bIsCreditNtfRcvd = true;

      if ((p_cb) && !p_cb->num_buff) p_cb->num_buff++;
    }
    nfc_cb.bSetmodeOnReq = true; /* Sending mode set ON is required on RF
                                    deactivation to resume DWP */
    nfa_hci_cb.bIsHciResponseTimedout = false;
    if (status == NFC_STATUS_DWP_APDU_DROPPPED) {
      nfc_cb.bRetransmitDwpPacket = true;
    }
    if (status == NFC_STATUS_WIRED_SESSION_ABORTED) {
      nfc_cb.bIsDwpResPending = true;
      nfa_hci_cb.IsWiredSessionAborted = true;
    }
    nfa_sys_stop_timer(&nfa_hci_cb.timer);
    if (p_cb && p_cb->p_cback)
      (*p_cb->p_cback)(nfa_hci_cb.conn_id, NFC_HCI_RESTART_TIMER,
                       (tNFC_CONN*)&evt_data);
  }
#endif

  if (nfc_cb.p_resp_cback) {
    evt_data.status = (tNFC_STATUS)status;
    (*nfc_cb.p_resp_cback)(event, &evt_data);
  }
}

/*******************************************************************************
**
** Function         nfc_ncif_error_status
**
** Description      This function is called to report an error event to data
**                  cback
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_error_status(uint8_t conn_id, uint8_t status) {
  tNFC_CONN_CB* p_cb = nfc_find_conn_cb_by_conn_id(conn_id);
  if (p_cb && p_cb->p_cback) {
    tNFC_CONN nfc_conn;
    nfc_conn.status = status;
    (*p_cb->p_cback)(conn_id, NFC_ERROR_CEVT, &nfc_conn);
  }
  android::util::stats_write(android::util::NFC_ERROR_OCCURRED,
                             (int32_t)ERROR_NTF, (int32_t)0, (int32_t)status);
}

/*******************************************************************************
**
** Function         nfc_ncif_proc_rf_field_ntf
**
** Description      This function is called to process RF field notification
**
** Returns          void
**
*******************************************************************************/
#if (NFC_RW_ONLY == FALSE)
void nfc_ncif_proc_rf_field_ntf(uint8_t rf_status) {
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("nfc_ncif_proc_rf_field_ntf");
  tNFC_RESPONSE evt_data;
#if (NXP_EXTNS == TRUE)
  if(nfcFL.eseFL._ESE_DUAL_MODE_PRIO_SCHEME ==
          nfcFL.eseFL._ESE_WIRED_MODE_RESUME) {
      if (rf_status == 0x01) {
          DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("nfc_ncif_proc_rf_field_ntf ON_EVT");
          nfc_cb.bBlockWiredMode = true;
          nfc_cb.bBlkPwrlinkAndModeSetCmd = true;
          nfc_stop_timer(&nfc_cb.nci_wait_setMode_Ntf_timer); /* stop mode set Ntf
                                                           timer if it is
                                                           allready started */
          nfc_start_timer(&nfc_cb.rf_filed_event_timeout_timer,
                  (uint16_t)(NFC_TTYPE_NCI_WAIT_RF_FIELD_NTF),
                  NFC_NCI_RFFIELD_EVT_TIMEOUT);
      } else {
          nfc_stop_timer(&nfc_cb.rf_filed_event_timeout_timer);
          if (nfc_cb.bBlockWiredMode) {
            /*Timeout for field off is reduced to 1s which will be enough to get
             * next field ntf if any*/
            nfc_start_timer(&nfc_cb.rf_filed_event_timeout_timer,
                            (uint16_t)(NFC_TTYPE_NCI_WAIT_RF_FIELD_NTF), 1);
          }
      }
  }
#endif

  if (nfc_cb.p_resp_cback) {
    evt_data.status = (tNFC_STATUS)NFC_STATUS_OK;
    evt_data.rf_field.rf_field = rf_status;
    (*nfc_cb.p_resp_cback)(NFC_RF_FIELD_REVT, &evt_data);
  }
}
#endif

#if (NXP_EXTNS == TRUE)
/*******************************************************************************
**
** Function         nfc_ncif_allow_dwp_transmission
**
** Description      This function is called when CE deactivation/RF event
*timedout/RF OFF event
**                  eSE action notification
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_allow_dwp_transmission() {
    DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("nfc_ncif_allow_dwp_transmission Enter");
    if(nfcFL.eseFL._ESE_DUAL_MODE_PRIO_SCHEME !=
            nfcFL.eseFL._ESE_WIRED_MODE_RESUME) {
        DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("DUAL_MODE_PRIO_SCHEME != WIRED_MODE_RESUME. Returning");
        return;
    }
  tNFC_RESPONSE evt_data;
  tNFC_CONN_CB* p_cb;
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("nfc_ncif_allow_dwp_transmission %d, %d ",
                   nfc_cb.bIsDwpResPending, nfc_cb.bRetransmitDwpPacket);
  p_cb = nfc_find_conn_cb_by_conn_id(nfa_hci_cb.conn_id);

  if ((p_cb) && (p_cb->conn_id == NFC_NFCEE_CONN_ID)) {
    if (nfa_hci_cb.bIsHciResponseTimedout) {
      nfa_hci_cb.IsHciTimerChanged = false;
      nfc_cb.bRetransmitDwpPacket = false;
      nfa_hci_cb.bIsHciResponseTimedout = false;
      nfc_cb.bIsDwpResPending = false;
    }
    if (nfc_cb.bIsDwpResPending) {
      nfa_sys_stop_timer(&nfa_hci_cb.timer);
      nfc_cb.bIsDwpResPending = false;
      nfa_hci_cb.IsHciTimerChanged = true;
      if (p_cb->p_cback)
        (*p_cb->p_cback)(nfa_hci_cb.conn_id, NFC_HCI_RESTART_TIMER,
                         (tNFC_CONN*)&evt_data);
    } else if (nfc_cb.bRetransmitDwpPacket) {
      nfa_sys_stop_timer(&nfa_hci_cb.timer);
      nfc_ncif_retransmit_data(p_cb, nfc_cb.temp_data);
      nfc_cb.bRetransmitDwpPacket = false;
      if (p_cb->p_cback)
        (*p_cb->p_cback)(nfa_hci_cb.conn_id, NFC_HCI_RESTART_TIMER,
                         (tNFC_CONN*)&evt_data);
    } else {
      nfc_ncif_send_data(p_cb, nullptr);
    }
  }
}
/*******************************************************************************
**
** Function         nfc_ncif_onWiredModeHold_timeout
**
** Description      This function is called when RF field event timedout
**                  To aviod infinite dwp block on RF_ON event
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_onWiredModeHold_timeout() {
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("nfc_ncif_onWiredModeHold_timeout");
  if(nfcFL.eseFL._ESE_DUAL_MODE_PRIO_SCHEME !=
          nfcFL.eseFL._ESE_WIRED_MODE_RESUME) {
      DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("DUAL_MODE_PRIO_SCHEME != WIRED_MODE_RESUME. Returning");
      return;
  }
  if (nfc_cb.bBlockWiredMode) {
    nfc_ncif_resume_dwp_wired_mode();
  }
}
/*******************************************************************************
**
** Function         nfc_ncif_resume_dwp_wired_mode
**
** Description      This function is called to resume DWP session when RF is
*session is over
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_resume_dwp_wired_mode() {
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("nfc_ncif_resume_dwp_wired_mode");
  if(nfcFL.eseFL._ESE_DUAL_MODE_PRIO_SCHEME !=
          nfcFL.eseFL._ESE_WIRED_MODE_RESUME) {
      DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("DUAL_MODE_PRIO_SCHEME != WIRED_MODE_RESUME. Returning");
      return;
  }
  tNFC_CONN_CB* p_cb;
  p_cb = nfc_find_conn_cb_by_conn_id(nfa_hci_cb.conn_id);
  if (p_cb == nullptr) {
    DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("Control block not found. Returning");
    return;
  }

  nfc_cb.bBlkPwrlinkAndModeSetCmd = false;
  nfc_cb.bIssueModeSetCmd = false;
  if (nfc_cb.pwr_link_cmd.bPwrLinkCmdRequested) {
    nfc_stop_quick_timer(&nfc_cb.nci_wait_pwrLinkRsp_timer);
    if (!nfc_cb.bCeActivatedeSE && nfc_cb.bSetmodeOnReq)
      nfc_cb.bIssueModeSetCmd = true;
    DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("pwr link cmd to send");
    nci_snd_pwr_nd_lnk_ctrl_cmd(NFCEE_ID_ESE, nfc_cb.pwr_link_cmd.param,
                                nfc_cb.pwr_link_cmd.reqSrc);
    nfc_cb.pwr_link_cmd.bPwrLinkCmdRequested = false;
    nfc_cb.pwr_link_cmd.reqSrc = NFC_INTF_REQ_SRC_DWP;
    if (nfc_cb.pwr_link_cmd.param == 0x01) {
      /* Mode set command not sent during Standby , so allow DWP
       * transmission need to be called to unblock Standby command. */
      nfc_cb.bBlockWiredMode = false;
      nfc_cb.bCeActivatedeSE = false;
      nfc_ncif_allow_dwp_transmission();
    }
  } else if (((nfc_cb.bSetmodeOnReq) || (!GKI_queue_is_empty(&p_cb->tx_q))) &&
             (!nfc_cb.bCeActivatedeSE)) {
    nfc_stop_quick_timer(&nfc_cb.nci_wait_setModeRsp_timer);
    DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("mode set cmd to send");
    nfc_cb.bSetmodeOnReq = true;
    nci_snd_nfcee_mode_set(NFCEE_ID_ESE, NFC_MODE_ACTIVATE);
    nfc_start_timer(&nfc_cb.nci_wait_setMode_Ntf_timer,
                    (uint16_t)NFC_TYPE_NCI_WAIT_SETMODE_NTF,
                    NFC_NCI_SETMODE_NTF_TIMEOUT);
  } else {
    nfc_cb.bBlockWiredMode = false;
    nfc_cb.bCeActivatedeSE = false;
    /*Reset modeSet flags to unblock the next send request*/
    nfc_cb.bSetmodeOnReq = false;
    nfc_cb.flags &= ~NFC_FL_WAIT_MODE_SET_NTF;
    nfc_ncif_allow_dwp_transmission();
  }
}
/*******************************************************************************
**
** Function         nfc_ncif_modeSet_Ntf_timeout
**
** Description      This function is called when mode set ntf timedout
**                  To aviod infinite wait for mode set ntf
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_modeSet_Ntf_timeout() {
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("nfc_ncif_modeSet_Ntf_timeout");
  if(nfcFL.eseFL._ESE_DUAL_MODE_PRIO_SCHEME !=
          nfcFL.eseFL._ESE_WIRED_MODE_RESUME) {
      DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("DUAL_MODE_PRIO_SCHEME != WIRED_MODE_RESUME. Returning");
      return;
  }
  if (nfc_cb.bBlockWiredMode) {
    nfc_cb.bBlockWiredMode = false;
    nfc_cb.bCeActivatedeSE = false;
    nfc_ncif_allow_dwp_transmission();
  }
}
/*******************************************************************************
**
** Function         nfc_ncif_modeSet_rsp_timeout
**
** Description      This function is called when mode set rsp timedout
**                  To aviod infinite wait for mode set rsp
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_modeSet_rsp_timeout() {
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("nfc_ncif_modeSet_rsp_timeout");
  if(nfcFL.eseFL._ESE_DUAL_MODE_PRIO_SCHEME !=
          nfcFL.eseFL._ESE_WIRED_MODE_RESUME) {
      DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("DUAL_MODE_PRIO_SCHEME != WIRED_MODE_RESUME. Returning");
      return;
  }
  tNFC_NFCEE_MODE_SET_REVT mode_set_info;
  tNFC_RESPONSE_CBACK* p_cback = nfc_cb.p_resp_cback;
  tNFC_NFCEE_INFO_REVT nfcee_info;
  tNFC_RESPONSE* p_evt = (tNFC_RESPONSE*)&nfcee_info;
  tNFC_RESPONSE_EVT event = NFC_NFCEE_INFO_REVT;
  p_evt = (tNFC_RESPONSE*)&mode_set_info;
  event = NFC_NFCEE_MODE_SET_REVT;
  mode_set_info.nfcee_id = NFCEE_ID_ESE;
  mode_set_info.mode = NFC_MODE_ACTIVATE;
  mode_set_info.status = NCI_STATUS_OK;
  if (p_cback) {
    (*p_cback)(event, p_evt);
  }
}
/*******************************************************************************
**
** Function         nfc_ncif_pwr_link_rsp_timeout
**
** Description      This function is called when pwr link cmd is ignored due to
*rf session ongoing and sending fake resp
**                  to aviod infinite wait for power link Rsp
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_pwr_link_rsp_timeout() {
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("nfc_ncif_pwr_link_rsp_timeout");
  if(nfcFL.eseFL._ESE_DUAL_MODE_PRIO_SCHEME !=
          nfcFL.eseFL._ESE_WIRED_MODE_RESUME) {
      DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("DUAL_MODE_PRIO_SCHEME != WIRED_MODE_RESUME. Returning");
      return;
  }
  tNFC_RESPONSE_CBACK* p_cback = nfc_cb.p_resp_cback;
  tNFC_NFCEE_EE_PWR_LNK_REVT pwr_lnk_ctrl;
  tNFC_NFCEE_INFO_REVT nfcee_info;
  tNFC_RESPONSE_EVT event = NFC_NFCEE_INFO_REVT;
  tNFC_RESPONSE* p_evt = (tNFC_RESPONSE*)&nfcee_info;
  p_evt = (tNFC_RESPONSE*)&pwr_lnk_ctrl;
  pwr_lnk_ctrl.status = NCI_STATUS_OK;
  pwr_lnk_ctrl.nfcee_id = NFCEE_ID_ESE;
  event = NFC_NFCEE_PWR_LNK_CTRL_REVT;
  if (p_cback) (*p_cback)(event, p_evt);
}
#endif

/*******************************************************************************
**
** Function         nfc_ncif_proc_credits
**
** Description      This function is called to process data credits
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_proc_credits(uint8_t* p, uint16_t plen) {
  uint8_t num, xx;
  tNFC_CONN_CB* p_cb;

  if (plen != 0) {
    num = *p++;
    plen--;
    if (num > plen) {
      android_errorWriteLog(0x534e4554, "118148142");
      return;
    }
    for (xx = 0; xx < num; xx++) {
      p_cb = nfc_find_conn_cb_by_conn_id(*p++);
      if (p_cb && p_cb->num_buff != NFC_CONN_NO_FC) {
        p_cb->num_buff += (*p);
#if (BT_USE_TRACES == true)
        if (p_cb->num_buff > p_cb->init_credits) {
          if (nfc_cb.nfc_state == NFC_STATE_OPEN) {
            /* if this happens in activated state, it's very likely that our
             * NFCC has issues */
            /* However, credit may be returned after deactivation */
            LOG(ERROR) << StringPrintf("num_buff:0x%x, init_credits:0x%x",
                                       p_cb->num_buff, p_cb->init_credits);
          }
          p_cb->num_buff = p_cb->init_credits;
        }
#endif
#if (NXP_EXTNS == TRUE)
        if (p_cb->conn_id != NFC_RF_CONN_ID) {
          nfc_stop_timer(&nfc_cb.nci_wait_data_ntf_timer);
        }
        if (p_cb->conn_id == NFC_NFCEE_CONN_ID) {
          nfc_cb.bIsCreditNtfRcvd = true;
        }
#endif
        if (get_i2c_fragmentation_enabled() == I2C_FRAGMENATATION_ENABLED) {
          nfc_ncif_update_data_queue();
        }
        /* check if there's nay data in tx q to be sent */
        nfc_ncif_send_data(p_cb, nullptr);
      }
      p++;
    }
  }
}
/*******************************************************************************
**
** Function         nfc_ncif_decode_rf_params
**
** Description      This function is called to process the detected technology
**                  and mode and the associated parameters for DISCOVER_NTF and
**                  ACTIVATE_NTF
**
** Returns          void
**
*******************************************************************************/
uint8_t* nfc_ncif_decode_rf_params(tNFC_RF_TECH_PARAMS* p_param, uint8_t* p) {
  tNFC_RF_PA_PARAMS* p_pa;
  uint8_t len, *p_start, u8;
  tNFC_RF_PB_PARAMS* p_pb;
  tNFC_RF_LF_PARAMS* p_lf;
  tNFC_RF_PF_PARAMS* p_pf;
  tNFC_RF_PISO15693_PARAMS* p_i93;
  tNFC_RF_ACM_P_PARAMS* acm_p;
  uint8_t mpl_idx = 0;
  uint8_t gb_idx = 0, mpl;
  uint8_t plen;
  plen = len = *p++;
  p_start = p;
  memset(&p_param->param, 0, sizeof(tNFC_RF_TECH_PARAMU));
 if (NCI_DISCOVERY_TYPE_POLL_A == p_param->mode ||
      (NCI_DISCOVERY_TYPE_POLL_A_ACTIVE == p_param->mode &&
       NFC_GetNCIVersion() != NCI_VERSION_2_0)) {
    p_pa = &p_param->param.pa;
    /*
SENS_RES Response   2 bytes Defined in [DIGPROT] Available after Technology
Detection
NFCID1 length   1 byte  Length of NFCID1 Available after Collision Resolution
NFCID1  4, 7, or 10 bytes   Defined in [DIGPROT]Available after Collision
Resolution
SEL_RES Response    1 byte  Defined in [DIGPROT]Available after Collision
Resolution
HRx Length  1 Octets    Length of HRx Parameters collected from the response to
the T1T RID command.
HRx 0 or 2 Octets   If present, the first byte SHALL contain HR0 and the second
byte SHALL contain HR1 as defined in [DIGITAL].
    */
    if (plen < 3) {
      goto invalid_packet;
    }
    plen -= 3;
    STREAM_TO_ARRAY(p_pa->sens_res, p, 2);
    p_pa->nfcid1_len = *p++;
    if (p_pa->nfcid1_len > NCI_NFCID1_MAX_LEN)
      p_pa->nfcid1_len = NCI_NFCID1_MAX_LEN;

    if (plen < p_pa->nfcid1_len + 1) {
      goto invalid_packet;
    }
    plen -= (p_pa->nfcid1_len + 1);
    STREAM_TO_ARRAY(p_pa->nfcid1, p, p_pa->nfcid1_len);
    u8 = *p++;

    if (u8) {
      if (plen < 1) {
        goto invalid_packet;
      }
      plen--;
      p_pa->sel_rsp = *p++;
    }

    if (len ==
        (7 + p_pa->nfcid1_len + u8)) /* 2(sens_res) + 1(len) +
                                        p_pa->nfcid1_len + 1(len) + u8 + hr
                                        (1:len + 2) */
    {
      p_pa->hr_len = *p++;
      if (p_pa->hr_len == NCI_T1T_HR_LEN) {
        p_pa->hr[0] = *p++;
        p_pa->hr[1] = *p;
      }
    }
  } else if (NCI_DISCOVERY_TYPE_POLL_B == p_param->mode) {
    /*
SENSB_RES Response length (n)   1 byte  Length of SENSB_RES Response (Byte 2 -
Byte 12 or 13)Available after Technology Detection
SENSB_RES Response Byte 2 - Byte 12 or 13   11 or 12 bytes  Defined in [DIGPROT]
Available after Technology Detection
    */
    p_pb = &p_param->param.pb;

    if (plen < 1) {
      goto invalid_packet;
    }
    plen--;
    p_pb->sensb_res_len = *p++;
    if (p_pb->sensb_res_len > NCI_MAX_SENSB_RES_LEN)
      p_pb->sensb_res_len = NCI_MAX_SENSB_RES_LEN;

    if (plen < p_pb->sensb_res_len) {
      goto invalid_packet;
    }
    plen -= p_pb->sensb_res_len;
    STREAM_TO_ARRAY(p_pb->sensb_res, p, p_pb->sensb_res_len);
    memcpy(p_pb->nfcid0, p_pb->sensb_res, NFC_NFCID0_MAX_LEN);
  } else if (NCI_DISCOVERY_TYPE_POLL_F == p_param->mode ||
             (NCI_DISCOVERY_TYPE_POLL_F_ACTIVE == p_param->mode &&
              NFC_GetNCIVersion() != NCI_VERSION_2_0)) {
    /*
Bit Rate    1 byte  1   212 kbps/2   424 kbps/0 and 3 to 255  RFU
SENSF_RES Response length.(n) 1 byte  Length of SENSF_RES (Byte 2 - Byte 17 or
19).Available after Technology Detection
SENSF_RES Response Byte 2 - Byte 17 or 19  n bytes Defined in [DIGPROT]
Available after Technology Detection
    */
    p_pf = &p_param->param.pf;

    if (plen < 2) {
      goto invalid_packet;
    }
    plen -= 2;
    p_pf->bit_rate = *p++;
    p_pf->sensf_res_len = *p++;
    if (p_pf->sensf_res_len > NCI_MAX_SENSF_RES_LEN)
      p_pf->sensf_res_len = NCI_MAX_SENSF_RES_LEN;

    if (plen < p_pf->sensf_res_len) {
      goto invalid_packet;
    }
    plen -= p_pf->sensf_res_len;
    STREAM_TO_ARRAY(p_pf->sensf_res, p, p_pf->sensf_res_len);

    if (p_pf->sensf_res_len < NCI_MRTI_UPDATE_INDEX + 1) {
      goto invalid_packet;
    }
    memcpy(p_pf->nfcid2, p_pf->sensf_res, NCI_NFCID2_LEN);
    p_pf->mrti_check = p_pf->sensf_res[NCI_MRTI_CHECK_INDEX];
    p_pf->mrti_update = p_pf->sensf_res[NCI_MRTI_UPDATE_INDEX];
  } else if (NCI_DISCOVERY_TYPE_LISTEN_F == p_param->mode ||
             (NCI_DISCOVERY_TYPE_LISTEN_F_ACTIVE == p_param->mode &&
              NFC_GetNCIVersion() != NCI_VERSION_2_0)) {
    p_lf = &p_param->param.lf;

    if (plen < 1) {
      goto invalid_packet;
    }
    plen--;
    u8 = *p++;
    if (u8) {
      if (plen < NCI_NFCID2_LEN) {
        goto invalid_packet;
      }
      plen -= NCI_NFCID2_LEN;
      STREAM_TO_ARRAY(p_lf->nfcid2, p, NCI_NFCID2_LEN);
    }
  } else if (NCI_DISCOVERY_TYPE_POLL_V == p_param->mode) {
    p_i93 = &p_param->param.pi93;

    if (plen < 2) {
      goto invalid_packet;
    }
    plen -= 2;
    p_i93->flag = *p++;
    p_i93->dsfid = *p++;

    if (plen < NFC_ISO15693_UID_LEN) {
      goto invalid_packet;
    }
    plen -= NFC_ISO15693_UID_LEN;
    STREAM_TO_ARRAY(p_i93->uid, p, NFC_ISO15693_UID_LEN);
  } else if (NCI_DISCOVERY_TYPE_POLL_KOVIO == p_param->mode) {
    p_param->param.pk.uid_len = len;
    if (p_param->param.pk.uid_len > NFC_KOVIO_MAX_LEN) {
      LOG(ERROR) << StringPrintf("Kovio UID len:0x%x exceeds max(0x%x)",
                       p_param->param.pk.uid_len, NFC_KOVIO_MAX_LEN);
      p_param->param.pk.uid_len = NFC_KOVIO_MAX_LEN;
    }
    STREAM_TO_ARRAY(p_param->param.pk.uid, p, p_param->param.pk.uid_len);
  } else if (NCI_DISCOVERY_TYPE_POLL_ACTIVE == p_param->mode) {
    acm_p = &p_param->param.acm_p;

    if (plen < 1) {
      goto invalid_packet;
    }
    plen--;
    acm_p->atr_res_len = *p++;
    if (acm_p->atr_res_len > 0) {
      if (acm_p->atr_res_len > NFC_MAX_ATS_LEN)
        acm_p->atr_res_len = NFC_MAX_ATS_LEN;

      if (plen < acm_p->atr_res_len) {
        goto invalid_packet;
      }
      plen -= acm_p->atr_res_len;
      STREAM_TO_ARRAY(acm_p->atr_res, p, acm_p->atr_res_len);
      /* ATR_RES
      Byte 3~12 Byte 13 Byte 14 Byte 15 Byte 16 Byte 17 Byte 18~18+n
      NFCID3T   DIDT    BST     BRT     TO      PPT     [GT0 ... GTn] */
      mpl_idx = 14;
      gb_idx = NCI_P_GEN_BYTE_INDEX;

      if (acm_p->atr_res_len < mpl_idx + 1) {
        goto invalid_packet;
      }
      acm_p->waiting_time = acm_p->atr_res[NCI_L_NFC_DEP_TO_INDEX] & 0x0F;
      mpl = ((acm_p->atr_res[mpl_idx]) >> 4) & 0x03;
      acm_p->max_payload_size = nfc_mpl_code_to_size[mpl];
      if (acm_p->atr_res_len > gb_idx) {
        acm_p->gen_bytes_len = acm_p->atr_res_len - gb_idx;
        if (acm_p->gen_bytes_len > NFC_MAX_GEN_BYTES_LEN)
          acm_p->gen_bytes_len = NFC_MAX_GEN_BYTES_LEN;
        memcpy(acm_p->gen_bytes, &acm_p->atr_res[gb_idx], acm_p->gen_bytes_len);
      }
    }
  } else if (NCI_DISCOVERY_TYPE_LISTEN_ACTIVE == p_param->mode) {
    acm_p = &p_param->param.acm_p;

    if (plen < 1) {
      goto invalid_packet;
    }
    plen--;
    acm_p->atr_res_len = *p++;
    if (acm_p->atr_res_len > 0) {
      if (acm_p->atr_res_len > NFC_MAX_ATS_LEN)
        acm_p->atr_res_len = NFC_MAX_ATS_LEN;

      if (plen < acm_p->atr_res_len) {
        goto invalid_packet;
      }
      plen -= acm_p->atr_res_len;
      STREAM_TO_ARRAY(acm_p->atr_res, p, acm_p->atr_res_len);
      /* ATR_REQ
      Byte 3~12 Byte 13 Byte 14 Byte 15 Byte 16 Byte 17~17+n
      NFCID3I   DIDI    BSI     BRI     PPI     [GI0 ... GIn] */
      mpl_idx = 13;
      gb_idx = NCI_L_GEN_BYTE_INDEX;

      if (acm_p->atr_res_len < mpl_idx + 1) {
        goto invalid_packet;
      }
      mpl = ((acm_p->atr_res[mpl_idx]) >> 4) & 0x03;
      acm_p->max_payload_size = nfc_mpl_code_to_size[mpl];
      if (acm_p->atr_res_len > gb_idx) {
        acm_p->gen_bytes_len = acm_p->atr_res_len - gb_idx;
        if (acm_p->gen_bytes_len > NFC_MAX_GEN_BYTES_LEN)
          acm_p->gen_bytes_len = NFC_MAX_GEN_BYTES_LEN;
        memcpy(acm_p->gen_bytes, &acm_p->atr_res[gb_idx], acm_p->gen_bytes_len);
      }
    }
  }
invalid_packet:
  return (p_start + len);
}

/*******************************************************************************
**
** Function         nfc_ncif_proc_discover_ntf
**
** Description      This function is called to process discover notification
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_proc_discover_ntf(uint8_t* p, uint16_t plen) {
  tNFC_DISCOVER evt_data;

  if (nfc_cb.p_discv_cback) {
    // validate packet length should be larger than (NCI header + rf_disc_id +
    // protocol + mode + length of rf parameters).
    if (plen < NCI_MSG_HDR_SIZE + 4) {
      evt_data.status = NCI_STATUS_FAILED;
      goto invalid_packet;
    }
    plen -= (NCI_MSG_HDR_SIZE + 4);
    p += NCI_MSG_HDR_SIZE;
    evt_data.status = NCI_STATUS_OK;
    evt_data.result.rf_disc_id = *p++;
    evt_data.result.protocol = *p++;

    /* fill in tNFC_RESULT_DEVT */
    evt_data.result.rf_tech_param.mode = *p++;

    // validate packet length should be larger than (rf_tech_param + ntf_type)
    if (plen < *p + 1) {
      evt_data.status = NCI_STATUS_FAILED;
      goto invalid_packet;
    }
    plen -= (*p + 1);
    p = nfc_ncif_decode_rf_params(&evt_data.result.rf_tech_param, p);

    evt_data.result.more = *p++;

  invalid_packet:
    (*nfc_cb.p_discv_cback)(NFC_RESULT_DEVT, &evt_data);
  }
}
/*******************************************************************************
**
** Function         nfc_ncif_proc_isodep_nak_presence_check_status
**
** Description      This function is called to handle response and notification for
**                  presence check nak command
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_proc_isodep_nak_presence_check_status (uint8_t status, bool is_ntf) {
  rw_t4t_handle_isodep_nak_rsp(status , is_ntf);
}
#if (NXP_EXTNS == TRUE)
/*******************************************************************************
**
** Function         nfc_ncif_proc_rf_wtx_ntf
**
** Description      This function is called to process rf wtx notification
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_proc_rf_wtx_ntf(__attribute__((unused)) uint8_t* p,
                              __attribute__((unused)) uint16_t plen) {
  tNFC_CONN_CB* p_cb = nullptr;
  p_cb = nfc_find_conn_cb_by_conn_id(NFC_RF_CONN_ID);

  if (nullptr != p_cb) {
    if ((p_cb->conn_id == NFC_RF_CONN_ID) && (p_cb->p_cback)) {
      /* Indicate upper layer that local device started receiving rf wtx */
      (*p_cb->p_cback)(p_cb->conn_id, NFC_RF_WTX_CEVT, nullptr);
    }
  }
}

#endif

/*******************************************************************************
**
** Function         nfc_ncif_proc_activate
**
** Description      This function is called to process de-activate
**                  response and notification
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_proc_activate(uint8_t* p, uint8_t len) {
  tNFC_DISCOVER evt_data;
  tNFC_INTF_PARAMS* p_intf = &evt_data.activate.intf_param;
  tNFC_INTF_PA_ISO_DEP* p_pa_iso;
  tNFC_INTF_LB_ISO_DEP* p_lb_iso;
  tNFC_INTF_PB_ISO_DEP* p_pb_iso;
#if (NFC_RW_ONLY == FALSE)
  tNFC_INTF_PA_NFC_DEP* p_pa_nfc;
  int mpl_idx = 0;
  uint8_t gb_idx = 0, mpl;
#endif
  uint8_t t0;
  tNCI_DISCOVERY_TYPE mode;
  tNFC_CONN_CB* p_cb = &nfc_cb.conn_cb[NFC_RF_CONN_ID];
  uint8_t* pp, len_act;
  uint8_t buff_size, num_buff;
  tNFC_RF_PA_PARAMS* p_pa;
  uint8_t plen = len, pplen = 0;

  nfc_set_state(NFC_STATE_OPEN);

  memset(p_intf, 0, sizeof(tNFC_INTF_PARAMS));
  // validate packet length should be larger than (rf_disc_id + type +
  // protocol + mode + buff_size + num_buff + length of rf parameters).
  if (plen < 7) {
    evt_data.status = NCI_STATUS_FAILED;
    goto invalid_packet;
  }
  plen -= 7;

  evt_data.activate.rf_disc_id = *p++;
  p_intf->type = *p++;
  evt_data.activate.protocol = *p++;

  if (evt_data.activate.protocol == NCI_PROTOCOL_18092_ACTIVE)
    evt_data.activate.protocol = NCI_PROTOCOL_NFC_DEP;

#if (NXP_EXTNS == TRUE)
  if ((evt_data.activate.protocol == NCI_PROTOCOL_UNKNOWN) &&
      (p_intf->type == NCI_INTERFACE_FRAME))
    evt_data.activate.protocol = NCI_PROTOCOL_T3BT;
#endif

  evt_data.activate.rf_tech_param.mode = *p++;
  buff_size = *p++;
  num_buff = *p++;
  /* fill in tNFC_activate_DEVT */
  // validate remaining packet length should be larger than (rf_tech_param +
  // data_mode + tx_bitrate + rx_bitrte + len_act).
  if (plen < *p + 4) {
    evt_data.status = NCI_STATUS_FAILED;
    goto invalid_packet;
  }
  plen -= (*p + 4);
  p = nfc_ncif_decode_rf_params(&evt_data.activate.rf_tech_param, p);

  evt_data.activate.data_mode = *p++;
  evt_data.activate.tx_bitrate = *p++;
  evt_data.activate.rx_bitrate = *p++;
  mode = evt_data.activate.rf_tech_param.mode;
  len_act = *p++;
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("nfc_ncif_proc_activate:%d %d, mode:0x%02x", len, len_act,
                   mode);

#if (NXP_EXTNS == TRUE)
  if ((evt_data.activate.protocol == NCI_PROTOCOL_NFC_DEP) &&
      (mode == NCI_DISCOVERY_TYPE_LISTEN_F)) {
    sListenActivated = true;
    nfc_start_timer(&nfc_cb.listen_activation_timer_list,
                    (uint16_t)(NFC_TTYPE_LISTEN_ACTIVATION), 2);
  }
  /*Stop rf_filed timer only if SPI is not open*/
  if ((nfcFL.eseFL._ESE_DUAL_MODE_PRIO_SCHEME ==
       nfcFL.eseFL._ESE_WIRED_MODE_RESUME) &&
      nfc_cb.bBlockWiredMode &&
      (NFC_INTF_REQ_SRC_DWP == nfc_cb.pwr_link_cmd.reqSrc)) {
    nfc_stop_timer(&nfc_cb.rf_filed_event_timeout_timer);
  }
#endif

  /* just in case the interface reports activation parameters not defined in the
   * NCI spec */
  p_intf->intf_param.frame.param_len = len_act;
  if (p_intf->intf_param.frame.param_len > NFC_MAX_RAW_PARAMS)
    p_intf->intf_param.frame.param_len = NFC_MAX_RAW_PARAMS;
  pp = p;

  if (plen < p_intf->intf_param.frame.param_len) {
    evt_data.status = NCI_STATUS_FAILED;
    goto invalid_packet;
  }
  STREAM_TO_ARRAY(p_intf->intf_param.frame.param, pp,
                  p_intf->intf_param.frame.param_len);
  if (evt_data.activate.intf_param.type == NCI_INTERFACE_ISO_DEP) {
    /* Make max payload of NCI aligned to max payload of ISO-DEP for better
     * performance */
    if (buff_size > NCI_ISO_DEP_MAX_INFO) buff_size = NCI_ISO_DEP_MAX_INFO;

    switch (mode) {
      case NCI_DISCOVERY_TYPE_POLL_A:
        p_pa_iso = &p_intf->intf_param.pa_iso;

        if (plen < 1) {
          evt_data.status = NCI_STATUS_FAILED;
          goto invalid_packet;
        }
        plen--;
        p_pa_iso->ats_res_len = *p++;

        if (p_pa_iso->ats_res_len == 0) break;

        if (p_pa_iso->ats_res_len > NFC_MAX_ATS_LEN)
          p_pa_iso->ats_res_len = NFC_MAX_ATS_LEN;

        if (plen < p_pa_iso->ats_res_len) {
          evt_data.status = NCI_STATUS_FAILED;
          goto invalid_packet;
        }
        plen -= p_pa_iso->ats_res_len;
        STREAM_TO_ARRAY(p_pa_iso->ats_res, p, p_pa_iso->ats_res_len);

        pplen = p_pa_iso->ats_res_len;
        pp = &p_pa_iso->ats_res[NCI_ATS_T0_INDEX];
        t0 = p_pa_iso->ats_res[NCI_ATS_T0_INDEX];
        pp++;                           /* T0 */
        pplen--;
        if (t0 & NCI_ATS_TA_MASK) {
          if (pplen < 1) {
            evt_data.status = NCI_STATUS_FAILED;
            goto invalid_packet;
          }
          pplen--;
          pp++; /* TA */
        }
        if (t0 & NCI_ATS_TB_MASK) {
          /* FWI (Frame Waiting time Integer) & SPGI (Start-up Frame Guard time
           * Integer) */
          if (pplen < 1) {
            evt_data.status = NCI_STATUS_FAILED;
            goto invalid_packet;
          }
          pplen--;
          p_pa_iso->fwi = (((*pp) >> 4) & 0x0F);
          p_pa_iso->sfgi = ((*pp) & 0x0F);
          pp++; /* TB */
        }
        if (t0 & NCI_ATS_TC_MASK) {
          if (pplen < 1) {
            evt_data.status = NCI_STATUS_FAILED;
            goto invalid_packet;
          }
          pplen--;
          p_pa_iso->nad_used = ((*pp) & 0x01);
          pp++; /* TC */
        }
        p_pa_iso->his_byte_len =
            (uint8_t)(p_pa_iso->ats_res_len - (pp - p_pa_iso->ats_res));
        if (p_pa_iso->his_byte_len > NFC_MAX_HIS_BYTES_LEN)
          p_pa_iso->his_byte_len = NFC_MAX_HIS_BYTES_LEN;
        if (pplen < p_pa_iso->his_byte_len) {
          evt_data.status = NCI_STATUS_FAILED;
          goto invalid_packet;
        }
        memcpy(p_pa_iso->his_byte, pp, p_pa_iso->his_byte_len);
        break;

      case NCI_DISCOVERY_TYPE_LISTEN_A:
        if (plen < 1) {
          evt_data.status = NCI_STATUS_FAILED;
          goto invalid_packet;
        }
        plen--;
        p_intf->intf_param.la_iso.rats = *p++;
        gettimeofday(&timer_start, nullptr);
        break;

      case NCI_DISCOVERY_TYPE_POLL_B:
        /* ATTRIB RSP
        Byte 1   Byte 2 ~ 2+n-1
        MBLI/DID Higher layer - Response
        */
        p_pb_iso = &p_intf->intf_param.pb_iso;

        if (plen < 1) {
          evt_data.status = NCI_STATUS_FAILED;
          goto invalid_packet;
        }
        plen--;
        p_pb_iso->attrib_res_len = *p++;

        if (p_pb_iso->attrib_res_len == 0) break;

        if (p_pb_iso->attrib_res_len > NFC_MAX_ATTRIB_LEN)
          p_pb_iso->attrib_res_len = NFC_MAX_ATTRIB_LEN;

        if (plen < p_pb_iso->attrib_res_len) {
          evt_data.status = NCI_STATUS_FAILED;
          goto invalid_packet;
        }
        plen -= p_pb_iso->attrib_res_len;
        STREAM_TO_ARRAY(p_pb_iso->attrib_res, p, p_pb_iso->attrib_res_len);
        p_pb_iso->mbli = (p_pb_iso->attrib_res[0]) >> 4;
        if (p_pb_iso->attrib_res_len > NFC_PB_ATTRIB_REQ_FIXED_BYTES) {
          p_pb_iso->hi_info_len =
              p_pb_iso->attrib_res_len - NFC_PB_ATTRIB_REQ_FIXED_BYTES;
          if (p_pb_iso->hi_info_len > NFC_MAX_GEN_BYTES_LEN)
            p_pb_iso->hi_info_len = NFC_MAX_GEN_BYTES_LEN;
          memcpy(p_pb_iso->hi_info,
                 &p_pb_iso->attrib_res[NFC_PB_ATTRIB_REQ_FIXED_BYTES],
                 p_pb_iso->hi_info_len);
        }
        break;

      case NCI_DISCOVERY_TYPE_LISTEN_B:
        /* ATTRIB CMD
        Byte 2~5 Byte 6  Byte 7  Byte 8  Byte 9  Byte 10 ~ 10+k-1
        NFCID0   Param 1 Param 2 Param 3 Param 4 Higher layer - INF
        */
        p_lb_iso = &p_intf->intf_param.lb_iso;

        if (plen < 1) {
          evt_data.status = NCI_STATUS_FAILED;
          goto invalid_packet;
        }
        plen--;
        p_lb_iso->attrib_req_len = *p++;

        if (p_lb_iso->attrib_req_len == 0) break;

        if (p_lb_iso->attrib_req_len > NFC_MAX_ATTRIB_LEN)
          p_lb_iso->attrib_req_len = NFC_MAX_ATTRIB_LEN;

        if (plen < p_lb_iso->attrib_req_len) {
          evt_data.status = NCI_STATUS_FAILED;
          goto invalid_packet;
        }
        plen -= p_lb_iso->attrib_req_len;
        STREAM_TO_ARRAY(p_lb_iso->attrib_req, p, p_lb_iso->attrib_req_len);

        if (p_lb_iso->attrib_req_len < NFC_NFCID0_MAX_LEN) {
          evt_data.status = NCI_STATUS_FAILED;
          goto invalid_packet;
        }
        memcpy(p_lb_iso->nfcid0, p_lb_iso->attrib_req, NFC_NFCID0_MAX_LEN);
        if (p_lb_iso->attrib_req_len > NFC_LB_ATTRIB_REQ_FIXED_BYTES) {
          p_lb_iso->hi_info_len =
              p_lb_iso->attrib_req_len - NFC_LB_ATTRIB_REQ_FIXED_BYTES;
          if (p_lb_iso->hi_info_len > NFC_MAX_GEN_BYTES_LEN)
            p_lb_iso->hi_info_len = NFC_MAX_GEN_BYTES_LEN;
          memcpy(p_lb_iso->hi_info,
                 &p_lb_iso->attrib_req[NFC_LB_ATTRIB_REQ_FIXED_BYTES],
                 p_lb_iso->hi_info_len);
        }
        gettimeofday(&timer_start, nullptr);
        break;
    }

  }
#if (NFC_RW_ONLY == FALSE)
  else if (evt_data.activate.intf_param.type == NCI_INTERFACE_NFC_DEP) {
#if (NXP_EXTNS == TRUE)
      if ((nfcFL.eseFL._ESE_DUAL_MODE_PRIO_SCHEME ==
              nfcFL.eseFL._ESE_WIRED_MODE_RESUME) && nfc_cb.bBlockWiredMode) {
          nfc_ncif_resume_dwp_wired_mode();
      }
#endif
    /* Make max payload of NCI aligned to max payload of NFC-DEP for better
     * performance */
    if (buff_size > NCI_NFC_DEP_MAX_DATA) buff_size = NCI_NFC_DEP_MAX_DATA;

    p_pa_nfc = &p_intf->intf_param.pa_nfc;

    /* Active mode, no info in activation parameters (NCI 2.0) */
    if ((NFC_GetNCIVersion() == NCI_VERSION_2_0) &&
        ((mode == NCI_DISCOVERY_TYPE_POLL_ACTIVE) ||
         (mode == NCI_DISCOVERY_TYPE_LISTEN_ACTIVE))) {
        p_pa_nfc->atr_res_len =
                  evt_data.activate.rf_tech_param.param.acm_p.atr_res_len;
    } else {
      if (plen < 1) {
        evt_data.status = NCI_STATUS_FAILED;
        goto invalid_packet;
      }
      plen--;
      p_pa_nfc->atr_res_len = *p++;
    }

    if (p_pa_nfc->atr_res_len > 0) {
      if (p_pa_nfc->atr_res_len > NFC_MAX_ATS_LEN)
        p_pa_nfc->atr_res_len = NFC_MAX_ATS_LEN;

      if ((NFC_GetNCIVersion() == NCI_VERSION_2_0) &&
          ((mode == NCI_DISCOVERY_TYPE_POLL_ACTIVE) ||
           (mode == NCI_DISCOVERY_TYPE_LISTEN_ACTIVE))) {
         /* NCI 2.0 : ATR_RES is included in RF technology parameters in active mode */
         memcpy(p_pa_nfc->atr_res,
                evt_data.activate.rf_tech_param.param.acm_p.atr_res,
                p_pa_nfc->atr_res_len);
      } else {
        if (plen < p_pa_nfc->atr_res_len) {
          evt_data.status = NCI_STATUS_FAILED;
          goto invalid_packet;
        }
        plen -= p_pa_nfc->atr_res_len;
        STREAM_TO_ARRAY(p_pa_nfc->atr_res, p, p_pa_nfc->atr_res_len);
      }

      if ((mode == NCI_DISCOVERY_TYPE_POLL_A) ||
          (mode == NCI_DISCOVERY_TYPE_POLL_F) ||
          ((mode == NCI_DISCOVERY_TYPE_POLL_A_ACTIVE ||
            mode == NCI_DISCOVERY_TYPE_POLL_F_ACTIVE) &&
           NFC_GetNCIVersion() != NCI_VERSION_2_0) ||
          (NFC_GetNCIVersion() == NCI_VERSION_2_0 &&
           mode == NCI_DISCOVERY_TYPE_POLL_ACTIVE)) {
        /* ATR_RES
        Byte 3~12 Byte 13 Byte 14 Byte 15 Byte 16 Byte 17 Byte 18~18+n
        NFCID3T   DIDT    BST     BRT     TO      PPT     [GT0 ... GTn] */
        mpl_idx = 14;
        gb_idx = NCI_P_GEN_BYTE_INDEX;

        if (p_pa_nfc->atr_res_len < NCI_L_NFC_DEP_TO_INDEX + 1) {
          evt_data.status = NCI_STATUS_FAILED;
          goto invalid_packet;
        }
        p_pa_nfc->waiting_time =
            p_pa_nfc->atr_res[NCI_L_NFC_DEP_TO_INDEX] & 0x0F;
      } else if ((mode == NCI_DISCOVERY_TYPE_LISTEN_A) ||
                 (mode == NCI_DISCOVERY_TYPE_LISTEN_F) ||
                 (NFC_GetNCIVersion() != NCI_VERSION_2_0 &&
                  (mode == NCI_DISCOVERY_TYPE_LISTEN_A_ACTIVE ||
                   mode == NCI_DISCOVERY_TYPE_LISTEN_F_ACTIVE)) ||
                 (NFC_GetNCIVersion() == NCI_VERSION_2_0 &&
                  mode == NCI_DISCOVERY_TYPE_LISTEN_ACTIVE)) {
        /* ATR_REQ
        Byte 3~12 Byte 13 Byte 14 Byte 15 Byte 16 Byte 17~17+n
        NFCID3I   DIDI    BSI     BRI     PPI     [GI0 ... GIn] */
        mpl_idx = 13;
        gb_idx = NCI_L_GEN_BYTE_INDEX;
      }

      if (p_pa_nfc->atr_res_len < mpl_idx + 1) {
        evt_data.status = NCI_STATUS_FAILED;
        goto invalid_packet;
      }
      mpl = ((p_pa_nfc->atr_res[mpl_idx]) >> 4) & 0x03;
      p_pa_nfc->max_payload_size = nfc_mpl_code_to_size[mpl];
      if (p_pa_nfc->atr_res_len > gb_idx) {
        p_pa_nfc->gen_bytes_len = p_pa_nfc->atr_res_len - gb_idx;
        if (p_pa_nfc->gen_bytes_len > NFC_MAX_GEN_BYTES_LEN)
          p_pa_nfc->gen_bytes_len = NFC_MAX_GEN_BYTES_LEN;
        memcpy(p_pa_nfc->gen_bytes, &p_pa_nfc->atr_res[gb_idx],
               p_pa_nfc->gen_bytes_len);
      }
    }
  }
#endif
  else if ((evt_data.activate.intf_param.type == NCI_INTERFACE_FRAME) &&
           (evt_data.activate.protocol == NCI_PROTOCOL_T1T)) {
    p_pa = &evt_data.activate.rf_tech_param.param.pa;
    if ((len_act == NCI_T1T_HR_LEN) && (p_pa->hr_len == 0)) {
      p_pa->hr_len = NCI_T1T_HR_LEN;

      if (plen < 2) {
        evt_data.status = NCI_STATUS_FAILED;
        goto invalid_packet;
      }
      plen -= 2;
      p_pa->hr[0] = *p++;
      p_pa->hr[1] = *p++;
    }
  }
#if (NXP_EXTNS == TRUE)
  /*
   * Code to handle the Reader over SWP.
   * 1. Do not activate tag for this NTF.
   * 2. Pass this info to JNI as START_READER_EVT.
   */
  else if (evt_data.activate.intf_param.type == nfcFL.nfcMwFL._NCI_INTERFACE_UICC_DIRECT ||
           evt_data.activate.intf_param.type == nfcFL.nfcMwFL._NCI_INTERFACE_ESE_DIRECT) {
    DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("nfc_ncif_proc_activate:interface type  %x",
                     evt_data.activate.intf_param.type);
  }
#endif

  p_cb->act_protocol = evt_data.activate.protocol;
  p_cb->act_interface = evt_data.activate.intf_param.type;
#if (NXP_EXTNS == TRUE)
  p_cb->sel_res = evt_data.activate.rf_tech_param.param.pa.sel_rsp;
#endif
  p_cb->buff_size = buff_size;
  p_cb->num_buff = num_buff;
  p_cb->init_credits = num_buff;

invalid_packet:
  if (nfc_cb.p_discv_cback) {
    (*nfc_cb.p_discv_cback)(NFC_ACTIVATE_DEVT, &evt_data);
  }
}

/*******************************************************************************
**
** Function         nfc_ncif_proc_deactivate
**
** Description      This function is called to process de-activate
**                  response and notification
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_proc_deactivate(uint8_t status, uint8_t deact_type, bool is_ntf) {
  tNFC_DISCOVER evt_data = {0};
  tNFC_CONN_CB* p_cb = &nfc_cb.conn_cb[NFC_RF_CONN_ID];
#if (NXP_EXTNS == TRUE)
    tRW_T3T_CB *p_t3tcb = &rw_cb.tcb.t3t;
#endif
  void* p_data;

  nfc_set_state(NFC_STATE_IDLE);
  evt_data.deactivate.status = status;
  evt_data.deactivate.type = deact_type;
  evt_data.deactivate.is_ntf = is_ntf;
  if (NFC_GetNCIVersion() == NCI_VERSION_2_0) {
    evt_data.deactivate.reason = nfc_cb.deact_reason;
  }

  while ((p_data = GKI_dequeue(&p_cb->rx_q)) != nullptr) {
    GKI_freebuf(p_data);
  }

  while ((p_data = GKI_dequeue(&p_cb->tx_q)) != nullptr) {
    GKI_freebuf(p_data);
  }

  if (p_cb->p_cback) {
    tNFC_CONN nfc_conn;
    nfc_conn.deactivate = evt_data.deactivate;
    (*p_cb->p_cback)(NFC_RF_CONN_ID, NFC_DEACTIVATE_CEVT, &nfc_conn);
  }

#if (NXP_EXTNS == TRUE)
    /*Stop rf_filed timer only if SPI is not open*/
    if((nfcFL.eseFL._ESE_DUAL_MODE_PRIO_SCHEME == nfcFL.eseFL._ESE_WIRED_MODE_RESUME) &&
        (deact_type != NFC_DEACTIVATE_TYPE_SLEEP) && is_ntf)
    {
      if (nfc_cb.bBlockWiredMode &&
          (NFC_INTF_REQ_SRC_DWP == nfc_cb.pwr_link_cmd.reqSrc)) {
            nfc_stop_timer(&nfc_cb.rf_filed_event_timeout_timer);
            nfc_start_timer(&nfc_cb.rf_filed_event_timeout_timer, (uint16_t)(NFC_TTYPE_NCI_WAIT_RF_FIELD_NTF), NFC_NCI_RFFIELD_EVT_TIMEOUT);
        }
    }

    if (p_t3tcb->poll_timer.in_use)
    {
        DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: stopping t3t polling timer", __func__);
        nfc_stop_quick_timer (&p_t3tcb->poll_timer);
    }

  if ((nfc_cb.flags & (NFC_FL_DISCOVER_PENDING | NFC_FL_CONTROL_REQUESTED)) &&
      (deact_type == NFC_DEACTIVATE_TYPE_DISCOVERY) && (is_ntf == true)) {
    DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf(
        "Abnormal State, Deactivate NTF is ignored, MW is already going to "
        "Discovery state");
    return;
  }
#endif

  if (nfc_cb.p_discv_cback) {
    (*nfc_cb.p_discv_cback)(NFC_DEACTIVATE_DEVT, &evt_data);
  }

  // clear previous stored tick count if not comsumed
  if (timer_start.tv_sec != 0 || timer_start.tv_usec != 0) {
    memset(&timer_start, 0, sizeof(timer_start));
  }
}
/*******************************************************************************
**
** Function         nfc_ncif_proc_ee_action
**
** Description      This function is called to process NFCEE ACTION NTF
**
** Returns          void
**
*******************************************************************************/
#if ((NFC_NFCEE_INCLUDED == true) && (NFC_RW_ONLY == FALSE))
void nfc_ncif_proc_ee_action(uint8_t* p, uint16_t plen) {
  tNFC_EE_ACTION_REVT evt_data;
  tNFC_RESPONSE_CBACK* p_cback = nfc_cb.p_resp_cback;
  uint8_t data_len, ulen, tag, *p_data;
  uint8_t max_len;

  if (p_cback) {
    memset(&evt_data.act_data, 0, sizeof(tNFC_ACTION_DATA));
    evt_data.status = NFC_STATUS_OK;
    evt_data.nfcee_id = *p++;
    evt_data.act_data.trigger = *p++;
    data_len = *p++;
    if (plen >= 3) plen -= 3;
    if (data_len > plen) data_len = (uint8_t)plen;

    switch (evt_data.act_data.trigger) {
      case NCI_EE_TRIG_7816_SELECT:
        if (data_len > NFC_MAX_AID_LEN) data_len = NFC_MAX_AID_LEN;
        DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("AID len = %d", data_len);
        evt_data.act_data.param.aid.len_aid = data_len;
        STREAM_TO_ARRAY(evt_data.act_data.param.aid.aid, p, data_len);
        break;
      case NCI_EE_TRIG_RF_PROTOCOL:
        evt_data.act_data.param.protocol = *p++;
        break;
      case NCI_EE_TRIG_RF_TECHNOLOGY:
        evt_data.act_data.param.technology = *p++;
        break;
      case NCI_EE_TRIG_APP_INIT:
        while (data_len > NFC_TL_SIZE) {
          data_len -= NFC_TL_SIZE;
          tag = *p++;
          ulen = *p++;
          if (ulen > data_len) ulen = data_len;
          p_data = nullptr;
          max_len = ulen;
          switch (tag) {
            case NCI_EE_ACT_TAG_AID: /* AID                 */
              if (max_len > NFC_MAX_AID_LEN) max_len = NFC_MAX_AID_LEN;
              evt_data.act_data.param.app_init.len_aid = max_len;
              p_data = evt_data.act_data.param.app_init.aid;
              break;
            case NCI_EE_ACT_TAG_DATA: /* hex data for app    */
              if (max_len > NFC_MAX_APP_DATA_LEN)
                max_len = NFC_MAX_APP_DATA_LEN;
              evt_data.act_data.param.app_init.len_data = max_len;
              p_data = evt_data.act_data.param.app_init.data;
              break;
          }
          if (p_data) {
            STREAM_TO_ARRAY(p_data, p, max_len);
          }
          data_len -= ulen;
        }
        break;
    }

#if (NXP_EXTNS == TRUE)
    if(nfcFL.eseFL._ESE_DUAL_MODE_PRIO_SCHEME ==
            nfcFL.eseFL._ESE_WIRED_MODE_RESUME) {
      /*Stop rf_filed timer only if SPI is not open*/
      if (NFC_INTF_REQ_SRC_DWP == nfc_cb.pwr_link_cmd.reqSrc)
        nfc_stop_timer(&nfc_cb.rf_filed_event_timeout_timer);
        tNFC_CONN_CB* p_cb;
        p_cb = nfc_find_conn_cb_by_conn_id(nfa_hci_cb.conn_id);
        if (evt_data.nfcee_id != 0xC0) {
            nfc_cb.bBlockWiredMode = true;
            nfc_cb.bBlkPwrlinkAndModeSetCmd = true;
            if (!nfc_cb.bIsDwpResPending && nfa_hci_cb.assembling) {
                    /* To faster transceive time out when CE is activated with UICC during
                     * chained response being received*/
                    DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("restarted hci timer during chained response");
                    nfa_hci_cb.IsWiredSessionAborted = true;
                    nfa_sys_stop_timer(&nfa_hci_cb.timer);
                    if (p_cb && p_cb->p_cback)
                      (*p_cb->p_cback)(nfa_hci_cb.conn_id, NFC_HCI_RESTART_TIMER,
                              (tNFC_CONN*)&evt_data);
            }
        } else {
            if (evt_data.act_data.trigger == NCI_EE_TRIG_RF_TECHNOLOGY) {
                nfc_cb.bBlockWiredMode = true;
            } else if (nfc_cb.bBlockWiredMode) {
                nfc_cb.bCeActivatedeSE = true;
                nfc_ncif_resume_dwp_wired_mode();
            }
            nfc_main_flush_cmd_queue();
        }
    }
#endif
    tNFC_RESPONSE nfc_response;
    nfc_response.ee_action = evt_data;
    (*p_cback)(NFC_EE_ACTION_REVT, &nfc_response);
  }
}

/*******************************************************************************
**
** Function         nfc_ncif_proc_ee_discover_req
**
** Description      This function is called to process NFCEE DISCOVER REQ NTF
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_proc_ee_discover_req(uint8_t* p, uint16_t plen) {
  tNFC_RESPONSE_CBACK* p_cback = nfc_cb.p_resp_cback;
  tNFC_EE_DISCOVER_REQ_REVT ee_disc_req;
  tNFC_EE_DISCOVER_INFO* p_info;
  uint8_t u8;

  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("nfc_ncif_proc_ee_discover_req %d len:%d", *p, plen);

  if (*p > NFC_MAX_EE_DISC_ENTRIES) {
    android_errorWriteLog(0x534e4554, "122361874");
    LOG(ERROR) << __func__ << "Exceed NFC_MAX_EE_DISC_ENTRIES";
    return;
  }

  if (p_cback) {
    u8 = *p;
    ee_disc_req.status = NFC_STATUS_OK;
    ee_disc_req.num_info = *p++;
    p_info = ee_disc_req.info;
    if (plen) plen--;
    while ((u8 > 0) && (plen >= NFC_EE_DISCOVER_ENTRY_LEN)) {
      p_info->op = *p++;                  /* T */
      if (*p != NFC_EE_DISCOVER_INFO_LEN) /* L */
      {
        DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("bad entry len:%d", *p);
        return;
      }
      p++;
      /* V */
      p_info->nfcee_id = *p++;
      p_info->tech_n_mode = *p++;
      p_info->protocol = *p++;
      u8--;
      plen -= NFC_EE_DISCOVER_ENTRY_LEN;
      p_info++;
    }
    tNFC_RESPONSE nfc_response;
    nfc_response.ee_discover_req = ee_disc_req;
    (*p_cback)(NFC_EE_DISCOVER_REQ_REVT, &nfc_response);
  }
}

/*******************************************************************************
**
** Function         nfc_ncif_proc_get_routing
**
** Description      This function is called to process get routing notification
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_proc_get_routing(__attribute__((unused)) uint8_t* p,
                               __attribute__((unused))uint8_t len) {
  __attribute__((unused)) tNFC_STATUS status = NFC_STATUS_CONTINUE;

#if (NXP_EXTNS == FALSE)
  if (nfc_cb.p_resp_cback) {
    more = *p++;
    num_entries = *p++;
    for (xx = 0; xx < num_entries; xx++) {
      if ((more == false) && (xx == (num_entries - 1))) status = NFC_STATUS_OK;
      evt_data.status = (tNFC_STATUS)status;
      evt_data.nfcee_id = *p++;
      evt_data.num_tlvs = *p++;
      evt_data.tlv_size = 0;
      pn = evt_data.param_tlvs;
      for (yy = 0; yy < evt_data.num_tlvs; yy++) {
        tl = *(p + 1);
        tl += NFC_TL_SIZE;
        evt_data.tlv_size += tl;
        if (evt_data.tlv_size > NFC_MAX_EE_TLV_SIZE) {
          android_errorWriteLog(0x534e4554, "117554809");
          LOG(ERROR) << __func__ << "Invalid data format";
          return;
        }
        STREAM_TO_ARRAY(pn, p, tl);
        pn += tl;
      }
      (*nfc_cb.p_resp_cback)(NFC_GET_ROUTING_REVT, (tNFC_RESPONSE*)&evt_data);
    }
  }
  if (nfc_cb.p_resp_cback) {
    more = *p;
    if (more == false) {
      status = NFC_STATUS_OK;
    }
    evt_data.status = (tNFC_STATUS)status;
    evt_data.num_tlvs = *(p + 1);
    evt_data.tlv_size = len;
    memcpy(evt_data.param_tlvs, p, len);
    tNFC_RESPONSE nfc_response;
    nfc_response.get_routing = evt_data;
    (*nfc_cb.p_resp_cback)(NFC_GET_ROUTING_REVT, &nfc_response);
  }
#endif
}
#endif

/*******************************************************************************
**
** Function         nfc_ncif_proc_conn_create_rsp
**
** Description      This function is called to process connection create
**                  response
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_proc_conn_create_rsp(uint8_t* p,
                                  __attribute__((unused)) uint16_t plen,
                                   uint8_t dest_type) {
  tNFC_CONN_CB* p_cb;
  tNFC_STATUS status;
  tNFC_CONN_CBACK* p_cback;
  tNFC_CONN evt_data;
  uint8_t conn_id;

  /* find the pending connection control block */
  p_cb = nfc_find_conn_cb_by_conn_id(NFC_PEND_CONN_ID);
  if (p_cb) {
    p += NCI_MSG_HDR_SIZE;
    status = *p++;
    p_cb->buff_size = *p++;
    p_cb->num_buff = p_cb->init_credits = *p++;
    conn_id = *p++;
    evt_data.conn_create.status = status;
    evt_data.conn_create.dest_type = dest_type;
    evt_data.conn_create.id = p_cb->id;
    evt_data.conn_create.buff_size = p_cb->buff_size;
    evt_data.conn_create.num_buffs = p_cb->num_buff;
    p_cback = p_cb->p_cback;
    if (status == NCI_STATUS_OK) {
      nfc_set_conn_id(p_cb, conn_id);
#if (NXP_EXTNS == TRUE)
      if (p_cb->id == 1) {
        nfcc_dh_conn_id = conn_id;
      }
#endif
    } else {
      nfc_free_conn_cb(p_cb);
    }

    if (p_cback) (*p_cback)(conn_id, NFC_CONN_CREATE_CEVT, &evt_data);
  }
}

/*******************************************************************************
**
** Function         nfc_ncif_report_conn_close_evt
**
** Description      This function is called to report connection close event
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_report_conn_close_evt(uint8_t conn_id, tNFC_STATUS status) {
  tNFC_CONN evt_data;
  tNFC_CONN_CBACK* p_cback;
  tNFC_CONN_CB* p_cb;

  p_cb = nfc_find_conn_cb_by_conn_id(conn_id);
  if (p_cb) {
    p_cback = p_cb->p_cback;
    nfc_free_conn_cb(p_cb);
    evt_data.status = status;
    if (p_cback) (*p_cback)(conn_id, NFC_CONN_CLOSE_CEVT, &evt_data);
  }
}

/*******************************************************************************
**
** Function         nfc_ncif_proc_reset_rsp
**
** Description      This function is called to process reset
**                  response/notification
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_proc_reset_rsp(uint8_t* p, bool is_ntf) {
  uint8_t* temp = p, len;
  uint8_t* p_len = p - 1;
  uint8_t status = NCI_STATUS_FAILED;
  uint8_t wait_for_ntf = FALSE;

  status = *p_len > 0 ? *p++ : NCI_STATUS_FAILED;
  if (*p_len > 2 && is_ntf) {
#if (NXP_EXTNS == TRUE)
      if(nfcFL.nfccFL._NFCC_FORCE_NCI1_0_INIT) {
          if (status == 0x02) {
              DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf(
                      "CORE_RESET_NTF 2 reason Command Received status nfc_state : 0x%x : "
                      "0x%x",
                      status, nfc_cb.nfc_state);
              wait_for_ntf = false;
              status = NCI_STATUS_OK;
          } else if (status == 0x00) {
              DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf(
                      "CORE_RESET_NTF 2 reason Unrecoverable Error status nfc_state : 0x%x "
                      ": 0x%x",
                      status, nfc_cb.nfc_state);
              core_reset_init_num_buff = true;
              if(nfa_dm_is_active())
                  nfc_ncif_cmd_timeout();
          } else {
              DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("CORE_RESET_NTF 1 nfc_state :0x%x ", nfc_cb.nfc_state);
              DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("CORE_RESET_NTF 1 status :0x%x ", status);
              core_reset_init_num_buff = true;
              if(nfa_dm_is_active())
                  nfc_ncif_cmd_timeout();
          }
      }
      else {
          DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("reset notification nfc_state :0x%x ", nfc_cb.nfc_state);
          DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("reset notification!!:0x%x ", status);

          if(status == NCI2_0_RESET_TRIGGER_TYPE_CORE_RESET_CMD_RECEIVED ||
                  status == NCI2_0_RESET_TRIGGER_TYPE_POWERED_ON) {
              DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("CORE_RESET_NTF Received status nfc_state : 0x%x : 0x%x",
                      status ,nfc_cb.nfc_state);
              nfc_stop_timer(&nfc_cb.nci_wait_rsp_timer);
              p++;
              STREAM_TO_UINT8(nfc_cb.nci_version, p);
              LOG(ERROR) << StringPrintf(" CORE_RESET_NTF nci_version%x", nfc_cb.nci_version);
              status = NCI_STATUS_OK;
          } else {
              core_reset_init_num_buff = true;
              /* CORE_RESET_NTF received error case , trigger recovery*/
              DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("CORE_RESET_NTF Received status nfc_state : 0x%x : 0x%x",
                      status ,nfc_cb.nfc_state);
              if(nfa_dm_is_active())
                  nfc_ncif_cmd_timeout();
              status = NCI_STATUS_FAILED;
          }
      }
  } else {
      if(nfcFL.nfccFL._NFCC_FORCE_NCI1_0_INIT) {
          if ((*p_len) == NCI_CORE_RESET_RSP_LEN(NCI_VERSION_1_0)) {
              nfc_cb.nci_version = NCI_VERSION_1_0;
          }
          DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("CORE_RESET_RSP 2 nfc_state :0x%x ", nfc_cb.nfc_state);
          len = *(--temp);  // extract the no of params in reset response
          if (nfc_cb.flags & (NFC_FL_RESTARTING | NFC_FL_POWER_CYCLE_NFCC)) {
              nfc_reset_all_conn_cbs();
          }
          if (len == 0x01)  // If response length is 01 means NCI2.0
          {
              wait_for_ntf = true;
          }
      }
      else {

          DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("CORE_RESET_RSP len :0x%x ", *p_len);
          if ((*p_len) == NCI_CORE_RESET_RSP_LEN(NCI_VERSION_2_0)) {
              wait_for_ntf = true;
          }
          else if ((*p_len) == NCI_CORE_RESET_RSP_LEN(NCI_VERSION_1_0)) {
              nfc_cb.nci_version = NCI_VERSION_1_0;
          }

          DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("reset response nfc_state :0x%x ", nfc_cb.nfc_state);
          if (nfc_cb.flags & (NFC_FL_RESTARTING | NFC_FL_POWER_CYCLE_NFCC)) {
              nfc_reset_all_conn_cbs();
          }
      }
  }
#else
    LOG(ERROR) << StringPrintf("reset notification!!:0x%x ", status);
    /* clean up, if the state is OPEN
     * FW does not report reset ntf right now */
    if (status == NCI2_0_RESET_TRIGGER_TYPE_CORE_RESET_CMD_RECEIVED ||
        status == NCI2_0_RESET_TRIGGER_TYPE_POWERED_ON) {
      DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("CORE_RESET_NTF Received status nfc_state : 0x%x : 0x%x",
                       status, nfc_cb.nfc_state);
      nfc_stop_timer(&nfc_cb.nci_wait_rsp_timer);
      p++;
      STREAM_TO_UINT8(nfc_cb.nci_version, p);
      DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf(" CORE_RESET_NTF nci_version%x", nfc_cb.nci_version);
      status = NCI_STATUS_OK;
    } else {
      /* CORE_RESET_NTF received error case , trigger recovery*/
      DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("CORE_RESET_NTF Received status nfc_state : 0x%x : 0x%x",
                       status, nfc_cb.nfc_state);
      nfc_ncif_cmd_timeout();
      status = NCI_STATUS_FAILED;
    }
    if (nfc_cb.nfc_state == NFC_STATE_OPEN) {
      /*if any conn_cb is connected, close it.
        if any pending outgoing packets are dropped.*/
      nfc_reset_all_conn_cbs();
    }
  } else {
    DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("CORE_RESET_RSP len :0x%x ", *p_len);
    if ((*p_len) == NCI_CORE_RESET_RSP_LEN(NCI_VERSION_2_0)) {
      wait_for_ntf = TRUE;
    } else if ((*p_len) == NCI_CORE_RESET_RSP_LEN(NCI_VERSION_1_0)) {
      nfc_cb.nci_version = NCI_VERSION_1_0;
    }
  }

  if (nfc_cb.flags & (NFC_FL_RESTARTING | NFC_FL_POWER_CYCLE_NFCC)) {
    nfc_reset_all_conn_cbs();
  }
#endif

  if (status == NCI_STATUS_OK) {
#if (NXP_EXTNS == TRUE)
      if(nfcFL.nfccFL._NFCC_FORCE_NCI1_0_INIT) {
          if (!wait_for_ntf) {
              DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("Got CORE_RESET_NTF 2 sending CORE_INIT_CMD 1");
              nci_snd_core_init(NCI_VERSION_1_0);
          } else {
              DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("Waiting for CORE_RESET_NTF 2 reason CMD received");
              /* start NFC command-timeout timer */
              nfc_start_timer(&nfc_cb.nci_wait_rsp_timer,
                      (uint16_t)(NFC_TTYPE_NCI_WAIT_RSP),
                      nfc_cb.nci_wait_rsp_tout);
          }
      }
      else {
#endif
          if(wait_for_ntf == TRUE) {
            /* reset version reported by NFCC is NCI2.0 , start a timer for 2000ms to
             * wait for NTF*/
            nfc_start_timer (&nfc_cb.nci_wait_rsp_timer,
                             (uint16_t)(NFC_TTYPE_NCI_WAIT_RSP),
                             nfc_cb.nci_wait_rsp_tout);
          } else {
            /*MW tries to reInitialize, so clear nfa_dm_cb.params before
             *proceeding, to avoid having previously initialized values if any*/
            memset(&nfa_dm_cb.params, 0x00, sizeof(tNFA_DM_PARAMS));
            if (nfc_cb.nci_version == NCI_VERSION_1_0)
              nci_snd_core_init(NCI_VERSION_1_0);
            else
              nci_snd_core_init(NCI_VERSION_2_0);
          }
#if (NXP_EXTNS == TRUE)
      }
#endif
  } else {
#if (NXP_EXTNS == TRUE)
    if(!core_reset_init_num_buff)
    {
        if(!nfa_dm_is_active())
        {
            status = NCI_STATUS_NOT_INITIALIZED;
            NFC_Disable();
        }
#endif
        LOG(ERROR) << StringPrintf("Failed to reset NFCC");
        nfc_enabled (status, nullptr);
#if (NXP_EXTNS == TRUE)
    }
#endif
  }
}

#if (NXP_EXTNS == TRUE)
uint8_t nfc_hal_nfcc_reset(void) {
  phNxpNci_Extn_Cmd_t inData;
  phNxpNci_Extn_Resp_t outData;
  /*NCI_RESET_CMD*/
  uint8_t cmd_reset_nci[] = {0x20, 0x00, 0x01, 0x00};
  uint8_t core_status = NCI_STATUS_FAILED;
  uint8_t retry_count = 0;
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("Inside nfc_hal_nfcc_reset");

  memset(&inData, 0x00, sizeof(phNxpNci_Extn_Cmd_t));
  memset(&outData, 0x00, sizeof(phNxpNci_Extn_Resp_t));
  inData.cmd_len = sizeof(cmd_reset_nci);
  memcpy(inData.p_cmd, cmd_reset_nci,
         sizeof(cmd_reset_nci));
  do {
    core_status =
        nfc_cb.p_hal->nciTransceive(&inData,&outData);

    retry_count++;
  } while (NCI_STATUS_OK != core_status &&
           retry_count < (NFC_NFCC_INIT_MAX_RETRY + 1));
  return core_status;
}
uint8_t nfc_hal_nfcc_init() {
  phNxpNci_Extn_Cmd_t inData;
  phNxpNci_Extn_Resp_t outData;
  /*NCI_INIT_CMD*/
  uint8_t cmd_init_nci[] = {0x20, 0x01, 0x00};
  uint8_t cmd_init_nci2_0[] = {0x20,0x01,0x02,0x00,0x00};
  uint8_t init_status = NCI_STATUS_FAILED;
  uint8_t retry_count = 0;
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("Inside nfc_hal_nfcc_init");

  memset(&inData, 0x00, sizeof(phNxpNci_Extn_Cmd_t));
  memset(&outData, 0x00, sizeof(phNxpNci_Extn_Resp_t));
  if(nfc_cb.nci_version == NCI_VERSION_1_0)
  {
     inData.cmd_len = sizeof(cmd_init_nci);
     memcpy(inData.p_cmd, cmd_init_nci, sizeof(cmd_init_nci));
  }
  else
  {
     inData.cmd_len = sizeof(cmd_init_nci2_0);
     memcpy(inData.p_cmd, cmd_init_nci2_0, sizeof(cmd_init_nci2_0));
  }
  do {
    init_status =
        nfc_cb.p_hal->nciTransceive(&inData,&outData);
    retry_count++;
  } while (NCI_STATUS_OK != init_status &&
           retry_count < (NFC_NFCC_INIT_MAX_RETRY + 1));
  return init_status;
}
#endif
/*******************************************************************************
**
** Function         nfc_ncif_proc_init_rsp
**
** Description      This function is called to process init response
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_proc_init_rsp(NFC_HDR* p_msg) {
  uint8_t *p, status;
  tNFC_CONN_CB* p_cb = &nfc_cb.conn_cb[NFC_RF_CONN_ID];
#if (NXP_EXTNS == TRUE)
  static uint8_t retry_cnt = 0;
  uint16_t fw_status, fw_mw_ver_status = NCI_STATUS_FAILED;
  tNFC_FWUpdate_Info_t fw_update_inf;
  memset(&fw_update_inf, 0x00, sizeof(tNFC_FWUpdate_Info_t));
#endif
  p = (uint8_t*)(p_msg + 1) + p_msg->offset;

  /* handle init params in nfc_enabled */
  status = *(p + NCI_MSG_HDR_SIZE);
#if (NXP_EXTNS == TRUE)
  if(nfc_cb.nci_version != NCI_VERSION_2_0)
  {
      fw_status = nfc_ncif_store_FWVersion(p);
  }
  else
  fw_status = NCI_STATUS_OK;

  if((nfcFL.chipType != pn547C2) && nfcFL.nfcMwFL._NFC_NXP_AID_MAX_SIZE_DYN) {
      nfc_ncif_set_MaxRoutingTableSize(p);
  }

#endif

  /* TODO To be removed after 553 bringup */
  fw_mw_ver_status = NCI_STATUS_OK;
  if (status == NCI_STATUS_OK
#if (NXP_EXTNS == TRUE)
      && fw_status == NCI_STATUS_OK && NCI_STATUS_OK == fw_mw_ver_status
#endif
      ) {
#if (NXP_EXTNS == TRUE)
    retry_cnt = 0;
#endif
    if(nfc_cb.nci_version == NCI_VERSION_UNKNOWN) {
      nci_snd_core_reset(NCI_RESET_TYPE_RESET_CFG);
    } else {
      p_cb->id = NFC_RF_CONN_ID;
      // check scbr bit as per NCI 2.0 spec
      nfc_cb.isScbrSupported = p[5] & NCI_SCBR_MASK;
      DLOG_IF(INFO, nfc_debug_enabled)
          << StringPrintf("scbr support: 0x%x", nfc_cb.isScbrSupported);
      p_cb->act_protocol = NCI_PROTOCOL_UNKNOWN;

      nfc_set_state(NFC_STATE_W4_POST_INIT_CPLT);

      nfc_cb.p_nci_init_rsp = p_msg;
      nfc_cb.p_hal->core_initialized(p_msg->len, p);
      check_nfcee_session_and_reset();
    }
  }
#if (NXP_EXTNS == TRUE)
  /*Recovery is added in case of corrupted init rsp is received
   * eg: 400119 - only 3 bytes are received. In this case since the
   * rsp buffer[3] is status and the value is 0x00 because of memset for the rsp
   * buffer
   * was done before copying the init response.
   * Hence FW version check is added to do check the proper status and go for a
   * recovery*/
  else if (retry_cnt++ < NFC_NFCC_INIT_MAX_RETRY &&
           fw_status != NCI_STATUS_OK && NCI_STATUS_OK == fw_mw_ver_status) {
    GKI_send_event(NFC_TASK, NFC_TASK_EVT_TRANSPORT_READY);
    GKI_freebuf(p_msg);
  }
#endif
  else {
#if (NXP_EXTNS == TRUE)
    status = NCI_STATUS_FAILED;
    retry_cnt = 0;
#endif
    if(nfc_cb.nci_version == NCI_VERSION_UNKNOWN) {
      nfc_cb.nci_version = NCI_VERSION_1_0;
      nci_snd_core_reset(NCI_RESET_TYPE_RESET_CFG);
    } else {
      nfc_enabled(status, nullptr);
      GKI_freebuf(p_msg);
    }
  }
}

#if (NXP_EXTNS == TRUE)
/*******************************************************************************
**
** Function         nfc_ncif_store_FWVersion
**
** Description      This function is called to fill the structure with FW
*Version
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS nfc_ncif_store_FWVersion(uint8_t* p_buf) {
  uint8_t status = NFC_STATUS_OK;
  int len = p_buf[2] + 2; /*include 2 byte header*/
  memset(&nfc_fw_version, 0, sizeof(nfc_fw_version));
  nfc_fw_version.rom_code_version = p_buf[len - 2];
  nfc_fw_version.major_version = p_buf[len - 1];
  nfc_fw_version.minor_version = p_buf[len];
  if (nfc_fw_version.rom_code_version == 0 ||
      nfc_fw_version.major_version == 0) {
    status = NFC_STATUS_FAILED;
  }
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("FW Version: %x.%x.%x", nfc_fw_version.rom_code_version,
                   nfc_fw_version.major_version, nfc_fw_version.minor_version);
  return status;
}
/*******************************************************************************
**
** Function         nfc_ncif_set_MaxRoutingTableSize
**
** Description      This function is called to get the Max Routing Table Size
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS nfc_ncif_set_MaxRoutingTableSize(uint8_t* p_buf) {
  DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf("nfc_ncif_set_MaxRoutingTableSize Enter");

  if (!((nfcFL.chipType != pn547C2) &&
        nfcFL.nfcMwFL._NFC_NXP_AID_MAX_SIZE_DYN)) {
    DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf(
        "chipType : pn547C2 or NFC_NXP_AID_MAX_SIZE_DYN not available.. Returning");
    return NFC_STATUS_FAILED;
  }
  uint8_t status = NFC_STATUS_OK;
  if (nfc_cb.nci_version == NCI_VERSION_2_0) {
    maxRoutingTableSize = p_buf[9];
    maxRoutingTableSize = maxRoutingTableSize | p_buf[10] << 8;
  } else {
    int len = p_buf[2] + 2; /*include 2 byte header*/
    maxRoutingTableSize = p_buf[len - 8];
    maxRoutingTableSize = maxRoutingTableSize << 8 | p_buf[len - 9];
  }
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("MAX Routing size : %d", maxRoutingTableSize);
  if (maxRoutingTableSize == 0) {
    status = NFC_STATUS_FAILED;
  }
  return status;
}

/*******************************************************************************
**
** Function         NFC_getMaxRoutingTableSize
**
** Description      This function is called to get the Max routing Table size
**
** Returns          uint16_t
**
*******************************************************************************/
uint16_t nfc_ncif_getMaxRoutingTableSize() {
    if(!((nfcFL.chipType != pn547C2) && nfcFL.nfcMwFL._NFC_NXP_AID_MAX_SIZE_DYN)) {
        DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("getMaxRoutingTableSize : chipType : pn547C2 or "
                "NFC_NXP_AID_MAX_SIZE_DYN not available.. Returning");
        return 0;

    }
    return maxRoutingTableSize;
}

/*******************************************************************************
**
** Function         nfc_ncif_storeScreenState
**
** Description      This function is called to store the last screen state
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_storeScreenState(uint8_t state) {
  /* store the screen state */
  gScreenState = state;
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("nfc_ncif_storeScreenState gScreenState : %d", gScreenState);
}
#endif
/*******************************************************************************
**
** Function         nfc_ncif_proc_get_config_rsp
**
** Description      This function is called to process get config response
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_proc_get_config_rsp(NFC_HDR* p_evt) {
  uint8_t* p;
  tNFC_RESPONSE_CBACK* p_cback = nfc_cb.p_resp_cback;
  tNFC_RESPONSE evt_data;

  p_evt->offset += NCI_MSG_HDR_SIZE;
  p_evt->len -= NCI_MSG_HDR_SIZE;
  if (p_cback) {
    p = (uint8_t*)(p_evt + 1) + p_evt->offset;
    evt_data.get_config.status = *p++;
    evt_data.get_config.tlv_size = p_evt->len;
    evt_data.get_config.p_param_tlvs = p;
    (*p_cback)(NFC_GET_CONFIG_REVT, &evt_data);
  }
}

/*******************************************************************************
**
** Function         nfc_ncif_proc_t3t_polling_ntf
**
** Description      Handle NCI_MSG_RF_T3T_POLLING NTF
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_proc_t3t_polling_ntf(uint8_t* p, uint16_t plen) {
  uint8_t status;
  uint8_t num_responses;

  if (plen < NFC_TL_SIZE) {
    return;
  }

  /* Pass result to RW_T3T for processing */
  STREAM_TO_UINT8(status, p);
  STREAM_TO_UINT8(num_responses, p);
  plen -= NFC_TL_SIZE;
  rw_t3t_handle_nci_poll_ntf(status, num_responses, (uint8_t)plen, p);
}

/*******************************************************************************
**
** Function         nfc_data_event
**
** Description      Report Data event on the given connection control block
**
** Returns          void
**
*******************************************************************************/
void nfc_data_event(tNFC_CONN_CB* p_cb) {
  NFC_HDR* p_evt;
  tNFC_DATA_CEVT data_cevt;

  uint8_t* p;

  if (p_cb->p_cback) {
    while ((p_evt = (NFC_HDR*)GKI_getfirst(&p_cb->rx_q)) != nullptr) {
      if (p_evt->layer_specific & NFC_RAS_FRAGMENTED) {
        /* Not the last fragment */
        if (!(p_evt->layer_specific & NFC_RAS_TOO_BIG)) {
          /* buffer can hold more */
          if ((p_cb->conn_id != NFC_RF_CONN_ID) || (nfc_cb.reassembly)) {
            /* If not rf connection or If rf connection and reassembly
             * requested,
             * try to Reassemble next packet */
            break;
          }
        }
      }

      p_evt = (NFC_HDR*)GKI_dequeue(&p_cb->rx_q);

      if (p_evt == nullptr) {
        break;
      }
      /* report data event */
      p_evt->offset += NCI_MSG_HDR_SIZE;
      p_evt->len -= NCI_MSG_HDR_SIZE;

      if (p_evt->layer_specific)
        data_cevt.status = NFC_STATUS_CONTINUE;
      else {
        nfc_cb.reassembly = true;
        data_cevt.status = NFC_STATUS_OK;
      }

      data_cevt.p_data = p_evt;
      /* adjust payload, if needed */
      if (p_cb->conn_id == NFC_RF_CONN_ID) {
        /* if NCI_PROTOCOL_T1T/NCI_PROTOCOL_T2T/NCI_PROTOCOL_T3T, the status
         * byte needs to be removed
         */
        if ((p_cb->act_protocol >= NCI_PROTOCOL_T1T) &&
            (p_cb->act_protocol <= NCI_PROTOCOL_T3T)) {
#if (NXP_EXTNS == TRUE)
          if ((p_cb->act_protocol == NCI_PROTOCOL_T2T) &&
              (p_cb->act_interface == NCI_INTERFACE_FIRST_VS) &&
              (p_cb->sel_res == 0x01)) {
            LOG(ERROR) << StringPrintf("Skylander tag detected dont handle this");
          } else
#endif
          {
              p_evt->len--;
              p = (uint8_t*)(p_evt + 1);
              data_cevt.status = *(p + p_evt->offset + p_evt->len);
              if((NFC_GetNCIVersion() == NCI_VERSION_2_0) &&
                  (p_cb->act_protocol == NCI_PROTOCOL_T2T) &&
                  (p_cb->act_interface == NCI_INTERFACE_FRAME)) {
             if((data_cevt.status != NFC_STATUS_OK) &&
                     ((data_cevt.status >= T2T_STATUS_OK_1_BIT)&&(data_cevt.status <= T2T_STATUS_OK_7_BIT)))
             {
                 DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("nfc_data_event: T2T tag data xchange");
                 data_cevt.status = NFC_STATUS_OK;
             }
              }
          }
        }
#if (NXP_EXTNS == TRUE)
        if ( p_cb->act_protocol == NCI_PROTOCOL_T5T) {
#else
        if ((NFC_GetNCIVersion() == NCI_VERSION_2_0) &&
            (p_cb->act_protocol == NCI_PROTOCOL_T5T)) {
#endif
          p_evt->len--;
          p = (uint8_t*)(p_evt + 1);
          data_cevt.status = *(p + p_evt->offset + p_evt->len);
        }
      }
      tNFC_CONN nfc_conn;
      nfc_conn.data = data_cevt;
      (*p_cb->p_cback)(p_cb->conn_id, NFC_DATA_CEVT, &nfc_conn);
      p_evt = nullptr;
    }
  }
}

/*******************************************************************************
**
** Function         nfc_ncif_proc_data
**
** Description      Find the connection control block associated with the data
**                  packet. Assemble the data packet, if needed.
**                  Report the Data event.
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_proc_data(NFC_HDR* p_msg) {
  uint8_t* pp, cid;
  tNFC_CONN_CB* p_cb;
  uint8_t pbf;
  NFC_HDR* p_last;
  uint8_t* ps, *pd;
  uint16_t size;
  NFC_HDR* p_max = nullptr;
  uint16_t len;

  pp = (uint8_t*)(p_msg + 1) + p_msg->offset;
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("nfc_ncif_proc_data 0x%02x%02x%02x", pp[0], pp[1], pp[2]);
  NCI_DATA_PRS_HDR(pp, pbf, cid, len);
  p_cb = nfc_find_conn_cb_by_conn_id(cid);
#if (NXP_EXTNS == TRUE)
  if ((nfcFL.eseFL._ESE_DUAL_MODE_PRIO_SCHEME ==
          nfcFL.eseFL._ESE_WIRED_MODE_RESUME) &&
          ((p_cb) && (cid == NFC_RF_CONN_ID) && (nfc_cb.bBlockWiredMode))) {
    nfc_ncif_resume_dwp_wired_mode();
  }
#endif
  if (p_cb && (p_msg->len >= NCI_DATA_HDR_SIZE)) {
    DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("nfc_ncif_proc_data len:%d", len);

    len = p_msg->len - NCI_MSG_HDR_SIZE;
    p_msg->layer_specific = 0;
    if (pbf) {
      DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf(
          "nfc_ncif_proc_data requesting reassembly for chained data");
      NFC_SetReassemblyFlag(true);
      p_msg->layer_specific = NFC_RAS_FRAGMENTED;
    }
    p_last = (NFC_HDR*)GKI_getlast(&p_cb->rx_q);
    if (p_last && (p_last->layer_specific & NFC_RAS_FRAGMENTED)) {
      /* last data buffer is not last fragment, append this new packet to the
       * last */
      size = GKI_get_buf_size(p_last);
      if (size < (NFC_HDR_SIZE + p_last->len + p_last->offset + len)) {
        /* the current size of p_last is not big enough to hold the new
         * fragment, p_msg */
        if (size != GKI_MAX_BUF_SIZE) {
          /* try the biggest GKI pool */
          p_max = (NFC_HDR*)GKI_getpoolbuf(GKI_MAX_BUF_SIZE_POOL_ID);
          if (p_max) {
            /* copy the content of last buffer to the new buffer */
            memcpy(p_max, p_last, NFC_HDR_SIZE);
            pd = (uint8_t*)(p_max + 1) + p_max->offset;
            ps = (uint8_t*)(p_last + 1) + p_last->offset;
            memcpy(pd, ps, p_last->len);

            /* place the new buffer in the queue instead */
            GKI_remove_from_queue(&p_cb->rx_q, p_last);
            GKI_freebuf(p_last);
            GKI_enqueue(&p_cb->rx_q, p_max);
            p_last = p_max;
          }
        }
        if (p_max == nullptr) {
          /* Biggest GKI Pool not available (or)
           * Biggest available GKI Pool is not big enough to hold the new
           * fragment, p_msg */
          p_last->layer_specific |= NFC_RAS_TOO_BIG;
        }
      }

      ps = (uint8_t*)(p_msg + 1) + p_msg->offset + NCI_MSG_HDR_SIZE;

      if (!(p_last->layer_specific & NFC_RAS_TOO_BIG)) {
        pd = (uint8_t*)(p_last + 1) + p_last->offset + p_last->len;
        memcpy(pd, ps, len);
        p_last->len += len;
        /* do not need to update pbf and len in NCI header.
         * They are stripped off at NFC_DATA_CEVT and len may exceed 255 */
        DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("nfc_ncif_proc_data len:%d", p_last->len);
        p_last->layer_specific = p_msg->layer_specific;
        GKI_freebuf(p_msg);
        nfc_data_event(p_cb);
      } else {
        /* Not enough memory to add new buffer
         * Send data already in queue first with status Continue */
        nfc_data_event(p_cb);
        /* now enqueue the new buffer to the rx queue */
        GKI_enqueue(&p_cb->rx_q, p_msg);
      }
    } else {
      /* if this is the first fragment on RF link */
      if ((p_msg->layer_specific & NFC_RAS_FRAGMENTED) &&
          (p_cb->conn_id == NFC_RF_CONN_ID) && (p_cb->p_cback)) {
        /* Indicate upper layer that local device started receiving data */
        (*p_cb->p_cback)(p_cb->conn_id, NFC_DATA_START_CEVT, nullptr);
      }
      /* enqueue the new buffer to the rx queue */
      GKI_enqueue(&p_cb->rx_q, p_msg);
      nfc_data_event(p_cb);
    }
    return;
  }
  GKI_freebuf(p_msg);
}
/*******************************************************************************
**
** Function         nfc_modeset_ntf_timeout
**
** Description      This function is invoked on mode set ntf timeout
**
** Returns          void
**
*******************************************************************************/
void nfc_modeset_ntf_timeout()
{
  LOG(ERROR) << StringPrintf("nfc_ncif_modeSet_Ntf_timeout");
  tNFC_RESPONSE nfc_response;
  nfc_response.mode_set.status = NCI_STATUS_FAILED;
  nfc_response.mode_set.nfcee_id = *nfc_cb.last_cmd;
  nfc_response.mode_set.mode = NCI_NFCEE_MD_DEACTIVATE;

  tNFC_RESPONSE_CBACK  *p_cback = nfc_cb.p_resp_cback;
  tNFC_RESPONSE_EVT event    = NFC_NFCEE_MODE_SET_REVT;



    if (p_cback)
        (*p_cback) (event, &nfc_response);
}

#if (NXP_EXTNS == TRUE)
/*******************************************************************************
**
** Function         nfc_ncif_credit_ntf_timeout
**
** Description      Handle a credit ntf  timeout
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_credit_ntf_timeout() {
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("nfc_ncif_credit_ntf_timeout : Enter");

#if (NXP_EXTNS == TRUE)
  tNFC_CONN_CB* p_cb;
#endif
  if (get_i2c_fragmentation_enabled() == I2C_FRAGMENATATION_ENABLED) {
    nfc_cb.i2c_data_t.data_ntf_timeout = 1;
  } else {
    // No need to wait for response since credit ntf timeout calls recovery
    // Stop the response timer
    nfa_sys_stop_timer(&nfa_hci_cb.timer);

    // send timeout event to upper layer
    nfa_hci_rsp_timeout();

    // will be used to reset credit buffer after recovery
    core_reset_init_num_buff = true;
#if (NXP_EXTNS == TRUE)
    p_cb = nfc_find_conn_cb_by_conn_id(NFC_NFCEE_CONN_ID);
    if (p_cb && (p_cb->num_buff != NFC_CONN_NO_FC) && (p_cb->num_buff == 0))
      p_cb->num_buff++;

    p_cb = nfc_find_conn_cb_by_conn_id(NFC_RF_CONN_ID);
    if (p_cb && (p_cb->num_buff != NFC_CONN_NO_FC) && (p_cb->num_buff == 0))
      p_cb->num_buff++;
#endif
  }

  if (get_i2c_fragmentation_enabled() == I2C_FRAGMENATATION_ENABLED) {
    // nfc_cb.nfc_state = NFC_STATE_RECOVERY;
    DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("nfc_ncif_credit_ntf_timeout :decrementing window");
    // TODO: Write logic for VEN_RESET.
    nfc_cb.p_hal->power_cycle();
    // Remove the pending cmds from the cmd queue. send any pending rsp/cback to
    // jni
    nfc_ncif_empty_cmd_queue();
    // Cancel any ongoing data transfer.
    /**
     * send core reset - keep config
     * send core init
     * send discovery
     * */

    // Update the cmd window, since rsp has not came.
    // nfc_ncif_update_window ();
    nfc_ncif_empty_data_queue();
    if (nfc_cb.i2c_data_t.data_ntf_timeout) {
      nfc_cb.i2c_data_t.data_ntf_timeout = 0;
    }
  }
  LOG(ERROR) << StringPrintf("cmd timeout sending core reset!!!");
  nfc_ncif_cmd_timeout();
  // nci_snd_core_reset(0x00);
}

/*******************************************************************************
**
** Function         nfc_ncif_process_proprietary_rsp
**
** Description      Process the response to avoid collision
**                  while rawVsCbflag is set
**
** Returns          true if proprietary response else false
**
*******************************************************************************/
bool nfc_ncif_proc_proprietary_rsp(uint8_t mt, uint8_t gid, uint8_t oid) {
  bool stat = FALSE;
#if (NXP_EXTNS == TRUE)
  bool isRstRsp = FALSE;
#endif
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: mt=%u, gid=%u, oid=%u", __func__, mt, gid, oid);

  switch (mt) {
    case NCI_MT_DATA:
      /* check for Data Response */
      if (gid != 0x03 && oid != 0x00) stat = TRUE;
      break;

#if (NXP_EXTNS == TRUE)
    case NCI_MT_RSP:
      switch (gid) {
        case NCI_GID_CORE:
          stat = TRUE;
          if (oid == 0x00) isRstRsp = TRUE;
          break;
        default:
          stat = TRUE;
          break;
      }
      break;
#endif

    case NCI_MT_NTF:
      switch (gid) {
        case NCI_GID_CORE:
          /* check for CORE_RESET_NTF or CORE_CONN_CREDITS_NTF */
#if (NXP_EXTNS == TRUE)
          if (oid != 0x06) stat = TRUE;
          if (oid == 0x00) nfc_cb.rawVsCbflag = false;
#else
          if (oid != 0x00 && oid != 0x06) stat = TRUE;
#endif
          break;
        case NCI_GID_RF_MANAGE:
          /* check for CORE_CONN_CREDITS_NTF or NFA_EE_ACTION_NTF or
           * NFA_EE_DISCOVERY_REQ_NTF */
          if (oid != 0x06 && oid != 0x09 && oid != 0x0A) stat = TRUE;
          break;
        case NCI_GID_EE_MANAGE:
          if (oid != 0x00
#if (NXP_EXTNS == TRUE)
          && oid != 0x01
#endif
           ) stat = TRUE;
          break;
        default:
          stat = TRUE;
          break;
      }
      break;

    default:
      stat = TRUE;
      break;
  }
#if (NXP_EXTNS == TRUE)
  if (stat && !isRstRsp) {
    nfc_cb.rawVsCbflag = false;
  }
#endif
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf(
      "%s: exit status=%u rawVsCbflag=%u", __func__, stat, nfc_cb.rawVsCbflag);
  return stat;
}

#endif

#if (NXP_EXTNS == TRUE)
void disc_deact_ntf_timeout_handler(tNFC_RESPONSE_EVT event) {
    if(!(nfcFL.nfcNxpEse && nfcFL.eseFL._ESE_ETSI_READER_ENABLE)) {
        DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("disc_deact_ntf_timeout_handler : nfcNxpEse or "
                "ETSI_READER_ENABLE not available. Returning..");
        return;
    }
    tNFC_RESPONSE evt_data;
    etsi_reader_in_progress = false;
    if (nfc_cb.p_resp_cback) {
        evt_data.status = (tNFC_STATUS)NFC_STATUS_HW_TIMEOUT;
        (*nfc_cb.p_resp_cback)(event, &evt_data);
    }
}
#endif

#if (NXP_EXTNS == TRUE)
/*******************************************************************************
**
** Function:        nfc_ncif_reset_nfcc
**
** Description:     Reset the NFCC
**
** Returns:         NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFC_STATUS nfc_ncif_reset_nfcc() {
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s : Enter", __func__);
  if(!nfcFL.nfccFL._NFC_NXP_STAT_DUAL_UICC_EXT_SWITCH) {
      DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("NFC_NXP_STAT_DUAL_UICC_EXT_SWITCH"
              " feature is not available!!");
      return (NFA_STATUS_FAILED);
  }

  phNxpNci_Extn_Cmd_t inData;
  phNxpNci_Extn_Resp_t outData;
  uint8_t status = NCI_STATUS_FAILED;
  int retry_count = 0;

  /*NCI_INIT_CMD*/
  static uint8_t cmd_init_nci[] = {0x20, 0x01, 0x00};
  /*NCI_RESET_CMD*/
  static uint8_t cmd_reset_nci[] = {0x20, 0x00, 0x01,
                                    0x00};  // keep configuration
  /*NXP_ACT_PROP_EXTN*/
  static uint8_t cmd_act_prop_extn[] = {0x2F, 0x02, 0x00};
  /*NXP_CORE_STANDBY*/
  static uint8_t cmd_core_standby[] = {0x2F, 0x00, 0x01, 0x01};

  /*Nfcc power cycle*/
  nfc_cb.p_hal->power_cycle();

  /*Transceive NCI_INIT_CMD*/
  memset(&inData, 0x00, sizeof(phNxpNci_Extn_Cmd_t));
  memset(&outData, 0x00, sizeof(phNxpNci_Extn_Resp_t));
  inData.cmd_len = sizeof(cmd_reset_nci);
  memcpy(inData.p_cmd, cmd_reset_nci,
         sizeof(cmd_reset_nci));

  do {
    status = nfc_cb.p_hal->nciTransceive(&inData,&outData);
    retry_count++;
  } while ((status != NCI_STATUS_OK) && (retry_count <= 3));
  if (status != NCI_STATUS_OK) goto TheEndReset;

  /*Transceive NCI_RESET_CMD*/
  retry_count = 0;
  memset(&inData, 0x00, sizeof(phNxpNci_Extn_Cmd_t));
  memset(&outData, 0x00, sizeof(phNxpNci_Extn_Resp_t));
  inData.cmd_len = sizeof(cmd_init_nci);
  memcpy(inData.p_cmd, cmd_init_nci, sizeof(cmd_init_nci));
  do {
    status = nfc_cb.p_hal->nciTransceive(&inData,&outData);
    retry_count++;
  } while ((status != NCI_STATUS_OK) && (retry_count <= 3));
  if (status != NCI_STATUS_OK) goto TheEndReset;

  /*Transceive NXP_ACT_PROP_EXTN*/
  retry_count = 0;
  memset(&inData, 0x00, sizeof(phNxpNci_Extn_Cmd_t));
  memset(&outData, 0x00, sizeof(phNxpNci_Extn_Resp_t));
  inData.cmd_len = sizeof(cmd_act_prop_extn);
  memcpy(inData.p_cmd, cmd_act_prop_extn,
         sizeof(cmd_act_prop_extn));

  do {
    status = nfc_cb.p_hal->nciTransceive(&inData,&outData);
    retry_count++;
  } while ((status != NCI_STATUS_OK) && (retry_count <= 3));
  if (status != NCI_STATUS_OK) goto TheEndReset;

  /*Transceive NXP_CORE_STANDBY*/
  retry_count = 0;
  memset(&inData, 0x00, sizeof(phNxpNci_Extn_Cmd_t));
  memset(&outData, 0x00, sizeof(phNxpNci_Extn_Resp_t));
  inData.cmd_len = sizeof(cmd_core_standby);
  memcpy(inData.p_cmd, cmd_core_standby,
         sizeof(cmd_core_standby));

  do {
    status = nfc_cb.p_hal->nciTransceive(&inData,&outData);
    retry_count++;
  } while ((status != NCI_STATUS_OK) && (retry_count <= 3));
TheEndReset:
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s : Exit", __func__);
  return status;
}
/*******************************************************************************
**
** Function         uicc_eeprom_get_config
**
** Description      get UICC configuration from EEPROM
**
** Returns          None
**
*******************************************************************************/
void uicc_eeprom_get_config(uint8_t* config_resp) {
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s : Enter", __func__);
  if(!nfcFL.nfccFL._NFC_NXP_STAT_DUAL_UICC_EXT_SWITCH) {
      DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("NFC_NXP_STAT_DUAL_UICC_EXT_SWITCH"
              " feature is not available!!");
      return;
  }
  uint8_t cmd_get_dualUicc_config[] = {0x20, 0x03, 0x03, 0x01, 0xA0, 0xEC};

  phNxpNci_Extn_Cmd_t inData;
  phNxpNci_Extn_Resp_t outData;
  int uicc_mode = 0;
  uint8_t config_status = NCI_STATUS_FAILED;
  uint8_t retry_count = 0;

  if (NfcConfig::hasKey(NAME_NXP_DUAL_UICC_ENABLE)) {
    uicc_mode = NfcConfig::getUnsigned(NAME_NXP_DUAL_UICC_ENABLE);
    DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("NXP_DUAL_UICC_ENABLE : 0x%02x", uicc_mode);
  } else {
    uicc_mode = 0x00;
    DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf(
        "NXP_DUAL_UICC_ENABLE not found; taking default value : 0x%02x", uicc_mode);
  }
  memset(&inData, 0x00, sizeof(phNxpNci_Extn_Cmd_t));
  memset(&outData, 0x00, sizeof(phNxpNci_Extn_Resp_t));;
  inData.cmd_len = sizeof(cmd_get_dualUicc_config);
  memcpy(inData.p_cmd, cmd_get_dualUicc_config,
         sizeof(cmd_get_dualUicc_config));
  do {
    config_status =
        nfc_cb.p_hal->nciTransceive(&inData,&outData);
    retry_count++;
  } while ((config_status != NCI_STATUS_OK) && (retry_count <= 3));
  if (config_status == NCI_STATUS_OK &&
      outData.rsp_len > 0) {
    memcpy(config_resp, outData.p_rsp, outData.rsp_len);
  } else {
    DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s :nciTransceive Failed", __func__);
  }
}

/*******************************************************************************
**
** Function         uicc_eeprom_set_config
**
** Description      set UICC configuration in EEPROM
**
** Returns         No Data
**
*******************************************************************************/
void uicc_eeprom_set_config(uint8_t* config_rsp) {
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s : Enter", __func__);
  if(!nfcFL.nfccFL._NFC_NXP_STAT_DUAL_UICC_EXT_SWITCH) {
      DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("NFC_NXP_STAT_DUAL_UICC_EXT_SWITCH"
              " feature is not available!!");
      return;
  }
  uint8_t cmd_set_dualUicc_config[] = {0x20, 0x02, 0x05, 0x01,
                                       0xA0, 0xEC, 0x01, 0x11};

  phNxpNci_Extn_Cmd_t inData;
  phNxpNci_Extn_Resp_t outData;
  int uicc_mode = 0;
  uint8_t config_status = NCI_STATUS_FAILED;
  uint8_t retry_count = 0;

  if (NfcConfig::hasKey(NAME_NXP_DUAL_UICC_ENABLE)) {
    uicc_mode = NfcConfig::getUnsigned(NAME_NXP_DUAL_UICC_ENABLE);
    DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("NXP_DUAL_UICC_ENABLE : 0x%02x", uicc_mode);
  } else {
    uicc_mode = 0x00;
    DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf(
        "NXP_DUAL_UICC_ENABLE not found; taking default value : 0x%02x", uicc_mode);
  }

  memset(&inData, 0x00, sizeof(phNxpNci_Extn_Cmd_t));
  memset(&outData, 0x00, sizeof(phNxpNci_Extn_Resp_t));

  if (uicc_mode == 0x00) {
    cmd_set_dualUicc_config[7] = 0x01;
  } else if (config_rsp != nullptr) {
    cmd_set_dualUicc_config[7] = config_rsp[8];
  }

  inData.cmd_len = sizeof(cmd_set_dualUicc_config);
  memcpy(inData.p_cmd, cmd_set_dualUicc_config,
         sizeof(cmd_set_dualUicc_config));
  do {
    config_status =
        nfc_cb.p_hal->nciTransceive(&inData,&outData);
    retry_count++;
  } while ((config_status != NCI_STATUS_OK) && (retry_count <= 3));
  if ((config_status != NCI_STATUS_OK) && (retry_count = 4)) {
    DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s :nciTransceive Failed", __func__);
  }
}

/*******************************************************************************
**
** Function         isRedundantEndOfApdu
**
** Description      Checks if END_OF_APDU is redundant
**
** Returns         true/false
**
*******************************************************************************/
bool isRedundantEndOfApdu(uint8_t* data, uint8_t len) {
  /*Ignore END_OF_APDU if powerLink NFCC_DECIDES is not sent*/
  uint8_t endOfApdu[2] = {0x99, 0x61};
  if ((nfc_cb.pwr_link_cmd.param != NFCC_DECIDES) &&
      (len == sizeof(endOfApdu)) &&
      (memcmp(data, endOfApdu, sizeof(endOfApdu)) == 0x00)) {
    return true;
  } else {
    return false;
  }
}
#endif
