/******************************************************************************
 *
 *  Copyright 2019-2020 NXP
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
 *
 *****************************************************************************/

/******************************************************************************
 *
 *  This file contains the action functions the NFA_SCR Module.
 *
 ******************************************************************************/

#include <android-base/stringprintf.h>
#include <base/logging.h>
#include <string.h>
#include "nfa_hci_defs.h"
#include "nfa_scr_int.h"
#include "nfa_ee_int.h"
#include "nfa_dm_int.h"
#include "nfa_nfcee_int.h"
using android::base::StringPrintf;

#define NFA_SCR_INVALID_EVT 0xFF

extern bool nfc_debug_enabled;

/******************************************************************************
**
** Function         nfa_scr_set_reader_mode
**
** Description      This API sets the mode of Secure Reader module and a
**                  callback to notify events to JNI.
**                  Note: if mode is false then p_cback must be nullptr
**
**Parameters        Reader mode, JNI callback to receive reader events
**
** Returns:         void
**
******************************************************************************/
void nfa_scr_set_reader_mode(bool mode, tNFA_SCR_CBACK* scr_cback) {
  bool status = false;

  if (mode == true) {
    nfa_scr_cb.scr_cback = scr_cback;         /* Register a JNI callback */
    status = nfa_scr_cb.scr_evt_cback(NFA_SCR_APP_START_REQ_EVT, NFA_STATUS_OK);
  } else {
    nfa_scr_cb.app_stop_req = true;
    status = nfa_scr_cb.scr_evt_cback(NFA_SCR_APP_STOP_REQ_EVT, NFA_STATUS_OK);
  }

  if(status == false) {
    nfa_scr_cb.app_stop_req = false;
    nfa_scr_notify_evt((uint8_t)NFA_SCR_SET_READER_MODE_EVT, NFA_STATUS_FAILED);
  }
}

/******************************************************************************
**
** Function         nfa_scr_notify_evt
**
** Description      This API shall be called by the SCR module to notify
**                  its state based events to the JNI
**
** Returns:         void
**
******************************************************************************/
void nfa_scr_notify_evt(uint8_t event, uint8_t status) {
  if (nfa_scr_cb.scr_cback != nullptr) {
    nfa_scr_cb.scr_cback(event, status);
  } else {
    DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: scr_cback is null", __func__);
  }
}


/*******************************************************************************
**
** Function       nfa_scr_proc_rdr_req_ntf
**
** Description    1. If the NTF(610A) has been received for the Reader over SE,
**                EE module shall call this function to perform the requested
**                operation.
**
** Parameters     A pointer to the tNFC_EE_DISCOVER_REQ_REVT struct variable
**
** Return         True if event is processed else false
**
*******************************************************************************/
bool nfa_scr_proc_rdr_req_ntf(tNFC_EE_DISCOVER_REQ_REVT* p_cbk) {
  uint8_t secure_reader_evt = NFA_SCR_INVALID_EVT;
  uint8_t xx, report_ntf = 0;
  tNFA_EE_ECB* p_cb = nullptr;
  bool is_scr_requested = false;

  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: Enter", __func__);
  if(nfa_scr_cb.scr_evt_cback == nullptr) {
    DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("SCR module isn't requested by APP.");
    return is_scr_requested;
  }

  for (xx = 0; xx < p_cbk->num_info; xx++) {
    p_cb = nfa_ee_find_ecb(p_cbk->info[xx].nfcee_id);
    if (!p_cb) {
      DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf(
          "Cannot find cb for NFCEE: 0x%x", p_cbk->info[xx].nfcee_id);
      p_cb = nfa_ee_find_ecb(NFA_EE_INVALID);
      if (p_cb) {
        p_cb->nfcee_id = p_cbk->info[xx].nfcee_id;
        p_cb->ecb_flags |= NFA_EE_ECB_FLAGS_ORDER;
      } else {
        LOG(ERROR) << StringPrintf("Cannot allocate cb for NFCEE: 0x%x",
                                 p_cbk->info[xx].nfcee_id);
        continue;
      }
    } else {
      report_ntf |= nfa_ee_ecb_to_mask(p_cb);
    }
    /* code to handle and store Reader type(A/B) requested for Reader over SE */
    if (p_cbk->info[xx].op == NFC_EE_DISC_OP_ADD) {
      if (p_cbk->info[xx].tech_n_mode == NFC_DISCOVERY_TYPE_POLL_A) {
        p_cb->pa_protocol = p_cbk->info[xx].protocol;
        secure_reader_evt = NFA_SCR_START_REQ_EVT;
      } else if (p_cbk->info[xx].tech_n_mode == NFC_DISCOVERY_TYPE_POLL_B) {
        p_cb->pb_protocol = p_cbk->info[xx].protocol;
        secure_reader_evt = NFA_SCR_START_REQ_EVT;
      }
    } else {
      if (p_cbk->info[xx].tech_n_mode == NFC_DISCOVERY_TYPE_POLL_A) {
        p_cb->pa_protocol = 0xFF;
        secure_reader_evt = NFA_SCR_STOP_REQ_EVT;
      } else if (p_cbk->info[xx].tech_n_mode == NFC_DISCOVERY_TYPE_POLL_B) {
        p_cb->pb_protocol = 0xFF;
        secure_reader_evt = NFA_SCR_STOP_REQ_EVT;
      }
    }
  }

  if (report_ntf != 0x00 && secure_reader_evt != NFA_SCR_INVALID_EVT) {
    /* Report to SCR state machine */
    is_scr_requested = nfa_scr_cb.scr_evt_cback(secure_reader_evt, NFA_STATUS_OK);
  }
  return is_scr_requested;
}


bool nfa_scr_is_req_evt(uint8_t event, uint8_t status) {

  bool is_requested = false;
  uint8_t evt = NFA_SCR_INVALID_EVT;

  switch(event) {
  /* event from nfa_dm_nfc_response_cback   */
  case NFA_DM_SET_CONFIG_EVT:                /* 0x02 */
    evt = NFA_SCR_CORE_SET_CONF_RSP_EVT;
    break;
  /* events from nfa_dm_conn_cback_event_notify() */
  case NFA_RF_DISCOVERY_STARTED_EVT:         /* 0x30 */
    evt = NFA_SCR_RF_DISCOVERY_RSP_EVT;
    break;
  case NFA_DM_RF_DEACTIVATE_RSP:             /* 0x07 */
    evt = NFA_SCR_RF_DEACTIVATE_RSP_EVT;
    break;
  /* events from nfa_dm_disc_discovery_cback() */
  case NFA_DM_RF_DEACTIVATE_NTF:             /* 0x08 */
    evt = NFA_SCR_RF_DEACTIVATE_NTF_EVT;
    break;
  case NFA_DM_RF_INTF_ACTIVATED_NTF:         /* 0x05 */
    evt = NFA_SCR_RF_INTF_ACTIVATED_NTF_EVT;
    break;

  case NFA_SCR_ESE_RECOVERY_START_EVT:       /* 0x0B */
    [[fallthrough]];
  case NFA_SCR_ESE_RECOVERY_COMPLETE_EVT:    /* 0x0C */
    [[fallthrough]];
  case NFA_SCR_MULTIPLE_TARGET_DETECTED_EVT: /* 0x0D */
    evt = event;
    break;
  }
  if(evt != NFA_SCR_INVALID_EVT) {
    is_requested = nfa_scr_cb.scr_evt_cback(evt , status);
  }
  return is_requested;
}
