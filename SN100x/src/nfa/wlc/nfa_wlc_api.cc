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
#include <nfa_wlc_api.h>

using android::base::StringPrintf;

extern bool nfc_debug_enabled;
wlc_t wlc_cb;

/*******************************************************************************
**
** Function         NFA_WlcUpdateDiscovery
**
** Description      Updates discovery parameters which will reflect in following
**                  start RF discovery command
**
** Returns          NFA_STATUS_OK if successful
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_WlcInitSubsystem(tNFA_WLC_EVENT_CBACK* p_cback) {
  nfa_sys_register(NFA_ID_WLC, &nfa_wlc_sys_reg);
   tNFA_WLC_OPERATION* p_msg;
  if ((p_msg = (tNFA_WLC_OPERATION*)GKI_getbuf(
           (uint16_t)(sizeof(tNFA_WLC_OPERATION)))) != NULL) {
    p_msg->hdr.event = NFA_WLC_OP_REQUEST_EVT;
    p_msg->op = NFA_WLC_OP_INIT;
    p_msg->param.p_cback = p_cback;
    nfa_sys_sendmsg(p_msg);
    return NFA_STATUS_OK;
  }
  return NFA_STATUS_FAILED;
}

/*******************************************************************************
**
** Function         NFA_WlcDeInitSubsystem
**
** Description      remove attached subsystem
**
** Returns          none
**
*******************************************************************************/
void NFA_WlcDeInitSubsystem() {
  wlc_cb.p_wlc_evt_cback = nullptr;
  nfa_dm_update_wlc_data(nullptr);
  memset(&wlc_cb.wlcData, 0, sizeof(wlc_cb.wlcData));
  nfa_sys_deregister(NFA_ID_WLC);
}

/*******************************************************************************
**
** Function         NFA_WlcUpdateDiscovery
**
** Description      Updates discovery parameters which will reflect in following
**                  start RF discovery command
**
** Returns          NFA_STATUS_OK if successful
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_WlcUpdateDiscovery(tNFA_WLC_DISC_REQ requestOperation) {
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s", __func__);
  tNFA_WLC_OPERATION* p_msg;
  if ((p_msg = (tNFA_WLC_OPERATION*)GKI_getbuf(
           (uint16_t)(sizeof(tNFA_WLC_OPERATION)))) != NULL) {
    p_msg->hdr.event = NFA_WLC_OP_REQUEST_EVT;
    p_msg->op = NFA_WLC_OP_UPDATE_DISCOVERY;
    p_msg->param.discReq = requestOperation;
    nfa_sys_sendmsg(p_msg);
    return NFA_STATUS_OK;
  }
  return NFA_STATUS_FAILED;
}

/*******************************************************************************
**
** Function         NFA_WlcRfIntfExtStart
**
** Description      This function is called to start RF Interface Extensions
**                  The RF_INTF_EXT_START_CMD SHALL NOT be sent in states other
**                  than RFST_POLL_ACTIVE and RFST_LISTEN_ACTIVE.
**                  When the Rf Interface Start is completed,
**                  WLC_RF_INTF_EXT_START_EVT is reported using
**                  tNFA_WLC_EVENT_CBACK
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
extern tNFA_STATUS NFA_WlcRfIntfExtStart(tNFA_RF_INTF_EXT_PARAMS* p_params) {
  DLOG_IF(INFO, nfc_debug_enabled) << __func__;
  if ((nfa_dm_cb.disc_cb.disc_state != NFA_DM_RFST_POLL_ACTIVE) &&
      (nfa_dm_cb.disc_cb.disc_state != NFA_DM_RFST_LISTEN_ACTIVE))
    return NFA_STATUS_FAILED;
  if (p_params == nullptr) return NFA_STATUS_FAILED;

  tNFA_WLC_OPERATION* p_msg;
  if ((p_msg = (tNFA_WLC_OPERATION*)GKI_getbuf(
           (uint16_t)(sizeof(tNFA_WLC_OPERATION)))) != nullptr) {
    p_msg->hdr.event = NFA_WLC_OP_REQUEST_EVT;
    p_msg->op = NFA_WLC_OP_RF_INTF_EXT_START;
    memcpy(&p_msg->param.rfIntfExt, p_params, sizeof(tNFA_RF_INTF_EXT_PARAMS));

    nfa_sys_sendmsg(p_msg);

    return (NFA_STATUS_OK);
  }

  return (NFA_STATUS_FAILED);
}

/*******************************************************************************
**
** Function         NFA_WlcRfIntfExtStop
**
** Description      This function is called to stop RF Interface Extensions.
**                  The RF_INTF_EXT_STOP_CMD SHALL NOT be sent unless the
**                  RF Interface Extension is started
**                  When the Rf Interface Stop is completed,
**                  WLC_RF_INTF_EXT_STOP_EVT is reported using
**                  tNFA_WLC_EVENT_CBACK
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
extern tNFA_STATUS NFA_WlcRfIntfExtStop(tNFA_RF_INTF_EXT_PARAMS* p_params) {
  DLOG_IF(INFO, nfc_debug_enabled) << __func__;
  if (!wlc_cb.isRfIntfExtStarted) return NFA_STATUS_FAILED;
  if (p_params == nullptr) return NFA_STATUS_FAILED;
  tNFA_WLC_OPERATION* p_msg;
  if ((p_msg = (tNFA_WLC_OPERATION*)GKI_getbuf(
           (uint16_t)(sizeof(tNFA_WLC_OPERATION)))) != nullptr) {
    p_msg->hdr.event = NFA_WLC_OP_REQUEST_EVT;
    p_msg->op = NFA_WLC_OP_RF_INTF_EXT_STOP;
    memcpy(&p_msg->param.rfIntfExt, p_params, sizeof(tNFA_RF_INTF_EXT_PARAMS));

    nfa_sys_sendmsg(p_msg);

    return (NFA_STATUS_OK);
  }

  return (NFA_STATUS_FAILED);
}
#endif