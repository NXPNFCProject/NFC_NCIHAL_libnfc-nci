/******************************************************************************
 *
 *  Copyright 2019-2020, 2022 NXP
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
 *This is the main implementation file for the NFA Secure Reader(ETSI/POS).
 *
 *****************************************************************************/
#include <android-base/stringprintf.h>
#include <base/logging.h>
#include <string.h>
#include <vector>
#include "nci_defs.h"
#include "nfa_api.h"
#include "nfc_api.h"
#include "nfc_config.h"
#include "nfa_rw_api.h"
#include "nfa_ee_int.h"
#include "nfa_dm_int.h"
#include "nfa_scr_int.h"

using android::base::StringPrintf;

extern bool nfc_debug_enabled;

/******************************************************************************
 **  Constants
 *****************************************************************************/
#define NFA_SCR_CARD_REMOVE_TIMEOUT 1000   // 1 Second
#define NFA_SCR_RECOVERY_TIMEOUT 5 * 1000  // 5 Second
#define LPCD_BYTE_POS 12
#define LPCD_BIT_POS 7

#define IS_DISC_MAP_RSP_EXPECTED                           \
  ((nfa_scr_cb.state == NFA_SCR_STATE_START_IN_PROGRESS || \
    nfa_scr_cb.state == NFA_SCR_STATE_STOP_IN_PROGRESS) && \
   nfa_scr_cb.sub_state == NFA_SCR_SUBSTATE_WAIT_DISC_MAP_RSP)

#define IS_RECOVERY_STARTED       (nfa_scr_cb.error == NFA_SCR_ERROR_RECOVERY_STARTED)
#define IS_EVT_RECOVERY_COMPLETED (event == NFA_SCR_ESE_RECOVERY_COMPLETE_EVT)
#define IS_SCR_ERROR              (nfa_scr_cb.error != NFA_SCR_NO_ERROR)
#define IS_NO_SCR_ERROR           (nfa_scr_cb.error == NFA_SCR_NO_ERROR)
#define IS_APP_STOP_ERROR         (nfa_scr_cb.error == NFA_SCR_ERROR_APP_STOP_REQ)
#define IS_EVT_APP_STOP_REQUEST   (event == NFA_SCR_APP_STOP_REQ_EVT)
#define IS_RDR_REQUEST(event) ((event) == NFA_SCR_START_REQ_EVT || (event) == NFA_SCR_STOP_REQ_EVT)
#define IS_EMVCO_RESTART_REQ ((nfa_scr_cb.sub_state == NFA_SCR_SUBSTATE_WAIT_DEACTIVATE_RSP)\
        || (nfa_scr_cb.sub_state == NFA_SCR_SUBSTATE_WAIT_DEACTIVATE_NTF))
/******************************************************************************
 **  Global Variables
 *****************************************************************************/
tNFA_SCR_CB nfa_scr_cb;
static uint32_t ntf_timeout_cnt = 0x00;

/******************************************************************************
 **  Internal Functions
 *****************************************************************************/
void nfa_scr_discovermap_cb(tNFC_DISCOVER_EVT event, tNFC_DISCOVER *p_data);

/*** Static Functions ***/
static bool nfa_scr_handle_start_req(void);
static bool nfa_scr_handle_stop_req(void);
static void nfa_scr_send_prop_set_conf(bool set);
static vector<uint8_t> nfa_scr_get_prop_set_conf_cmd(bool set);
static bool nfa_scr_handle_get_conf_rsp(uint8_t status);
static void nfa_scr_send_prop_get_set_conf(bool rdr_start_req);
static void nfa_scr_send_prop_get_conf(void);
static bool nfa_scr_handle_deact_rsp_ntf(uint8_t status);
static bool nfa_scr_send_discovermap_cmd(uint8_t status);
static void nfa_scr_tag_op_timeout_handler(void);
static void nfa_scr_rm_card_timeout_handler(void);
static void nfa_scr_rdr_req_guard_timeout_handler(void);
static bool is_emvco_polling_restart_req(void);

static const tNFA_SYS_REG nfa_scr_sys_reg = {nullptr, nfa_scr_evt_hdlr,
                                             nfa_scr_sys_disable, nullptr};

/******************************************************************************
 **  NFA SCR callback to be called from other modules
 *****************************************************************************/
bool nfa_scr_cback(uint8_t event, uint8_t status);

/*******************************************************************************
**
** Function         nfa_scr_init
**
** Description      Initialize NFA Secure Reader Module
**
** Returns          None
**
*******************************************************************************/
void nfa_scr_init(void) {
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: enter",__func__);
  /* initialize control block */
  memset(&nfa_scr_cb, 0, sizeof(tNFA_SCR_CB));

  nfa_scr_cb.state = NFA_SCR_STATE_STOPPED;
  nfa_scr_cb.error = NFA_SCR_NO_ERROR;
  nfa_scr_cb.sub_state = NFA_SCR_SUBSTATE_INVALID;
  nfa_scr_cb.last_scr_req = NFA_SCR_UNKOWN_EVT;
  nfa_scr_cb.deact_ntf_timeout = NfcConfig::getUnsigned(NAME_NFA_DM_DISC_NTF_TIMEOUT, 0);
  nfa_scr_cb.tag_op_timeout = NfcConfig::getUnsigned(NAME_NXP_SWP_RD_TAG_OP_TIMEOUT, 20);
  nfa_scr_cb.poll_prof_sel_cfg = NfcConfig::getUnsigned(NAME_NFA_CONFIG_FORMAT, 1);
  nfa_scr_cb.rdr_req_guard_time = NfcConfig::getUnsigned(NAME_NXP_RDR_REQ_GUARD_TIME, 0);
  nfa_scr_cb.configure_lpcd = NfcConfig::getUnsigned(NAME_NXP_RDR_DISABLE_ENABLE_LPCD, 1);
  /* register message handler on NFA SYS */
  nfa_sys_register(NFA_ID_SCR, &nfa_scr_sys_reg);
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: exit",__func__);
}

/*******************************************************************************
**
** Function         nfa_scr_sys_disable
**
** Description      Clean up SCR sub-system
**
**
** Returns          void
**
*******************************************************************************/
void nfa_scr_sys_disable(void) {

  if(nfa_scr_cb.scr_evt_cback) {
    /* Stop timers */
    nfa_sys_stop_timer(&nfa_scr_cb.scr_tle);
    nfa_sys_stop_timer(&nfa_scr_cb.scr_rec_tle);
  }
  /* deregister message handler on NFA SYS */
  nfa_sys_deregister(NFA_ID_SCR);
  nfa_scr_finalize(false);
}
/*******************************************************************************
**
** Function       nfa_scr_is_proc_rdr_req
**
** Description    This function will be effective only if NAME_NXP_RDR_REQ_GUARD_TIME is non-zero
**                If 610A has been received & timer has not be started then
**                  - save the action and start the timer.
**                else
**                  - stop the timer and process the current event
**
** Return         True if event is processed/expected else false
**
*******************************************************************************/
bool nfa_scr_is_proc_rdr_req(uint8_t event) {
  bool is_proc_rdr_req = true;
  if (IS_RDR_REQUEST(event) && !nfa_scr_cb.restart_emvco_poll) {
    if(nfa_scr_cb.is_timer_started) {
      /* Stop guard timer & continue with normal seq */
      nfa_sys_stop_timer(&nfa_scr_cb.scr_guard_tle);
      nfa_scr_cb.is_timer_started = 0;
      nfa_scr_cb.last_scr_req = NFA_SCR_UNKOWN_EVT;
    } else {
      /* save the event & start the timer */
      nfa_scr_cb.last_scr_req = event;
      nfa_scr_cb.is_timer_started = 1;
      /* Start timer to post NFA_SCR_RDR_REQ_GUARD_TIMEOUT_EVT on expire in order to
       * process the last reader requested event */
      nfa_sys_start_timer(&nfa_scr_cb.scr_guard_tle, NFA_SCR_RDR_REQ_GUARD_TIMEOUT_EVT,
              nfa_scr_cb.rdr_req_guard_time);
      is_proc_rdr_req = false;
    }
  } else if (nfa_scr_cb.restart_emvco_poll && event == NFA_SCR_START_REQ_EVT) {
    nfa_scr_cb.restart_emvco_poll = 0;
  }
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: exit status:%s", __func__,
          is_proc_rdr_req ? "true": "false");
  return is_proc_rdr_req;
}

void set_smr_cback(tNFA_SCR_EVT_CBACK* nfa_smr_cback) {
  nfa_scr_cb.scr_evt_cback =
      nfa_smr_cback; /* Register a SCR event handler cback */
}
/*******************************************************************************
**
** Function       nfa_scr_cback
**
** Description    1. If the NTF(610A) has been received for Start/Stop Reader
**               over eSE then EE module shall call this with
**               NFA_SCR_START_REQ_EVT, NFA_SCR_STOP_REQ_EVT respectively.
**                2. If SCR module has been triggered and NFC_TASK has received rsp
**               this callback shall be invoked with respective events.
**
** Return         True if event is processed/expected else false
**
*******************************************************************************/
bool nfa_scr_cback(uint8_t event, uint8_t status) {
  bool stat = false;
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: enter event=%u, status=%u state=%u,"
          "substate=%u", __func__, event, status, nfa_scr_cb.state, nfa_scr_cb.sub_state);

  if(!nfa_scr_is_evt_allowed(event)) {
    return stat;
  }
  if(nfa_scr_cb.rdr_req_guard_time && !nfa_scr_is_proc_rdr_req(event)) {
    return true; /* Guard timer has been started */
  }

  switch (event) {
    case NFA_SCR_APP_START_REQ_EVT:
      stat = nfa_scr_proc_app_start_req();
      break;
    case NFA_SCR_START_REQ_EVT:
      stat = nfa_scr_handle_start_req();
      break;
    case NFA_SCR_STOP_REQ_EVT:
      if(!IS_SCR_START_SUCCESS) {
        stat = true; /* 610A for reader mode shall be only be processed by SCR module */
        break;
      }
      [[fallthrough]];
    case NFA_SCR_APP_STOP_REQ_EVT:
      stat = nfa_scr_handle_stop_req();
      break;
    case NFA_SCR_RF_DEACTIVATE_RSP_EVT:
      [[fallthrough]];
    case NFA_SCR_RF_DEACTIVATE_NTF_EVT:
      stat = nfa_scr_handle_deact_rsp_ntf(status);
      break;
    case NFA_SCR_CORE_GET_CONF_RSP_EVT:
      stat = nfa_scr_handle_get_conf_rsp(status);
      break;
    case NFA_SCR_CORE_SET_CONF_RSP_EVT:
      stat = nfa_scr_send_discovermap_cmd(status);
      break;
    case NFA_SCR_RF_DISCOVER_MAP_RSP_EVT:
      stat = nfa_scr_start_polling(status);
      break;
    case NFA_SCR_RF_DISCOVERY_RSP_EVT:
      stat = nfa_scr_rdr_processed(status);
      break;
    case NFA_SCR_RF_INTF_ACTIVATED_NTF_EVT:
      stat = nfa_scr_handle_act_ntf(status);
      break;
    case NFA_SCR_ESE_RECOVERY_START_EVT:
      stat = nfa_scr_error_handler(NFA_SCR_ERROR_RECOVERY_STARTED);
      break;
    case NFA_SCR_ESE_RECOVERY_COMPLETE_EVT:
      stat = nfa_scr_error_handler(NFA_SCR_ERROR_RECOVERY_COMPLETED);
      break;
    case NFA_SCR_MULTIPLE_TARGET_DETECTED_EVT:
      if(status == NXP_NFC_EMVCO_PCD_COLLISION_DETECTED) {
        /* Notify upper layer about STATUS_EMVCO_PCD_COLLISION */
        nfa_scr_notify_evt(NFA_SCR_MULTI_TARGET_DETECTED_EVT);
        stat = true;
      }
      break;
    default:
      break;
  }
  return stat;
}

/*******************************************************************************
**
** Function         nfa_scr_evt_hdlr
**
** Description      Processing all internal event for NFA SCR
**
** Returns          TRUE if p_msg needs to be deallocated
**
*******************************************************************************/
bool nfa_scr_evt_hdlr(NFC_HDR* p_msg) {
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("nfa_scr_evt_hdlr event:%s "
          "(0x%04x)", nfa_scr_get_event_name(p_msg->event).c_str(), p_msg->event);

  switch (p_msg->event) {
    case NFA_SCR_API_SET_READER_MODE_EVT: {
      tNFA_SCR_API_SET_READER_MODE* p_evt_data =
          (tNFA_SCR_API_SET_READER_MODE*)p_msg;

      if (p_evt_data->type == NFA_SCR_MPOS) {
        set_smr_cback(nfa_scr_cback);
      } else {
      }
      nfa_scr_set_reader_mode(p_evt_data->mode,
                              (tNFA_SCR_CBACK*)p_evt_data->scr_cback);
      break;
    }
    case NFA_SCR_TAG_OP_TIMEOUT_EVT: {
      nfa_scr_tag_op_timeout_handler();
      break;
    }
    case NFA_SCR_RM_CARD_TIMEOUT_EVT: {
      nfa_scr_rm_card_timeout_handler();
      break;
    }
    case NFA_SCR_ERROR_REC_TIMEOUT_EVT: {
      (void)nfa_scr_error_handler(NFA_SCR_ERROR_RECOVERY_TIMEOUT);
      break;
    }
    case NFA_SCR_RDR_REQ_GUARD_TIMEOUT_EVT: {
      nfa_scr_rdr_req_guard_timeout_handler();
      break;
    }
  }
  return true;
}

/*******************************************************************************
**
** Function         nfa_scr_get_event_name
**
** Description      This function returns the event code name.
**
** Returns          pointer to the name
**
*******************************************************************************/
std::string nfa_scr_get_event_name(uint16_t event) {
  switch (event) {
    case NFA_SCR_API_SET_READER_MODE_EVT:
      return "SET_READER_MODE";
    case NFA_SCR_RM_CARD_TIMEOUT_EVT:
      return "REMOVE_CARD_EVT";
    case NFA_SCR_ERROR_REC_TIMEOUT_EVT:
      return "ERROR_REC_TIMEOUT_EVT";
    case NFA_SCR_RDR_REQ_GUARD_TIMEOUT_EVT:
      return "RDR_REQ_GUARD_TIMEOUT_EVT";
    default:
      return "UNKNOWN";
  }
}


/*******************************************************************************
**
** Function         nfa_scr_reader_handle_timout_evt
**
** Description      This method shall be called to handle the timeout event.
**
** Parameter        void
**
** Returns          true
**
*******************************************************************************/
void nfa_scr_tag_op_timeout_handler () {
  DLOG_IF(ERROR, nfc_debug_enabled) << StringPrintf("%s:Enter", __func__);
  nfa_scr_notify_evt(NFA_SCR_TIMEOUT_EVT);
}

/*******************************************************************************
**
** Function         nfa_scr_reader_handle_timout_evt
**
** Description      This method shall be called to handle the timeout event.
**
** Parameter        void
**
** Returns          void
**
*******************************************************************************/
void nfa_scr_rm_card_timeout_handler() {

  nfa_scr_notify_evt(NFA_SCR_REMOVE_CARD_EVT);

  if ((nfa_scr_cb.deact_ntf_timeout != 0x00)) {
    ntf_timeout_cnt++;
    if (ntf_timeout_cnt == nfa_scr_cb.deact_ntf_timeout) {
      DLOG_IF(ERROR, nfc_debug_enabled) << StringPrintf("%s:DISC_NTF_TIMEOUT", __func__);
      abort();
    }
  }
  if (IS_SCR_STOP_IN_PROGRESS) {
    nfa_sys_start_timer(&nfa_scr_cb.scr_tle, NFA_SCR_RM_CARD_TIMEOUT_EVT,
            NFA_SCR_CARD_REMOVE_TIMEOUT);
  }
}
/*******************************************************************************
**
** Function         nfa_scr_rdr_req_guard_timeout_handler
**
** Description      This method shall be called once the Reader Req guard timer
**                  is expired.
**
** Returns          void
**
*******************************************************************************/
void nfa_scr_rdr_req_guard_timeout_handler(void) {
  if (nfa_scr_cb.scr_evt_cback != nullptr) {
    nfa_scr_cb.scr_evt_cback(nfa_scr_cb.last_scr_req , NFA_STATUS_OK);
  }
  nfa_scr_cb.last_scr_req = NFA_SCR_UNKOWN_EVT;
}

/*******************************************************************************
**
** Function         nfa_scr_get_error_name
**
** Description      This function returns the Error code name.
**
** Returns          pointer to the name
**
*******************************************************************************/
static std::string nfa_scr_get_error_name(tNFA_SCR_ERROR error) {

  switch (error) {
    case NFA_SCR_ERROR_SEND_CONF_CMD:
      return "SEND_CONF_CMD_FAILED";
    case NFA_SCR_ERROR_START_RF_DISC:
      return "START_RF_DISC";
    case NFA_SCR_ERROR_NCI_RSP_STATUS_FAILED:
      return "NCI_RSP_STATUS_FAILED";
    case NFA_SCR_ERROR_RECOVERY_STARTED:
      return "RECOVERY_STARTED";
    case NFA_SCR_ERROR_RECOVERY_COMPLETED:
      return "RECOVERY_COMPLETED";
    case NFA_SCR_ERROR_RECOVERY_TIMEOUT:
      return "RECOVERY_TIMEOUT";
    case NFA_SCR_ERROR_APP_STOP_REQ:
      return "APP_STOP_REQ";
    default:
      return "UNKNOWN";
  }
}

/*******************************************************************************
**
** Function         nfa_scr_is_evt_allowed
**
** Description      This API shall be called to check if requested event is
**                  allowed in current SCR state/error.
**                  1. No error encountered, all the events shall be allowed.
**                  2. If NFA_SCR_ERROR_RECOVERY_STARTED, only
**                     NFA_SCR_ESE_RECOVERY_COMPLETE_EVT event shall be allowed
**                  3. Any error except NFA_SCR_ERROR_RECOVERY_STARTED,
**                      only NFA_SCR_APP_STOP_REQ_EVT event shall be allowed.
** Returns         True if event is allowed.
**
*******************************************************************************/
bool nfa_scr_is_evt_allowed(uint8_t event) {
  bool is_allowed = false;
  if (IS_NO_SCR_ERROR || (IS_RECOVERY_STARTED && IS_EVT_RECOVERY_COMPLETED) ||
      (!IS_RECOVERY_STARTED && IS_EVT_APP_STOP_REQUEST) ||
      (IS_APP_STOP_ERROR && event == NFA_SCR_STOP_REQ_EVT)) {
    is_allowed = true;
    /* Clear the error flag */
    if(IS_SCR_ERROR) {
      nfa_scr_cb.error = NFA_SCR_NO_ERROR;
    }
  }
  if(!is_allowed) {
    DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s:Event(%d) is not allowed event in "
            "ERROR:%s", __func__, event, nfa_scr_get_error_name(nfa_scr_cb.error).c_str());
  }
  return is_allowed;
}

/*******************************************************************************
**
** Function         nfa_scr_error_handler
**
** Description      This method shall be called by SCR module to handle all the
**                  errors occurred during operation. It shall take care of all
**                  SCR states during error handling. Once error is handled, it
**                  shall notify failure event to upper layer.
**
** Parameter        Error Code
**
** Returns         True,
**
*******************************************************************************/
bool nfa_scr_error_handler(tNFA_SCR_ERROR error) {
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s:Enter ERROR:%s(%u) ", __func__,
          nfa_scr_get_error_name(error).c_str(), error);

  tNFA_SCR_EVT event = NFA_SCR_STOP_FAIL_EVT;
  nfa_scr_cb.error = error;
  bool notify_app = true;
  bool status = true;

  switch (error) {
    case NFA_SCR_ERROR_SEND_CONF_CMD:
      if(IS_SCR_START_IN_PROGRESS) {
        event = NFA_SCR_START_FAIL_EVT;
        nfa_scr_cb.state = NFA_SCR_STATE_START_CONFIG; /* Allow to start NFC Forum polling */
      } else {
        nfa_scr_cb.state = NFA_SCR_STATE_START_SUCCESS; /* Allow to trigger SCR_STOP_SEQ */
      }
      break;
    case NFA_SCR_ERROR_START_RF_DISC:
      event = NFA_SCR_START_FAIL_EVT;
      nfa_scr_cb.state = NFA_SCR_STATE_START_SUCCESS; /* Allow to trigger SCR_STOP_SEQ */
      break;
    case NFA_SCR_ERROR_APP_STOP_REQ:
      /* Change the sub state in which Reader stop ntf to remove can be received
       */
      nfa_scr_cb.sub_state = NFA_SCR_SUBSTATE_WAIT_ACTIVETED_NTF;
      [[fallthrough]];
    case NFA_SCR_ERROR_NCI_RSP_STATUS_FAILED:
      if(IS_SCR_START_IN_PROGRESS) {
        event = NFA_SCR_START_FAIL_EVT;
      }
      nfa_scr_cb.state = NFA_SCR_STATE_START_SUCCESS; /* Allow to trigger SCR_STOP_SEQ */
      break;
    case NFA_SCR_ERROR_RECOVERY_STARTED:
      status = false;     /* libnfc-nci shall continue with recovery handling */
      notify_app = false; /* This event shall not be notified to upper layer  */
      if(IS_SCR_START_SUCCESS || IS_SCR_STOP_SUCCESS) {
        nfa_scr_cb.error = NFA_SCR_NO_ERROR;
      } else {
        /* Start Recovery Timer */
        nfa_sys_start_timer(&nfa_scr_cb.scr_rec_tle,
                NFA_SCR_ERROR_REC_TIMEOUT_EVT, NFA_SCR_RECOVERY_TIMEOUT);
      }
      break;
    case NFA_SCR_ERROR_RECOVERY_COMPLETED:
      status = false;      /* libnfc-nci shall continue with recovery handling */
      if(IS_SCR_START_SUCCESS || IS_SCR_STOP_SUCCESS) {
       nfa_scr_cb.error = NFA_SCR_NO_ERROR;
       notify_app = false; /* This event shall not be notified to upper layer  */
       break;
      }
      nfa_sys_stop_timer(&nfa_scr_cb.scr_rec_tle); /* Stop Recovery Timer */
      [[fallthrough]];
    case NFA_SCR_ERROR_RECOVERY_TIMEOUT:
      if(IS_SCR_START_IN_PROGRESS || IS_SCR_APP_REQESTED) {
        event = NFA_SCR_START_FAIL_EVT;
      }
      nfa_scr_cb.state = NFA_SCR_STATE_START_SUCCESS; /* Allow to trigger SCR_STOP_SEQ */
      break;
    default:
      break;
  }

  if(notify_app)
    nfa_scr_notify_evt(event); /* Notify upper layer about SCR failure event */

  return status;
}
/*******************************************************************************
 **
 ** Function:        is_emvco_polling_restart_req
 **
 ** Description:     This method shall be used to check if emvco poll restart is requested

 ** Returns:         'TRUE' if emvco poll restart is needed else 'FALSE'
 **
 ******************************************************************************/
bool is_emvco_polling_restart_req(void) {
  if (nfa_scr_cb.restart_emvco_poll) {
    nfa_scr_cb.scr_evt_cback(NFA_SCR_START_REQ_EVT,NFA_STATUS_OK);
    return true;
  }
  return false;
}
/*******************************************************************************
 **
 ** Function:        nfa_scr_trigger_stop_seq
 **
 ** Description:     This method shall be called to start SCR_STOP sequence

 ** Returns:         True if sequence is triggered successfully
 **
 ******************************************************************************/
bool nfa_scr_trigger_stop_seq() {
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: Enter", __func__);
  nfa_sys_stop_timer(&nfa_scr_cb.scr_tle); /* Stop tag_op_timeout timer */
  bool is_expected = true;
  if (nfa_dm_cb.disc_cb.disc_state == NFA_DM_RFST_IDLE) {
    /* Simulate the cmd-rsp by changing the SCR's states as libnfc is already in idle state
     * If emvco polling restart is requested then start the polling
     *  else Simulate the behavior to continue with normal flow */
    nfa_scr_cb.state = NFA_SCR_STATE_STOP_CONFIG;
    nfa_scr_cb.sub_state = NFA_SCR_SUBSTATE_WAIT_DEACTIVATE_NTF;
    if (nfa_scr_cb.rdr_req_guard_time && !is_emvco_polling_restart_req()) {
      nfa_scr_cb.scr_evt_cback(NFA_SCR_RF_DEACTIVATE_NTF_EVT,NFA_STATUS_OK);
    }
  } else if(NFA_STATUS_OK == NFA_StopRfDiscovery()) {
    nfa_scr_cb.state = NFA_SCR_STATE_STOP_CONFIG;
    nfa_scr_cb.sub_state = NFA_SCR_SUBSTATE_WAIT_DEACTIVATE_RSP;
  } else {
    is_expected = false;
  }
  return is_expected;
}
/*******************************************************************************
 **
 ** Function:        nfa_scr_handle_start_req
 **
 ** Description:     This function will trigger the mPOS start sequence once
 **                  610A is received to start reader & App has requested for it.
 **
 ** Returns:         always true to consume the 610A for readr start request
 **
 ******************************************************************************/
static bool nfa_scr_handle_start_req() {
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: enter", __func__);
  bool is_expected = true;
  if(IS_SCR_APP_REQESTED && nfa_scr_cb.sub_state == NFA_SCR_SUBSTATE_WAIT_START_RDR_NTF) {
    DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("EMV-CO polling profile");
    nfa_scr_cb.state = NFA_SCR_STATE_START_IN_PROGRESS;
    nfa_scr_send_prop_get_set_conf(true);
  } else if (IS_SCR_START_SUCCESS && nfa_scr_cb.rdr_req_guard_time) {
    /* If NXP_RDR_REQ_GUARD_TIME is not provided this code snippet shall not be applied
     * i.e. nfa_scr_cb.restart_emvco_poll shall not be set to 1 */
    switch(nfa_scr_cb.sub_state) {
      case NFA_SCR_SUBSTATE_WAIT_ACTIVETED_NTF:
        if(nfa_scr_cb.wait_for_deact_ntf) { /* Activated ntf has been received */
          nfa_scr_cb.restart_emvco_poll = 1;
          nfa_scr_cb.scr_evt_cback(NFA_SCR_STOP_REQ_EVT,NFA_STATUS_OK);
          break;
        }
        [[fallthrough]];
      case NFA_SCR_SUBSTATE_WAIT_POLL_RSP:
        DLOG_IF(INFO, nfc_debug_enabled)
                << StringPrintf("%s: EMVCO polling is already started", __func__);
        break;
    }
  } else if (nfa_scr_cb.state == NFA_SCR_STATE_STOP_CONFIG && nfa_scr_cb.rdr_req_guard_time) {
    switch(nfa_scr_cb.sub_state) {
      case NFA_SCR_SUBSTATE_WAIT_DEACTIVATE_RSP:
        [[fallthrough]];
      case NFA_SCR_SUBSTATE_WAIT_DEACTIVATE_NTF:
        if(nfa_scr_cb.wait_for_deact_ntf) { /* Activated ntf has been received */
          nfa_scr_cb.restart_emvco_poll = 1;
        } else {
          is_expected = nfa_scr_start_polling(NFA_STATUS_OK);
        }
        break;
    }
  } else {
    is_expected = false;
  }
  return is_expected;
}

/*******************************************************************************
 **
 ** Function:        nfa_scr_proc_app_start_req
 **
 ** Description:     This function shall be called by application while starting
 **                  RederMode to handle the SCR states accordingly
 **
 ** Returns:         Always returns true
 **
 ******************************************************************************/
bool nfa_scr_proc_app_start_req() {
  nfa_scr_cb.state = NFA_SCR_STATE_START_CONFIG;
  nfa_scr_cb.sub_state = NFA_SCR_SUBSTATE_WAIT_START_RDR_NTF;
  /* Notify the NFA_ScrSetReaderMode(true) success to JNI */
  nfa_scr_notify_evt((uint8_t)NFA_SCR_SET_READER_MODE_EVT, NFA_STATUS_OK);
  return true;
}

/*******************************************************************************
 **
 ** Function:        nfa_scr_finalize
 **
 ** Description:
 **
 ** Returns:         Always returns true
 **
 ******************************************************************************/
bool nfa_scr_finalize(bool notify) {
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: enter", __func__);
  nfa_scr_cb.state = NFA_SCR_STATE_STOPPED;
  nfa_scr_cb.sub_state = NFA_SCR_SUBSTATE_INVALID;
  nfa_scr_cb.rdr_stop_req = false;
  nfa_scr_cb.app_stop_req = false;
  nfa_scr_cb.error = NFA_SCR_NO_ERROR;
  memset(&ntf_timeout_cnt,0x00,sizeof(uint32_t));
  if(notify) {
      /* Notify the NFA_ScrSetReaderMode(false) success to JNI */
      nfa_scr_notify_evt((uint8_t)NFA_SCR_SET_READER_MODE_EVT, NFA_STATUS_OK);
  }
  /* clear JNI and SCR callbacks pointers */
  nfa_scr_cb.scr_cback = nullptr;
  set_smr_cback(nullptr);
  return true;
}
/*******************************************************************************
 **
 ** Function:        nfa_scr_handle_stop_req
 **
 ** Description:     This method shall be called to handle STOP SCR request
 **              State                            Actions
 **              NFA_SCR_STATE_START_IN_PROGRESS  Do Nothing, app_stop_req will trigger
 **                                               STOP_SEQ once START_SEQ is done,.
 **              NFA_SCR_STATE_START_SUCCESS      Trigger SCR_STOP_SEQUENCE
 **              NFA_SCR_STATE_START_CONFIG       nfa_scr_finalize
 **              NFA_SCR_STATE_STOP_SUCCESS       nfa_scr_finalize
 **              NFA_SCR_STATE_STOP_CONFIG        Stop is already in progress, do nothing
 **              NFA_SCR_STATE_STOP_IN_PROGRESS   Stop is already in progress, do nothing
 ** Parameter        None
 **
 ** Returns:         True if sequence is triggered successfully
 **
 ******************************************************************************/
static bool nfa_scr_handle_stop_req() {
  bool status = true;
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: enter", __func__);

  switch(nfa_scr_cb.state) {
  case NFA_SCR_STATE_START_IN_PROGRESS:
    DLOG_IF(INFO, nfc_debug_enabled)
            << StringPrintf("%s: SCR will be stopped once start seq is over", __func__);
    break;
  case NFA_SCR_STATE_START_SUCCESS:
    nfa_sys_stop_timer(&nfa_scr_cb.scr_tle); /* Stop tag_op_timeout timer */
    status = nfa_scr_trigger_stop_seq();
    break;
  case NFA_SCR_STATE_START_CONFIG:
    [[fallthrough]];
  case NFA_SCR_STATE_STOP_SUCCESS:
    status = nfa_scr_finalize();
    break;
  case NFA_SCR_STATE_STOP_CONFIG:
    if(nfa_scr_cb.wait_for_deact_ntf == false) {
      nfa_scr_cb.state = NFA_SCR_STATE_STOP_IN_PROGRESS;
      nfa_scr_send_prop_get_set_conf(false);
    } else { /* Proceed with reader stop once RF_DEACTIVATE_NTF is received */
      DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: "
              "Reader will be stopped once card is removed from field", __func__);
    }
    break;
  case NFA_SCR_STATE_STOP_IN_PROGRESS:
    DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: Stop is already in progress", __func__);
    break;
  default:
    status =false;
    break;
  }
  return status;
}

/*******************************************************************************
 **
 ** Function:        nfa_scr_send_prop_set_conf_cb
 **
 ** Description:     callback for Proprietary set Config command
 **
 ** Returns:         void
 **
 *******************************************************************************/
void nfa_scr_send_prop_set_conf_cb(uint8_t event, uint16_t param_len, uint8_t *p_param) {
  (void)event;

  if(p_param) {
    if(nfa_scr_cb.scr_evt_cback != nullptr) {
      if(nfa_scr_cb.scr_evt_cback(NFA_SCR_CORE_SET_CONF_RSP_EVT,p_param[3])) {
        return;
      }
    }
    DLOG_IF(ERROR, nfc_debug_enabled) << StringPrintf(
        "%s: param_len=%u, p_param=%p", __func__, param_len, p_param);
  }
  return;
}

/*******************************************************************************
 **
 ** Function:        nfa_scr_get_prop_set_conf_cmd
 **
 ** Description:     This method will reader a prop CORE_SET_CONFIG_CMD from the
 **                  Config file and will modify it as per the request.
 **
 ** Returns:         Command as a C++ vector
 **
 *******************************************************************************/
static vector<uint8_t> nfa_scr_get_prop_set_conf_cmd(bool set) {
  vector<uint8_t> cmd_buf;
  bool resize_cmd = false;
  if (NfcConfig::hasKey(NAME_NXP_PROP_RESET_EMVCO_CMD)) {
    cmd_buf = NfcConfig::getBytes(NAME_NXP_PROP_RESET_EMVCO_CMD);
  }
  if(cmd_buf.size() != 0x08) {
    DLOG_IF(ERROR, nfc_debug_enabled)
            << StringPrintf("%s: Prop set conf is not provided", __func__);
    cmd_buf.resize(0);
    nfa_scr_error_handler(NFA_SCR_ERROR_SEND_CONF_CMD);
  } else if (nfa_scr_cb.configure_lpcd == 2) {
    if(set) {
      cmd_buf[7] = nfa_scr_cb.poll_prof_sel_cfg; /*EMV-CO Poll for certification*/
      if(nfa_scr_cb.p_nfa_get_confg.param_tlvs[LPCD_BYTE_POS] & (1 << LPCD_BIT_POS)) {
        /* Disable the LPCD */
        nfa_scr_cb.p_nfa_get_confg.param_tlvs[LPCD_BYTE_POS] &= ~(1 << LPCD_BIT_POS);
        resize_cmd = true;
      }
    } else {
      if(!(nfa_scr_cb.p_nfa_get_confg.param_tlvs[LPCD_BYTE_POS] & (1 << LPCD_BIT_POS))) {
        /* Eable the LPCD */
        nfa_scr_cb.p_nfa_get_confg.param_tlvs[LPCD_BYTE_POS] |= (1 << LPCD_BIT_POS);
        resize_cmd = true;
      }
    }
    if(resize_cmd) {//resize the vector here
      cmd_buf.at(2) += (nfa_scr_cb.p_nfa_get_confg.tlv_size - 2); //Modified len
      cmd_buf.at(3) = 2; //Modify number of TLVs
      cmd_buf.insert(cmd_buf.end(), nfa_scr_cb.p_nfa_get_confg.param_tlvs,
              nfa_scr_cb.p_nfa_get_confg.param_tlvs + (nfa_scr_cb.p_nfa_get_confg.tlv_size - 2));
    }
  } else if(set) {
    cmd_buf[7] = nfa_scr_cb.poll_prof_sel_cfg; /*EMV-CO Poll for certification*/
  }
  return cmd_buf;
}

/*******************************************************************************
 **
 ** Function:        nfa_scr_send_prop_set_conf
 **
 ** Description:     Enable/disable EmvCo polling based on i/p param
 **
 ** Returns:         None
 **
 *******************************************************************************/
static void nfa_scr_send_prop_set_conf(bool set) {
  tNFA_STATUS stat = NFA_STATUS_FAILED;
  vector<uint8_t> cmd_buf;
  DLOG_IF(INFO, nfc_debug_enabled)
          << StringPrintf("%s: enter status = %s", __func__, (set?"Set":"Reset"));

  cmd_buf = nfa_scr_get_prop_set_conf_cmd(set);
  if(!cmd_buf.size()) {
    return;
  }

  NFA_SetEmvCoState(set);
  stat = NFA_SendRawVsCommand(cmd_buf.size(),(uint8_t*)&cmd_buf[0], nfa_scr_send_prop_set_conf_cb);

  if (stat == NFA_STATUS_OK) {
    LOG(INFO) << StringPrintf("%s: Success NFA_SendRawVsCommand", __func__);
    nfa_scr_cb.sub_state = NFA_SCR_SUBSTATE_WAIT_PROP_SET_CONF_RSP;
  } else {
    DLOG_IF(ERROR, nfc_debug_enabled) << StringPrintf("%s: Failed NFA_SendRawVsCommand", __func__);
    nfa_scr_error_handler(NFA_SCR_ERROR_SEND_CONF_CMD);
  }
}

/*******************************************************************************
 **
 ** Function:        nfa_scr_handle_get_conf_rsp
 **
 ** Description:     This API, based on the state, will send a prop set config
 **                  to enable and/or disable the NFC Forum mode & LPCD
 **
 ** Returns:         True if response is expected and SET_CONF sends successfully
 **                  otherwise false
 **
 *******************************************************************************/
static bool nfa_scr_handle_get_conf_rsp(uint8_t status) {
  bool is_expected =  false;
  bool start_emvco_polling = false;
  DLOG_IF(INFO, nfc_debug_enabled)
          << StringPrintf("%s: enter status = %u", __func__, status);

  if(nfa_scr_cb.sub_state == NFA_SCR_SUBSTATE_WAIT_PROP_GET_CONF_RSP) {
    switch(nfa_scr_cb.state) {
      case NFA_SCR_STATE_START_IN_PROGRESS:
        start_emvco_polling = true;
        [[fallthrough]];
      case NFA_SCR_STATE_STOP_IN_PROGRESS:
        is_expected = true;
        break;
    }
  }

  if (is_expected) {
    IS_STATUS_ERROR(status);
    nfa_scr_send_prop_set_conf(start_emvco_polling);
  }
  return is_expected;
}

/*******************************************************************************
 **
 ** Function:        nfa_scr_send_prop_get_conf_cb
 **
 ** Description:     callback for Proprietary set Config command
 **
 ** Returns:         void
 **
 *******************************************************************************/
void nfa_scr_send_prop_get_conf_cb(uint8_t event, uint16_t param_len, uint8_t *p_param) {
  (void)event;

  if(p_param) {
    if(nfa_scr_cb.scr_evt_cback != nullptr) {
      nfa_scr_cb.p_nfa_get_confg.tlv_size = p_param[2];
      nfa_scr_cb.p_nfa_get_confg.param_tlvs = &p_param[5];
      nfa_scr_cb.scr_evt_cback(NFA_SCR_CORE_GET_CONF_RSP_EVT,p_param[3]);
      nfa_scr_cb.p_nfa_get_confg.tlv_size = 0;
      nfa_scr_cb.p_nfa_get_confg.param_tlvs = nullptr;
    }
    DLOG_IF(ERROR, nfc_debug_enabled) << StringPrintf(
        "%s: param_len=%u, p_param=%p", __func__, param_len, p_param);
  }
  return;
}

/*******************************************************************************
 **
 ** Function:        nfa_scr_send_prop_get_set_conf
 **
 ** Description:     Send NFC GET/SET CONF COMMAND
 **
 ** Returns:         None
 **
 *******************************************************************************/
static void nfa_scr_send_prop_get_set_conf(bool rdr_start_req) {
  if(nfa_scr_cb.configure_lpcd == 2) {
    nfa_scr_send_prop_get_conf();
  } else {
    nfa_scr_send_prop_set_conf(rdr_start_req);
  }
}
/*******************************************************************************
 **
 ** Function:        nfa_scr_send_prop_get_conf
 **
 ** Description:     Send NFC GET CONF COMMAND
 **
 ** Returns:         None
 **
 *******************************************************************************/
static void nfa_scr_send_prop_get_conf(void) {
  tNFA_STATUS stat = NFA_STATUS_FAILED;
  vector<uint8_t> cmd_buf {0x20, 0x03, 0x03, 0x01, 0xA0, 0x68};
  DLOG_IF(INFO, nfc_debug_enabled)
          << StringPrintf("%s: enter", __func__);

  stat = NFA_SendRawVsCommand(cmd_buf.size(),(uint8_t*)&cmd_buf[0], nfa_scr_send_prop_get_conf_cb);

  if (stat == NFA_STATUS_OK) {
    LOG(INFO) << StringPrintf("%s: Success NFA_SendRawVsCommand", __func__);
    nfa_scr_cb.sub_state = NFA_SCR_SUBSTATE_WAIT_PROP_GET_CONF_RSP;
  } else {
    DLOG_IF(ERROR, nfc_debug_enabled) << StringPrintf("%s: Failed NFA_SendRawVsCommand", __func__);
    nfa_scr_error_handler(NFA_SCR_ERROR_SEND_CONF_CMD);
  }
}

/*******************************************************************************
 **
 ** Function:        nfa_scr_handle_deact_rsp_ntf
 **
 ** Description:     This API, based on the state, either will send a prop get
 **                  config or will start a timer to receive RF_DEACTIVATE_NTF &
 **                  Notify app of REMOVE_CARD_NTF periodically.
 **
 ** Returns:         True if response is expected and GET_CONF sends successfully
 **                  otherwise false
 **
 *******************************************************************************/
static bool nfa_scr_handle_deact_rsp_ntf(uint8_t status) {
  bool is_expected =  false;
  DLOG_IF(INFO, nfc_debug_enabled)
          << StringPrintf("%s: enter status = %u", __func__, status);

  if(nfa_scr_cb.state == NFA_SCR_STATE_STOP_CONFIG) {
      switch(nfa_scr_cb.sub_state) {
        case NFA_SCR_SUBSTATE_WAIT_DEACTIVATE_RSP: {
          IS_STATUS_ERROR(status);
          if(nfa_scr_cb.wait_for_deact_ntf == true) {
            is_expected = true;
            nfa_scr_cb.sub_state = NFA_SCR_SUBSTATE_WAIT_DEACTIVATE_NTF;
            /* Start timer to post NFA_SCR_REMOVE_CARD_EVT */
            nfa_sys_start_timer(&nfa_scr_cb.scr_tle, NFA_SCR_RM_CARD_TIMEOUT_EVT,
                     NFA_SCR_CARD_REMOVE_TIMEOUT);
            DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("Waiting for STOP disc ntf");
            break;
          }
        [[fallthrough]];
        }
        case NFA_SCR_SUBSTATE_WAIT_DEACTIVATE_NTF: {
          is_expected = true;
          if(nfa_scr_cb.wait_for_deact_ntf == true) {
            nfa_scr_cb.wait_for_deact_ntf = false;
            /* Stop the NFA_SCR_REMOVE_CARD_EVT posting timer */
            nfa_sys_stop_timer(&nfa_scr_cb.scr_tle);
          }
          /* App stop request shall have highest prio than 610A for start rdr mode */
          if(nfa_scr_cb.app_stop_req) {
            nfa_scr_cb.state = NFA_SCR_STATE_STOP_IN_PROGRESS;
            nfa_scr_send_prop_get_set_conf(false);
            break;
          }
          if(!is_emvco_polling_restart_req()) {
            /* Notify app of NFA_SCR_STOP_SUCCESS_EVT */
            nfa_scr_notify_evt(NFA_SCR_STOP_SUCCESS_EVT);
          }
          break;
       }
    }
  }
  return is_expected;
}
/*******************************************************************************
 **
 ** Function:        nfa_scr_discovermap_cb
 **
 ** Description:     callback for Discover Map command
 **
 ** Returns:         void
 **
 *******************************************************************************/
void nfa_scr_discovermap_cb(tNFC_DISCOVER_EVT event, tNFC_DISCOVER *p_data) {
  switch(event) {
  case NFC_MAP_DEVT:
    if(nfa_scr_cb.scr_evt_cback != nullptr && IS_DISC_MAP_RSP_EXPECTED) {
      if(nfa_scr_cb.scr_evt_cback(NFA_SCR_RF_DISCOVER_MAP_RSP_EVT,p_data->status)) {
        return;
      }
    }
   break;
  }
  return;
}
/*******************************************************************************
 **
 ** Function:        nfa_scr_send_discovermap_cmd
 **
 ** Description:     Send discover map command based on the current state of SCR
 *module
 **
 ** Returns:         true
 **
 *******************************************************************************/
static bool nfa_scr_send_discovermap_cmd(uint8_t status) {
  tNCI_DISCOVER_MAPS* p_intf_mapping = nullptr;
  bool is_expected = false;
  uint8_t num_disc_maps = NFC_NUM_INTERFACE_MAP;
  const tNCI_DISCOVER_MAPS
      nfc_interface_mapping_ese[NFC_SWP_RD_NUM_INTERFACE_MAP] = {
          /* Protocols that use Frame Interface do not need to be included in
             the interface mapping */
          {NCI_PROTOCOL_ISO_DEP, NCI_INTERFACE_MODE_POLL,
           NCI_INTERFACE_ESE_DIRECT}};

  const tNCI_DISCOVER_MAPS nfc_interface_mapping_default[NFC_NUM_INTERFACE_MAP] = {
          /* Protocols that use Frame Interface do not need to be included in
             the interface mapping */
        {NCI_PROTOCOL_ISO_DEP, NCI_INTERFACE_MODE_POLL_N_LISTEN,
         NCI_INTERFACE_ISO_DEP}
        #if (NFC_RW_ONLY == FALSE)
        , /* this can not be set here due to 2079xB0 NFCC issues */
        {NCI_PROTOCOL_NFC_DEP, NCI_INTERFACE_MODE_POLL_N_LISTEN,
         NCI_INTERFACE_NFC_DEP}
        #endif
        , /* This mapping is for Felica on DH  */
        {NCI_PROTOCOL_T3T, NCI_INTERFACE_MODE_LISTEN, NCI_INTERFACE_FRAME}};

  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: enter status=%u", __func__, status);

  if (NFA_SCR_SUBSTATE_WAIT_PROP_SET_CONF_RSP == nfa_scr_cb.sub_state) {
    switch(nfa_scr_cb.state) {
    case NFA_SCR_STATE_START_IN_PROGRESS:
      IS_STATUS_ERROR(status);
      DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: mapping intf to ESE", __func__);
      p_intf_mapping = (tNCI_DISCOVER_MAPS*)nfc_interface_mapping_ese;
      num_disc_maps = NFC_SWP_RD_NUM_INTERFACE_MAP;
      is_expected = true;
      break;
    case NFA_SCR_STATE_STOP_IN_PROGRESS:
      IS_STATUS_ERROR(status);
      DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: mapping intf to default", __func__);
      p_intf_mapping = (tNCI_DISCOVER_MAPS*)nfc_interface_mapping_default;
      is_expected = true;
      break;
    }
  }

  if(is_expected == true) {
    (void)NFC_DiscoveryMap(num_disc_maps, p_intf_mapping, nfa_scr_discovermap_cb);
    nfa_scr_cb.sub_state = NFA_SCR_SUBSTATE_WAIT_DISC_MAP_RSP;
  }
  return is_expected;
}

/*******************************************************************************
 **
 ** Function:        nfa_scr_start_polling
 **
 ** Description:     if SCR mode start is in progress, start discovery
 **                  if SCR mode stop is in progress, notify NFA_SCR_RDR_STOP_SUCCESS
 **                  to JNI so that application can Start Nfc Forum Polling
 **
 ** Returns:         true
 **
 *******************************************************************************/
bool nfa_scr_start_polling(uint8_t status) {
  bool is_expected = true;;
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: enter status=%u, "
          "state=%u substate=%u", __func__, status, nfa_scr_cb.state, nfa_scr_cb.sub_state);
  if(nfa_scr_cb.sub_state == NFA_SCR_SUBSTATE_WAIT_DISC_MAP_RSP || IS_EMVCO_RESTART_REQ) {
    switch (nfa_scr_cb.state) {
    case NFA_SCR_STATE_STOP_IN_PROGRESS:
      IS_STATUS_ERROR(status);
      nfa_scr_cb.state = NFA_SCR_STATE_STOP_SUCCESS;
      nfa_scr_finalize();
      break;
    case NFA_SCR_STATE_STOP_CONFIG: /* IS_EMVCO_RESTART_REQ */
      nfa_scr_cb.state = NFA_SCR_STATE_START_IN_PROGRESS;
      [[fallthrough]];
    case NFA_SCR_STATE_START_IN_PROGRESS:
      if(NFA_STATUS_OK == NFA_StartRfDiscovery()) {
        nfa_scr_cb.sub_state = NFA_SCR_SUBSTATE_WAIT_POLL_RSP;
      } else {
        nfa_scr_error_handler(NFA_SCR_ERROR_START_RF_DISC);
      }
      break;
    default :
      is_expected = false;
      break;
    }
  } else {
    is_expected = false;
  }

  return is_expected;
}

bool nfa_scr_handle_act_ntf(uint8_t status) {
  (void)status;
  bool is_expected = false;
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: enter", __func__);
  if(nfa_scr_cb.state == NFA_SCR_STATE_START_SUCCESS &&
          nfa_scr_cb.sub_state == NFA_SCR_SUBSTATE_WAIT_ACTIVETED_NTF) {
    is_expected = true;
    nfa_scr_cb.wait_for_deact_ntf = true;
    nfa_sys_stop_timer(&nfa_scr_cb.scr_tle); /* Stop tag_op_timeout timer */
    nfa_scr_notify_evt((uint8_t)NFA_SCR_TARGET_ACTIVATED_EVT, NFA_STATUS_OK);
  }
  return is_expected;
}

bool nfa_scr_rdr_processed(uint8_t status) {
  bool is_expected = false;
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: enter status=%u", __func__, status);
  if(IS_SCR_START_IN_PROGRESS && nfa_scr_cb.sub_state == NFA_SCR_SUBSTATE_WAIT_POLL_RSP) {
    IS_STATUS_ERROR(status);
    nfa_scr_cb.state = NFA_SCR_STATE_START_SUCCESS;
    nfa_scr_cb.sub_state = NFA_SCR_SUBSTATE_WAIT_ACTIVETED_NTF;
    /* Notify app of NFA_SCR_RDR_START_SUCCESS */
    nfa_scr_notify_evt(NFA_SCR_START_SUCCESS_EVT);
    nfa_sys_stop_timer(&nfa_scr_cb.scr_tle);
    /* Start tag_op_timeout timer */
    nfa_sys_start_timer(&nfa_scr_cb.scr_tle, NFA_SCR_TAG_OP_TIMEOUT_EVT,
            nfa_scr_cb.tag_op_timeout * NFA_SCR_CARD_REMOVE_TIMEOUT);
    is_expected = true;
    if(nfa_scr_cb.app_stop_req) { /* Trigger STOP sequence here*/
      if(nfa_scr_handle_stop_req() != true) {
        DLOG_IF(ERROR, nfc_debug_enabled)
                << StringPrintf("%s: Failed to start the SCR_STOP seq", __func__);
      }
    }
  }
  return is_expected;
}
