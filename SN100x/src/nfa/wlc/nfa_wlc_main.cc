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
 ******************************************************************************/
#if (NXP_EXTNS == TRUE)
#include <android-base/stringprintf.h>
#include <base/logging.h>
#include <nfa_wlc_int.h>
using android::base::StringPrintf;

extern bool nfc_debug_enabled;
/*Local static function declarations*/
static bool nfa_wlc_handle_event(NFC_HDR* p_msg);
static void wlcCallback(uint16_t dmEvent, uint8_t status);
static bool isWlcActivation(tNFA_DM_DISC_TECH_PROTO_MASK& disc_mask,
                            tNFC_RF_TECH_N_MODE tech_n_mode,
                            tNFC_PROTOCOL protocol);
static void nfa_wlc_sys_enable(void) {}
static void nfa_wlc_sys_disable(void) {}

const tNFA_SYS_REG nfa_wlc_sys_reg = {nfa_wlc_sys_enable, nfa_wlc_handle_event,
                                      nfa_wlc_sys_disable, NULL};

/* NFA_WLC actions */
const tNFA_WLC_ACTION nfa_wlc_action_tbl[] = {
    WlcHandleOpReq, /* NFA_WLC_OP_REQUEST_EVT            */
};

bool nfa_wlc_handle_event(NFC_HDR* p_msg) {
  uint16_t act_idx;

  /* Get NFA_WLC sub-event */
  if ((act_idx = (p_msg->event & 0x00FF)) < (NFA_WLC_MAX_EVT & 0xFF)) {
    return (*nfa_wlc_action_tbl[act_idx])((tNFA_WLC_MSG*)p_msg);
  } else {
    DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf(
        "nfa_wlc_handle_event: unhandled event 0x%02X", p_msg->event);
  }
  return true;
}

/*******************************************************************************
**
** Function         registerDmCallback
**
** Description      Registers/Deregisters WLC specific data with DM
**
** Returns          none
**
*******************************************************************************/
void registerWlcCallbackToDm() {
  wlc_cb.wlcData.p_wlc_cback = wlcCallback;
  wlc_cb.wlcData.p_is_wlc_activated = isWlcActivation;
  nfa_dm_update_wlc_data(&wlc_cb.wlcData);
}

/*******************************************************************************
**
** Function         wlcCallback
**
** Description      callback for WLC specific DM events
**
** Returns          none
**
*******************************************************************************/
static void wlcCallback(uint16_t dmEvent, uint8_t status) {
  DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf("%s dmEvent:0x%x", __func__, dmEvent);
  tNFA_CONN_EVT_DATA eventData;
  switch (dmEvent) {
    case NFC_WLC_FEATURE_SUPPORTED_REVT:
      eventData.status = status;
      (*wlc_cb.p_wlc_evt_cback)(WLC_FEATURE_SUPPORTED_EVT, &eventData);
      break;
    case NFC_RF_INTF_EXT_START_REVT:
      if (status == NFC_STATUS_OK) wlc_cb.isRfIntfExtStarted = true;
      eventData.status = status;
      (*wlc_cb.p_wlc_evt_cback)(WLC_RF_INTF_EXT_START_EVT, &eventData);
      break;
    case NFC_RF_INTF_EXT_STOP_REVT:
      if (status == NFC_STATUS_OK) wlc_cb.isRfIntfExtStarted = false;
      eventData.status = status;
      (*wlc_cb.p_wlc_evt_cback)(WLC_RF_INTF_EXT_STOP_EVT, &eventData);
      break;
    default:
      DLOG_IF(INFO, nfc_debug_enabled)
          << StringPrintf("%s Unknown Event!!", __func__);
      break;
  }
}

/*******************************************************************************
**
** Function         isWlcActivation
**
** Description      Checks if WLC listener(TAG) detected
**
** Returns          true if WLC tag found else false
**
*******************************************************************************/
static bool isWlcActivation(tNFA_DM_DISC_TECH_PROTO_MASK& disc_mask,
                            tNFC_RF_TECH_N_MODE tech_n_mode,
                            tNFC_PROTOCOL protocol) {
  if (NFC_DISCOVERY_TYPE_POLL_WLC == tech_n_mode) {
    switch (protocol) {
      case NFC_PROTOCOL_T1T:
        disc_mask = NFA_DM_DISC_MASK_PA_T1T;
        break;
      case NFC_PROTOCOL_T2T:
        disc_mask = NFA_DM_DISC_MASK_PA_T2T;
        break;
      case NFC_PROTOCOL_ISO_DEP:
        disc_mask = NFA_DM_DISC_MASK_PA_ISO_DEP;
        break;
      case NFC_PROTOCOL_NFC_DEP:
        disc_mask = NFA_DM_DISC_MASK_PA_NFC_DEP;
        break;
      case NFC_PROTOCOL_T3BT:
        disc_mask = NFA_DM_DISC_MASK_PB_T3BT;
        break;
      case NFC_PROTOCOL_T3T:
        disc_mask = NFA_DM_DISC_MASK_PF_T3T;
        break;
      default:
        return false;
    }
    DLOG_IF(INFO, nfc_debug_enabled)
        << StringPrintf("tech_n_mode:0x%X, protocol:0x%X, disc_mask:0x%X",
                        tech_n_mode, protocol, disc_mask);
    tNFA_CONN_EVT_DATA eventData;
    eventData.status = NFC_STATUS_OK;
    (*wlc_cb.p_wlc_evt_cback)(WLC_TAG_DETECTED_EVT, &eventData);
    return true;
  }
  return false;
}

#endif
