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
#include <nfc_int.h>
#include <iostream>
#include <vector>
using android::base::StringPrintf;

extern bool nfc_debug_enabled;
tNFA_STATUS nfa_wlc_init(tNFA_WLC_EVENT_CBACK* p_cback);
tNFA_STATUS nfa_wlc_deInit();
tNFA_STATUS nfa_wlc_update_discovery_param(tNFA_WLC_DISC_REQ requestOperation);
tNFA_STATUS nfa_wlc_rf_intf_ext_start(tNFA_RF_INTF_EXT_PARAMS* startParam);
tNFA_STATUS nfa_wlc_rf_intf_ext_stop(tNFA_RF_INTF_EXT_PARAMS* stopParam);
/*******************************************************************************
**
** Function         WlcHandleOpReq
**
** Description      Handler for tNFA_WLC_OP request event, operation request
**
** Returns          true if caller should free p_data
**                  false if caller does not need to free p_data
**
*******************************************************************************/
bool WlcHandleOpReq(tNFA_WLC_MSG* p_data) {
  DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf("%s Request:%d", __func__, p_data->op_req.op);
  tNFA_STATUS status = NFA_STATUS_FAILED;
  switch (p_data->op_req.op) {
    case NFA_WLC_OP_INIT:
    status = nfa_wlc_init(p_data->op_req.param.p_cback);
      break;
    case NFA_WLC_OP_UPDATE_DISCOVERY:
      status = nfa_wlc_update_discovery_param(p_data->op_req.param.discReq);
      break;
    case NFA_WLC_OP_RF_INTF_EXT_START:
      status = nfa_wlc_rf_intf_ext_start(&p_data->op_req.param.rfIntfExt);
      break;
    case NFA_WLC_OP_RF_INTF_EXT_STOP:
      status = nfa_wlc_rf_intf_ext_stop(&p_data->op_req.param.rfIntfExt);
      break;
  }
  if (status != NFA_STATUS_OK) return false;
  return true;
}

/*******************************************************************************
**
** Function         nfa_wlc_init
**
** Description      Initializes WLC system specific information
**
** Returns          NFA_STATUS_OK (message buffer to be freed by caller)
**
*******************************************************************************/
tNFA_STATUS nfa_wlc_init(tNFA_WLC_EVENT_CBACK* p_cback) {
    wlc_cb.p_wlc_evt_cback = p_cback;
  registerWlcCallbackToDm();
  return NFA_STATUS_OK;
}

/*******************************************************************************
**
** Function         nfa_wlc_update_discovery_param
**
** Description      sets a flag, if enabled, while adding discovery parameters
**                  for discovery command, WLC Poll mode(0x73) will be included.
**
** Returns          NFA_STATUS_OK (message buffer to be freed by caller)
**
*******************************************************************************/
tNFA_STATUS nfa_wlc_update_discovery_param(tNFA_WLC_DISC_REQ requestOperation) {
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s ", __func__);
  nfc_cb.isWlcPollEnabled = (requestOperation == SET);
  return NFA_STATUS_OK;
}

/*******************************************************************************
**
** Function         nfa_dm_act_rf_intf_ext_start
**
** Description      Process RF interface extension start
**
** Returns          NFA_STATUS_OK (message buffer to be freed by caller)
**
*******************************************************************************/
tNFA_STATUS nfa_wlc_rf_intf_ext_start(tNFA_RF_INTF_EXT_PARAMS* startParam) {
  tNFA_STATUS status = NFA_STATUS_OK;
  status = NFC_RfIntfExtStart(startParam->rfIntfExtType, startParam->data,
                              startParam->dataLen);
  if (status != NFA_STATUS_OK) {
    tNFA_CONN_EVT_DATA conn_evt;
    conn_evt.status = NFA_STATUS_FAILED;
    (*wlc_cb.p_wlc_evt_cback)(WLC_RF_INTF_EXT_START_EVT, &conn_evt);
  }
  return NFA_STATUS_OK;
}

/*******************************************************************************
**
** Function         nfa_wlc_rf_intf_ext_stop
**
** Description      Process RF interface extension stop
**
** Returns          NFA_STATUS_OK (message buffer to be freed by caller)
**
*******************************************************************************/
tNFA_STATUS nfa_wlc_rf_intf_ext_stop(tNFA_RF_INTF_EXT_PARAMS* stopParam) {
  tNFA_STATUS status = NFA_STATUS_OK;
  status = NFC_RfIntfExtStop(stopParam->rfIntfExtType, stopParam->data,
                             stopParam->dataLen);
  if (status != NFA_STATUS_OK) {
    tNFA_CONN_EVT_DATA conn_evt;
    conn_evt.status = NFA_STATUS_FAILED;
    (*wlc_cb.p_wlc_evt_cback)(WLC_RF_INTF_EXT_STOP_EVT, &conn_evt);
  }
  return NFA_STATUS_OK;
}

#endif