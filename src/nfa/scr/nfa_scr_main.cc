/******************************************************************************
 *
 *  Copyright 2019 NXP
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
 *This is the main implementation file for the NFA Smart Card Reader(ETSI/POS).
 *
 *****************************************************************************/
#include <android-base/stringprintf.h>
#include <base/logging.h>
#include <string.h>
#include <vector>
#include "nci_defs.h"
#include "nfa_api.h"
#include "nfa_ee_int.h"
#include "nfa_rw_api.h"
#include "nfa_scr_int.h"
#include "nfc_config.h"

using android::base::StringPrintf;

extern bool nfc_debug_enabled;

/******************************************************************************
 **  Constants
 *****************************************************************************/
#define NFC_NUM_INTERFACE_MAP 3
#define NFC_SWP_RD_NUM_INTERFACE_MAP 1
#define NFA_SCR_CARD_REMOVE_TIMEOUT 1000  // 1 Second

#define IS_STATUS_ERROR(status)                                          \
  {                                                                      \
    if (NFA_STATUS_OK != status) {                                       \
      return nfa_scr_error_handler(NFA_SCR_ERROR_NCI_RSP_STATUS_FAILED); \
    }                                                                    \
  }

#define IS_DISC_MAP_RSP_EXPECTED                           \
  ((nfa_scr_cb.state == NFA_SCR_STATE_START_IN_PROGRESS || \
    nfa_scr_cb.state == NFA_SCR_STATE_STOP_IN_PROGRESS) && \
   nfa_scr_cb.sub_state == NFA_SCR_SUBSTATE_WAIT_DISC_MAP_RSP)

/******************************************************************************
 **  Global Variables
 *****************************************************************************/
tNFA_SCR_CB nfa_scr_cb;
static uint32_t ntf_timeout_cnt = 0x00;

/******************************************************************************
 **  Internal Functions
 *****************************************************************************/
void nfa_scr_discovermap_cb(tNFC_DISCOVER_EVT event, tNFC_DISCOVER* p_data);
/*** Static Functions ***/
static bool nfa_scr_proc_secure_rdr_req(uint8_t op);
static bool nfa_scr_handle_stop_req();
static bool nfa_scr_trigger_stop_seq();
static void nfa_scr_send_prop_set_conf(bool set);
static std::vector<uint8_t> nfa_scr_get_prop_set_conf_cmd(bool set);
static bool nfa_scr_handle_deact_rsp_ntf(uint8_t status);
static bool nfa_scr_send_discovermap_cmd(uint8_t status);
static bool nfa_scr_start_polling(uint8_t status);
static bool nfa_scr_rdr_processed(uint8_t status);
static bool nfa_scr_handle_act_ntf(uint8_t status);
static void nfa_scr_tag_op_timeout_handler(void);
static void nfa_scr_rm_card_timeout_handler(void);

static const tNFA_SYS_REG nfa_scr_sys_reg = {nullptr, nfa_scr_evt_hdlr,
                                             nfa_scr_sys_disable, nullptr};

/******************************************************************************
 **  NFA SCR callback to be called from other modules
 *****************************************************************************/
bool nfa_scr_cback(uint8_t event, uint8_t* p_data);

/*******************************************************************************
**
** Function         nfa_scr_init
**
** Description      Initialize NFA Smart Card Module
**
** Returns          None
**
*******************************************************************************/
void nfa_scr_init(void) {
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: enter", __func__);
  /* initialize control block */
  memset(&nfa_scr_cb, 0, sizeof(tNFA_SCR_CB));

  nfa_scr_cb.state = NFA_SCR_STATE_STOPPED;
  if (NfcConfig::hasKey(NAME_NFA_DM_DISC_NTF_TIMEOUT)) {
    nfa_scr_cb.deact_ntf_timeout =
        NfcConfig::getUnsigned(NAME_NFA_DM_DISC_NTF_TIMEOUT);
  } else {
    nfa_scr_cb.deact_ntf_timeout = 0; /* Infinite Time */
  }
  if (NfcConfig::hasKey(NAME_NXP_SWP_RD_TAG_OP_TIMEOUT)) {
    nfa_scr_cb.tag_op_timeout =
        NfcConfig::getUnsigned(NAME_NXP_SWP_RD_TAG_OP_TIMEOUT);
  } else {
    nfa_scr_cb.tag_op_timeout = 20; /* 20 seconds */
  }
  nfa_scr_cb.poll_prof_sel_cfg = NfcConfig::getUnsigned(NAME_NFA_CONFIG_FORMAT);
  if (nfa_scr_cb.poll_prof_sel_cfg == 0x00) {
    nfa_scr_cb.poll_prof_sel_cfg = 1;
  }
  /* register message handler on NFA SYS */
  nfa_sys_register(NFA_ID_SCR, &nfa_scr_sys_reg);
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: exit", __func__);
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
  if (nfa_scr_cb.scr_evt_cback) {
    bool status = false;
    /* Trigger the SCR_STOP seq */
    nfa_scr_cb.start_nfcforum_poll = true;
    status = nfa_scr_cb.scr_evt_cback(NFA_SCR_STOP_REQ_EVT, NFA_STATUS_OK);

    /* Stop timers */
    nfa_sys_stop_timer(&nfa_scr_cb.scr_tle);
  }
  /* deregister message handler on NFA SYS */
  nfa_sys_deregister(NFA_ID_SCR);
}

/*******************************************************************************
**
** Function       nfa_scr_cback
**
** Description    1. If the NTF(610A) has been received for Start/Stop Reader
**               over eSE then EE module shall call this with
**               NFA_SCR_START_REQ_EVT, NFA_SCR_STOP_REQ_EVT respectively.
**                2. If SCR module has been triggered and NFC_TASK has received
*rsp
**               this callback shall be invoked with respective events.
**
** Return         True if event is processed/expected else false
**
*******************************************************************************/
bool nfa_scr_cback(uint8_t event, uint8_t status) {
  bool stat = false;
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf(
      "%s: enter event=%u, status=%u state=%u,"
      "substate=%u",
      __func__, event, status, nfa_scr_cb.state, nfa_scr_cb.sub_state);
  switch (event) {
    case NFA_SCR_APP_REQ_EVT:
      [[fallthrough]];
    case NFA_SCR_STOP_REQ_EVT:
      stat = nfa_scr_proc_secure_rdr_req(event);
      break;
    case NFA_SCR_START_REQ_EVT:
      nfa_scr_cb.state = NFA_SCR_STATE_START_IN_PROGRESS;
      [[fallthrough]];
    case NFA_SCR_RF_DEACTIVATE_RSP_EVT:
      [[fallthrough]];
    case NFA_SCR_RF_DEACTIVATE_NTF_EVT:
      stat = nfa_scr_handle_deact_rsp_ntf(status);
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
    case NFA_SCR_CORE_RESET_NTF_EVT:
      stat = nfa_scr_error_handler(NFA_SCR_CORE_RESET_NTF);
      break;
    case NFA_SCR_CORE_GEN_ERROR_NTF_EVT:
      stat = nfa_scr_error_handler(NFA_SCR_CORE_GEN_ERROR_NTF);
      break;
    case NFA_SCR_CORE_INTF_ERROR_NTF_EVT:
      stat = nfa_scr_error_handler(NFA_SCR_CORE_INTF_ERROR_NTF);
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
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf(
      "nfa_scr_evt_hdlr event:%s "
      "(0x%04x)",
      nfa_scr_get_event_name(p_msg->event).c_str(), p_msg->event);

  switch (p_msg->event) {
    case NFA_SCR_API_SET_READER_MODE_EVT: {
      tNFA_SCR_API_SET_READER_MODE* p_rd_evt_data =
          (tNFA_SCR_API_SET_READER_MODE*)p_msg;
      nfa_scr_set_reader_mode(p_rd_evt_data->mode,
                              (tNFA_SCR_CBACK*)p_rd_evt_data->scr_cback,
                              nfa_scr_cback);
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
void nfa_scr_tag_op_timeout_handler() {
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
** Returns          true
**
*******************************************************************************/
void nfa_scr_rm_card_timeout_handler() {
  nfa_scr_notify_evt(NFA_SCR_REMOVE_CARD_EVT);

  if ((nfa_scr_cb.deact_ntf_timeout != 0x00)) {
    ntf_timeout_cnt++;
    if (ntf_timeout_cnt == nfa_scr_cb.deact_ntf_timeout) {
      DLOG_IF(ERROR, nfc_debug_enabled)
          << StringPrintf("%s:DISC_NTF_TIMEOUT", __func__);
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
  tNFA_SCR_EVT event = NFA_SCR_FAIL_EVT;

  switch (error) {
    case NFA_SCR_ERROR:
      [[fallthrough]];
    case NFA_SCR_ERROR_NCI_RSP_STATUS_FAILED:
      if (IS_SCR_START_IN_PROGRESS) {
        event = NFA_SCR_START_FAIL_EVT;
      } else if (IS_SCR_STOP_IN_PROGRESS) {
        event = NFA_SCR_STOP_FAIL_EVT;
      } else {
        event = NFA_SCR_FAIL_EVT;
      }
      break;
    case NFA_SCR_ERROR_STATE_MISMATCH: {
      DLOG_IF(ERROR, nfc_debug_enabled)
          << StringPrintf("%s: STATE_MISMATCH", __func__);
      break;
    }
    case NFA_SCR_CORE_RESET_NTF: {
      DLOG_IF(ERROR, nfc_debug_enabled)
          << StringPrintf("%s: CORE_RESET_NTF", __func__);
      break;
    }
    case NFA_SCR_CORE_GEN_ERROR_NTF: {
      DLOG_IF(ERROR, nfc_debug_enabled)
          << StringPrintf("%s: CORE_GEN_ERROR_NTF", __func__);
      break;
    }
    case NFA_SCR_CORE_INTF_ERROR_NTF: {
      DLOG_IF(ERROR, nfc_debug_enabled)
          << StringPrintf("%s: CORE_INTF_ERROR_NTF", __func__);
      break;
    }
  }
  /* Notify upper layer about SCR failure event */
  nfa_scr_notify_evt(event);

  return true;
}

/*******************************************************************************
 **
 ** Function:        nfa_scr_trigger_stop_seq
 **
 ** Description:     This method shall be called to start SCR_STOP sequence

 ** Returns:         True if sequence is triggered successfully
 **
 ******************************************************************************/
static bool nfa_scr_trigger_stop_seq() {
  if (NFA_STATUS_OK == NFA_StopRfDiscovery()) {
    nfa_scr_cb.state = NFA_SCR_STATE_STOP_CONFIG;
    nfa_scr_cb.sub_state = NFA_SCR_SUBSTATE_WAIT_DEACTIVATE_RSP;
    return true;
  }
  nfa_scr_error_handler(NFA_SCR_ERROR);
  return false;
}

/*******************************************************************************
 **
 ** Function:        nfa_scr_handle_stop_req
 **
 ** Description:     This method shall be called to handle STOP SCR request
 **              State                            Actions
 **              NFA_SCR_STATE_START_IN_PROGRESS  Set flag to trig STOP_SEQ
 *followed by STAR_SEQ
 **              NFA_SCR_STATE_START_SUCCESS      Trigger complete
 *SCR_STOP_SEQUENCE
 **              NFA_SCR_STATE_START_CONFIG       Send only Start Discovery
 *command
 **              NFA_SCR_STATE_STOP_SUCCESS       Send only Start Discovery
 *command
 **              NFA_SCR_STATE_STOP_CONFIG        Stop is already in progress,
 *do nothing
 **              NFA_SCR_STATE_STOP_IN_PROGRESS   Stop is already in progress,
 *do nothing
 ** Parameter        None
 **
 ** Returns:         True if sequence is triggered successfully
 **
 ******************************************************************************/
static bool nfa_scr_handle_stop_req() {
  bool status = true;
  switch (nfa_scr_cb.state) {
    case NFA_SCR_STATE_START_IN_PROGRESS:
      /* Set a flag to trigger SCR_STOP seq once SCR_START sequence is over */
      DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf(
          "%s: SCR will be stopped once start seq is over", __func__);
      nfa_scr_cb.stop_scr_mode = true;
      break;
    case NFA_SCR_STATE_START_SUCCESS: /* Trigger the SCR_STOP sequence here */
      DLOG_IF(INFO, nfc_debug_enabled)
          << StringPrintf("%s: Starting STOP sequence", __func__);
      nfa_sys_stop_timer(&nfa_scr_cb.scr_tle); /* Stop tag_op_timeout timer */
      status = nfa_scr_trigger_stop_seq();
      break;
    case NFA_SCR_STATE_START_CONFIG: /* Send only Start Discovery command */
      nfa_scr_cb.state = NFA_SCR_STATE_STOP_SUCCESS;
      nfa_scr_cb.sub_state = NFA_SCR_SUBSTATE_WAIT_DISC_MAP_RSP;
      [[fallthrough]];
    case NFA_SCR_STATE_STOP_SUCCESS: /* Send only Start Discovery command */
      status = nfa_scr_start_polling(NFA_STATUS_OK);
      break;
    case NFA_SCR_STATE_STOP_CONFIG: /* Stop is already in progress, do nothing
                                     */
      [[fallthrough]];
    case NFA_SCR_STATE_STOP_IN_PROGRESS: /* Stop is in progress, do nothing */
      DLOG_IF(INFO, nfc_debug_enabled)
          << StringPrintf("%s: STOP_IN_PROGRESS", __func__);
      break;
    default:
      nfa_scr_error_handler(
          NFA_SCR_ERROR_STATE_MISMATCH); /* Shall never meet this condition */
      status = false;
      break;
  }
  return status;
}
/*******************************************************************************
 **
 ** Function:        nfa_scr_proc_secure_rdr_req
 **
 ** Description:     This function shall called either by application to request
 **                  SCR_START or once 610A is received for remove request,
 **
 ** Returns:         True if request handled successfully else false
 **
 ******************************************************************************/
static bool nfa_scr_proc_secure_rdr_req(uint8_t event) {
  bool status = false;
  DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf("%s: enter event = %u", __func__, event);
  switch (event) {
    case NFA_SCR_APP_REQ_EVT:
      if (IS_SCR_IDLE) {
        nfa_scr_cb.state = NFA_SCR_STATE_START_CONFIG;
        nfa_scr_cb.sub_state = NFA_SCR_SUBSTATE_WAIT_DEACTIVATE_RSP;
        memset(&ntf_timeout_cnt, 0x00, sizeof(uint32_t));
        /* Notify the NFA_ScrSetReaderMode(true) success to JNI */
        status = true;
      } else {
        DLOG_IF(INFO, nfc_debug_enabled)
            << StringPrintf("%s:SCR is On", __func__);
      }
      nfa_scr_notify_evt((uint8_t)NFA_SCR_SET_READER_MODE_EVT, NFA_STATUS_OK);
      break;
    case NFA_SCR_STOP_REQ_EVT:
      status = nfa_scr_handle_stop_req();
      break;
    default:
      DLOG_IF(ERROR, nfc_debug_enabled)
          << StringPrintf("%s: Invalid reader mode %u", __func__, event);
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
void nfa_scr_send_prop_set_conf_cb(uint8_t event, uint16_t param_len,
                                   uint8_t* p_param) {
  (void)event;

  if (p_param) {
    if (nfa_scr_cb.scr_evt_cback != nullptr) {
      if (nfa_scr_cb.scr_evt_cback(NFA_SCR_CORE_SET_CONF_RSP_EVT, p_param[3])) {
        return;
      }
    }
  }
  DLOG_IF(ERROR, nfc_debug_enabled) << StringPrintf(
      "%s: param_len=%u, p_param=%p", __func__, param_len, p_param);
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
static std::vector<uint8_t> nfa_scr_get_prop_set_conf_cmd(bool set) {
  std::vector<uint8_t> cmd_buf;
  if (NfcConfig::hasKey(NAME_NXP_PROP_RESET_EMVCO_CMD)) {
    cmd_buf = NfcConfig::getBytes(NAME_NXP_PROP_RESET_EMVCO_CMD);
  }
  if (cmd_buf.size() != 0x08) {
    DLOG_IF(ERROR, nfc_debug_enabled)
        << StringPrintf("%s: Prop set conf is not provided", __func__);
    nfa_scr_error_handler(NFA_SCR_ERROR);
  } else if (set) {
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
 ** Returns:         Non
 **
 *******************************************************************************/
static void nfa_scr_send_prop_set_conf(bool set) {
  tNFA_STATUS stat = NFA_STATUS_FAILED;
  std::vector<uint8_t> cmd_buf;
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf(
      "%s: enter status = %s", __func__, (set ? "Set" : "Reset"));

  cmd_buf = nfa_scr_get_prop_set_conf_cmd(set);
  if (cmd_buf.size() != 0x08) {
    return;
  }

  NFA_SetEmvCoState(set);
  stat = NFA_SendRawVsCommand(cmd_buf.size(), (uint8_t*)&cmd_buf[0],
                              nfa_scr_send_prop_set_conf_cb);

  if (stat == NFA_STATUS_OK) {
    LOG(INFO) << StringPrintf("%s: Success NFA_SendRawVsCommand", __func__);
    nfa_scr_cb.sub_state = NFA_SCR_SUBSTATE_WAIT_PROP_SET_CONF_RSP;
  } else {
    DLOG_IF(ERROR, nfc_debug_enabled)
        << StringPrintf("%s: Failed NFA_SendRawVsCommand", __func__);
    nfa_scr_error_handler(NFA_SCR_ERROR);
  }
}
/*******************************************************************************
 **
 ** Function:        nfa_scr_handle_deact_rsp_ntf
 **
 ** Description:     This API, based on the state, either will send a prop set
 **                  config or will start a timer to receive RF_DEACTIVATE_NTF &
 **                  Notify app of REMOVE_CARD_NTF periodically.
 **
 ** Returns:         True if response is expected and SET_CONF sends
 *successfully
 **                  otherwise false
 **
 *******************************************************************************/
static bool nfa_scr_handle_deact_rsp_ntf(uint8_t status) {
  bool is_expected = true;
  DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf("%s: enter status = %u", __func__, status);

  switch (nfa_scr_cb.state) {
    case NFA_SCR_STATE_START_IN_PROGRESS: {
      if (nfa_scr_cb.sub_state == NFA_SCR_SUBSTATE_WAIT_DEACTIVATE_RSP) {
        DLOG_IF(INFO, nfc_debug_enabled)
            << StringPrintf("EMV-CO polling profile");
        is_expected = true;
        nfa_scr_send_prop_set_conf(true); /*EMV-CO Poll*/
      }
      break;
    }
    case NFA_SCR_STATE_STOP_CONFIG: {
      switch (nfa_scr_cb.sub_state) {
        case NFA_SCR_SUBSTATE_WAIT_DEACTIVATE_RSP: {
          IS_STATUS_ERROR(status);
          if (nfa_scr_cb.wait_for_deact_ntf == true) {
            is_expected = true;
            nfa_scr_cb.sub_state = NFA_SCR_SUBSTATE_WAIT_DEACTIVATE_NTF;
            /* Start timer to post NFA_SCR_REMOVE_CARD_EVT */
            nfa_sys_start_timer(&nfa_scr_cb.scr_tle,
                                NFA_SCR_RM_CARD_TIMEOUT_EVT,
                                NFA_SCR_CARD_REMOVE_TIMEOUT);
            DLOG_IF(INFO, nfc_debug_enabled)
                << StringPrintf("Waiting for STOP disc ntf");
            break;
          }
          [[fallthrough]];
        }
        case NFA_SCR_SUBSTATE_WAIT_DEACTIVATE_NTF: {
          is_expected = true;
          if (nfa_scr_cb.wait_for_deact_ntf == true) {
            nfa_scr_cb.wait_for_deact_ntf = false;
            /* Stop the NFA_SCR_REMOVE_CARD_EVT posting timer */
            nfa_sys_stop_timer(&nfa_scr_cb.scr_tle);
          }
          nfa_scr_cb.state = NFA_SCR_STATE_STOP_IN_PROGRESS;
          nfa_scr_send_prop_set_conf(false); /* Nfc-Forum Poll */
          DLOG_IF(INFO, nfc_debug_enabled)
              << StringPrintf("NFC forum polling profile");
          break;
        }
      }
      break;
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
void nfa_scr_discovermap_cb(tNFC_DISCOVER_EVT event, tNFC_DISCOVER* p_data) {
  switch (event) {
    case NFC_MAP_DEVT:
      if (nfa_scr_cb.scr_evt_cback != nullptr && IS_DISC_MAP_RSP_EXPECTED) {
        if (nfa_scr_cb.scr_evt_cback(NFA_SCR_RF_DISCOVER_MAP_RSP_EVT,
                                     p_data->status)) {
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
  const tNCI_DISCOVER_MAPS
      nfc_interface_mapping_ese[NFC_SWP_RD_NUM_INTERFACE_MAP] = {
          /* Protocols that use Frame Interface do not need to be included in
             the interface mapping */
          {NCI_PROTOCOL_ISO_DEP, NCI_INTERFACE_MODE_POLL,
           NCI_INTERFACE_ESE_DIRECT}};

  const tNCI_DISCOVER_MAPS
      nfc_interface_mapping_default[NFC_NUM_INTERFACE_MAP] = {
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
        {NCI_PROTOCOL_T3T, NCI_INTERFACE_MODE_LISTEN, NCI_INTERFACE_FRAME}
      };

  DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf("%s: enter status=%u", __func__, status);

  if (NFA_SCR_SUBSTATE_WAIT_PROP_SET_CONF_RSP == nfa_scr_cb.sub_state) {
    switch (nfa_scr_cb.state) {
      case NFA_SCR_STATE_START_IN_PROGRESS:
        IS_STATUS_ERROR(status);
        DLOG_IF(INFO, nfc_debug_enabled)
            << StringPrintf("%s: mapping intf to ESE", __func__);
        p_intf_mapping = (tNCI_DISCOVER_MAPS*)nfc_interface_mapping_ese;
        is_expected = true;
        break;
      case NFA_SCR_STATE_STOP_IN_PROGRESS:
        IS_STATUS_ERROR(status);
        DLOG_IF(INFO, nfc_debug_enabled)
            << StringPrintf("%s: mapping intf to default", __func__);
        p_intf_mapping = (tNCI_DISCOVER_MAPS*)nfc_interface_mapping_default;
        is_expected = true;
        break;
    }
  }

  if (is_expected == true) {
    (void)NFC_DiscoveryMap(NFC_SWP_RD_NUM_INTERFACE_MAP, p_intf_mapping,
                           nfa_scr_discovermap_cb);
    nfa_scr_cb.sub_state = NFA_SCR_SUBSTATE_WAIT_DISC_MAP_RSP;
  }
  return is_expected;
}

/*******************************************************************************
 **
 ** Function:        nfa_scr_start_polling
 **
 ** Description:     if SCR mode start is in progress, start discovery
 **                  if SCR mode stop is in progress, notify
 *NFA_SCR_RDR_STOP_SUCCESS
 **                  to JNI so that it can stop emvco poll and proceed for
 *NFCForum poll
 ** Returns:         true
 **
 *******************************************************************************/
static bool nfa_scr_start_polling(uint8_t status) {
  bool is_expected = true;
  ;
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf(
      "%s: enter status=%u, "
      "state=%u substate=%u",
      __func__, status, nfa_scr_cb.state, nfa_scr_cb.sub_state);
  if (nfa_scr_cb.sub_state == NFA_SCR_SUBSTATE_WAIT_DISC_MAP_RSP) {
    switch (nfa_scr_cb.state) {
      case NFA_SCR_STATE_STOP_IN_PROGRESS:
        IS_STATUS_ERROR(status);
        nfa_scr_cb.state = NFA_SCR_STATE_STOP_SUCCESS;
        /* Notify app of NFA_SCR_RDR_STOP_SUCCESS */
        nfa_scr_notify_evt(NFA_SCR_STOP_SUCCESS_EVT);
        if (!nfa_scr_cb.start_nfcforum_poll) {
          break; /* Don't send RF_DISCOVERY_CMD */
        }
        nfa_scr_cb.start_nfcforum_poll = false;
        [[fallthrough]]; /* App has requested to continue with RF_DISCOVERY_CMD
                          */
      case NFA_SCR_STATE_STOP_SUCCESS:
        /* 1. SCR module has proceeded with NFA_SCR_STOP_REQ_EVT because of
         * received
         * RF_NFCEE_DISCOVERY_REQ_NTF followed by app's request to stop the SCR
         */
        [[fallthrough]];
      case NFA_SCR_STATE_START_IN_PROGRESS:
        if (nfa_scr_cb.state != NFA_SCR_STATE_STOP_IN_PROGRESS) {
          IS_STATUS_ERROR(status);
        }
        (void)NFA_StartRfDiscovery();
        nfa_scr_cb.sub_state = NFA_SCR_SUBSTATE_WAIT_POLL_RSP;
        break;
      default:
        is_expected = false;
        break;
    }
  } else {
    is_expected = false;
  }

  return is_expected;
}

static bool nfa_scr_handle_act_ntf(uint8_t status) {
  (void)status;
  bool is_expected = false;
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: enter", __func__);
  if (nfa_scr_cb.state == NFA_SCR_STATE_START_SUCCESS &&
      nfa_scr_cb.sub_state == NFA_SCR_SUBSTATE_WAIT_ACTIVETED_NTF) {
    is_expected = true;
    nfa_scr_cb.wait_for_deact_ntf = true;
    nfa_sys_stop_timer(&nfa_scr_cb.scr_tle); /* Stop tag_op_timeout timer */
    nfa_scr_notify_evt((uint8_t)NFA_SCR_TARGET_ACTIVATED_EVT, NFA_STATUS_OK);
  }
  return is_expected;
}

static bool nfa_scr_rdr_processed(uint8_t status) {
  bool is_expected = false;
  DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf("%s: enter status=%u", __func__, status);
  if (nfa_scr_cb.sub_state == NFA_SCR_SUBSTATE_WAIT_POLL_RSP) {
    switch (nfa_scr_cb.state) {
      case NFA_SCR_STATE_STOP_SUCCESS:
        IS_STATUS_ERROR(status);
        is_expected = true;
        nfa_scr_cb.state = NFA_SCR_STATE_STOPPED;
        nfa_scr_cb.sub_state = NFA_SCR_SUBSTATE_INVALID;
        /* Notify the NFA_ScrSetReaderMode(false) success to JNI */
        nfa_scr_notify_evt((uint8_t)NFA_SCR_SET_READER_MODE_EVT, status);
        /* clear JNI and SCR callbacks pointers */
        nfa_scr_cb.scr_cback = nullptr;
        nfa_scr_cb.scr_evt_cback = nullptr;
        break;
      case NFA_SCR_STATE_START_IN_PROGRESS:
        nfa_scr_cb.state = NFA_SCR_STATE_START_SUCCESS;
        nfa_scr_cb.sub_state = NFA_SCR_SUBSTATE_WAIT_ACTIVETED_NTF;
        /* Notify app of NFA_SCR_RDR_START_SUCCESS */
        nfa_scr_notify_evt(NFA_SCR_START_SUCCESS_EVT);
        nfa_sys_stop_timer(&nfa_scr_cb.scr_tle);
        /* Start tag_op_timeout timer */
        nfa_sys_start_timer(
            &nfa_scr_cb.scr_tle, NFA_SCR_TAG_OP_TIMEOUT_EVT,
            nfa_scr_cb.tag_op_timeout * NFA_SCR_CARD_REMOVE_TIMEOUT);
        is_expected = true;
        if (nfa_scr_cb.stop_scr_mode) { /* Trigger STOP sequence here*/
          nfa_scr_cb.stop_scr_mode = false;
          if (nfa_scr_handle_stop_req() != true) {
            DLOG_IF(ERROR, nfc_debug_enabled) << StringPrintf(
                "%s: Failed to start the SCR_STOP seq", __func__);
          }
        }
        break;
    }
  }
  return is_expected;
}
