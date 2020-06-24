/******************************************************************************
 *
 *  Copyright 2020 NXP
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
 *This is the main implementation file for the Mifare Classic Reader over eSE.
 *
 *****************************************************************************/
#include <android-base/stringprintf.h>
#include <base/logging.h>
#include <string.h>
#include <vector>
#include "nfa_dm_int.h"
#include "nfa_rw_api.h"
#include "nfa_scr_int.h"

using android::base::StringPrintf;

#define EMVCO_PTOFILE_BYTE_POS 3

#define SET_STATE(new_state) (nfa_scr_cb.state = new_state)
#define SET_SUB_STATE(new_sub_state) (nfa_scr_cb.sub_state = new_sub_state)
#define IS_STATE(req_state) (nfa_scr_cb.state == req_state)
#define IS_SUB_STATE(req_sub_state) (nfa_scr_cb.sub_state == req_sub_state)

extern bool nfc_debug_enabled;

/* extern functions */
extern void nfa_scr_send_prop_get_conf_cb(uint8_t event, uint16_t param_len,
                                          uint8_t* p_param);
extern void nfa_scr_send_prop_set_conf_cb(uint8_t event, uint16_t param_len,
                                          uint8_t* p_param);
extern void nfa_scr_discovermap_cb(tNFC_DISCOVER_EVT event,
                                   tNFC_DISCOVER* p_data);
/*** Static Functions ***/
static bool nfa_mfc_handle_start_req(void);
static bool nfa_mfc_handle_get_conf_rsp(uint8_t status);
static bool nfa_mfc_send_discovermap_cmd(uint8_t status);
static bool nfa_mfc_handle_stop_req();
static bool nfa_mfc_handle_deact_rsp_ntf(uint8_t status);
static bool nfa_mfc_trigger_stop_seq();
/*******************************************************************************
**
** Function       nfa_mfc_cback
**
** Description    If app has requested for MFC reader over eSE,
**                CASE I: If the NTF(610A) has been received for Start/Stop
**                reader over eSE then EE module shall call this api with
**                NFA_SCR_START_REQ_EVT, NFA_SCR_STOP_REQ_EVT respectively.
**                2. If SCR module has been triggered and NFC_TASK has received
**                rsp for NFCC cmd, it shall be invoked with respective events.
**
** Return         True if event is processed/expected else false
**
*******************************************************************************/
bool nfa_mfc_cback(uint8_t event, uint8_t status) {
  bool stat = false;
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf(
      "%s: enter event=%u, status=%u state=%u,"
      "substate=%u",
      __func__, event, status, nfa_scr_cb.state, nfa_scr_cb.sub_state);
  if (!nfa_scr_is_evt_allowed(event)) {
    return stat;
  }

  switch (event) {
    case NFA_SCR_APP_START_REQ_EVT:
      stat = nfa_scr_proc_app_start_req();
      break;
    case NFA_SCR_START_REQ_EVT:
      stat = nfa_mfc_handle_start_req();
      break;
    case NFA_SCR_STOP_REQ_EVT:
      if (!IS_SCR_START_SUCCESS) {
        stat = true; /* 610A for reader mode shall be only be processed by SCR
                        module */
        break;
      }
      nfa_scr_cb.rdr_stop_req = true;
      [[fallthrough]];
    case NFA_SCR_APP_STOP_REQ_EVT:
      stat = nfa_mfc_handle_stop_req();
      break;
    case NFA_SCR_RF_DEACTIVATE_RSP_EVT:
      [[fallthrough]];
    case NFA_SCR_RF_DEACTIVATE_NTF_EVT:
      stat = nfa_mfc_handle_deact_rsp_ntf(status);
      break;
    case NFA_SCR_CORE_GET_CONF_RSP_EVT:
      stat = nfa_mfc_handle_get_conf_rsp(status);
      break;
    case NFA_SCR_CORE_SET_CONF_RSP_EVT:
      stat = nfa_mfc_send_discovermap_cmd(status);
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
      if (status == NXP_NFC_EMVCO_PCD_COLLISION_DETECTED) {
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
 ** Function:        nfa_mfc_handle_start_req
 **
 ** Description:     This function will trigger the MCR start sequence if app
 **                  has requested & 610A is received to start MFC reader.
 **
 ** Returns:         always true to consume the 610A for reader start request
 **
 ******************************************************************************/
static bool nfa_mfc_handle_start_req(void) {
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: enter", __func__);
  tNFA_STATUS status = NFA_STATUS_FAILED;
  vector<uint8_t> cmd_buf{0x20, 0x03, 0x03, 0x01, 0xA0, 0x44};

  if (IS_SCR_APP_REQESTED &&
      IS_SUB_STATE(NFA_SCR_SUBSTATE_WAIT_START_RDR_NTF)) {
    SET_STATE(NFA_SCR_STATE_START_IN_PROGRESS);
    status = NFA_SendRawVsCommand(cmd_buf.size(), cmd_buf.data(),
                                  nfa_scr_send_prop_get_conf_cb);
    if (NFA_STATUS_OK == status) {
      SET_SUB_STATE(NFA_SCR_SUBSTATE_WAIT_PROP_GET_CONF_RSP);
    } else {
      DLOG_IF(ERROR, nfc_debug_enabled)
          << StringPrintf("%s: Failed NFA_GetConfig", __func__);
      nfa_scr_error_handler(NFA_SCR_ERROR_SEND_CONF_CMD);
    }
  }
  return true;
}

/*******************************************************************************
 **
 ** Function:        nfa_mfc_send_prop_set_conf
 **
 ** Description:     Disable EmvCo polling profile
 **
 ** Returns:         Non
 **
 *******************************************************************************/
static void nfa_mfc_send_prop_set_conf() {
  tNFA_STATUS status = NFA_STATUS_FAILED;
  vector<uint8_t> cmd_buf{0x20, 0x02, 0x05, 0x01, 0xA0, 0x44, 0x01, 0x00};

  status = NFA_SendRawVsCommand(cmd_buf.size(), cmd_buf.data(),
                                nfa_scr_send_prop_set_conf_cb);
  if (NFA_STATUS_OK == status) {
    SET_SUB_STATE(NFA_SCR_SUBSTATE_WAIT_PROP_SET_CONF_RSP);
  } else {
    DLOG_IF(ERROR, nfc_debug_enabled)
        << StringPrintf("%s: Failed Set NFC Forum Profile", __func__);
    nfa_scr_error_handler(NFA_SCR_ERROR_SEND_CONF_CMD);
  }
}

/*******************************************************************************
 **
 ** Function:        nfa_mfc_handle_get_conf_rsp
 **
 ** Description:     This API, based on the state, either will send a prop set
 **                  config to Disable EMVCO polling mode or Discover Map cmd
 **                  for Mifare Classic reader
 **
 ** Returns:         True if response is expected and cmd is sent successfully
 **                  otherwise false
 **
 *******************************************************************************/
static bool nfa_mfc_handle_get_conf_rsp(uint8_t status) {
  bool is_expected = false;
  DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf("%s: enter status = %u", __func__, status);

  IS_STATUS_ERROR(status);
  if (IS_STATE(NFA_SCR_STATE_START_IN_PROGRESS) &&
      IS_SUB_STATE(NFA_SCR_SUBSTATE_WAIT_PROP_GET_CONF_RSP)) {
    is_expected = true;
    if (nfa_scr_cb.p_nfa_get_confg.param_tlvs[EMVCO_PTOFILE_BYTE_POS] != 0x00) {
      // send Set config for
      nfa_mfc_send_prop_set_conf();
    } else {
      // Send discover map to default
      (void)nfa_mfc_send_discovermap_cmd(status);
    }
  }
  return is_expected;
}

/*******************************************************************************
 **
 ** Function:        nfa_mfc_send_discovermap_cmd
 **
 ** Description:     Send discover map command based on the current state of
 *                   SCR module
 **
 ** Returns:         true
 **
 *******************************************************************************/
static bool nfa_mfc_send_discovermap_cmd(uint8_t status) {
  tNCI_DISCOVER_MAPS* p_intf_mapping = nullptr;
  uint8_t num_disc_maps = NFC_NUM_INTERFACE_MAP;
  const tNCI_DISCOVER_MAPS
      nfc_interface_mapping_ese_mfc[NFC_SWP_RD_NUM_INTERFACE_MAP] = {
          {NCI_PROTOCOL_MFC, NCI_INTERFACE_MODE_POLL,
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

  switch (nfa_scr_cb.state) {
    case NFA_SCR_STATE_START_IN_PROGRESS:
      if (IS_SUB_STATE(NFA_SCR_SUBSTATE_WAIT_PROP_SET_CONF_RSP) ||
          IS_SUB_STATE(NFA_SCR_SUBSTATE_WAIT_PROP_GET_CONF_RSP)) {
        IS_STATUS_ERROR(status);
        DLOG_IF(INFO, nfc_debug_enabled)
            << StringPrintf("%s: mapping intf to ESE", __func__);
        p_intf_mapping = (tNCI_DISCOVER_MAPS*)nfc_interface_mapping_ese_mfc;
        num_disc_maps = NFC_SWP_RD_NUM_INTERFACE_MAP;
        NFA_SetEmvCoState(true); /*Disable P2P prio logic */
      }
      break;
    case NFA_SCR_STATE_STOP_IN_PROGRESS:
      if (IS_SUB_STATE(NFA_SCR_SUBSTATE_WAIT_DEACTIVATE_NTF) ||
          IS_SUB_STATE(NFA_SCR_SUBSTATE_WAIT_DEACTIVATE_RSP)) {
        IS_STATUS_ERROR(status);
        DLOG_IF(INFO, nfc_debug_enabled)
            << StringPrintf("%s: mapping intf to default", __func__);
        p_intf_mapping = (tNCI_DISCOVER_MAPS*)nfc_interface_mapping_default;
        NFA_SetEmvCoState(false); /*Enable P2P prio logic */
      }
      break;
  }

  if (p_intf_mapping != nullptr) {
    (void)NFC_DiscoveryMap(num_disc_maps, p_intf_mapping,
                           nfa_scr_discovermap_cb);
    SET_SUB_STATE(NFA_SCR_SUBSTATE_WAIT_DISC_MAP_RSP);
    return true;
  }
  return false;
}

/*******************************************************************************
 **
 ** Function:        nfa_mfc_handle_stop_req
 **
 ** Description:     This method shall be called to handle STOP SCR request
 **
 ** Returns:         True if sequence is triggered successfully
 **
 ******************************************************************************/
static bool nfa_mfc_handle_stop_req() {
  bool status = true;
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: enter", __func__);

  switch (nfa_scr_cb.state) {
    case NFA_SCR_STATE_START_IN_PROGRESS:
      DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf(
          "%s: SCR will be stopped once start seq is over", __func__);
      break;
    case NFA_SCR_STATE_START_SUCCESS:
      nfa_sys_stop_timer(&nfa_scr_cb.scr_tle); /* Stop tag_op_timeout timer */
      status = nfa_mfc_trigger_stop_seq();
      break;
    case NFA_SCR_STATE_START_CONFIG:
      [[fallthrough]];
    case NFA_SCR_STATE_STOP_SUCCESS:
      status = nfa_scr_finalize();
      break;
    case NFA_SCR_STATE_STOP_CONFIG:
      if (nfa_scr_cb.wait_for_deact_ntf == false &&
              nfa_scr_cb.rdr_stop_req == false) {
        /* avoid concurrent scenario for App and 610A reader stop req */
        nfa_scr_cb.state = NFA_SCR_STATE_STOP_IN_PROGRESS;
        (void)nfa_mfc_send_discovermap_cmd(NFA_STATUS_OK);
      } else { /* Proceed with reader stop once RF_DEACTIVATE_NTF is received */
        DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf(
            "%s: Reader will be stopped once RF_DISC_NTF is processed", __func__);
      }
      break;
    case NFA_SCR_STATE_STOP_IN_PROGRESS:
      DLOG_IF(INFO, nfc_debug_enabled)
          << StringPrintf("%s: Stop is already in progress", __func__);
      break;
    default:
      status = false;
      break;
  }
  return status;
}

/*******************************************************************************
 **
 ** Function:        nfa_mfc_handle_deact_rsp_ntf
 **
 ** Description:     This API, based on the state, either will send a prop get
 **                  config or will start a timer to receive RF_DEACTIVATE_NTF &
 **                  Notify app of REMOVE_CARD_NTF periodically.
 **
 ** Returns:         True if response is expected and GET_CONF sends
 *successfully
 **                  otherwise false
 **
 *******************************************************************************/
static bool nfa_mfc_handle_deact_rsp_ntf(uint8_t status) {
  bool is_expected = false;
  DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf("%s: enter status = %u", __func__, status);
  if (IS_STATE(NFA_SCR_STATE_STOP_CONFIG)) {
    switch (nfa_scr_cb.sub_state) {
      case NFA_SCR_SUBSTATE_WAIT_DEACTIVATE_RSP: {
        IS_STATUS_ERROR(status);
        if (nfa_scr_cb.wait_for_deact_ntf == true) {
          is_expected = true;
          SET_SUB_STATE(NFA_SCR_SUBSTATE_WAIT_DEACTIVATE_NTF);
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
        }
        if (nfa_scr_cb.app_stop_req) {
          if (nfa_scr_cb.rdr_stop_req) {
            SET_STATE(NFA_SCR_STATE_STOP_IN_PROGRESS);
            (void)nfa_mfc_send_discovermap_cmd(NFA_STATUS_OK);
          } else {
            nfa_scr_cb.app_stop_req = false;
            nfa_scr_error_handler(NFA_SCR_ERROR_APP_STOP_REQ);
          }
        } else {
          nfa_scr_cb.rdr_stop_req = false;
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
 ** Function:        nfa_mfc_trigger_stop_seq
 **
 ** Description:     This method shall be called to start SCR_STOP sequence

 ** Returns:         True if sequence is triggered successfully
 **
 ******************************************************************************/
static bool nfa_mfc_trigger_stop_seq() {
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: Enter", __func__);
  nfa_sys_stop_timer(&nfa_scr_cb.scr_tle); /* Stop tag_op_timeout timer */
  bool is_expected = true;
  if (nfa_dm_cb.disc_cb.disc_state == NFA_DM_RFST_IDLE) {
    /* Simulate the cmd-rsp by changing the SCR's states as libnfc is already
     * in idle state. This can be NFA_SCR_ERROR_APP_STOP_REQ scenario hence,
     * to recover, configureSecureReader(false) shall be allowed from APP */
    nfa_scr_cb.state = NFA_SCR_STATE_STOP_CONFIG;
    nfa_scr_cb.sub_state = NFA_SCR_SUBSTATE_WAIT_DEACTIVATE_NTF;
    nfa_scr_cb.scr_evt_cback(NFA_SCR_RF_DEACTIVATE_NTF_EVT,NFA_STATUS_OK);
  } else if(NFA_STATUS_OK == NFA_StopRfDiscovery()) {
    nfa_scr_cb.state = NFA_SCR_STATE_STOP_CONFIG;
    nfa_scr_cb.sub_state = NFA_SCR_SUBSTATE_WAIT_DEACTIVATE_RSP;
  } else {
    is_expected = false;
  }
  return is_expected;
}
