/******************************************************************************
 *
 *  Copyright (C) 2010-2014 Broadcom Corporation
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
 *  Copyright 2018-2024 NXP
 *
 ******************************************************************************/
/******************************************************************************
 *
 *  This file contains the action functions for the NFA HCI.
 *
 ******************************************************************************/
#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <log/log.h>
#include <string.h>

#include "nfa_dm_int.h"
#include "nfa_hci_api.h"
#include "nfa_hci_defs.h"
#include "nfa_hci_int.h"
#if (NXP_EXTNS == TRUE)
#include "nfa_ee_int.h"
#if(NXP_SRD == TRUE)
#include "nfa_srd_int.h"
#endif
#include "nfa_nv_co.h"
#include "nfa_scr_int.h"
#endif
using android::base::StringPrintf;

/* Static local functions       */
static void nfa_hci_api_register(tNFA_HCI_EVENT_DATA* p_evt_data);
static void nfa_hci_api_get_gate_pipe_list(tNFA_HCI_EVENT_DATA* p_evt_data);
static void nfa_hci_api_alloc_gate(tNFA_HCI_EVENT_DATA* p_evt_data);
static void nfa_hci_api_get_host_list(tNFA_HCI_EVENT_DATA* p_evt_data);
static bool nfa_hci_api_get_reg_value(tNFA_HCI_EVENT_DATA* p_evt_data);
static bool nfa_hci_api_set_reg_value(tNFA_HCI_EVENT_DATA* p_evt_data);
static bool nfa_hci_api_create_pipe(tNFA_HCI_EVENT_DATA* p_evt_data);
static void nfa_hci_api_open_pipe(tNFA_HCI_EVENT_DATA* p_evt_data);
static void nfa_hci_api_close_pipe(tNFA_HCI_EVENT_DATA* p_evt_data);
static void nfa_hci_api_delete_pipe(tNFA_HCI_EVENT_DATA* p_evt_data);
static bool nfa_hci_api_send_event(tNFA_HCI_EVENT_DATA* p_evt_data);
static bool nfa_hci_api_send_cmd(tNFA_HCI_EVENT_DATA* p_evt_data);
static void nfa_hci_api_send_rsp(tNFA_HCI_EVENT_DATA* p_evt_data);
static void nfa_hci_api_add_static_pipe(tNFA_HCI_EVENT_DATA* p_evt_data);

static void nfa_hci_handle_identity_mgmt_gate_pkt(uint8_t* p_data,
                                                  tNFA_HCI_DYN_PIPE* p_pipe);
static void nfa_hci_handle_loopback_gate_pkt(uint8_t* p_data, uint16_t data_len,
                                             tNFA_HCI_DYN_PIPE* p_pipe);
static void nfa_hci_handle_connectivity_gate_pkt(uint8_t* p_data,
                                                 uint16_t data_len,
                                                 tNFA_HCI_DYN_PIPE* p_pipe);
static void nfa_hci_handle_generic_gate_cmd(uint8_t* p_data, uint8_t data_len,
                                            tNFA_HCI_DYN_PIPE* p_pipe);
static void nfa_hci_handle_generic_gate_rsp(uint8_t* p_data, uint8_t data_len,
                                            tNFA_HCI_DYN_PIPE* p_pipe);
static void nfa_hci_handle_generic_gate_evt(uint8_t* p_data, uint16_t data_len,
                                            tNFA_HCI_DYN_GATE* p_gate,
                                            tNFA_HCI_DYN_PIPE* p_pipe);

#if(NXP_EXTNS == TRUE)
static void nfa_hci_handle_identity_mgmt_app_gate_hcp_msg_data (uint8_t *p_data, uint16_t data_len,
                                                                  tNFA_HCI_DYN_PIPE *p_pipe);
static void nfa_hci_handle_apdu_app_gate_hcp_msg_data (uint8_t *p_data, uint16_t data_len,
                                                       tNFA_HCI_DYN_PIPE *p_pipe);
static bool nfa_hci_api_send_apdu (tNFA_HCI_EVENT_DATA *p_evt_data);
static bool nfa_hci_api_abort_apdu (tNFA_HCI_EVENT_DATA *p_evt_data);
static void nfa_hci_handle_clear_all_pipe_cmd(uint8_t source_host);
static void nfa_hci_api_add_prop_host_info(uint8_t propHostIndx);
static void nfa_hci_get_pipe_state_cb(uint8_t event, uint16_t param_len, uint8_t* p_param);
static void nfa_hci_update_pipe_status(uint8_t local_gateId,
                                       uint8_t dest_gateId, uint8_t pipeId,
                                       uint8_t hostId);
static bool nfa_hci_check_nfcee_init_complete(uint8_t host_id);
static void nfa_hci_notify_w4_atr_timeout();
#endif
/*******************************************************************************
**
** Function         nfa_hci_check_pending_api_requests
**
** Description      This function handles pending API requests
**
** Returns          none
**
*******************************************************************************/
void nfa_hci_check_pending_api_requests(void) {
  NFC_HDR* p_msg;
  tNFA_HCI_EVENT_DATA* p_evt_data;
  bool b_free;

  /* If busy, or API queue is empty, then exit */
  if ((nfa_hci_cb.hci_state != NFA_HCI_STATE_IDLE) ||
      ((p_msg = (NFC_HDR*)GKI_dequeue(&nfa_hci_cb.hci_host_reset_api_q)) ==
       nullptr))
    return;

  /* Process API request */
  p_evt_data = (tNFA_HCI_EVENT_DATA*)p_msg;

  /* Save the application handle */
  nfa_hci_cb.app_in_use = p_evt_data->comm.hci_handle;

  b_free = true;
  switch (p_msg->event) {
    case NFA_HCI_API_CREATE_PIPE_EVT:
      if (nfa_hci_api_create_pipe(p_evt_data) == false) b_free = false;
      break;

    case NFA_HCI_API_GET_REGISTRY_EVT:
      if (nfa_hci_api_get_reg_value(p_evt_data) == false) b_free = false;
      break;

    case NFA_HCI_API_SET_REGISTRY_EVT:
      if (nfa_hci_api_set_reg_value(p_evt_data) == false) b_free = false;
      break;

    case NFA_HCI_API_SEND_CMD_EVT:
      if (nfa_hci_api_send_cmd(p_evt_data) == false) b_free = false;
      break;
    case NFA_HCI_API_SEND_EVENT_EVT:
      if (nfa_hci_api_send_event(p_evt_data) == false) b_free = false;
      break;
#if(NXP_EXTNS == TRUE)
    case NFA_HCI_API_SEND_APDU_EVT:
      if (nfa_hci_api_send_apdu (p_evt_data) == false) b_free = false;
      break;

    case NFA_HCI_API_ABORT_APDU_EVT:
      if (nfa_hci_api_abort_apdu (p_evt_data) == false) b_free = false;
      break;
#endif
  }

  if (b_free) GKI_freebuf(p_msg);
}

/*******************************************************************************
**
** Function         nfa_hci_check_api_requests
**
** Description      This function handles API requests
**
** Returns          none
**
*******************************************************************************/
void nfa_hci_check_api_requests(void) {
  NFC_HDR* p_msg;
  tNFA_HCI_EVENT_DATA* p_evt_data;

  for (;;) {
    /* If busy, or API queue is empty, then exit */
    if ((nfa_hci_cb.hci_state != NFA_HCI_STATE_IDLE) ||
        ((p_msg = (NFC_HDR*)GKI_dequeue(&nfa_hci_cb.hci_api_q)) == nullptr))
      break;

    /* Process API request */
    p_evt_data = (tNFA_HCI_EVENT_DATA*)p_msg;

    /* Save the application handle */
    nfa_hci_cb.app_in_use = p_evt_data->comm.hci_handle;

    switch (p_msg->event) {
      case NFA_HCI_API_REGISTER_APP_EVT:
        nfa_hci_api_register(p_evt_data);
        break;

      case NFA_HCI_API_DEREGISTER_APP_EVT:
        nfa_hci_api_deregister(p_evt_data);
        break;

      case NFA_HCI_API_GET_APP_GATE_PIPE_EVT:
        nfa_hci_api_get_gate_pipe_list(p_evt_data);
        break;

      case NFA_HCI_API_ALLOC_GATE_EVT:
        nfa_hci_api_alloc_gate(p_evt_data);
        break;

      case NFA_HCI_API_DEALLOC_GATE_EVT:
        nfa_hci_api_dealloc_gate(p_evt_data);
        break;

      case NFA_HCI_API_GET_HOST_LIST_EVT:
        nfa_hci_api_get_host_list(p_evt_data);
        break;

      case NFA_HCI_API_GET_REGISTRY_EVT:
        if (nfa_hci_api_get_reg_value(p_evt_data) == false) continue;
        break;

      case NFA_HCI_API_SET_REGISTRY_EVT:
        if (nfa_hci_api_set_reg_value(p_evt_data) == false) continue;
        break;

      case NFA_HCI_API_CREATE_PIPE_EVT:
        if (nfa_hci_api_create_pipe(p_evt_data) == false) continue;
        break;

      case NFA_HCI_API_OPEN_PIPE_EVT:
        nfa_hci_api_open_pipe(p_evt_data);
        break;

      case NFA_HCI_API_CLOSE_PIPE_EVT:
        nfa_hci_api_close_pipe(p_evt_data);
        break;

      case NFA_HCI_API_DELETE_PIPE_EVT:
        nfa_hci_api_delete_pipe(p_evt_data);
        break;

      case NFA_HCI_API_SEND_CMD_EVT:
        if (nfa_hci_api_send_cmd(p_evt_data) == false) continue;
        break;

      case NFA_HCI_API_SEND_RSP_EVT:
        nfa_hci_api_send_rsp(p_evt_data);
        break;

      case NFA_HCI_API_SEND_EVENT_EVT:
        if (nfa_hci_api_send_event(p_evt_data) == false) continue;
        break;

      case NFA_HCI_API_ADD_STATIC_PIPE_EVT:
        nfa_hci_api_add_static_pipe(p_evt_data);
        break;
#if(NXP_EXTNS == TRUE)
      case NFA_HCI_API_SEND_APDU_EVT:
        if (nfa_hci_api_send_apdu (p_evt_data) == false) continue;
          break;

      case NFA_HCI_API_ABORT_APDU_EVT:
        if (nfa_hci_api_abort_apdu (p_evt_data) == false) continue;
          break;
#endif
      default:
        LOG(ERROR) << StringPrintf("Unknown event: 0x%04x", p_msg->event);
        break;
    }

    GKI_freebuf(p_msg);
  }
}

/*******************************************************************************
**
** Function         nfa_hci_api_register
**
** Description      action function to register the events for the given AID
**
** Returns          None
**
*******************************************************************************/
static void nfa_hci_api_register(tNFA_HCI_EVENT_DATA* p_evt_data) {
  tNFA_HCI_EVT_DATA evt_data;
  char* p_app_name = p_evt_data->app_info.app_name;
  tNFA_HCI_CBACK* p_cback = p_evt_data->app_info.p_cback;
  int xx, yy;
  uint8_t num_gates = 0, num_pipes = 0;
  tNFA_HCI_DYN_GATE* pg = nfa_hci_cb.cfg.dyn_gates;

  /* First, see if the application was already registered */
  for (xx = 0; xx < NFA_HCI_MAX_APP_CB; xx++) {
    if ((nfa_hci_cb.cfg.reg_app_names[xx][0] != 0) &&
        !strncmp(p_app_name, &nfa_hci_cb.cfg.reg_app_names[xx][0],
                 strlen(p_app_name))) {
      LOG(DEBUG) << StringPrintf("nfa_hci_api_register (%s)  Reusing: %u",
                                 p_app_name, xx);
      break;
    }
  }

  if (xx != NFA_HCI_MAX_APP_CB) {
    nfa_hci_cb.app_in_use = (tNFA_HANDLE)(xx | NFA_HANDLE_GROUP_HCI);
    /* The app was registered, find the number of gates and pipes associated to
     * the app */

    for (yy = 0; yy < NFA_HCI_MAX_GATE_CB; yy++, pg++) {
      if (pg->gate_owner == nfa_hci_cb.app_in_use) {
        num_gates++;
        num_pipes += nfa_hciu_count_pipes_on_gate(pg);
      }
    }
  } else {
    /* Not registered, look for a free entry */
    for (xx = 0; xx < NFA_HCI_MAX_APP_CB; xx++) {
      if (nfa_hci_cb.cfg.reg_app_names[xx][0] == 0) {
        memset(&nfa_hci_cb.cfg.reg_app_names[xx][0], 0,
               sizeof(nfa_hci_cb.cfg.reg_app_names[xx]));
        strlcpy(&nfa_hci_cb.cfg.reg_app_names[xx][0], p_app_name,
                NFA_MAX_HCI_APP_NAME_LEN);
        nfa_hci_cb.nv_write_needed = true;
        LOG(DEBUG) << StringPrintf("nfa_hci_api_register (%s)  Allocated: %u",
                                   p_app_name, xx);
        break;
      }
    }

    if (xx == NFA_HCI_MAX_APP_CB) {
      LOG(ERROR) << StringPrintf("nfa_hci_api_register (%s)  NO ENTRIES",
                                 p_app_name);

      evt_data.hci_register.status = NFA_STATUS_FAILED;
      p_evt_data->app_info.p_cback(NFA_HCI_REGISTER_EVT, &evt_data);
      return;
    }
  }

  evt_data.hci_register.num_pipes = num_pipes;
  evt_data.hci_register.num_gates = num_gates;
  nfa_hci_cb.p_app_cback[xx] = p_cback;

  nfa_hci_cb.cfg.b_send_conn_evts[xx] = p_evt_data->app_info.b_send_conn_evts;

  evt_data.hci_register.hci_handle = (tNFA_HANDLE)(xx | NFA_HANDLE_GROUP_HCI);

  evt_data.hci_register.status = NFA_STATUS_OK;

  /* notify NFA_HCI_REGISTER_EVT to the application */
  p_evt_data->app_info.p_cback(NFA_HCI_REGISTER_EVT, &evt_data);
}

/*******************************************************************************
**
** Function         nfa_hci_api_deregister
**
** Description      action function to deregister the given application
**
** Returns          None
**
*******************************************************************************/
void nfa_hci_api_deregister(tNFA_HCI_EVENT_DATA* p_evt_data) {
  tNFA_HCI_EVT_DATA evt_data;
  tNFA_HCI_CBACK* p_cback = nullptr;
  int xx;
  tNFA_HCI_DYN_PIPE* p_pipe;
  tNFA_HCI_DYN_GATE* p_gate;

  /* If needed, find the application registration handle */
  if (p_evt_data != nullptr) {
    for (xx = 0; xx < NFA_HCI_MAX_APP_CB; xx++) {
      if ((nfa_hci_cb.cfg.reg_app_names[xx][0] != 0) &&
          !strncmp(p_evt_data->app_info.app_name,
                   &nfa_hci_cb.cfg.reg_app_names[xx][0],
                   strlen(p_evt_data->app_info.app_name))) {
        LOG(DEBUG) << StringPrintf("nfa_hci_api_deregister (%s) inx: %u",
                                   p_evt_data->app_info.app_name, xx);
        break;
      }
    }

    if (xx == NFA_HCI_MAX_APP_CB) {
      LOG(WARNING) << StringPrintf("Unknown app: %s",
                                   p_evt_data->app_info.app_name);
      return;
    }
    nfa_hci_cb.app_in_use = (tNFA_HANDLE)(xx | NFA_HANDLE_GROUP_HCI);
    p_cback = nfa_hci_cb.p_app_cback[xx];
  } else {
    nfa_sys_stop_timer(&nfa_hci_cb.timer);
    /* We are recursing through deleting all the app's pipes and gates */
    p_cback = nfa_hci_cb.p_app_cback[nfa_hci_cb.app_in_use & NFA_HANDLE_MASK];
  }

  /* See if any pipe is owned by this app */
  if (nfa_hciu_find_pipe_by_owner(nfa_hci_cb.app_in_use) == nullptr) {
    /* No pipes, release all gates owned by this app */
    while ((p_gate = nfa_hciu_find_gate_by_owner(nfa_hci_cb.app_in_use)) !=
           nullptr)
      nfa_hciu_release_gate(p_gate->gate_id);

    memset(&nfa_hci_cb.cfg
                .reg_app_names[nfa_hci_cb.app_in_use & NFA_HANDLE_MASK][0],
           0, NFA_MAX_HCI_APP_NAME_LEN + 1);
    nfa_hci_cb.p_app_cback[nfa_hci_cb.app_in_use & NFA_HANDLE_MASK] = nullptr;

    nfa_hci_cb.nv_write_needed = true;

    evt_data.hci_deregister.status = NFC_STATUS_OK;

    if (nfa_hci_cb.hci_state == NFA_HCI_STATE_APP_DEREGISTER)
      nfa_hci_cb.hci_state = NFA_HCI_STATE_IDLE;

    /* notify NFA_HCI_DEREGISTER_EVT to the application */
    if (p_cback) p_cback(NFA_HCI_DEREGISTER_EVT, &evt_data);
  } else if ((p_pipe = nfa_hciu_find_active_pipe_by_owner(
                  nfa_hci_cb.app_in_use)) == nullptr) {
    /* No pipes, release all gates owned by this app */
    while ((p_gate = nfa_hciu_find_gate_with_nopipes_by_owner(
                nfa_hci_cb.app_in_use)) != nullptr)
      nfa_hciu_release_gate(p_gate->gate_id);

    nfa_hci_cb.p_app_cback[nfa_hci_cb.app_in_use & NFA_HANDLE_MASK] = nullptr;

    nfa_hci_cb.nv_write_needed = true;

    evt_data.hci_deregister.status = NFC_STATUS_FAILED;

    if (nfa_hci_cb.hci_state == NFA_HCI_STATE_APP_DEREGISTER)
      nfa_hci_cb.hci_state = NFA_HCI_STATE_IDLE;

    /* notify NFA_HCI_DEREGISTER_EVT to the application */
    if (p_cback) p_cback(NFA_HCI_DEREGISTER_EVT, &evt_data);
  } else {
    /* Delete all active pipes created for the application before de registering
     **/
    nfa_hci_cb.hci_state = NFA_HCI_STATE_APP_DEREGISTER;

    nfa_hciu_send_delete_pipe_cmd(p_pipe->pipe_id);
  }
}

/*******************************************************************************
**
** Function         nfa_hci_api_get_gate_pipe_list
**
** Description      action function to get application allocated gates and
**                  application created pipes
**
** Returns          None
**
*******************************************************************************/
static void nfa_hci_api_get_gate_pipe_list(tNFA_HCI_EVENT_DATA* p_evt_data) {
  tNFA_HCI_EVT_DATA evt_data;
  int xx, yy;
  tNFA_HCI_DYN_GATE* pg = nfa_hci_cb.cfg.dyn_gates;
  tNFA_HCI_DYN_PIPE* pp = nfa_hci_cb.cfg.dyn_pipes;

  evt_data.gates_pipes.num_gates = 0;
  evt_data.gates_pipes.num_pipes = 0;

  for (xx = 0; xx < NFA_HCI_MAX_GATE_CB; xx++, pg++) {
    if (pg->gate_owner == p_evt_data->get_gate_pipe_list.hci_handle) {
      evt_data.gates_pipes.gate[evt_data.gates_pipes.num_gates++] = pg->gate_id;

      pp = nfa_hci_cb.cfg.dyn_pipes;

      /* Loop through looking for a match */
      for (yy = 0; yy < NFA_HCI_MAX_PIPE_CB; yy++, pp++) {
        if (pp->local_gate == pg->gate_id)
          evt_data.gates_pipes.pipe[evt_data.gates_pipes.num_pipes++] =
              *(tNFA_HCI_PIPE_INFO*)pp;
      }
    }
  }
#if(NXP_EXTNS == TRUE)
  evt_data.gates_pipes.num_host_created_pipes = 0;
#else
  evt_data.gates_pipes.num_uicc_created_pipes = 0;
#endif

  /* Loop through all pipes that are connected to connectivity gate */
  for (xx = 0, pp = nfa_hci_cb.cfg.dyn_pipes; xx < NFA_HCI_MAX_PIPE_CB;
       xx++, pp++) {
    if (pp->pipe_id != 0 && pp->local_gate == NFA_HCI_CONNECTIVITY_GATE) {
#if(NXP_EXTNS == TRUE)
      memcpy(&evt_data.gates_pipes.host_created_pipe
                  [evt_data.gates_pipes.num_host_created_pipes++],
#else
      memcpy(&evt_data.gates_pipes.uicc_created_pipe
                  [evt_data.gates_pipes.num_uicc_created_pipes++],
#endif
             pp, sizeof(tNFA_HCI_PIPE_INFO));
    } else if (pp->pipe_id != 0 && pp->local_gate == NFA_HCI_LOOP_BACK_GATE) {
#if(NXP_EXTNS == TRUE)
      memcpy(&evt_data.gates_pipes.host_created_pipe
                  [evt_data.gates_pipes.num_host_created_pipes++],
#else
      memcpy(&evt_data.gates_pipes.uicc_created_pipe
                        [evt_data.gates_pipes.num_uicc_created_pipes++],
#endif
             pp, sizeof(tNFA_HCI_PIPE_INFO));
#if(NXP_EXTNS == TRUE)
    } else if (pp->local_gate == NFA_HCI_ID_MNGMNT_APP_GATE) {
        /* Identity Management Gate - Pipes connected to this gate are to be saved
        ** to other_uicc_pipe array and to increment num_other_uicc_pipes
        */
        memcpy(&evt_data.gates_pipes.host_created_pipe
                    [evt_data.gates_pipes.num_host_created_pipes++],
               pp, sizeof(tNFA_HCI_PIPE_INFO));
#endif
    } else if (pp->pipe_id >= NFA_HCI_FIRST_DYNAMIC_PIPE &&
               pp->pipe_id <= NFA_HCI_LAST_DYNAMIC_PIPE && pp->pipe_id &&
               pp->local_gate >= NFA_HCI_FIRST_PROP_GATE &&
               pp->local_gate <= NFA_HCI_LAST_PROP_GATE) {
      for (yy = 0, pg = nfa_hci_cb.cfg.dyn_gates; yy < NFA_HCI_MAX_GATE_CB;
           yy++, pg++) {
        if (pp->local_gate == pg->gate_id) {
          if (!pg->gate_owner)
#if(NXP_EXTNS == TRUE)
            memcpy(&evt_data.gates_pipes.host_created_pipe
                        [evt_data.gates_pipes.num_host_created_pipes++],
#else
            memcpy(&evt_data.gates_pipes.uicc_created_pipe
                        [evt_data.gates_pipes.num_uicc_created_pipes++],
#endif
                   pp, sizeof(tNFA_HCI_PIPE_INFO));
          break;
        }
      }
    }
  }

  evt_data.gates_pipes.status = NFA_STATUS_OK;

  /* notify NFA_HCI_GET_GATE_PIPE_LIST_EVT to the application */
  nfa_hciu_send_to_app(NFA_HCI_GET_GATE_PIPE_LIST_EVT, &evt_data,
                       p_evt_data->get_gate_pipe_list.hci_handle);
}

/*******************************************************************************
**
** Function         nfa_hci_api_alloc_gate
**
** Description      action function to allocate gate
**
** Returns          None
**
*******************************************************************************/
static void nfa_hci_api_alloc_gate(tNFA_HCI_EVENT_DATA* p_evt_data) {
  tNFA_HANDLE app_handle = p_evt_data->comm.hci_handle;
  tNFA_HCI_EVT_DATA evt_data;
  tNFA_HCI_DYN_GATE* p_gate;

  p_gate = nfa_hciu_alloc_gate(p_evt_data->gate_info.gate, app_handle);

  if (p_gate) {
    if (!p_gate->gate_owner) {
      /* No app owns the gate yet */
      p_gate->gate_owner = app_handle;
    } else if (p_gate->gate_owner != app_handle) {
      /* Some other app owns the gate */
      p_gate = nullptr;
      LOG(ERROR) << StringPrintf("The Gate (0X%02x) already taken!",
                                 p_evt_data->gate_info.gate);
    }
  }

  evt_data.allocated.gate = p_gate ? p_gate->gate_id : 0;
  evt_data.allocated.status = p_gate ? NFA_STATUS_OK : NFA_STATUS_FAILED;

  /* notify NFA_HCI_ALLOCATE_GATE_EVT to the application */
  nfa_hciu_send_to_app(NFA_HCI_ALLOCATE_GATE_EVT, &evt_data, app_handle);
}

/*******************************************************************************
**
** Function         nfa_hci_api_dealloc_gate
**
** Description      action function to deallocate the given generic gate
**
** Returns          None
**
*******************************************************************************/
void nfa_hci_api_dealloc_gate(tNFA_HCI_EVENT_DATA* p_evt_data) {
  tNFA_HCI_EVT_DATA evt_data;
  uint8_t gate_id;
  tNFA_HCI_DYN_GATE* p_gate;
  tNFA_HCI_DYN_PIPE* p_pipe;
  tNFA_HANDLE app_handle;

  /* p_evt_data may be NULL if we are recursively deleting pipes */
  if (p_evt_data) {
    gate_id = p_evt_data->gate_dealloc.gate;
    app_handle = p_evt_data->gate_dealloc.hci_handle;

  } else {
    nfa_sys_stop_timer(&nfa_hci_cb.timer);
    gate_id = nfa_hci_cb.local_gate_in_use;
    app_handle = nfa_hci_cb.app_in_use;
  }

  evt_data.deallocated.gate = gate_id;
  ;

  p_gate = nfa_hciu_find_gate_by_gid(gate_id);

  if (p_gate == nullptr) {
    evt_data.deallocated.status = NFA_STATUS_UNKNOWN_GID;
  } else if (p_gate->gate_owner != app_handle) {
    evt_data.deallocated.status = NFA_STATUS_FAILED;
  } else {
    /* See if any pipe is owned by this app */
    if (nfa_hciu_find_pipe_on_gate(p_gate->gate_id) == nullptr) {
      nfa_hciu_release_gate(p_gate->gate_id);

      nfa_hci_cb.nv_write_needed = true;
      evt_data.deallocated.status = NFA_STATUS_OK;

      if (nfa_hci_cb.hci_state == NFA_HCI_STATE_REMOVE_GATE)
        nfa_hci_cb.hci_state = NFA_HCI_STATE_IDLE;
    } else if ((p_pipe = nfa_hciu_find_active_pipe_on_gate(p_gate->gate_id)) ==
               nullptr) {
      /* UICC is not active at the moment and cannot delete the pipe */
      nfa_hci_cb.nv_write_needed = true;
      evt_data.deallocated.status = NFA_STATUS_FAILED;

      if (nfa_hci_cb.hci_state == NFA_HCI_STATE_REMOVE_GATE)
        nfa_hci_cb.hci_state = NFA_HCI_STATE_IDLE;
    } else {
      /* Delete pipes on the gate */
      nfa_hci_cb.local_gate_in_use = gate_id;
      nfa_hci_cb.app_in_use = app_handle;
      nfa_hci_cb.hci_state = NFA_HCI_STATE_REMOVE_GATE;

      nfa_hciu_send_delete_pipe_cmd(p_pipe->pipe_id);
      return;
    }
  }

  nfa_hciu_send_to_app(NFA_HCI_DEALLOCATE_GATE_EVT, &evt_data, app_handle);
}

/*******************************************************************************
**
** Function         nfa_hci_api_get_host_list
**
** Description      action function to get the host list from HCI network
**
** Returns          None
**
*******************************************************************************/
static void nfa_hci_api_get_host_list(tNFA_HCI_EVENT_DATA* p_evt_data) {
  uint8_t app_inx = p_evt_data->get_host_list.hci_handle & NFA_HANDLE_MASK;

  nfa_hci_cb.app_in_use = p_evt_data->get_host_list.hci_handle;

  /* Send Get Host List command on "Internal request" or requested by registered
   * application with valid handle and callback function */
  if ((nfa_hci_cb.app_in_use == NFA_HANDLE_INVALID) ||
      ((app_inx < NFA_HCI_MAX_APP_CB) &&
       (nfa_hci_cb.p_app_cback[app_inx] != nullptr))) {
    nfa_hciu_send_get_param_cmd(NFA_HCI_ADMIN_PIPE, NFA_HCI_HOST_LIST_INDEX);
  }
}

/*******************************************************************************
**
** Function         nfa_hci_api_create_pipe
**
** Description      action function to create a pipe
**
** Returns          TRUE, if the command is processed
**                  FALSE, if command is queued for processing later
**
*******************************************************************************/
static bool nfa_hci_api_create_pipe(tNFA_HCI_EVENT_DATA* p_evt_data) {
  tNFA_HCI_DYN_GATE* p_gate =
      nfa_hciu_find_gate_by_gid(p_evt_data->create_pipe.source_gate);
  tNFA_HCI_EVT_DATA evt_data;
  bool report_failed = false;

  /* Verify that the app owns the gate that the pipe is being created on */
  if ((p_gate == nullptr) ||
      (p_gate->gate_owner != p_evt_data->create_pipe.hci_handle)) {
    report_failed = true;
    LOG(ERROR) << StringPrintf(
        "nfa_hci_api_create_pipe Cannot create pipe! APP: 0x%02x does not own "
        "the gate:0x%x",
        p_evt_data->create_pipe.hci_handle,
        p_evt_data->create_pipe.source_gate);
  } else if (nfa_hciu_check_pipe_between_gates(
                 p_evt_data->create_pipe.source_gate,
                 p_evt_data->create_pipe.dest_host,
                 p_evt_data->create_pipe.dest_gate)) {
    report_failed = true;
    LOG(ERROR) << StringPrintf(
        "nfa_hci_api_create_pipe : Cannot create multiple pipe between the "
        "same two gates!");
  }

  if (report_failed) {
    evt_data.created.source_gate = p_evt_data->create_pipe.source_gate;
    evt_data.created.status = NFA_STATUS_FAILED;

    nfa_hciu_send_to_app(NFA_HCI_CREATE_PIPE_EVT, &evt_data,
                         p_evt_data->open_pipe.hci_handle);
  } else {
    if (nfa_hciu_is_host_reseting(p_evt_data->create_pipe.dest_gate)) {
      GKI_enqueue(&nfa_hci_cb.hci_host_reset_api_q, (NFC_HDR*)p_evt_data);
      return false;
    }

    nfa_hci_cb.local_gate_in_use = p_evt_data->create_pipe.source_gate;
    nfa_hci_cb.remote_gate_in_use = p_evt_data->create_pipe.dest_gate;
    nfa_hci_cb.remote_host_in_use = p_evt_data->create_pipe.dest_host;
    nfa_hci_cb.app_in_use = p_evt_data->create_pipe.hci_handle;

    nfa_hciu_send_create_pipe_cmd(p_evt_data->create_pipe.source_gate,
                                  p_evt_data->create_pipe.dest_host,
                                  p_evt_data->create_pipe.dest_gate);
  }
  return true;
}

/*******************************************************************************
**
** Function         nfa_hci_api_open_pipe
**
** Description      action function to open a pipe
**
** Returns          None
**
*******************************************************************************/
static void nfa_hci_api_open_pipe(tNFA_HCI_EVENT_DATA* p_evt_data) {
  tNFA_HCI_EVT_DATA evt_data;
  tNFA_HCI_DYN_PIPE* p_pipe =
      nfa_hciu_find_pipe_by_pid(p_evt_data->open_pipe.pipe);
  tNFA_HCI_DYN_GATE* p_gate = nullptr;

  if (p_pipe != nullptr) p_gate = nfa_hciu_find_gate_by_gid(p_pipe->local_gate);

  if ((p_pipe != nullptr) && (p_gate != nullptr) &&
      (nfa_hciu_is_active_host(p_pipe->dest_host)) &&
      (p_gate->gate_owner == p_evt_data->open_pipe.hci_handle)) {
    if (p_pipe->pipe_state == NFA_HCI_PIPE_CLOSED) {
      nfa_hciu_send_open_pipe_cmd(p_evt_data->open_pipe.pipe);
    } else {
      evt_data.opened.pipe = p_evt_data->open_pipe.pipe;
      evt_data.opened.status = NFA_STATUS_OK;

      nfa_hciu_send_to_app(NFA_HCI_OPEN_PIPE_EVT, &evt_data,
                           p_evt_data->open_pipe.hci_handle);
    }
  } else {
    evt_data.opened.pipe = p_evt_data->open_pipe.pipe;
    evt_data.opened.status = NFA_STATUS_FAILED;

    nfa_hciu_send_to_app(NFA_HCI_OPEN_PIPE_EVT, &evt_data,
                         p_evt_data->open_pipe.hci_handle);
  }
}

/*******************************************************************************
**
** Function         nfa_hci_api_get_reg_value
**
** Description      action function to get the reg value of the specified index
**
** Returns          TRUE, if the command is processed
**                  FALSE, if command is queued for processing later
**
*******************************************************************************/
static bool nfa_hci_api_get_reg_value(tNFA_HCI_EVENT_DATA* p_evt_data) {
  tNFA_HCI_DYN_PIPE* p_pipe =
      nfa_hciu_find_pipe_by_pid(p_evt_data->get_registry.pipe);
  tNFA_HCI_DYN_GATE* p_gate;
  tNFA_STATUS status = NFA_STATUS_FAILED;
  tNFA_HCI_EVT_DATA evt_data;

  if (p_pipe != nullptr) {
    p_gate = nfa_hciu_find_gate_by_gid(p_pipe->local_gate);

    if ((p_gate != nullptr) && (nfa_hciu_is_active_host(p_pipe->dest_host)) &&
        (p_gate->gate_owner == p_evt_data->get_registry.hci_handle)) {
      nfa_hci_cb.app_in_use = p_evt_data->get_registry.hci_handle;

      if (nfa_hciu_is_host_reseting(p_pipe->dest_host)) {
        GKI_enqueue(&nfa_hci_cb.hci_host_reset_api_q, (NFC_HDR*)p_evt_data);
        return false;
      }

      if (p_pipe->pipe_state == NFA_HCI_PIPE_CLOSED) {
        LOG(WARNING) << StringPrintf(
            "nfa_hci_api_get_reg_value pipe:%d not open",
            p_evt_data->get_registry.pipe);
      } else {
        status = nfa_hciu_send_get_param_cmd(p_evt_data->get_registry.pipe,
                                             p_evt_data->get_registry.reg_inx);
        if (status == NFA_STATUS_OK) return true;
      }
    }
  }

  evt_data.cmd_sent.status = status;

  /* Send NFA_HCI_CMD_SENT_EVT to notify failure */
  nfa_hciu_send_to_app(NFA_HCI_CMD_SENT_EVT, &evt_data,
                       p_evt_data->get_registry.hci_handle);
  return true;
}

/*******************************************************************************
**
** Function         nfa_hci_api_set_reg_value
**
** Description      action function to set the reg value at specified index
**
** Returns          TRUE, if the command is processed
**                  FALSE, if command is queued for processing later
**
*******************************************************************************/
static bool nfa_hci_api_set_reg_value(tNFA_HCI_EVENT_DATA* p_evt_data) {
  tNFA_HCI_DYN_PIPE* p_pipe =
      nfa_hciu_find_pipe_by_pid(p_evt_data->set_registry.pipe);
  tNFA_HCI_DYN_GATE* p_gate;
  tNFA_STATUS status = NFA_STATUS_FAILED;
  tNFA_HCI_EVT_DATA evt_data;

  if (p_pipe != nullptr) {
    p_gate = nfa_hciu_find_gate_by_gid(p_pipe->local_gate);

    if ((p_gate != nullptr) && (nfa_hciu_is_active_host(p_pipe->dest_host)) &&
        (p_gate->gate_owner == p_evt_data->set_registry.hci_handle)) {
      nfa_hci_cb.app_in_use = p_evt_data->set_registry.hci_handle;

      if (nfa_hciu_is_host_reseting(p_pipe->dest_host)) {
        GKI_enqueue(&nfa_hci_cb.hci_host_reset_api_q, (NFC_HDR*)p_evt_data);
        return false;
      }

      if (p_pipe->pipe_state == NFA_HCI_PIPE_CLOSED) {
        LOG(WARNING) << StringPrintf(
            "nfa_hci_api_set_reg_value pipe:%d not open",
            p_evt_data->set_registry.pipe);
      } else {
        status = nfa_hciu_send_set_param_cmd(
            p_evt_data->set_registry.pipe, p_evt_data->set_registry.reg_inx,
            p_evt_data->set_registry.size, p_evt_data->set_registry.data);
        if (status == NFA_STATUS_OK) return true;
      }
    }
  }
  evt_data.cmd_sent.status = status;

  /* Send NFA_HCI_CMD_SENT_EVT to notify failure */
  nfa_hciu_send_to_app(NFA_HCI_CMD_SENT_EVT, &evt_data,
                       p_evt_data->set_registry.hci_handle);
  return true;
}

/*******************************************************************************
**
** Function         nfa_hci_api_close_pipe
**
** Description      action function to close a pipe
**
** Returns          None
**
*******************************************************************************/
static void nfa_hci_api_close_pipe(tNFA_HCI_EVENT_DATA* p_evt_data) {
  tNFA_HCI_EVT_DATA evt_data;
  tNFA_HCI_DYN_PIPE* p_pipe =
      nfa_hciu_find_pipe_by_pid(p_evt_data->close_pipe.pipe);
  tNFA_HCI_DYN_GATE* p_gate = nullptr;

  if (p_pipe != nullptr) p_gate = nfa_hciu_find_gate_by_gid(p_pipe->local_gate);

  if ((p_pipe != nullptr) && (p_gate != nullptr) &&
      (nfa_hciu_is_active_host(p_pipe->dest_host)) &&
      (p_gate->gate_owner == p_evt_data->close_pipe.hci_handle)) {
    if (p_pipe->pipe_state == NFA_HCI_PIPE_OPENED) {
      nfa_hciu_send_close_pipe_cmd(p_evt_data->close_pipe.pipe);
    } else {
      evt_data.closed.status = NFA_STATUS_OK;
      evt_data.closed.pipe = p_evt_data->close_pipe.pipe;

      nfa_hciu_send_to_app(NFA_HCI_CLOSE_PIPE_EVT, &evt_data,
                           p_evt_data->close_pipe.hci_handle);
    }
  } else {
    evt_data.closed.status = NFA_STATUS_FAILED;
    evt_data.closed.pipe = 0x00;

    nfa_hciu_send_to_app(NFA_HCI_CLOSE_PIPE_EVT, &evt_data,
                         p_evt_data->close_pipe.hci_handle);
  }
}

/*******************************************************************************
**
** Function         nfa_hci_api_delete_pipe
**
** Description      action function to delete a pipe
**
** Returns          None
**
*******************************************************************************/
static void nfa_hci_api_delete_pipe(tNFA_HCI_EVENT_DATA* p_evt_data) {
  tNFA_HCI_EVT_DATA evt_data;
  tNFA_HCI_DYN_PIPE* p_pipe =
      nfa_hciu_find_pipe_by_pid(p_evt_data->delete_pipe.pipe);
  tNFA_HCI_DYN_GATE* p_gate = nullptr;

  if (p_pipe != nullptr) {
    p_gate = nfa_hciu_find_gate_by_gid(p_pipe->local_gate);
    if ((p_gate != nullptr) &&
        (p_gate->gate_owner == p_evt_data->delete_pipe.hci_handle) &&
        (nfa_hciu_is_active_host(p_pipe->dest_host))) {
      nfa_hciu_send_delete_pipe_cmd(p_evt_data->delete_pipe.pipe);
      return;
    }
  }

  evt_data.deleted.status = NFA_STATUS_FAILED;
  evt_data.deleted.pipe = 0x00;
  nfa_hciu_send_to_app(NFA_HCI_DELETE_PIPE_EVT, &evt_data,
                       p_evt_data->close_pipe.hci_handle);
}

/*******************************************************************************
**
** Function         nfa_hci_api_send_cmd
**
** Description      action function to send command on the given pipe
**
** Returns          TRUE, if the command is processed
**                  FALSE, if command is queued for processing later
**
*******************************************************************************/
static bool nfa_hci_api_send_cmd(tNFA_HCI_EVENT_DATA* p_evt_data) {
  tNFA_STATUS status = NFA_STATUS_FAILED;
  tNFA_HCI_DYN_PIPE* p_pipe;
  tNFA_HCI_EVT_DATA evt_data;
  tNFA_HANDLE app_handle;
#if(NXP_EXTNS == TRUE)
  tNFA_HCI_PIPE_CMDRSP_INFO  *p_pipe_cmdrsp_info = nullptr;
#endif

  if ((p_pipe = nfa_hciu_find_pipe_by_pid(p_evt_data->send_cmd.pipe)) !=
      nullptr) {
    app_handle = nfa_hciu_get_pipe_owner(p_evt_data->send_cmd.pipe);

    if ((nfa_hciu_is_active_host(p_pipe->dest_host)) &&
        ((app_handle == p_evt_data->send_cmd.hci_handle ||
         p_pipe->local_gate == NFA_HCI_CONNECTIVITY_GATE
#if(NXP_EXTNS == TRUE)
        || p_pipe->local_gate == NFA_HCI_APDU_GATE ||
         p_pipe->local_gate == NFA_HCI_IDENTITY_MANAGEMENT_GATE
#endif
))) {
      if (nfa_hciu_is_host_reseting(p_pipe->dest_host)) {
        GKI_enqueue(&nfa_hci_cb.hci_host_reset_api_q, (NFC_HDR*)p_evt_data);
        return false;
      }
#if(NXP_EXTNS == TRUE)
      p_pipe_cmdrsp_info = nfa_hciu_get_pipe_cmdrsp_info (p_evt_data->send_cmd.pipe);
      if(p_pipe_cmdrsp_info == nullptr) {
        LOG(WARNING) << StringPrintf("nfa_hci_api_send_cmd p_pipe_cmdrsp_info is NULL");
      }
      else
#endif
      if (p_pipe->pipe_state == NFA_HCI_PIPE_OPENED) {
        nfa_hci_cb.pipe_in_use = p_evt_data->send_cmd.pipe;
        status = nfa_hciu_send_msg(p_pipe->pipe_id, NFA_HCI_COMMAND_TYPE,
                                   p_evt_data->send_cmd.cmd_code,
                                   p_evt_data->send_cmd.cmd_len,
                                   p_evt_data->send_cmd.data);
#if(NXP_EXTNS == TRUE)
        if (status == NFA_STATUS_OK) {
            nfa_hci_cb.pipe_in_use = p_evt_data->send_cmd.pipe;
            p_pipe_cmdrsp_info->w4_cmd_rsp = true;
            p_pipe_cmdrsp_info->cmd_inst_sent   = p_evt_data->send_cmd.cmd_code;
            if ((p_evt_data->send_cmd.cmd_code == NFA_HCI_ANY_GET_PARAMETER)
                ||(p_evt_data->send_cmd.cmd_code == NFA_HCI_ANY_SET_PARAMETER)) {
                p_pipe_cmdrsp_info->cmd_inst_param_sent = p_evt_data->send_cmd.data[0];
            }
            nfa_sys_start_timer (&(p_pipe_cmdrsp_info->rsp_timer), NFA_HCI_RSP_TIMEOUT_EVT,
                                 p_nfa_hci_cfg->hcp_response_timeout);
            return true;
        }
#else
        if (status == NFA_STATUS_OK) return true;
#endif
      } else {
        LOG(WARNING) << StringPrintf("nfa_hci_api_send_cmd pipe:%d not open",
                                     p_pipe->pipe_id);
      }
    } else {
      LOG(WARNING) << StringPrintf(
          "nfa_hci_api_send_cmd pipe:%d Owned by different application or "
          "Destination host is not active",
          p_pipe->pipe_id);
    }
  } else {
    LOG(WARNING) << StringPrintf("nfa_hci_api_send_cmd pipe:%d not found",
                                 p_evt_data->send_cmd.pipe);
  }

  evt_data.cmd_sent.status = status;

  /* Send NFA_HCI_CMD_SENT_EVT to notify failure */
  nfa_hciu_send_to_app(NFA_HCI_CMD_SENT_EVT, &evt_data,
                       p_evt_data->send_cmd.hci_handle);
  return true;
}

/*******************************************************************************
**
** Function         nfa_hci_api_send_rsp
**
** Description      action function to send response on the given pipe
**
** Returns          None
**
*******************************************************************************/
static void nfa_hci_api_send_rsp(tNFA_HCI_EVENT_DATA* p_evt_data) {
  tNFA_STATUS status = NFA_STATUS_FAILED;
  tNFA_HCI_DYN_PIPE* p_pipe;
  tNFA_HCI_EVT_DATA evt_data;
  tNFA_HANDLE app_handle;

  if ((p_pipe = nfa_hciu_find_pipe_by_pid(p_evt_data->send_rsp.pipe)) !=
      nullptr) {
    app_handle = nfa_hciu_get_pipe_owner(p_evt_data->send_rsp.pipe);

    if ((nfa_hciu_is_active_host(p_pipe->dest_host)) &&
        ((app_handle == p_evt_data->send_rsp.hci_handle ||
          p_pipe->local_gate == NFA_HCI_CONNECTIVITY_GATE))) {
      if (p_pipe->pipe_state == NFA_HCI_PIPE_OPENED) {
        status = nfa_hciu_send_msg(p_pipe->pipe_id, NFA_HCI_RESPONSE_TYPE,
                                   p_evt_data->send_rsp.response,
                                   p_evt_data->send_rsp.size,
                                   p_evt_data->send_rsp.data);
        if (status == NFA_STATUS_OK) return;
      } else {
        LOG(WARNING) << StringPrintf("nfa_hci_api_send_rsp pipe:%d not open",
                                     p_pipe->pipe_id);
      }
    } else {
      LOG(WARNING) << StringPrintf(
          "nfa_hci_api_send_rsp pipe:%d Owned by different application or "
          "Destination host is not active",
          p_pipe->pipe_id);
    }
  } else {
    LOG(WARNING) << StringPrintf("nfa_hci_api_send_rsp pipe:%d not found",
                                 p_evt_data->send_rsp.pipe);
  }

  evt_data.rsp_sent.status = status;

  /* Send NFA_HCI_RSP_SENT_EVT to notify failure */
  nfa_hciu_send_to_app(NFA_HCI_RSP_SENT_EVT, &evt_data,
                       p_evt_data->send_rsp.hci_handle);
}

/*******************************************************************************
**
** Function         nfa_hci_api_send_event
**
** Description      action function to send an event to the given pipe
**
** Returns          TRUE, if the event is processed
**                  FALSE, if event is queued for processing later
**
*******************************************************************************/
static bool nfa_hci_api_send_event(tNFA_HCI_EVENT_DATA* p_evt_data) {
  tNFA_STATUS status = NFA_STATUS_FAILED;
  tNFA_HCI_DYN_PIPE* p_pipe;
  tNFA_HCI_EVT_DATA evt_data;
  tNFA_HANDLE app_handle;
#if(NXP_EXTNS == TRUE)
  tNFA_HCI_PIPE_CMDRSP_INFO   *p_pipe_cmdrsp_info = nullptr;
#endif

  if ((p_pipe = nfa_hciu_find_pipe_by_pid(p_evt_data->send_evt.pipe)) !=
      nullptr) {
    app_handle = nfa_hciu_get_pipe_owner(p_evt_data->send_evt.pipe);

        if ((nfa_hciu_is_active_host(p_pipe->dest_host)) &&
                ((app_handle == p_evt_data->send_evt.hci_handle ||
                        p_pipe->local_gate == NFA_HCI_CONNECTIVITY_GATE
#if(NXP_EXTNS == TRUE)
||
                        p_pipe->local_gate == NFA_HCI_APDU_APP_GATE
#endif
))) {
            if (nfa_hciu_is_host_reseting(p_pipe->dest_host)) {
                GKI_enqueue(&nfa_hci_cb.hci_host_reset_api_q, (NFC_HDR*)p_evt_data);
                return false;
            }

      if (p_pipe->pipe_state == NFA_HCI_PIPE_OPENED) {
#if(NXP_EXTNS == TRUE)
        p_pipe_cmdrsp_info = nfa_hciu_get_pipe_cmdrsp_info (p_pipe->pipe_id);
#endif
        status = nfa_hciu_send_msg(
            p_pipe->pipe_id, NFA_HCI_EVENT_TYPE, p_evt_data->send_evt.evt_code,
            p_evt_data->send_evt.evt_len, p_evt_data->send_evt.p_evt_buf);

        if (status == NFA_STATUS_OK) {
          if (p_pipe->local_gate == NFA_HCI_LOOP_BACK_GATE) {
            nfa_hci_cb.w4_rsp_evt = true;
            nfa_hci_cb.hci_state = NFA_HCI_STATE_WAIT_RSP;
          }

          if (p_evt_data->send_evt.rsp_len) {
            nfa_hci_cb.pipe_in_use = p_evt_data->send_evt.pipe;
            nfa_hci_cb.rsp_buf_size = p_evt_data->send_evt.rsp_len;
            nfa_hci_cb.p_rsp_buf = p_evt_data->send_evt.p_rsp_buf;
#if(NXP_EXTNS == TRUE)
            if(p_pipe_cmdrsp_info != nullptr) {
              p_pipe_cmdrsp_info->w4_rsp_apdu_evt = true;
              p_pipe_cmdrsp_info->rsp_buf_size = p_evt_data->send_evt.rsp_len;
              p_pipe_cmdrsp_info->p_rsp_buf    = p_evt_data->send_evt.p_rsp_buf;
              p_pipe_cmdrsp_info->pipe_user = p_evt_data->send_evt.hci_handle;
            }
#endif
            if (p_evt_data->send_evt.rsp_timeout) {
#if(NXP_EXTNS == TRUE)
              if(p_pipe_cmdrsp_info != nullptr) {
                nfa_hci_cb.hci_state = NFA_HCI_STATE_WAIT_RSP;
                nfa_sys_start_timer (&(p_pipe_cmdrsp_info->rsp_timer),
                                         NFA_HCI_RSP_TIMEOUT_EVT,
                                         p_evt_data->send_evt.rsp_timeout);
              }
#else
              nfa_hci_cb.w4_rsp_evt = true;
              nfa_hci_cb.hci_state = NFA_HCI_STATE_WAIT_RSP;
              nfa_sys_start_timer(&nfa_hci_cb.timer, NFA_HCI_RSP_TIMEOUT_EVT,
                                  p_evt_data->send_evt.rsp_timeout);
#endif
            } else if (p_pipe->local_gate == NFA_HCI_LOOP_BACK_GATE) {
              nfa_sys_start_timer(&nfa_hci_cb.timer, NFA_HCI_RSP_TIMEOUT_EVT,
                                  p_nfa_hci_cfg->hcp_response_timeout);
            }
          } else {
            if (p_pipe->local_gate == NFA_HCI_LOOP_BACK_GATE) {
              nfa_hci_cb.pipe_in_use = p_evt_data->send_evt.pipe;
              nfa_sys_start_timer(&nfa_hci_cb.timer, NFA_HCI_RSP_TIMEOUT_EVT,
                                  p_nfa_hci_cfg->hcp_response_timeout);
            }
            nfa_hci_cb.rsp_buf_size = 0;
            nfa_hci_cb.p_rsp_buf = nullptr;
          }
        }
      } else {
        LOG(WARNING) << StringPrintf("nfa_hci_api_send_event pipe:%d not open",
                                     p_pipe->pipe_id);
      }
    } else {
      LOG(WARNING) << StringPrintf(
          "nfa_hci_api_send_event pipe:%d Owned by different application or "
          "Destination host is not active",
          p_pipe->pipe_id);
    }
  } else {
    LOG(WARNING) << StringPrintf("nfa_hci_api_send_event pipe:%d not found",
                                 p_evt_data->send_evt.pipe);
  }

  evt_data.evt_sent.status = status;

  /* Send NFC_HCI_EVENT_SENT_EVT to notify status */
  nfa_hciu_send_to_app(NFA_HCI_EVENT_SENT_EVT, &evt_data,
                       p_evt_data->send_evt.hci_handle);
  return true;
}

/*******************************************************************************
**
** Function         nfa_hci_api_add_static_pipe
**
** Description      action function to add static pipe
**
** Returns          None
**
*******************************************************************************/
static void nfa_hci_api_add_static_pipe(tNFA_HCI_EVENT_DATA* p_evt_data) {
  tNFA_HCI_DYN_GATE* pg;
  tNFA_HCI_DYN_PIPE* pp;
  tNFA_HCI_EVT_DATA evt_data;

  /* Allocate a proprietary gate */
  pg = nfa_hciu_alloc_gate(p_evt_data->add_static_pipe.gate,
                           p_evt_data->add_static_pipe.hci_handle);
  if (pg != nullptr) {
    /* Assign new owner to the gate */
    pg->gate_owner = p_evt_data->add_static_pipe.hci_handle;

    /* Add the dynamic pipe to the proprietary gate */
    if (nfa_hciu_add_pipe_to_gate(p_evt_data->add_static_pipe.pipe, pg->gate_id,
                                  p_evt_data->add_static_pipe.host,
                                  p_evt_data->add_static_pipe.gate) !=
        NFA_HCI_ANY_OK) {
      /* Unable to add the dynamic pipe, so release the gate */
      nfa_hciu_release_gate(pg->gate_id);
      evt_data.pipe_added.status = NFA_STATUS_FAILED;
      nfa_hciu_send_to_app(NFA_HCI_ADD_STATIC_PIPE_EVT, &evt_data,
                           p_evt_data->add_static_pipe.hci_handle);
      return;
    }
    pp = nfa_hciu_find_pipe_by_pid(p_evt_data->add_static_pipe.pipe);
    if (pp != nullptr) {
      /* This pipe is always opened */
      pp->pipe_state = NFA_HCI_PIPE_OPENED;
      evt_data.pipe_added.status = NFA_STATUS_OK;
      nfa_hciu_send_to_app(NFA_HCI_ADD_STATIC_PIPE_EVT, &evt_data,
                           p_evt_data->add_static_pipe.hci_handle);
      return;
    }
  }
  /* Unable to add static pipe */
  evt_data.pipe_added.status = NFA_STATUS_FAILED;
  nfa_hciu_send_to_app(NFA_HCI_ADD_STATIC_PIPE_EVT, &evt_data,
                       p_evt_data->add_static_pipe.hci_handle);
}

/*******************************************************************************
**
** Function         nfa_hci_handle_link_mgm_gate_cmd
**
** Description      This function handles incoming link management gate hci
**                  commands
**
** Returns          none
**
*******************************************************************************/
void nfa_hci_handle_link_mgm_gate_cmd(uint8_t* p_data, uint16_t data_len) {
  uint8_t index;
  uint8_t data[2];
  uint8_t rsp_len = 0;
  uint8_t response = NFA_HCI_ANY_OK;

  if ((nfa_hci_cb.cfg.link_mgmt_gate.pipe00_state != NFA_HCI_PIPE_OPENED) &&
      (nfa_hci_cb.inst != NFA_HCI_ANY_OPEN_PIPE)) {
    nfa_hciu_send_msg(NFA_HCI_LINK_MANAGEMENT_PIPE, NFA_HCI_RESPONSE_TYPE,
                      NFA_HCI_ANY_E_PIPE_NOT_OPENED, 0, nullptr);
    return;
  }

  switch (nfa_hci_cb.inst) {
    case NFA_HCI_ANY_SET_PARAMETER:
      if (data_len < 1) {
        response = NFA_HCI_ANY_E_CMD_NOT_SUPPORTED;
        break;
      }
      STREAM_TO_UINT8(index, p_data);

      if (index == 1 && data_len > 2) {
        STREAM_TO_UINT16(nfa_hci_cb.cfg.link_mgmt_gate.rec_errors, p_data);
      } else
        response = NFA_HCI_ANY_E_REG_PAR_UNKNOWN;
      break;

    case NFA_HCI_ANY_GET_PARAMETER:
      if (data_len < 1) {
        response = NFA_HCI_ANY_E_CMD_NOT_SUPPORTED;
        break;
      }
      STREAM_TO_UINT8(index, p_data);
      if (index == 1) {
        data[0] =
            (uint8_t)((nfa_hci_cb.cfg.link_mgmt_gate.rec_errors >> 8) & 0x00FF);
        data[1] = (uint8_t)(nfa_hci_cb.cfg.link_mgmt_gate.rec_errors & 0x000F);
        rsp_len = 2;
      } else
        response = NFA_HCI_ANY_E_REG_PAR_UNKNOWN;
      break;

    case NFA_HCI_ANY_OPEN_PIPE:
      data[0] = 0;
      rsp_len = 1;
      nfa_hci_cb.cfg.link_mgmt_gate.pipe00_state = NFA_HCI_PIPE_OPENED;
      break;

    case NFA_HCI_ANY_CLOSE_PIPE:
      nfa_hci_cb.cfg.link_mgmt_gate.pipe00_state = NFA_HCI_PIPE_CLOSED;
      break;

    default:
      response = NFA_HCI_ANY_E_CMD_NOT_SUPPORTED;
      break;
  }

  nfa_hciu_send_msg(NFA_HCI_LINK_MANAGEMENT_PIPE, NFA_HCI_RESPONSE_TYPE,
                    response, rsp_len, data);
}

/*******************************************************************************
**
** Function         nfa_hci_handle_pipe_open_close_cmd
**
** Description      This function handles all generic gates (excluding
**                  connectivity gate) commands
**
** Returns          none
**
*******************************************************************************/
void nfa_hci_handle_pipe_open_close_cmd(tNFA_HCI_DYN_PIPE* p_pipe) {
  uint8_t data[1];
  uint8_t rsp_len = 0;
  tNFA_HCI_RESPONSE response = NFA_HCI_ANY_OK;
  tNFA_HCI_DYN_GATE* p_gate;

  if (nfa_hci_cb.inst == NFA_HCI_ANY_OPEN_PIPE) {
    if ((p_gate = nfa_hciu_find_gate_by_gid(p_pipe->local_gate)) != nullptr)
      data[0] = nfa_hciu_count_open_pipes_on_gate(p_gate);
    else
      data[0] = 0;

    p_pipe->pipe_state = NFA_HCI_PIPE_OPENED;
    rsp_len = 1;
  } else if (nfa_hci_cb.inst == NFA_HCI_ANY_CLOSE_PIPE) {
    p_pipe->pipe_state = NFA_HCI_PIPE_CLOSED;
  }

  nfa_hciu_send_msg(p_pipe->pipe_id, NFA_HCI_RESPONSE_TYPE, response, rsp_len,
                    data);
}

/*******************************************************************************
**
** Function         nfa_hci_handle_admin_gate_cmd
**
** Description      This function handles incoming commands on ADMIN gate
**
** Returns          none
**
*******************************************************************************/
void nfa_hci_handle_admin_gate_cmd(uint8_t* p_data, uint16_t data_len) {
  uint8_t source_host, source_gate, dest_host, dest_gate, pipe;
  uint8_t data = 0;
  uint8_t rsp_len = 0;
  tNFA_HCI_RESPONSE response = NFA_HCI_ANY_OK;
  tNFA_HCI_DYN_GATE* pgate;
  tNFA_HCI_EVT_DATA evt_data;

  switch (nfa_hci_cb.inst) {
    case NFA_HCI_ANY_OPEN_PIPE:
      nfa_hci_cb.cfg.admin_gate.pipe01_state = NFA_HCI_PIPE_OPENED;
      data = 0;
      rsp_len = 1;
      break;

    case NFA_HCI_ANY_CLOSE_PIPE:
      nfa_hci_cb.cfg.admin_gate.pipe01_state = NFA_HCI_PIPE_CLOSED;
      /* Reopen the pipe immediately */
      nfa_hciu_send_msg(NFA_HCI_ADMIN_PIPE, NFA_HCI_RESPONSE_TYPE, response,
                        rsp_len, &data);
      nfa_hci_cb.app_in_use = NFA_HANDLE_INVALID;
      nfa_hciu_send_open_pipe_cmd(NFA_HCI_ADMIN_PIPE);
      return;
      break;

    case NFA_HCI_ADM_NOTIFY_PIPE_CREATED:
      if (data_len < 5) {
        response = NFA_HCI_ANY_E_CMD_NOT_SUPPORTED;
        break;
      }
      STREAM_TO_UINT8(source_host, p_data);
      STREAM_TO_UINT8(source_gate, p_data);
      STREAM_TO_UINT8(dest_host, p_data);
      STREAM_TO_UINT8(dest_gate, p_data);
      STREAM_TO_UINT8(pipe, p_data);

      if ((dest_gate == NFA_HCI_IDENTITY_MANAGEMENT_GATE) ||
          (dest_gate == NFA_HCI_LOOP_BACK_GATE)) {
        response = nfa_hciu_add_pipe_to_static_gate(dest_gate, pipe,
                                                    source_host, source_gate);
      } else {
        if ((pgate = nfa_hciu_find_gate_by_gid(dest_gate)) != nullptr) {
          /* If the gate is valid, add the pipe to it  */
          if (nfa_hciu_check_pipe_between_gates(dest_gate, source_host,
                                                source_gate)) {
            /* Already, there is a pipe between these two gates, so will reject
             */
            response = NFA_HCI_ANY_E_NOK;
          } else {
            response = nfa_hciu_add_pipe_to_gate(pipe, dest_gate, source_host,
                                                 source_gate);
            if (response == NFA_HCI_ANY_OK) {
              /* Tell the application a pipe was created with its gate */

              evt_data.created.status = NFA_STATUS_OK;
              evt_data.created.pipe = pipe;
              evt_data.created.source_gate = dest_gate;
              evt_data.created.dest_host = source_host;
              evt_data.created.dest_gate = source_gate;

              nfa_hciu_send_to_app(NFA_HCI_CREATE_PIPE_EVT, &evt_data,
                                   pgate->gate_owner);
            }
          }
        } else {
          response = NFA_HCI_ANY_E_NOK;
          if ((dest_gate >= NFA_HCI_FIRST_PROP_GATE) &&
              (dest_gate <= NFA_HCI_LAST_PROP_GATE)) {
            if (nfa_hciu_alloc_gate(dest_gate, 0))
              response = nfa_hciu_add_pipe_to_gate(pipe, dest_gate, source_host,
                                                   source_gate);
          }
        }
      }
      break;

    case NFA_HCI_ADM_NOTIFY_PIPE_DELETED:
      if (data_len < 1) {
        response = NFA_HCI_ANY_E_CMD_NOT_SUPPORTED;
        break;
      }
      STREAM_TO_UINT8(pipe, p_data);
      response = nfa_hciu_release_pipe(pipe);
      break;

    case NFA_HCI_ADM_NOTIFY_ALL_PIPE_CLEARED:
      if (data_len < 1) {
        response = NFA_HCI_ANY_E_CMD_NOT_SUPPORTED;
        break;
      }
      STREAM_TO_UINT8(source_host, p_data);
#if(NXP_EXTNS != TRUE)
      nfa_hciu_remove_all_pipes_from_host(source_host);
#endif
      if (source_host == NFA_HCI_HOST_CONTROLLER) {
        nfa_hci_cb.cfg.link_mgmt_gate.pipe00_state = NFA_HCI_PIPE_CLOSED;
        nfa_hci_cb.cfg.admin_gate.pipe01_state = NFA_HCI_PIPE_CLOSED;
#if(NXP_EXTNS == TRUE)
        nfa_hciu_remove_all_pipes_from_host(source_host);
#endif
        /* Reopen the admin pipe immediately */
        nfa_hci_cb.app_in_use = NFA_HANDLE_INVALID;
        nfa_hciu_send_open_pipe_cmd(NFA_HCI_ADMIN_PIPE);
        return;
      } else {
#if(NXP_EXTNS == TRUE)
          if(source_host == NFA_HCI_FIRST_PROP_HOST) {
            NFA_SCR_PROCESS_EVT(NFA_SCR_ESE_RECOVERY_START_EVT, NFA_STATUS_OK);
          }
          if (nfa_ee_cb.require_rf_restart &&
              nfa_dm_cb.disc_cb.disc_state != NFA_DM_RFST_IDLE) {
            nfa_ee_cb.ee_flags |= NFA_EE_FLAG_RECOVERY;
            nfa_dm_act_stop_rf_discovery(NULL);
          }
          nfa_hciu_send_msg(NFA_HCI_ADMIN_PIPE, NFA_HCI_RESPONSE_TYPE, response,
              rsp_len, &data);
          /* If starting up, handle events here */
          LOG(DEBUG) << StringPrintf("nfa_hci_handle_admin_gate_cmd %d",
                                     source_host);
          nfa_hci_handle_clear_all_pipe_cmd(source_host);
          return;
#else
        if ((source_host >= NFA_HCI_HOST_ID_UICC0) &&
            (source_host <
             (NFA_HCI_HOST_ID_UICC0 + NFA_HCI_MAX_HOST_IN_NETWORK))) {
          nfa_hci_cb.reset_host[source_host - NFA_HCI_HOST_ID_UICC0] =
              source_host;
        }

#endif
      }
      break;

    default:
      response = NFA_HCI_ANY_E_CMD_NOT_SUPPORTED;
      break;
  }

  nfa_hciu_send_msg(NFA_HCI_ADMIN_PIPE, NFA_HCI_RESPONSE_TYPE, response,
                    rsp_len, &data);
}

/*******************************************************************************
**
** Function         nfa_hci_handle_admin_gate_rsp
**
** Description      This function handles response received on admin gate
**
** Returns          none
**
*******************************************************************************/
void nfa_hci_handle_admin_gate_rsp(uint8_t* p_data, uint8_t data_len) {
  uint8_t source_host;
  uint8_t source_gate = nfa_hci_cb.local_gate_in_use;
  uint8_t dest_host = nfa_hci_cb.remote_host_in_use;
  uint8_t dest_gate = nfa_hci_cb.remote_gate_in_use;
  uint8_t pipe = 0;
  tNFA_STATUS status;
  tNFA_HCI_EVT_DATA evt_data;
  uint8_t default_session[NFA_HCI_SESSION_ID_LEN] = {0xFF, 0xFF, 0xFF, 0xFF,
                                                     0xFF, 0xFF, 0xFF, 0xFF};
#if(NXP_EXTNS != TRUE)
  uint8_t host_count = 0;
  uint8_t host_id = 0;
#endif
#if (NXP_EXTNS == TRUE)
  // Terminal Host Type as ETSI12  Byte1 -Host Id Byte2 - 00
  uint8_t terminalHostTsype[NFA_HCI_HOST_TYPE_LEN] = {0x01, 0x00};
#endif
  uint32_t os_tick;

  LOG(DEBUG) << StringPrintf(
      "nfa_hci_handle_admin_gate_rsp - LastCmdSent: %s  App: 0x%04x  Gate: "
      "0x%02x  Pipe: 0x%02x",
      nfa_hciu_instr_2_str(nfa_hci_cb.cmd_sent).c_str(), nfa_hci_cb.app_in_use,
      nfa_hci_cb.local_gate_in_use, nfa_hci_cb.pipe_in_use);

  /* If starting up, handle events here */
  if ((nfa_hci_cb.hci_state == NFA_HCI_STATE_STARTUP) ||
      (nfa_hci_cb.hci_state == NFA_HCI_STATE_RESTORE) ||
      (nfa_hci_cb.hci_state == NFA_HCI_STATE_WAIT_NETWK_ENABLE) ||
      (nfa_hci_cb.hci_state == NFA_HCI_STATE_RESTORE_NETWK_ENABLE
#if(NXP_EXTNS == TRUE)
      || nfa_hci_cb.hci_state == NFA_HCI_STATE_WAIT_RSP ||
       nfa_hci_cb.hci_state == NFA_HCI_STATE_IDLE
#endif
)) {
    if (nfa_hci_cb.inst == NFA_HCI_ANY_E_PIPE_NOT_OPENED) {
      nfa_hciu_send_open_pipe_cmd(NFA_HCI_ADMIN_PIPE);
      return;
    }

    if (nfa_hci_cb.inst != NFA_HCI_ANY_OK) {
      LOG(ERROR) << StringPrintf(
          "nfa_hci_handle_admin_gate_rsp - Initialization failed");
#if(NXP_EXTNS == TRUE)
      if (!nfa_hci_enable_one_nfcee ())
          nfa_hci_startup_complete (NFA_STATUS_OK);
#else
        nfa_hci_startup_complete (NFA_STATUS_FAILED);
#endif
      return;
    }

    switch (nfa_hci_cb.cmd_sent) {
      case NFA_HCI_ANY_SET_PARAMETER:
        if (nfa_hci_cb.param_in_use == NFA_HCI_SESSION_IDENTITY_INDEX) {
          /* Set WHITELIST */
          nfa_hciu_send_set_param_cmd(
              NFA_HCI_ADMIN_PIPE, NFA_HCI_WHITELIST_INDEX,
              p_nfa_hci_cfg->num_allowlist_host, p_nfa_hci_cfg->p_allowlist);
        } else if (nfa_hci_cb.param_in_use == NFA_HCI_WHITELIST_INDEX) {
#if (NXP_EXTNS == TRUE)
          LOG(DEBUG) << StringPrintf(
              "nfa_hci_handle_admin_gate_rsp - Set the HOST_TYPE as per ETSI "
              "12 !!!");
          /* Set the HOST_TYPE as per ETSI 12 */
          nfa_hciu_send_set_param_cmd(
              NFA_HCI_ADMIN_PIPE, NFA_HCI_HOST_TYPE_INDEX,
              NFA_HCI_HOST_TYPE_LEN, terminalHostTsype);
          return;
        } else if (nfa_hci_cb.param_in_use == NFA_HCI_HOST_TYPE_INDEX) {
#endif
          if ((nfa_hci_cb.hci_state == NFA_HCI_STATE_STARTUP) ||
              (nfa_hci_cb.hci_state == NFA_HCI_STATE_RESTORE))
            nfa_hci_dh_startup_complete();
          if (NFA_GetNCIVersion() >= NCI_VERSION_2_0) {
            nfa_hci_cb.hci_state = NFA_HCI_STATE_WAIT_NETWK_ENABLE;
            NFA_EeGetInfo(&nfa_hci_cb.num_nfcee, nfa_hci_cb.ee_info);
#if (NXP_EXTNS == TRUE)
            if (nfa_hci_enable_one_nfcee() == false) {
              LOG(DEBUG) << StringPrintf("nfa_hci_enable_one_nfcee() failed");
              nfa_hciu_send_get_param_cmd(NFA_HCI_ADMIN_PIPE,
                                          NFA_HCI_HOST_LIST_INDEX);
            }
#else
            nfa_hci_enable_one_nfcee();
#endif
          }
        }
        break;

      case NFA_HCI_ANY_GET_PARAMETER:
        if (nfa_hci_cb.param_in_use == NFA_HCI_HOST_LIST_INDEX) {
#if(NXP_EXTNS == TRUE)
          nfa_hciu_update_host_list(data_len , p_data);
          nfa_hci_cb.hci_state = NFA_HCI_STATE_WAIT_NETWK_ENABLE;
          nfa_sys_stop_timer(&nfa_hci_cb.timer);
          if (!nfa_hci_enable_one_nfcee ())
#else
          host_count = 0;
          while (host_count < NFA_HCI_MAX_HOST_IN_NETWORK) {
            nfa_hci_cb.inactive_host[host_count] =
                NFA_HCI_HOST_ID_UICC0 + host_count;
            host_count++;
          }

          host_count = 0;
          /* Collect active host in the Host Network */
          while (host_count < data_len) {
            host_id = (uint8_t)*p_data++;

            if ((host_id >= NFA_HCI_HOST_ID_UICC0) &&
                (host_id <
                 NFA_HCI_HOST_ID_UICC0 + NFA_HCI_MAX_HOST_IN_NETWORK)) {
              nfa_hci_cb.inactive_host[host_id - NFA_HCI_HOST_ID_UICC0] = 0x00;
              nfa_hci_cb.reset_host[host_id - NFA_HCI_HOST_ID_UICC0] = 0x00;
            }

            host_count++;
          }
#endif
              nfa_hci_startup_complete (NFA_STATUS_OK);
        } else if (nfa_hci_cb.param_in_use == NFA_HCI_SESSION_IDENTITY_INDEX) {
          /* The only parameter we get when initializing is the session ID.
           * Check for match. */
          if (data_len >= NFA_HCI_SESSION_ID_LEN &&
              !memcmp((uint8_t*)nfa_hci_cb.cfg.admin_gate.session_id, p_data,
                      NFA_HCI_SESSION_ID_LEN)) {
            /* Session has not changed, Set WHITELIST */
            nfa_hciu_send_set_param_cmd(
                NFA_HCI_ADMIN_PIPE, NFA_HCI_WHITELIST_INDEX,
                p_nfa_hci_cfg->num_allowlist_host, p_nfa_hci_cfg->p_allowlist);
          } else {
            /* Something wrong, NVRAM data could be corrupt or first start with
             * default session id */
            nfa_hciu_send_clear_all_pipe_cmd();
            nfa_hci_cb.b_hci_new_sessionId = true;
            if (data_len < NFA_HCI_SESSION_ID_LEN) {
              android_errorWriteLog(0x534e4554, "124524315");
            }
          }
        }
        break;

      case NFA_HCI_ANY_OPEN_PIPE:
        nfa_hci_cb.cfg.admin_gate.pipe01_state = NFA_HCI_PIPE_OPENED;
        if (nfa_hci_cb.b_hci_netwk_reset) {
          /* Something wrong, NVRAM data could be corrupt or first start with
           * default session id */
          nfa_hciu_send_clear_all_pipe_cmd();
          nfa_hci_cb.b_hci_netwk_reset = false;
          nfa_hci_cb.b_hci_new_sessionId = true;
        } else if (nfa_hci_cb.b_hci_new_sessionId) {
          nfa_hci_cb.b_hci_new_sessionId = false;

          /* Session ID is reset, Set New session id */
          memcpy(
              &nfa_hci_cb.cfg.admin_gate.session_id[NFA_HCI_SESSION_ID_LEN / 2],
              nfa_hci_cb.cfg.admin_gate.session_id,
              (NFA_HCI_SESSION_ID_LEN / 2));
          os_tick = GKI_get_os_tick_count();
          memcpy(nfa_hci_cb.cfg.admin_gate.session_id, (uint8_t*)&os_tick,
                 (NFA_HCI_SESSION_ID_LEN / 2));
          nfa_hciu_send_set_param_cmd(
              NFA_HCI_ADMIN_PIPE, NFA_HCI_SESSION_IDENTITY_INDEX,
              NFA_HCI_SESSION_ID_LEN,
              (uint8_t*)nfa_hci_cb.cfg.admin_gate.session_id);
        } else {
          /* First thing is to get the session ID */
          nfa_hciu_send_get_param_cmd(NFA_HCI_ADMIN_PIPE,
                                      NFA_HCI_SESSION_IDENTITY_INDEX);
        }
        break;

      case NFA_HCI_ADM_CLEAR_ALL_PIPE:
        nfa_hciu_remove_all_pipes_from_host(0);
        nfa_hci_cb.cfg.admin_gate.pipe01_state = NFA_HCI_PIPE_CLOSED;
        nfa_hci_cb.cfg.link_mgmt_gate.pipe00_state = NFA_HCI_PIPE_CLOSED;
        nfa_hci_cb.nv_write_needed = true;

        /* Open admin */
        nfa_hciu_send_open_pipe_cmd(NFA_HCI_ADMIN_PIPE);
        break;
#if(NXP_EXTNS == TRUE)
      case NFA_HCI_ADM_CREATE_PIPE:
          STREAM_TO_UINT8(source_host, p_data);
          STREAM_TO_UINT8 (source_gate, p_data);
          STREAM_TO_UINT8 (dest_host,   p_data);
          STREAM_TO_UINT8 (dest_gate,   p_data);
          STREAM_TO_UINT8 (pipe,        p_data);

          /* Sanity check */
          if (  (source_gate != nfa_hci_cb.local_gate_in_use)
              ||(dest_gate != nfa_hci_cb.remote_gate_in_use)
              ||(dest_host != nfa_hci_cb.remote_host_in_use)  )
          {
          LOG(DEBUG) << StringPrintf(
              "nfa_hci_handle_admin_gate_rsp sent create pipe with gate: %u "
              "got back: %u",
              nfa_hci_cb.local_gate_in_use, source_gate);

          if (!nfa_hci_enable_one_nfcee())
            nfa_hci_startup_complete(NFA_STATUS_OK);
          break;
          }

          nfa_hciu_add_pipe_to_gate(pipe, source_gate, dest_host, dest_gate);

          nfa_hciu_send_open_pipe_cmd (pipe);
          break;
#endif
    }
  } else {
    status =
        (nfa_hci_cb.inst == NFA_HCI_ANY_OK) ? NFA_STATUS_OK : NFA_STATUS_FAILED;

    switch (nfa_hci_cb.cmd_sent) {
      case NFA_HCI_ANY_SET_PARAMETER:
        if (nfa_hci_cb.hci_state == NFA_HCI_STATE_APP_DEREGISTER)
          nfa_hci_api_deregister(nullptr);
        else if (nfa_hci_cb.hci_state == NFA_HCI_STATE_REMOVE_GATE)
          nfa_hci_api_dealloc_gate(nullptr);
        break;

      case NFA_HCI_ANY_GET_PARAMETER:
        if (nfa_hci_cb.param_in_use == NFA_HCI_SESSION_IDENTITY_INDEX) {
          if (data_len >= NFA_HCI_SESSION_ID_LEN &&
              !memcmp((uint8_t*)default_session, p_data,
                      NFA_HCI_SESSION_ID_LEN)) {
            memcpy(&nfa_hci_cb.cfg.admin_gate
                        .session_id[(NFA_HCI_SESSION_ID_LEN / 2)],
                   nfa_hci_cb.cfg.admin_gate.session_id,
                   (NFA_HCI_SESSION_ID_LEN / 2));
            os_tick = GKI_get_os_tick_count();
            memcpy(nfa_hci_cb.cfg.admin_gate.session_id, (uint8_t*)&os_tick,
                   (NFA_HCI_SESSION_ID_LEN / 2));
            nfa_hci_cb.nv_write_needed = true;
            nfa_hciu_send_set_param_cmd(
                NFA_HCI_ADMIN_PIPE, NFA_HCI_SESSION_IDENTITY_INDEX,
                NFA_HCI_SESSION_ID_LEN,
                (uint8_t*)nfa_hci_cb.cfg.admin_gate.session_id);
          } else {
            if (data_len < NFA_HCI_SESSION_ID_LEN) {
              android_errorWriteLog(0x534e4554, "124524315");
            }
            if (nfa_hci_cb.hci_state == NFA_HCI_STATE_APP_DEREGISTER)
              nfa_hci_api_deregister(nullptr);
            else if (nfa_hci_cb.hci_state == NFA_HCI_STATE_REMOVE_GATE)
              nfa_hci_api_dealloc_gate(nullptr);
          }
        } else if (nfa_hci_cb.param_in_use == NFA_HCI_HOST_LIST_INDEX) {
          evt_data.hosts.status = status;
          if (data_len > NFA_HCI_MAX_HOST_IN_NETWORK) {
            data_len = NFA_HCI_MAX_HOST_IN_NETWORK;
            android_errorWriteLog(0x534e4554, "124524315");
          }
          evt_data.hosts.num_hosts = data_len;
          memcpy(evt_data.hosts.host, p_data, data_len);
#if(NXP_EXTNS == TRUE)
          nfa_hciu_update_host_list(data_len , p_data);
#else
          host_count = 0;
          while (host_count < NFA_HCI_MAX_HOST_IN_NETWORK) {
            nfa_hci_cb.inactive_host[host_count] =
                NFA_HCI_HOST_ID_UICC0 + host_count;
            host_count++;
          }

          host_count = 0;
          /* Collect active host in the Host Network */
          while (host_count < data_len) {
            host_id = (uint8_t)*p_data++;

            if ((host_id >= NFA_HCI_HOST_ID_UICC0) &&
                (host_id <
                 NFA_HCI_HOST_ID_UICC0 + NFA_HCI_MAX_HOST_IN_NETWORK)) {
              nfa_hci_cb.inactive_host[host_id - NFA_HCI_HOST_ID_UICC0] = 0x00;
              nfa_hci_cb.reset_host[host_id - NFA_HCI_HOST_ID_UICC0] = 0x00;
            }
            host_count++;
          }
#endif
          if (nfa_hciu_is_no_host_resetting())
            nfa_hci_check_pending_api_requests();
          nfa_hciu_send_to_app(NFA_HCI_HOST_LIST_EVT, &evt_data,
                               nfa_hci_cb.app_in_use);
        }
        break;

      case NFA_HCI_ADM_CREATE_PIPE:
        // p_data should have at least 5 bytes length for pipe info
        if (data_len >= 5 && status == NFA_STATUS_OK) {
          STREAM_TO_UINT8(source_host, p_data);
          STREAM_TO_UINT8(source_gate, p_data);
          STREAM_TO_UINT8(dest_host, p_data);
          STREAM_TO_UINT8(dest_gate, p_data);
          STREAM_TO_UINT8(pipe, p_data);

          /* Sanity check */
          if (source_gate != nfa_hci_cb.local_gate_in_use) {
            LOG(WARNING) << StringPrintf(
                "nfa_hci_handle_admin_gate_rsp sent create pipe with gate: %u "
                "got back: %u",
                nfa_hci_cb.local_gate_in_use, source_gate);
            break;
          }

          nfa_hciu_add_pipe_to_gate(pipe, source_gate, dest_host, dest_gate);
        } else if (data_len < 5 && status == NFA_STATUS_OK) {
          android_errorWriteLog(0x534e4554, "124524315");
          status = NFA_STATUS_FAILED;
        }

        /* Tell the application his pipe was created or not */
        evt_data.created.status = status;
        evt_data.created.pipe = pipe;
        evt_data.created.source_gate = source_gate;
        evt_data.created.dest_host = dest_host;
        evt_data.created.dest_gate = dest_gate;

        nfa_hciu_send_to_app(NFA_HCI_CREATE_PIPE_EVT, &evt_data,
                             nfa_hci_cb.app_in_use);
        break;

      case NFA_HCI_ADM_DELETE_PIPE:
        if (status == NFA_STATUS_OK) {
          nfa_hciu_release_pipe(nfa_hci_cb.pipe_in_use);

          /* If only deleting one pipe, tell the app we are done */
          if (nfa_hci_cb.hci_state == NFA_HCI_STATE_IDLE) {
            evt_data.deleted.status = status;
            evt_data.deleted.pipe = nfa_hci_cb.pipe_in_use;

            nfa_hciu_send_to_app(NFA_HCI_DELETE_PIPE_EVT, &evt_data,
                                 nfa_hci_cb.app_in_use);
          } else if (nfa_hci_cb.hci_state == NFA_HCI_STATE_APP_DEREGISTER)
            nfa_hci_api_deregister(nullptr);
          else if (nfa_hci_cb.hci_state == NFA_HCI_STATE_REMOVE_GATE)
            nfa_hci_api_dealloc_gate(nullptr);
        } else {
          /* If only deleting one pipe, tell the app we are done */
          if (nfa_hci_cb.hci_state == NFA_HCI_STATE_IDLE) {
            evt_data.deleted.status = status;
            evt_data.deleted.pipe = nfa_hci_cb.pipe_in_use;

            nfa_hciu_send_to_app(NFA_HCI_DELETE_PIPE_EVT, &evt_data,
                                 nfa_hci_cb.app_in_use);
          } else if (nfa_hci_cb.hci_state == NFA_HCI_STATE_APP_DEREGISTER) {
            nfa_hciu_release_pipe(nfa_hci_cb.pipe_in_use);
            nfa_hci_api_deregister(nullptr);
          } else if (nfa_hci_cb.hci_state == NFA_HCI_STATE_REMOVE_GATE) {
            nfa_hciu_release_pipe(nfa_hci_cb.pipe_in_use);
            nfa_hci_api_dealloc_gate(nullptr);
          }
        }
        break;

      case NFA_HCI_ANY_OPEN_PIPE:
        nfa_hci_cb.cfg.admin_gate.pipe01_state =
            status ? NFA_HCI_PIPE_CLOSED : NFA_HCI_PIPE_OPENED;
        nfa_hci_cb.nv_write_needed = true;
        if (nfa_hci_cb.cfg.admin_gate.pipe01_state == NFA_HCI_PIPE_OPENED) {
          /* First thing is to get the session ID */
          nfa_hciu_send_get_param_cmd(NFA_HCI_ADMIN_PIPE,
                                      NFA_HCI_SESSION_IDENTITY_INDEX);
        }
        break;

      case NFA_HCI_ADM_CLEAR_ALL_PIPE:
        nfa_hciu_remove_all_pipes_from_host(0);
        nfa_hci_cb.cfg.admin_gate.pipe01_state = NFA_HCI_PIPE_CLOSED;
        nfa_hci_cb.cfg.link_mgmt_gate.pipe00_state = NFA_HCI_PIPE_CLOSED;
        nfa_hci_cb.nv_write_needed = true;
        /* Open admin */
        nfa_hciu_send_open_pipe_cmd(NFA_HCI_ADMIN_PIPE);
        break;
    }
  }
}

/*******************************************************************************
**
** Function         nfa_hci_handle_admin_gate_evt
**
** Description      This function handles events received on admin gate
**
** Returns          none
**
*******************************************************************************/
void nfa_hci_handle_admin_gate_evt() {
  tNFA_HCI_EVT_DATA evt_data;
  tNFA_HCI_API_GET_HOST_LIST* p_msg;

  if (nfa_hci_cb.inst != NFA_HCI_EVT_HOT_PLUG) {
    LOG(ERROR) << StringPrintf(
        "nfa_hci_handle_admin_gate_evt - Unknown event on ADMIN Pipe");
    return;
  }

  LOG(DEBUG) << StringPrintf(
      "nfa_hci_handle_admin_gate_evt - HOT PLUG EVT event on ADMIN Pipe");
  nfa_hci_cb.num_hot_plug_evts++;

  if ((nfa_hci_cb.hci_state == NFA_HCI_STATE_WAIT_NETWK_ENABLE) ||
      (nfa_hci_cb.hci_state == NFA_HCI_STATE_RESTORE_NETWK_ENABLE)) {
    /* Received Hot Plug evt while waiting for other Host in the network to
     * bootup after DH host bootup is complete */
    if ((nfa_hci_cb.ee_disable_disc) &&
        (nfa_hci_cb.num_hot_plug_evts == (nfa_hci_cb.num_nfcee - 1)) &&
        (nfa_hci_cb.num_ee_dis_req_ntf < (nfa_hci_cb.num_nfcee - 1))) {
      /* Received expected number of Hot Plug event(s) before as many number of
       * EE DISC REQ Ntf(s) are received */
      nfa_sys_stop_timer(&nfa_hci_cb.timer);
      /* Received HOT PLUG EVT(s), now wait some more time for EE DISC REQ
       * Ntf(s) */
      nfa_sys_start_timer(&nfa_hci_cb.timer, NFA_HCI_RSP_TIMEOUT_EVT,
                          p_nfa_hci_cfg->hci_netwk_enable_timeout);
    }
  } else if ((nfa_hci_cb.hci_state == NFA_HCI_STATE_STARTUP) ||
             (nfa_hci_cb.hci_state == NFA_HCI_STATE_RESTORE)) {
    /* Received Hot Plug evt during DH host bootup */
    if ((nfa_hci_cb.ee_disable_disc) &&
        (nfa_hci_cb.num_hot_plug_evts == (nfa_hci_cb.num_nfcee - 1)) &&
        (nfa_hci_cb.num_ee_dis_req_ntf < (nfa_hci_cb.num_nfcee - 1))) {
      /* Received expected number of Hot Plug event(s) before as many number of
       * EE DISC REQ Ntf(s) are received */
      nfa_hci_cb.w4_hci_netwk_init = false;
    }
  } else {
    /* Received Hot Plug evt on UICC self reset */
    evt_data.rcvd_evt.evt_code = nfa_hci_cb.inst;
    /* Notify all registered application with the HOT_PLUG_EVT */
    nfa_hciu_send_to_all_apps(NFA_HCI_EVENT_RCVD_EVT, &evt_data);

    /* Send Get Host List after receiving any pending response */
    p_msg = (tNFA_HCI_API_GET_HOST_LIST*)GKI_getbuf(
        sizeof(tNFA_HCI_API_GET_HOST_LIST));
    if (p_msg != nullptr) {
      p_msg->hdr.event = NFA_HCI_API_GET_HOST_LIST_EVT;
      /* Set Invalid handle to identify this Get Host List command is internal
       */
      p_msg->hci_handle = NFA_HANDLE_INVALID;

      nfa_sys_sendmsg(p_msg);
    }
  }
}

/*******************************************************************************
**
** Function         nfa_hci_handle_dyn_pipe_pkt
**
** Description      This function handles data received via dynamic pipe
**
** Returns          none
**
*******************************************************************************/
void nfa_hci_handle_dyn_pipe_pkt(uint8_t pipe_id, uint8_t* p_data,
                                 uint16_t data_len) {
  tNFA_HCI_DYN_PIPE* p_pipe = nfa_hciu_find_pipe_by_pid(pipe_id);
  tNFA_HCI_DYN_GATE* p_gate;

  if (p_pipe == nullptr) {
    /* Invalid pipe ID */
    LOG(ERROR) << StringPrintf("nfa_hci_handle_dyn_pipe_pkt - Unknown pipe %d",
                               pipe_id);
    if (nfa_hci_cb.type == NFA_HCI_COMMAND_TYPE)
      nfa_hciu_send_msg(pipe_id, NFA_HCI_RESPONSE_TYPE, NFA_HCI_ANY_E_NOK, 0,
                        nullptr);
    return;
  }

  if (p_pipe->local_gate == NFA_HCI_IDENTITY_MANAGEMENT_GATE) {
    nfa_hci_handle_identity_mgmt_gate_pkt(p_data, p_pipe);
#if(NXP_EXTNS == TRUE)
  } else if (p_pipe->local_gate == NFA_HCI_ID_MNGMNT_APP_GATE) {
      nfa_hci_handle_identity_mgmt_app_gate_hcp_msg_data (p_data, data_len, p_pipe);
  } else if (p_pipe->local_gate == NFA_HCI_APDU_APP_GATE) {
      nfa_hci_handle_apdu_app_gate_hcp_msg_data (p_data, data_len, p_pipe);
#endif
  } else if (p_pipe->local_gate == NFA_HCI_LOOP_BACK_GATE) {
    nfa_hci_handle_loopback_gate_pkt(p_data, data_len, p_pipe);
  } else if (p_pipe->local_gate == NFA_HCI_CONNECTIVITY_GATE) {
    nfa_hci_handle_connectivity_gate_pkt(p_data, data_len, p_pipe);
  } else {
    p_gate = nfa_hciu_find_gate_by_gid(p_pipe->local_gate);
    if (p_gate == nullptr) {
      LOG(ERROR) << StringPrintf(
          "nfa_hci_handle_dyn_pipe_pkt - Pipe's gate %d is corrupt",
          p_pipe->local_gate);
      if (nfa_hci_cb.type == NFA_HCI_COMMAND_TYPE)
        nfa_hciu_send_msg(pipe_id, NFA_HCI_RESPONSE_TYPE, NFA_HCI_ANY_E_NOK, 0,
                          nullptr);
      return;
    }

    /* Check if data packet is a command, response or event */
    switch (nfa_hci_cb.type) {
      case NFA_HCI_COMMAND_TYPE:
        nfa_hci_handle_generic_gate_cmd(p_data, (uint8_t)data_len, p_pipe);
        break;

      case NFA_HCI_RESPONSE_TYPE:
        nfa_hci_handle_generic_gate_rsp(p_data, (uint8_t)data_len, p_pipe);
        break;

      case NFA_HCI_EVENT_TYPE:
        nfa_hci_handle_generic_gate_evt(p_data, data_len, p_gate, p_pipe);
        break;
    }
  }
}

/*******************************************************************************
**
** Function         nfa_hci_handle_identity_mgmt_gate_pkt
**
** Description      This function handles incoming Identity Management gate hci
**                  commands
**
** Returns          none
**
*******************************************************************************/
static void nfa_hci_handle_identity_mgmt_gate_pkt(uint8_t* p_data,
                                                  tNFA_HCI_DYN_PIPE* p_pipe) {
  uint8_t data[20];
  uint8_t index;
  uint8_t gate_rsp[3 + NFA_HCI_MAX_GATE_CB], num_gates;
  uint16_t rsp_len = 0;
  uint8_t* p_rsp = data;
  tNFA_HCI_RESPONSE response = NFA_HCI_ANY_OK;

  /* We never send commands on a pipe where the local gate is the identity
   * management
   * gate, so only commands should be processed.
   */
  if (nfa_hci_cb.type != NFA_HCI_COMMAND_TYPE) return;

  switch (nfa_hci_cb.inst) {
    case NFA_HCI_ANY_GET_PARAMETER:
      index = *(p_data++);
      if (p_pipe->pipe_state == NFA_HCI_PIPE_OPENED) {
        switch (index) {
          case NFA_HCI_VERSION_SW_INDEX:
            data[0] = (uint8_t)((NFA_HCI_VERSION_SW >> 16) & 0xFF);
            data[1] = (uint8_t)((NFA_HCI_VERSION_SW >> 8) & 0xFF);
            data[2] = (uint8_t)((NFA_HCI_VERSION_SW)&0xFF);
            rsp_len = 3;
            break;

          case NFA_HCI_HCI_VERSION_INDEX:
            data[0] = NFA_HCI_VERSION;
            rsp_len = 1;
            break;

          case NFA_HCI_VERSION_HW_INDEX:
            data[0] = (uint8_t)((NFA_HCI_VERSION_HW >> 16) & 0xFF);
            data[1] = (uint8_t)((NFA_HCI_VERSION_HW >> 8) & 0xFF);
            data[2] = (uint8_t)((NFA_HCI_VERSION_HW)&0xFF);
            rsp_len = 3;
            break;

          case NFA_HCI_VENDOR_NAME_INDEX:
            memcpy(data, NFA_HCI_VENDOR_NAME, strlen(NFA_HCI_VENDOR_NAME));
            rsp_len = (uint8_t)strlen(NFA_HCI_VENDOR_NAME);
            break;

          case NFA_HCI_MODEL_ID_INDEX:
            data[0] = NFA_HCI_MODEL_ID;
            rsp_len = 1;
            break;

          case NFA_HCI_GATES_LIST_INDEX:
            gate_rsp[0] = NFA_HCI_LOOP_BACK_GATE;
            gate_rsp[1] = NFA_HCI_IDENTITY_MANAGEMENT_GATE;
            gate_rsp[2] = NFA_HCI_CONNECTIVITY_GATE;
            num_gates = nfa_hciu_get_allocated_gate_list(&gate_rsp[3]);
            rsp_len = num_gates + 3;
            p_rsp = gate_rsp;
            break;

          default:
            response = NFA_HCI_ANY_E_NOK;
            break;
        }
      } else {
        response = NFA_HCI_ANY_E_PIPE_NOT_OPENED;
      }
      break;

    case NFA_HCI_ANY_OPEN_PIPE:
      data[0] = 0;
      rsp_len = 1;
      p_pipe->pipe_state = NFA_HCI_PIPE_OPENED;
      break;

    case NFA_HCI_ANY_CLOSE_PIPE:
      p_pipe->pipe_state = NFA_HCI_PIPE_CLOSED;
      break;

    default:
      response = NFA_HCI_ANY_E_CMD_NOT_SUPPORTED;
      break;
  }

  nfa_hciu_send_msg(p_pipe->pipe_id, NFA_HCI_RESPONSE_TYPE, response, rsp_len,
                    p_rsp);
}

/*******************************************************************************
**
** Function         nfa_hci_handle_generic_gate_cmd
**
** Description      This function handles all generic gates (excluding
**                  connectivity gate) commands
**
** Returns          none
**
*******************************************************************************/
static void nfa_hci_handle_generic_gate_cmd(uint8_t* p_data, uint8_t data_len,
                                            tNFA_HCI_DYN_PIPE* p_pipe) {
  tNFA_HCI_EVT_DATA evt_data;
  tNFA_HANDLE app_handle = nfa_hciu_get_pipe_owner(p_pipe->pipe_id);

  switch (nfa_hci_cb.inst) {
    case NFA_HCI_ANY_SET_PARAMETER:
      evt_data.registry.pipe = p_pipe->pipe_id;
      evt_data.registry.index = *p_data++;
      if (data_len > 0) data_len--;
      evt_data.registry.data_len = data_len;

      memcpy(evt_data.registry.reg_data, p_data, data_len);

      nfa_hciu_send_to_app(NFA_HCI_SET_REG_CMD_EVT, &evt_data, app_handle);
      break;

    case NFA_HCI_ANY_GET_PARAMETER:
      evt_data.registry.pipe = p_pipe->pipe_id;
      evt_data.registry.index = *p_data;
      evt_data.registry.data_len = 0;

      nfa_hciu_send_to_app(NFA_HCI_GET_REG_CMD_EVT, &evt_data, app_handle);
      break;

    case NFA_HCI_ANY_OPEN_PIPE:
      nfa_hci_handle_pipe_open_close_cmd(p_pipe);

      evt_data.opened.pipe = p_pipe->pipe_id;
      evt_data.opened.status = NFA_STATUS_OK;

      nfa_hciu_send_to_app(NFA_HCI_OPEN_PIPE_EVT, &evt_data, app_handle);
      break;

    case NFA_HCI_ANY_CLOSE_PIPE:
      nfa_hci_handle_pipe_open_close_cmd(p_pipe);

      evt_data.closed.pipe = p_pipe->pipe_id;
      evt_data.opened.status = NFA_STATUS_OK;

      nfa_hciu_send_to_app(NFA_HCI_CLOSE_PIPE_EVT, &evt_data, app_handle);
      break;

    default:
      /* Could be application specific command, pass it on */
      evt_data.cmd_rcvd.status = NFA_STATUS_OK;
      evt_data.cmd_rcvd.pipe = p_pipe->pipe_id;
      ;
      evt_data.cmd_rcvd.cmd_code = nfa_hci_cb.inst;
      evt_data.cmd_rcvd.cmd_len = data_len;

      if (data_len <= NFA_MAX_HCI_CMD_LEN)
        memcpy(evt_data.cmd_rcvd.cmd_data, p_data, data_len);

      nfa_hciu_send_to_app(NFA_HCI_CMD_RCVD_EVT, &evt_data, app_handle);
      break;
  }
}

/*******************************************************************************
**
** Function         nfa_hci_handle_generic_gate_rsp
**
** Description      This function handles all generic gates (excluding
**                  connectivity) response
**
** Returns          none
**
*******************************************************************************/
static void nfa_hci_handle_generic_gate_rsp(uint8_t* p_data, uint8_t data_len,
                                            tNFA_HCI_DYN_PIPE* p_pipe) {
  tNFA_HCI_EVT_DATA evt_data;
  tNFA_STATUS status = NFA_STATUS_OK;

  if (nfa_hci_cb.inst != NFA_HCI_ANY_OK) status = NFA_STATUS_FAILED;

  if (nfa_hci_cb.cmd_sent == NFA_HCI_ANY_OPEN_PIPE) {
    if (status == NFA_STATUS_OK) p_pipe->pipe_state = NFA_HCI_PIPE_OPENED;

    nfa_hci_cb.nv_write_needed = true;
    /* Tell application */
    evt_data.opened.status = status;
    evt_data.opened.pipe = p_pipe->pipe_id;

    nfa_hciu_send_to_app(NFA_HCI_OPEN_PIPE_EVT, &evt_data,
                         nfa_hci_cb.app_in_use);
  } else if (nfa_hci_cb.cmd_sent == NFA_HCI_ANY_CLOSE_PIPE) {
    p_pipe->pipe_state = NFA_HCI_PIPE_CLOSED;

    nfa_hci_cb.nv_write_needed = true;
    /* Tell application */
    evt_data.opened.status = status;
    ;
    evt_data.opened.pipe = p_pipe->pipe_id;

    nfa_hciu_send_to_app(NFA_HCI_CLOSE_PIPE_EVT, &evt_data,
                         nfa_hci_cb.app_in_use);
  } else if (nfa_hci_cb.cmd_sent == NFA_HCI_ANY_GET_PARAMETER) {
    /* Tell application */
    evt_data.registry.status = status;
    evt_data.registry.pipe = p_pipe->pipe_id;
    evt_data.registry.data_len = data_len;
    evt_data.registry.index = nfa_hci_cb.param_in_use;

    memcpy(evt_data.registry.reg_data, p_data, data_len);

    nfa_hciu_send_to_app(NFA_HCI_GET_REG_RSP_EVT, &evt_data,
                         nfa_hci_cb.app_in_use);
  } else if (nfa_hci_cb.cmd_sent == NFA_HCI_ANY_SET_PARAMETER) {
    /* Tell application */
    evt_data.registry.status = status;
    ;
    evt_data.registry.pipe = p_pipe->pipe_id;

    nfa_hciu_send_to_app(NFA_HCI_SET_REG_RSP_EVT, &evt_data,
                         nfa_hci_cb.app_in_use);
  } else {
    /* Could be a response to application specific command sent, pass it on */
    evt_data.rsp_rcvd.status = NFA_STATUS_OK;
    evt_data.rsp_rcvd.pipe = p_pipe->pipe_id;
    ;
    evt_data.rsp_rcvd.rsp_code = nfa_hci_cb.inst;
    evt_data.rsp_rcvd.rsp_len = data_len;

    if (data_len <= NFA_MAX_HCI_RSP_LEN)
      memcpy(evt_data.rsp_rcvd.rsp_data, p_data, data_len);

    nfa_hciu_send_to_app(NFA_HCI_RSP_RCVD_EVT, &evt_data,
                         nfa_hci_cb.app_in_use);
  }
}

/*******************************************************************************
**
** Function         nfa_hci_handle_connectivity_gate_pkt
**
** Description      This function handles incoming connectivity gate packets
**
** Returns          none
**
*******************************************************************************/
static void nfa_hci_handle_connectivity_gate_pkt(uint8_t* p_data,
                                                 uint16_t data_len,
                                                 tNFA_HCI_DYN_PIPE* p_pipe) {
  tNFA_HCI_EVT_DATA evt_data;

  if (nfa_hci_cb.type == NFA_HCI_COMMAND_TYPE) {
    switch (nfa_hci_cb.inst) {
      case NFA_HCI_ANY_OPEN_PIPE:
      case NFA_HCI_ANY_CLOSE_PIPE:
        nfa_hci_handle_pipe_open_close_cmd(p_pipe);
        break;

      case NFA_HCI_CON_PRO_HOST_REQUEST:
        /* A request to the DH to activate another host. This is not supported
         * for */
        /* now, we will implement it when the spec is clearer and UICCs need it.
         */
        nfa_hciu_send_msg(p_pipe->pipe_id, NFA_HCI_RESPONSE_TYPE,
                          NFA_HCI_ANY_E_CMD_NOT_SUPPORTED, 0, nullptr);
        break;

      default:
        nfa_hciu_send_msg(p_pipe->pipe_id, NFA_HCI_RESPONSE_TYPE,
                          NFA_HCI_ANY_E_CMD_NOT_SUPPORTED, 0, nullptr);
        break;
    }
  } else if (nfa_hci_cb.type == NFA_HCI_RESPONSE_TYPE) {
    if ((nfa_hci_cb.cmd_sent == NFA_HCI_ANY_OPEN_PIPE) &&
        (nfa_hci_cb.inst == NFA_HCI_ANY_OK))
      p_pipe->pipe_state = NFA_HCI_PIPE_OPENED;
    else if (nfa_hci_cb.cmd_sent == NFA_HCI_ANY_CLOSE_PIPE)
      p_pipe->pipe_state = NFA_HCI_PIPE_CLOSED;

    /* Could be a response to application specific command sent, pass it on */
    evt_data.rsp_rcvd.status = NFA_STATUS_OK;
    evt_data.rsp_rcvd.pipe = p_pipe->pipe_id;
    ;
    evt_data.rsp_rcvd.rsp_code = nfa_hci_cb.inst;
    evt_data.rsp_rcvd.rsp_len = data_len;

    if (data_len <= NFA_MAX_HCI_RSP_LEN)
      memcpy(evt_data.rsp_rcvd.rsp_data, p_data, data_len);

    nfa_hciu_send_to_app(NFA_HCI_RSP_RCVD_EVT, &evt_data,
                         nfa_hci_cb.app_in_use);
  } else if (nfa_hci_cb.type == NFA_HCI_EVENT_TYPE) {
#if(NXP_EXTNS == TRUE)
      /* If its a valid Connectivity gate event then pass it on
      ** to all apps registered for connectivity gate events
      */
    if ((nfa_hci_cb.inst == NFA_HCI_EVT_CONNECTIVITY) ||
        (nfa_hci_cb.inst == NFA_HCI_EVT_TRANSACTION) ||
        (nfa_hci_cb.inst == NFA_HCI_EVT_DPD_MONITOR) ||
        (nfa_hci_cb.inst == NFA_HCI_EVT_OPERATION_ENDED)) {
#endif
        evt_data.rcvd_evt.pipe = p_pipe->pipe_id;
        evt_data.rcvd_evt.evt_code = nfa_hci_cb.inst;
        evt_data.rcvd_evt.evt_len = data_len;
        evt_data.rcvd_evt.p_evt_buf = p_data;
#if(NXP_EXTNS == TRUE && NXP_SRD == TRUE)
        if (nfa_srd_check_hci_evt(&evt_data)) {
          return;
        }
#endif

        /* notify NFA_HCI_EVENT_RCVD_EVT to the application */
        nfa_hciu_send_to_apps_handling_connectivity_evts(NFA_HCI_EVENT_RCVD_EVT,
                                                         &evt_data);
#if(NXP_EXTNS == TRUE)
      }
#endif
  }
}

/*******************************************************************************
**
** Function         nfa_hci_handle_loopback_gate_pkt
**
** Description      This function handles incoming loopback gate hci events
**
** Returns          none
**
*******************************************************************************/
static void nfa_hci_handle_loopback_gate_pkt(uint8_t* p_data, uint16_t data_len,
                                             tNFA_HCI_DYN_PIPE* p_pipe) {
  uint8_t data[1];
  uint8_t rsp_len = 0;
  tNFA_HCI_RESPONSE response = NFA_HCI_ANY_OK;
  tNFA_HCI_EVT_DATA evt_data;

  /* Check if data packet is a command, response or event */
  if (nfa_hci_cb.type == NFA_HCI_COMMAND_TYPE) {
    if (nfa_hci_cb.inst == NFA_HCI_ANY_OPEN_PIPE) {
      data[0] = 0;
      rsp_len = 1;
      p_pipe->pipe_state = NFA_HCI_PIPE_OPENED;
    } else if (nfa_hci_cb.inst == NFA_HCI_ANY_CLOSE_PIPE) {
      p_pipe->pipe_state = NFA_HCI_PIPE_CLOSED;
    } else
      response = NFA_HCI_ANY_E_CMD_NOT_SUPPORTED;

    nfa_hciu_send_msg(p_pipe->pipe_id, NFA_HCI_RESPONSE_TYPE, response, rsp_len,
                      data);
  } else if (nfa_hci_cb.type == NFA_HCI_RESPONSE_TYPE) {
    if ((nfa_hci_cb.cmd_sent == NFA_HCI_ANY_OPEN_PIPE) &&
        (nfa_hci_cb.inst == NFA_HCI_ANY_OK))
      p_pipe->pipe_state = NFA_HCI_PIPE_OPENED;
    else if (nfa_hci_cb.cmd_sent == NFA_HCI_ANY_CLOSE_PIPE)
      p_pipe->pipe_state = NFA_HCI_PIPE_CLOSED;

    /* Could be a response to application specific command sent, pass it on */
    evt_data.rsp_rcvd.status = NFA_STATUS_OK;
    evt_data.rsp_rcvd.pipe = p_pipe->pipe_id;
    ;
    evt_data.rsp_rcvd.rsp_code = nfa_hci_cb.inst;
    evt_data.rsp_rcvd.rsp_len = data_len;

    if (data_len <= NFA_MAX_HCI_RSP_LEN)
      memcpy(evt_data.rsp_rcvd.rsp_data, p_data, data_len);

    nfa_hciu_send_to_app(NFA_HCI_RSP_RCVD_EVT, &evt_data,
                         nfa_hci_cb.app_in_use);
  } else if (nfa_hci_cb.type == NFA_HCI_EVENT_TYPE) {
    if (nfa_hci_cb.w4_rsp_evt) {
      evt_data.rcvd_evt.pipe = p_pipe->pipe_id;
      evt_data.rcvd_evt.evt_code = nfa_hci_cb.inst;
      evt_data.rcvd_evt.evt_len = data_len;
      evt_data.rcvd_evt.p_evt_buf = p_data;

      nfa_hciu_send_to_app(NFA_HCI_EVENT_RCVD_EVT, &evt_data,
                           nfa_hci_cb.app_in_use);
    } else if (nfa_hci_cb.inst == NFA_HCI_EVT_POST_DATA) {
      /* Send back the same data we got */
      nfa_hciu_send_msg(p_pipe->pipe_id, NFA_HCI_EVENT_TYPE,
                        NFA_HCI_EVT_POST_DATA, data_len, p_data);
    }
  }
}

/*******************************************************************************
**
** Function         nfa_hci_handle_generic_gate_evt
**
** Description      This function handles incoming Generic gate hci events
**
** Returns          none
**
*******************************************************************************/
static void nfa_hci_handle_generic_gate_evt(uint8_t* p_data, uint16_t data_len,
                                            tNFA_HCI_DYN_GATE* p_gate,
                                            tNFA_HCI_DYN_PIPE* p_pipe) {
  tNFA_HCI_EVT_DATA evt_data;

  evt_data.rcvd_evt.pipe = p_pipe->pipe_id;
  evt_data.rcvd_evt.evt_code = nfa_hci_cb.inst;
  evt_data.rcvd_evt.evt_len = data_len;

#if(NXP_EXTNS == TRUE)
  evt_data.rcvd_evt.status = NFA_STATUS_BUFFER_FULL;

#else
  if (nfa_hci_cb.assembly_failed)
    evt_data.rcvd_evt.status = NFA_STATUS_BUFFER_FULL;
  else
    evt_data.rcvd_evt.status = NFA_STATUS_OK;
#endif

  evt_data.rcvd_evt.p_evt_buf = p_data;
#if(NXP_EXTNS != TRUE)
  nfa_hci_cb.rsp_buf_size = 0;
  nfa_hci_cb.p_rsp_buf = nullptr;
#endif

  /* notify NFA_HCI_EVENT_RCVD_EVT to the application */
  nfa_hciu_send_to_app(NFA_HCI_EVENT_RCVD_EVT, &evt_data, p_gate->gate_owner);
}

#if(NXP_EXTNS == TRUE)
/*******************************************************************************
**
** Function         nfa_hci_getApduAndConnectivity_PipeStatus
**
** Description      API to retrieve APDU & Connectivity pipe created status from
**                  FirmWare
**
** Returns          If NCI command is SUCCESS/FAILED
**
*******************************************************************************/
tNFA_STATUS nfa_hci_getApduAndConnectivity_PipeStatus(uint8_t host_id) {
  tNFA_STATUS status = NFA_STATUS_OK;
  uint8_t p_data[NFA_MAX_HCI_CMD_LEN];
  uint8_t *p = p_data, *parm_len, *num_param;
  memset(p_data, 0, sizeof(p_data));
  NCI_MSG_BLD_HDR0(p, NCI_MT_CMD, NCI_GID_CORE);
  NCI_MSG_BLD_HDR1(p, NCI_MSG_CORE_GET_CONFIG);
  parm_len = p++;
  num_param = p++;
  *num_param = 0x02;
  switch (host_id) {
    case NFA_HCI_FIRST_PROP_HOST: {
      UINT8_TO_STREAM(p, NXP_NFC_SET_CONFIG_PARAM_EXT);
      UINT8_TO_STREAM(p, NXP_NFC_ESE_APDU_PIPE_STATUS);
      UINT8_TO_STREAM(p, NXP_NFC_SET_CONFIG_PARAM_EXT);
      UINT8_TO_STREAM(p, NXP_NFC_ESE_CONN_PIPE_STATUS);
    } break;
    case NFA_HCI_EUICC1_HOST: {
      UINT8_TO_STREAM(p, NXP_NFC_SET_CONFIG_PARAM_EXT);
      UINT8_TO_STREAM(p, NXP_NFC_EUICC_APDU_PIPE_STATUS);
      UINT8_TO_STREAM(p, NXP_NFC_SET_CONFIG_PARAM_EXT_ID1);
      UINT8_TO_STREAM(p, NXP_NFC_EUICC1_CONN_PIPE_STATUS);
    } break;
    case NFA_HCI_EUICC2_HOST: {
      UINT8_TO_STREAM(p, NXP_NFC_SET_CONFIG_PARAM_EXT);
      UINT8_TO_STREAM(p, NXP_NFC_EUICC_APDU_PIPE_STATUS);
      UINT8_TO_STREAM(p, NXP_NFC_SET_CONFIG_PARAM_EXT_ID1);
      UINT8_TO_STREAM(p, NXP_NFC_EUICC2_CONN_PIPE_STATUS);
    } break;
    default:
      *num_param = 0x00;
  }

  *parm_len = (p - num_param);
  if (*num_param != 0x00) {
    status =
        nfa_hciu_send_raw_cmd(p - p_data, p_data, nfa_hci_get_pipe_state_cb);
  }
  LOG(DEBUG) << StringPrintf("nfa_hci_getApduConnectivity_PipeStatus %x",
                             *num_param);

  return status;
}

/*******************************************************************************
**
** Function         nfa_hci_get_pipe_state_cb
**
** Description      Callback API to retrieve APDU & Connectivity pipe created
**                  status from FirmWare
**
** Returns          None
**
*******************************************************************************/
static void nfa_hci_get_pipe_state_cb(__attribute__((unused))uint8_t event, __attribute__((unused))uint16_t param_len, __attribute__((unused))uint8_t* p_param)
{
    uint8_t num_param_id         = 0x00;
    uint8_t NFA_PARAM_ID_INDEX   = 0x04;
    // Vars to hold the ParamID bytes of APDU Pipe status
    uint8_t param_id11 = 0x00, param_id12 = 0x00;
    // Vars to hold the ParamID bytes of CONN Pipe status
    uint8_t param_id21 = 0x00, param_id22 = 0x00;
    uint8_t apdu_pipe_status = NFA_HCI_PIPE_CLOSED;
    uint8_t conn_pipe_status = NFA_HCI_PIPE_CLOSED;
    uint8_t prop_host = NFA_HCI_FIRST_PROP_HOST;
    uint8_t apdu_pipe = NFA_HCI_APDUESE_PIPE;
    uint8_t conn_pipe = NFA_HCI_CONN_ESE_PIPE;

    nfa_sys_stop_timer (&nfa_hci_cb.timer);
    p_param += NFA_PARAM_ID_INDEX;
    STREAM_TO_UINT8(num_param_id , p_param);
    if (num_param_id == 0x02) {
      STREAM_TO_UINT8(param_id11, p_param);
      STREAM_TO_UINT8(param_id12, p_param);
      p_param++;
      STREAM_TO_UINT8(apdu_pipe_status, p_param);
      STREAM_TO_UINT8(param_id21, p_param);
      STREAM_TO_UINT8(param_id22, p_param);
      p_param++;
      STREAM_TO_UINT8(conn_pipe_status, p_param);

      // Code to indentify the hostId and PipeIds
      if (param_id11 == NXP_NFC_SET_CONFIG_PARAM_EXT &&
          param_id21 == NXP_NFC_SET_CONFIG_PARAM_EXT &&
          param_id12 == NXP_NFC_ESE_APDU_PIPE_STATUS &&
          param_id22 == NXP_NFC_ESE_CONN_PIPE_STATUS) {
        //eSE, use default set values
      } else if (param_id11 == NXP_NFC_SET_CONFIG_PARAM_EXT &&
          param_id12 == NXP_NFC_EUICC_APDU_PIPE_STATUS &&
          param_id21 == NXP_NFC_SET_CONFIG_PARAM_EXT_ID1) {
        if (param_id22 == NXP_NFC_EUICC1_CONN_PIPE_STATUS) {
          //eUICC1
          if(nfa_hci_cb.se_apdu_gate_support != NFC_EE_TYPE_EUICC_WITH_APDUPIPE19)
            apdu_pipe = NFA_HCI_APDU_EUICC_PIPE;

          conn_pipe = NFA_HCI_CONN_EUICC1_PIPE;
          prop_host = NFA_HCI_EUICC1_HOST;
        } else {
          //eUICC2
          if(nfa_hci_cb.se_apdu_gate_support != NFC_EE_TYPE_EUICC_WITH_APDUPIPE19)
            apdu_pipe = NFA_HCI_APDU_EUICC_PIPE;

          conn_pipe = NFA_HCI_CONN_EUICC2_PIPE;
          prop_host = NFA_HCI_EUICC2_HOST;
        }
      }
      /* The status can be 0x02, observed for eUICC2.
         NXP prop pipe status mapped to NFA pipe status */
      if (apdu_pipe_status == NXP_NFA_HCI_PIPE_OPENED) {
        apdu_pipe_status = NFA_HCI_PIPE_OPENED;
      }
      if (conn_pipe_status == NXP_NFA_HCI_PIPE_OPENED) {
        conn_pipe_status = NFA_HCI_PIPE_OPENED;
      }
      /*Update eSE APDU pipe status*/
      if (apdu_pipe_status == NFA_HCI_PIPE_OPENED) {
        nfa_hci_update_pipe_status(NFA_HCI_APDU_APP_GATE,
            NFA_HCI_APDU_GATE, apdu_pipe, prop_host);
      } else {
        apdu_pipe_status = NFA_HCI_PIPE_CLOSED;
      }
      /*Update eSE Connectivity pipe status*/
      if (conn_pipe_status == NFA_HCI_PIPE_OPENED) {
        nfa_hci_update_pipe_status(NFA_HCI_CONNECTIVITY_GATE,
            NFA_HCI_CONNECTIVITY_GATE, conn_pipe, prop_host);
      } else {
        conn_pipe_status = NFA_HCI_PIPE_CLOSED;
      }
    }
    /*If pipes are already available*/
    if (apdu_pipe_status == NFA_HCI_PIPE_OPENED &&
        nfa_hci_cb.se_apdu_gate_support) {
      nfa_hciu_send_get_param_cmd(apdu_pipe, NFA_HCI_MAX_C_APDU_SIZE_INDEX);
    } else {
      LOG(DEBUG) << StringPrintf(
          "nfa_hci_get_pipe_state_cb APDU pipe not available");
      if ((nfa_hci_check_nfcee_init_complete(prop_host) || conn_pipe_status) &&
          nfa_hci_cb.se_apdu_gate_support) {
        nfa_hciu_send_create_pipe_cmd(NFA_HCI_APDU_APP_GATE, prop_host,
                                      NFA_HCI_APDU_GATE);
      } else {
        LOG(DEBUG) << StringPrintf(
            "nfa_hci_get_pipe_state_cb APDU pipe not available"
            "wait for status NFCEE_INIT_COMPLETED");
        if ((nfa_hci_cb.hci_state == NFA_HCI_STATE_STARTUP) ||
          (nfa_hci_cb.hci_state == NFA_HCI_STATE_WAIT_NETWK_ENABLE))
        {
          if (!nfa_hci_enable_one_nfcee())
            nfa_hci_startup_complete(NFA_STATUS_OK);
        }
      }
    }
}

/*******************************************************************************
**
** Function         nfa_hci_update_pipe_status
**
** Description      API to update APDU & Connectivity pipe hci_cfg status
**
** Returns          None
**
*******************************************************************************/
static void nfa_hci_update_pipe_status(uint8_t local_gateId,
                                       uint8_t dest_gateId, uint8_t pipeId,
                                       uint8_t hostId) {
  uint8_t count = 0;

  nfa_hciu_add_pipe_to_gate(pipeId, local_gateId, hostId, dest_gateId);

  /*Set the pipe status HCI_OPENED*/
  for (count = 0; count < NFA_HCI_MAX_PIPE_CB; count++) {
    if (((nfa_hci_cb.cfg.dyn_pipes[count].dest_host) == hostId) &&
        ((nfa_hci_cb.cfg.dyn_pipes[count].dest_gate) == dest_gateId) &&
        ((nfa_hci_cb.cfg.dyn_pipes[count].local_gate) == local_gateId)) {
        LOG(DEBUG) << StringPrintf("Set the pipe state to open  -- %d !!!",
                                   nfa_hci_cb.cfg.dyn_pipes[count].pipe_id);
        nfa_hci_cb.cfg.dyn_pipes[count].pipe_state = NFA_HCI_PIPE_OPENED;
        break;
    }
  }
}

/*******************************************************************************
**
** Function         nfa_hci_check_nfcee_init_complete
**
** Description      API to check nfcee init complete, to start pipe creation
**
** Returns          None
**
*******************************************************************************/
static bool nfa_hci_check_nfcee_init_complete(uint8_t host_id)
{
    int yy = 0;
    bool recreate_pipe = false;
    for (yy = 0; yy < NFA_HCI_MAX_HOST_IN_NETWORK; yy++) {
      if((nfa_hci_cb.reset_host[yy].reset_cfg & NFCEE_INIT_COMPLETED) &&
        (nfa_hci_cb.reset_host[yy].host_id == host_id)) {
        recreate_pipe = true;
        LOG(DEBUG) << StringPrintf(
            "nfa_hci_enable_one_nfcee reset_cfg NFCEE_INIT_COMPLETED()");
        nfa_hciu_clear_host_resetting(host_id, NFCEE_INIT_COMPLETED);
        break;
      }
    }
    return recreate_pipe;
}

/*******************************************************************************
**
** Function         nfa_hci_notify_w4_atr_timeout
**
** Description      API to check w4_atr true report app about timeout
**
** Returns          None
**
*******************************************************************************/
static void nfa_hci_notify_w4_atr_timeout()
{
  tNFA_HCI_EVT_DATA         evt_data;
  evt_data.apdu_aborted.status  = NFA_STATUS_TIMEOUT;
  evt_data.apdu_aborted.host_id = 0;
  /* Send NFA_HCI_APDU_ABORTED_EVT to notify status */
  nfa_hciu_send_to_all_apps (NFA_HCI_APDU_ABORTED_EVT, &evt_data);
}
/*******************************************************************************
**
** Function         nfa_hci_set_apdu_pipe_ready_for_host
**
** Description      Set APDU pipe for specified host ready for sending APDU
**                  transaction on it
**
** Returns          TRUE, if APDU pipe set ready operation is compelete
**                  FALSE, not ready yet
**
*******************************************************************************/
static bool nfa_hci_set_apdu_pipe_ready_for_host (uint8_t host_id)
{
    uint8_t             gate_id;
    tNFA_HCI_DYN_PIPE *p_id_mgmnt_pipe;
    tNFA_HCI_DYN_PIPE *p_apdu_pipe;
    tNFA_HCI_DYN_PIPE *p_conn_pipe;

    if ((p_id_mgmnt_pipe = nfa_hciu_find_id_pipe_for_host (host_id)) == nullptr)
    {
        nfa_hciu_reset_apdu_pipe_registry_info_of_host (host_id);
        nfa_hciu_reset_gate_list_of_host (host_id);
        nfa_hci_cb.app_in_use = NFA_HCI_APP_HANDLE_NONE;
        nfa_hciu_send_create_pipe_cmd (NFA_HCI_ID_MNGMNT_APP_GATE, host_id,
                NFA_HCI_IDENTITY_MANAGEMENT_GATE);
    }
    else
    {
        if (p_id_mgmnt_pipe->pipe_state == NFA_HCI_PIPE_CLOSED)
        {
            nfa_hciu_reset_apdu_pipe_registry_info_of_host (host_id);
            nfa_hciu_reset_gate_list_of_host (host_id);
            nfa_hciu_send_open_pipe_cmd (p_id_mgmnt_pipe->pipe_id);
        }
        else
        {
            /* ID Management Pipe in open state */
            if (((p_apdu_pipe = nfa_hciu_find_dyn_apdu_pipe_for_host (host_id)) == nullptr &&
                    nfa_hci_cb.se_apdu_gate_support) || ((p_conn_pipe =
                            nfa_hciu_find_dyn_conn_pipe_for_host (host_id)) == nullptr))
            {
                /* No APDU Pipe, Check if APDU gate or General Purpose APDU gate exist */
                gate_id = nfa_hciu_find_server_apdu_gate_for_host (host_id);
                if (gate_id != 0)
                {
                    /* APDU Gate exist with no APDU Pipe on it, create now*/
                    nfa_hci_cb.app_in_use = NFA_HCI_APP_HANDLE_NONE;
                    if (IS_PROP_HOST(host_id)) {
                      nfa_hci_getApduAndConnectivity_PipeStatus(host_id);
                    } else {
                      if (nfa_hci_check_nfcee_init_complete(host_id))
                        nfa_hciu_send_create_pipe_cmd (NFA_HCI_APDU_APP_GATE,
                                                   host_id, gate_id);
                    }
                }
                else
                {
                    /* No APDU/General Purpose APDU Gate exist, Proceed to next UICC host */
                    return false;
                }
            }
            else
            {
                /* APDU Pipe exist */
                if ((p_apdu_pipe != nullptr) && (p_apdu_pipe->pipe_state
                        == NFA_HCI_PIPE_CLOSED))
                {
                    nfa_hciu_send_open_pipe_cmd (p_apdu_pipe->pipe_id);
                }
                else
                {
                    LOG(DEBUG) << StringPrintf(
                        "%s : APDU gate info not available", __func__);
                    return false;
                }
            }
        }
    }
    return true;
}

/*******************************************************************************
 **
 ** Function         nfa_hci_handle_pending_host_reset
 **
 ** Description      Find next active UICC/eSE reset pending in the reset cb.
 **
 ** Returns          TRUE, if all supported APDU pipes are ready
 **                  FALSE, if at least one APDU pipe is not ready yet
 **
 *******************************************************************************/
void nfa_hci_handle_pending_host_reset() {
    uint8_t xx;
    LOG(DEBUG) << StringPrintf("nfa_hci_handle_pending_host_reset");
    for(xx = 0; xx < NFA_HCI_MAX_HOST_IN_NETWORK; xx++) {
      if(nfa_hci_cb.reset_host[xx].reset_cfg & NFCEE_INIT_COMPLETED) {
        //nfa_hciu_clear_host_resetting(nfa_hci_cb.curr_nfcee, NFCEE_INIT_COMPLETED);
        tNFA_HCI_EVT_DATA             evt_data;
        evt_data.status  = NFA_STATUS_OK;
        evt_data.rcvd_evt.evt_code = NFA_HCI_EVT_INIT_COMPLETED;
        nfa_hciu_send_to_all_apps(NFA_HCI_EVENT_RCVD_EVT, &evt_data);
      }
      else if(nfa_hci_cb.reset_host[xx].reset_cfg & NFCEE_HCI_NOTIFY_ALL_PIPE_CLEARED) {
        nfa_hci_handle_clear_all_pipe_cmd(nfa_hci_cb.reset_host[xx].host_id);
        break;
      } else if (nfa_hci_cb.reset_host[xx].reset_cfg & NFCEE_UNRECOVERABLE_ERRROR) {
          nfa_hci_release_transceive(nfa_hci_cb.reset_host[xx].host_id,
                                     NFA_STATUS_HCI_UNRECOVERABLE_ERROR);
          LOG(INFO) << StringPrintf("Handle NFCEE_UNRECOVERABLE_ERROR for %d",
                                    nfa_hci_cb.reset_host[xx].host_id);
          nfa_hci_cb.curr_nfcee = nfa_hci_cb.reset_host[xx].host_id;
          nfa_hci_cb.next_nfcee_idx = 0x00;
          if (!nfa_hciu_check_host_resetting(nfa_hci_cb.reset_host[xx].host_id,
                                             NFCEE_RECOVERY_IN_PROGRESS)) {
                if (NFC_NfceeDiscover(true) == NFC_STATUS_FAILED) {
                    LOG(DEBUG) << StringPrintf(
                        "NFCEE_UNRECOVERABLE_ERRROR unable to handle");
                } else {
                  LOG(INFO) << StringPrintf("NFCEE_discovery cmd sent for %d",
                                            nfa_hci_cb.reset_host[xx].host_id);
                  nfa_hciu_add_host_resetting(nfa_hci_cb.reset_host[xx].host_id,
                                              NFCEE_RECOVERY_IN_PROGRESS);
                }
          } else {
                LOG(DEBUG) << StringPrintf(
                    "NFCEE re-initialization ongoing..........");
          }
          break;
        }
      }
    if(xx == NFA_HCI_MAX_HOST_IN_NETWORK)
      nfa_hci_check_pending_api_requests();
}

/*******************************************************************************
 **
 ** Function         nfa_hci_check_set_apdu_pipe_ready_for_next_host
 **
 ** Description      Find next active UICC/eSE host starting from the specified
 **                  host in the network and Set APDU pipe for specified host
 **                  ready for sending APDU transaction on it
 **
 ** Returns          TRUE, if all supported APDU pipes are ready
 **                  FALSE, if at least one APDU pipe is not ready yet
 **
 *******************************************************************************/
bool nfa_hci_check_set_apdu_pipe_ready_for_next_host ()
{
    tNFA_HCI_HOST_INFO *p_host;
    bool done = false;
    LOG(INFO) << StringPrintf("%s: %d", __func__, nfa_hci_cb.curr_nfcee);
    uint8_t xx;

    for(xx = 0; xx < NFA_HCI_MAX_HOST_IN_NETWORK; xx++) {
        p_host = &nfa_hci_cb.cfg.host[xx];
        uint8_t nfcee = 0;
        if (IS_VALID_HOST(nfa_hci_cb.curr_nfcee)) {
          nfcee = nfa_hci_cb.curr_nfcee;
          /* Mapping UICC Host ID as per specification */
          if (nfcee == NFA_HCI_FIRST_DYNAMIC_HOST) nfcee = NFA_HCI_UICC_HOST;
        }
        LOG(INFO) << StringPrintf("after updating NFCEE id %x", nfcee);
        if (nfcee == p_host->host_id) {
          nfa_hciu_clear_host_resetting(p_host->host_id,
                                        NFCEE_HCI_NOTIFY_ALL_PIPE_CLEARED);
          if (IS_PROP_HOST(p_host->host_id)) {
                /* if apdu_gatee_support set to NFC_EE_TYPE_NONE no apdu pipe
                   will be created if value set to NFC_EE_TYPE_ESE, create apdu
                   gate for eSE if value set to NFC_EE_TYPE_EUICC, create apdu
                   gate for eUICC if value set to
                   NFC_EE_TYPE_EUICC_WITH_APDUPIPE19 Create the apdu gate for
                   eUICC with APDU pipe 0x19 */
                if (!nfa_hci_is_apdu_pipe_required(p_host->host_id)) {
                    nfa_hci_check_nfcee_init_complete(p_host->host_id);
                    LOG(INFO) << StringPrintf("APDU pipe for SE-%x is skipped",
                                              p_host->host_id);
                    continue;
                }
                nfa_hci_api_add_prop_host_info(p_host->host_id);
                done = nfa_hci_set_apdu_pipe_ready_for_host(p_host->host_id);
          } else if (nfa_hci_cb.uicc_etsi_support) {
                done = nfa_hci_set_apdu_pipe_ready_for_host(p_host->host_id);
          } else {
                done = false;
          }
          break;
        }
    }
    return done;
}


/*******************************************************************************
**
** Function         nfa_hci_handle_identity_mngmnt_app_gate_hcp_msg_data
**
** Description      This function handles incoming identity management application
**                  gate hcp packets
**
** Returns          none
**
*******************************************************************************/
static void nfa_hci_handle_identity_mgmt_app_gate_hcp_msg_data (uint8_t *p_data, uint16_t data_len,
                                                                  tNFA_HCI_DYN_PIPE *p_pipe)
{
    uint8_t                       gate_id = 0;
    tNFA_HCI_DYN_PIPE             *p_apdu_pipe;
    tNFA_HCI_PIPE_CMDRSP_INFO     *p_pipe_cmdrsp_info;

    p_pipe_cmdrsp_info = nfa_hciu_get_pipe_cmdrsp_info (p_pipe->pipe_id);
    if (p_pipe_cmdrsp_info == nullptr)
    {
        LOG(ERROR) << StringPrintf(
         "nfa_hci_handle_identity_mngmnt_app_gate_hcp_msg_data (): Invalid Pipe [0x%02x]",
                           p_pipe->pipe_id);
        return;
    }

    if (nfa_hci_cb.type == NFA_HCI_RESPONSE_TYPE)
    {
        p_pipe_cmdrsp_info->w4_cmd_rsp = false;

        if (p_pipe_cmdrsp_info->cmd_inst_sent == NFA_HCI_ANY_OPEN_PIPE)
        {
            p_pipe_cmdrsp_info->cmd_inst_sent = 0;

            if (nfa_hci_cb.inst == NFA_HCI_ANY_OK)
            {
                p_pipe->pipe_state = NFA_HCI_PIPE_OPENED;
            }

            /*if (nfa_hci_cb.hci_state == NFA_HCI_STATE_WAIT_NETWK_ENABLE)
            {*/
                if (nfa_hci_cb.inst == NFA_HCI_ANY_OK)
                {
                    nfa_hciu_reset_apdu_pipe_registry_info_of_host (p_pipe->dest_host);
                    nfa_hciu_reset_gate_list_of_host (p_pipe->dest_host);
                    nfa_hciu_send_get_param_cmd (p_pipe->pipe_id, NFA_HCI_GATES_LIST_INDEX);
                    return;
                }
                else
                {
                    if (!nfa_hci_enable_one_nfcee ())
                      nfa_hci_startup_complete (NFA_STATUS_FAILED);
                }
                /*
            }
            else
            {
                nfa_hci_startup_complete (NFA_STATUS_FAILED);
            }*/
        }
        else if (p_pipe_cmdrsp_info->cmd_inst_sent == NFA_HCI_ANY_GET_PARAMETER)
        {
            p_pipe_cmdrsp_info->cmd_inst_sent = 0;

            if (p_pipe_cmdrsp_info->cmd_inst_param_sent == NFA_HCI_GATES_LIST_INDEX)
            {
                p_pipe_cmdrsp_info->cmd_inst_param_sent = 0;

                /*if (nfa_hci_cb.hci_state == NFA_HCI_STATE_WAIT_NETWK_ENABLE)
                {*/
                    if (nfa_hci_cb.inst == NFA_HCI_ANY_OK)
                    {
                        nfa_hciu_update_gate_list_of_host (p_pipe->dest_host, (uint8_t) data_len, p_data);
                        /* Check if Dynamic APDU pipe exist for the host */
                        if ((p_apdu_pipe = nfa_hciu_find_dyn_apdu_pipe_for_host (p_pipe->dest_host)) == nullptr)
                        {
                            /* No APDU Pipe exist, check if APDU gate or General Purpose APDU gate exist */
                            gate_id = nfa_hciu_find_server_apdu_gate_for_host (p_pipe->dest_host);

                            if (gate_id != 0)
                            {
                                /* APDU Gate exist with no APDU Pipe on it, create now*/
                                nfa_hci_cb.app_in_use = NFA_HCI_APP_HANDLE_NONE;
                                nfa_hciu_send_create_pipe_cmd (NFA_HCI_APDU_APP_GATE,
                                                               p_pipe->dest_host, gate_id);
                            }
                            else
                            {
                                /* No APDU/General Purpose APDU Gate exist, check next one */
                                if (!nfa_hci_enable_one_nfcee ())
                                    nfa_hci_startup_complete (NFA_STATUS_OK);
                            }
                        }
                        else
                        {
                            /* APDU Pipe exist */
                            if (p_apdu_pipe->pipe_state == NFA_HCI_PIPE_CLOSED)
                            {
                                /* APDU Pipe is in closed state, Open it */
                                nfa_hciu_send_open_pipe_cmd (p_apdu_pipe->pipe_id);
                                return;
                            }
                            else
                            {
                                /* APDU Pipe for this host is already open, Move to next one */
                                if (!nfa_hci_enable_one_nfcee ())
                                    nfa_hci_startup_complete (NFA_STATUS_OK);
                            }
                        }
                    }
                    else
                    {
                        if (!nfa_hci_enable_one_nfcee ())
                          nfa_hci_startup_complete (NFA_STATUS_FAILED);
                    }
               // }
            }
        }
    }
}

/*******************************************************************************
**
** Function         nfa_hci_handle_apdu_app_gate_hcp_msg_data
**
** Description      This function handles incoming APDU application gate hcp
**
** Returns          none
**
*******************************************************************************/
static void nfa_hci_handle_apdu_app_gate_hcp_msg_data (uint8_t *p_data, uint16_t data_len,
                                                       tNFA_HCI_DYN_PIPE *p_pipe)
{
    uint8_t                       cmd_inst_param;
    tNFA_HCI_EVT_DATA             evt_data;
    tNFA_HCI_PIPE_CMDRSP_INFO     *p_pipe_cmdrsp_info;
    tNFA_HCI_APDU_PIPE_REG_INFO   *p_apdu_pipe_reg_info;
    p_pipe_cmdrsp_info = nfa_hciu_get_pipe_cmdrsp_info (p_pipe->pipe_id);
    p_apdu_pipe_reg_info = nfa_hciu_find_apdu_pipe_registry_info_for_host (p_pipe->dest_host);
    if (p_pipe_cmdrsp_info == nullptr)
    {
        LOG(ERROR) << StringPrintf("nfa_hci_handle_apdu_app_gate_hcp_msg_data (): Invalid Pipe [0x%02x]",
                           p_pipe->pipe_id);
        return;
    }

    if (p_apdu_pipe_reg_info == nullptr)
    {
        LOG(ERROR) << StringPrintf ("nfa_hci_handle_apdu_app_gate_hcp_msg_data (): Invalid host [0x%02x]",
                           p_pipe->dest_host);
        return;
    }

    /* Check if data packet is a command, response or event */
    if (nfa_hci_cb.type == NFA_HCI_COMMAND_TYPE)
    {
         /* Just incase if server host close and open the APDU Pipe */
        switch (nfa_hci_cb.inst)
        {
        case NFA_HCI_ANY_OPEN_PIPE:
            nfa_hciu_reset_apdu_pipe_registry_info_of_host (p_pipe->dest_host);
            /* Set flag to indicate waiting for ETSI_HCI_EVT_ATR */
            p_pipe_cmdrsp_info->w4_atr_evt = true;
            [[fallthrough]];
        case NFA_HCI_ANY_CLOSE_PIPE:
            nfa_hci_handle_pipe_open_close_cmd (p_pipe);
            break;

        default:
            nfa_hciu_send_msg (p_pipe->pipe_id, NFA_HCI_RESPONSE_TYPE,
                               NFA_HCI_ANY_E_CMD_NOT_SUPPORTED, 0, nullptr);
            break;
        }
    }
    else if (nfa_hci_cb.type == NFA_HCI_RESPONSE_TYPE)
    {
        p_pipe_cmdrsp_info->w4_cmd_rsp = false;

        if (p_pipe_cmdrsp_info->cmd_inst_sent == NFA_HCI_ANY_OPEN_PIPE)
        {
            p_pipe_cmdrsp_info->cmd_inst_sent = 0;

            if (nfa_hci_cb.inst == NFA_HCI_ANY_OK)
            {
                p_pipe->pipe_state = NFA_HCI_PIPE_OPENED;
                /* Set flag to indicate waiting for ETSI_HCI_EVT_ATR */
                p_pipe_cmdrsp_info->w4_atr_evt = true;
                nfa_hci_cb.nv_write_needed = true;
            }

            //if (nfa_hci_cb.hci_state == NFA_HCI_STATE_WAIT_NETWK_ENABLE)
            {
                if (nfa_hci_cb.inst == NFA_HCI_ANY_OK)
                {
                    /* Read Registry */
                    nfa_hciu_send_get_param_cmd (p_pipe->pipe_id, NFA_HCI_MAX_C_APDU_SIZE_INDEX);
                }
                else
                {
                    /* Check APDU Pipe for next UICC host */
                    if (!nfa_hci_enable_one_nfcee ())
                        nfa_hci_startup_complete (NFA_STATUS_OK);
                }
            }
        }
        else if (p_pipe_cmdrsp_info->cmd_inst_sent == NFA_HCI_ANY_GET_PARAMETER)
        {
            cmd_inst_param = p_pipe_cmdrsp_info->cmd_inst_param_sent;

            p_pipe_cmdrsp_info->cmd_inst_sent       = 0;
            p_pipe_cmdrsp_info->cmd_inst_param_sent = 0;

            /* Only in NFA_HCI_STATE_W4_NETWK_ENABLE state, the registries are read */
            if (nfa_hci_cb.inst == NFA_HCI_ANY_OK)
            {
                if (cmd_inst_param == NFA_HCI_MAX_C_APDU_SIZE_INDEX)
                {
                    STREAM_TO_UINT16 (p_apdu_pipe_reg_info->max_cmd_apdu_size, p_data);

                    nfa_hciu_send_get_param_cmd (p_pipe->pipe_id, NFA_HCI_MAX_WAIT_TIME_INDEX);
                }
                else if (cmd_inst_param == NFA_HCI_MAX_WAIT_TIME_INDEX)
                {
                    STREAM_TO_UINT16 (p_apdu_pipe_reg_info->max_wait_time, p_data);

                    p_apdu_pipe_reg_info->reg_info_valid = true;
                    if (!nfa_hci_enable_one_nfcee ())
                    {
                        evt_data.init_completed.status = NFA_STATUS_OK;
                        nfa_hci_startup_complete (NFA_STATUS_OK);
                        NFA_SCR_PROCESS_EVT(NFA_SCR_ESE_RECOVERY_COMPLETE_EVT, NFA_STATUS_OK);
                        //Notify upper layer post Nfc init, if any recovery
                        nfa_hciu_send_to_all_apps(NFA_HCI_INIT_COMPLETED, &evt_data);
                    }
                }
            }
            else
            {
                if (!nfa_hci_enable_one_nfcee ())
                    nfa_hci_startup_complete (NFA_STATUS_OK);
            }
        }
    }
    else if (nfa_hci_cb.type == NFA_HCI_EVENT_TYPE)
    {
       LOG(ERROR) << StringPrintf("nfa_hci_handle_apdu_app_gate_hcp_msg_data (): nfa_hci_cb.inst: %x",
                           nfa_hci_cb.inst);//debug
        switch (nfa_hci_cb.inst)
        {
        case NFA_HCI_EVT_R_APDU:

            if (p_pipe_cmdrsp_info->w4_rsp_apdu_evt)
            {
                /* Waiting for Response APDU */
                p_pipe_cmdrsp_info->w4_rsp_apdu_evt = false;

                evt_data.apdu_rcvd.host_id  = p_pipe->dest_host;
                evt_data.apdu_rcvd.apdu_len = data_len;

                if (!p_pipe_cmdrsp_info->reassembly_failed)
                {
                    evt_data.apdu_rcvd.status        = NFA_STATUS_OK;
                }
                else
                {
                    evt_data.apdu_rcvd.status = NFA_STATUS_BUFFER_FULL;
                }

                evt_data.apdu_rcvd.p_apdu = p_data;

                /* notify NFA_HCI_RSP_APDU_RCVD_EVT to app that is waiting for response APDU */
                nfa_hciu_send_to_app (NFA_HCI_RSP_APDU_RCVD_EVT, &evt_data,
                                      p_pipe_cmdrsp_info->pipe_user);

                p_pipe_cmdrsp_info->p_rsp_buf    = nullptr;
                p_pipe_cmdrsp_info->rsp_buf_size = 0;
                nfa_hci_cb.m_wtx_count = 0;
                /* Release the temporary ownership for APDU Pipe given to the App */
                p_pipe_cmdrsp_info->pipe_user = NFA_HCI_APP_HANDLE_NONE;
            }
            else if (p_pipe_cmdrsp_info->w4_atr_evt)
            {
                LOG(ERROR) << StringPrintf("nfa_hci_handle_apdu_app_gate_hcp_msg_data (): nfa_hci_cb.inst: %x %x",
                           nfa_hci_cb.inst ,p_pipe_cmdrsp_info->w4_atr_evt);//debug
                evt_data.apdu_aborted.atr_len = data_len;
                evt_data.apdu_aborted.p_atr = p_data;
                evt_data.apdu_aborted.status  = NFA_STATUS_INVALID_PARAM;
                evt_data.apdu_aborted.host_id = p_pipe->dest_host;

                /* notify NFA_HCI_APDU_ABORTED_EVT to app that requested to ABORT command APDU */
                nfa_hciu_send_to_app (NFA_HCI_APDU_ABORTED_EVT, &evt_data,
                                                  p_pipe_cmdrsp_info->pipe_user);
              if(nfa_hci_cb.hci_state == NFA_HCI_STATE_WAIT_RSP)
                nfa_hci_cb.hci_state = NFA_HCI_STATE_IDLE;

              if (p_pipe_cmdrsp_info->rsp_buf_size)
              {
                p_pipe_cmdrsp_info->rsp_buf_size = 0;
                p_pipe_cmdrsp_info->p_rsp_buf    = nullptr;
              }
              p_pipe_cmdrsp_info->w4_atr_evt = false;
              /* Release the temporary ownership for APDU Pipe given to the App */
              p_pipe_cmdrsp_info->pipe_user = NFA_HCI_APP_HANDLE_NONE;
            }
            break;

        case NFA_HCI_EVT_WTX:
            if (p_pipe_cmdrsp_info->w4_rsp_apdu_evt ||
              p_pipe_cmdrsp_info->w4_atr_evt)
            {
                if(p_nfa_hci_cfg->max_wtx_count) {
                  if(nfa_hci_cb.m_wtx_count >= p_nfa_hci_cfg->max_wtx_count) {
                                LOG(DEBUG) << StringPrintf(
                                    "%s:  Max WTX count reached", __func__);
                                nfa_hci_cb.m_wtx_count = 0;
                                if (p_pipe_cmdrsp_info->w4_rsp_apdu_evt) {
                                    LOG(DEBUG) << StringPrintf(
                                        "%s:  Max WTX count w4_rsp_apdu_evt",
                                        __func__);

                                    evt_data.apdu_rcvd.apdu_len = 0;
                                    evt_data.apdu_rcvd.p_apdu = nullptr;
                                    evt_data.apdu_rcvd.status =
                                        NFA_STATUS_HCI_WTX_TIMEOUT;
                                    nfa_hciu_send_to_app(
                                        NFA_HCI_RSP_APDU_RCVD_EVT, &evt_data,
                                        p_pipe_cmdrsp_info->pipe_user);
                    } else if(p_pipe_cmdrsp_info->w4_atr_evt) {
                        evt_data.apdu_aborted.atr_len = 0;
                        evt_data.apdu_aborted.status  = NFA_STATUS_HCI_WTX_TIMEOUT;
                        evt_data.apdu_aborted.host_id = p_pipe->dest_host;
                         p_pipe_cmdrsp_info->w4_atr_evt = false;
                /* notify NFA_HCI_APDU_ABORTED_EVT to app that requested to ABORT command APDU */
                        nfa_hciu_send_to_app (NFA_HCI_APDU_ABORTED_EVT, &evt_data,
                                      p_pipe_cmdrsp_info->pipe_user);
                    }
                    return;
                  }
                  else
                  {
                    nfa_hci_cb.m_wtx_count++;
                  }
                }
                /* More processing time needed for APDU sent to UICC */
                nfa_sys_start_timer (&(p_pipe_cmdrsp_info->rsp_timer),
                          NFA_HCI_RSP_TIMEOUT_EVT, p_pipe_cmdrsp_info->rsp_timeout);
                LOG(DEBUG) << StringPrintf(
                    "nfa_hci_handle_apdu_app_gate_hcp_msg_data "
                    "()p_pipe_cmdrsp_info->rsp_timeout %x",
                    p_pipe_cmdrsp_info->rsp_timeout);  // debug
            }
            break;

        case NFA_HCI_EVT_ATR:
            LOG(DEBUG) << StringPrintf(
                "nfa_hci_handle_apdu_app_gate_hcp_msg_data (): EVT_ATR_RSP "
                "recived w4_atr_evt: %x p_pipe_cmdrsp_info->w4_rsp_apdu_evt: "
                "%x",
                p_pipe_cmdrsp_info->w4_atr_evt,
                p_pipe_cmdrsp_info->w4_rsp_apdu_evt);  // debug

            if (p_pipe_cmdrsp_info->w4_rsp_apdu_evt ||
                p_pipe_cmdrsp_info->w4_atr_evt) {
                p_pipe_cmdrsp_info->w4_rsp_apdu_evt = false;
                evt_data.apdu_aborted.atr_len = data_len;
                evt_data.apdu_aborted.p_atr = p_data;
                evt_data.apdu_aborted.status = NFA_STATUS_OK;
                evt_data.apdu_aborted.host_id = p_pipe->dest_host;

                /* notify NFA_HCI_APDU_ABORTED_EVT to app that requested to
                 * ABORT command APDU */
                nfa_hciu_send_to_all_apps(NFA_HCI_APDU_ABORTED_EVT, &evt_data);
                if (nfa_hci_cb.hci_state == NFA_HCI_STATE_WAIT_RSP)
                  nfa_hci_cb.hci_state = NFA_HCI_STATE_IDLE;
                nfa_hciu_clear_host_resetting(p_pipe->dest_host,
                                              NFCEE_INIT_COMPLETED);
                if (p_pipe_cmdrsp_info->rsp_buf_size) {
                  p_pipe_cmdrsp_info->rsp_buf_size = 0;
                  p_pipe_cmdrsp_info->p_rsp_buf = nullptr;
                }
                /* Release the temporary ownership for APDU Pipe given to the
                 * App */
                p_pipe_cmdrsp_info->pipe_user = NFA_HCI_APP_HANDLE_NONE;
            }
            p_pipe_cmdrsp_info->w4_atr_evt = false;
            break;
        default:
            /* Invalid Event, just drop */
            break;
        }
    }
}

/*******************************************************************************
**
** Function         nfa_hci_api_send_apdu
**
** Description      action function to send command APDU on APDU pipe
**
** Returns          TRUE, if the command APDU is processed
**                  FALSE, if event is queued for processing later
**
*******************************************************************************/
static bool nfa_hci_api_send_apdu (tNFA_HCI_EVENT_DATA *p_evt_data)
{
    uint8_t                       pipe_id= 0;
    uint8_t                       evt_code = NFA_HCI_EVT_UNKNOWN;
    uint32_t                      max_wait_time = NFA_HCI_EVT_SW_PROC_LATENCY;
    tNFC_STATUS                 status = NFA_STATUS_FAILED;
    tNFA_HCI_EVT_DATA           evt_data;
    tNFA_HCI_DYN_PIPE           *p_pipe;
    tNFA_HCI_PIPE_STATE         pipe_state = NFA_HCI_PIPE_CLOSED;
    tNFA_HCI_API_SEND_APDU_EVT  *p_send_apdu = (tNFA_HCI_API_SEND_APDU_EVT*)&p_evt_data->send_evt;
    tNFA_HCI_PIPE_CMDRSP_INFO   *p_pipe_cmdrsp_info = nullptr;
    tNFA_HCI_APDU_PIPE_REG_INFO *p_apdu_pipe_reg_info = nullptr;

    if (nfa_hciu_is_active_host (p_send_apdu->host_id))
    {
        if (nfa_hciu_is_host_reseting (p_send_apdu->host_id))
        {
            GKI_enqueue (&nfa_hci_cb.hci_host_reset_api_q, (NFC_HDR *) p_evt_data);
            return FALSE;
        }

        if ((p_pipe = nfa_hciu_find_dyn_apdu_pipe_for_host (p_send_apdu->host_id)) != nullptr)
        {
            pipe_id    = p_pipe->pipe_id;
            pipe_state = p_pipe->pipe_state;

            evt_code   = NFA_HCI_EVT_C_APDU;
        } else
        {
          LOG(ERROR) << StringPrintf("nfa_hci_api_send_apdu (): Pipe [0x%02x] Info not available", pipe_id);
        }

        if (pipe_id != NFA_HCI_INVALID_PIPE)
        {
            p_pipe_cmdrsp_info = nfa_hciu_get_pipe_cmdrsp_info (pipe_id);
            p_apdu_pipe_reg_info = nfa_hciu_find_apdu_pipe_registry_info_for_host (p_send_apdu->host_id);
        }

        if (  (p_pipe_cmdrsp_info == nullptr)
            ||(p_apdu_pipe_reg_info == nullptr)  )
        {
            LOG(ERROR) << StringPrintf("nfa_hci_api_send_apdu (): Pipe [0x%02x] Info not available", pipe_id);
            evt_data.apdu_rcvd.host_id  = p_send_apdu->host_id;
            evt_data.apdu_rcvd.apdu_len = 0;

            evt_data.apdu_rcvd.status = NFA_STATUS_FAILED;

            evt_data.apdu_rcvd.p_apdu = nullptr;
            nfa_hciu_send_to_app (NFA_HCI_RSP_APDU_RCVD_EVT, &evt_data,
                                      p_send_apdu->hci_handle);

        }
        else if (pipe_state != NFA_HCI_PIPE_OPENED)
        {
            LOG(ERROR) << StringPrintf("nfa_hci_api_send_apdu (): APDU Pipe[0x%02x] is closed", pipe_id);
            evt_data.apdu_rcvd.host_id  = p_send_apdu->host_id;
            evt_data.apdu_rcvd.apdu_len = 0;

            evt_data.apdu_rcvd.status = NFA_STATUS_FAILED;

            evt_data.apdu_rcvd.p_apdu = nullptr;
            nfa_hciu_send_to_app (NFA_HCI_RSP_APDU_RCVD_EVT, &evt_data,
                                      p_send_apdu->hci_handle);
        }
        else if (p_pipe_cmdrsp_info->w4_atr_evt)
        {
            LOG(DEBUG) << StringPrintf(
                "nfa_hci_api_send_apdu (): APDU Server is not initialized yet! "
                "sending failed to upper layer");
            evt_data.apdu_rcvd.host_id = p_send_apdu->host_id;
            evt_data.apdu_rcvd.apdu_len = 0;

            evt_data.apdu_rcvd.status = NFA_STATUS_FAILED;

            evt_data.apdu_rcvd.p_apdu = nullptr;
            nfa_hciu_send_to_app(NFA_HCI_RSP_APDU_RCVD_EVT, &evt_data,
                                 p_send_apdu->hci_handle);
        }
        else if (p_pipe_cmdrsp_info->w4_rsp_apdu_evt)
        {
            LOG(DEBUG) << StringPrintf(
                "nfa_hci_api_send_apdu (): APDU Pipe[0x%02x] is busy!",
                pipe_id);
        }
        // else if (  (p_apdu_pipe_reg_info->reg_info_valid)
        //          &&(p_send_apdu->cmd_apdu_len > p_apdu_pipe_reg_info->max_cmd_apdu_size)  )
        // {
        //     NFA_TRACE_WARNING1 ("nfa_hci_api_send_apdu (): APDU cannot exceed [0x%04x] bytes",
        //                             p_apdu_pipe_reg_info->max_cmd_apdu_size);
        // }
        else
        {
            /* Send Command APDU on APDU pipe */
            status = nfa_hciu_send_msg (pipe_id, NFA_HCI_EVENT_TYPE, evt_code,
                    p_send_apdu->cmd_apdu_len,
                    p_send_apdu->p_cmd_apdu);

            if (status == NFA_STATUS_OK)
            {
                /* Response APDU is expected */
                p_pipe_cmdrsp_info->w4_rsp_apdu_evt = true;
                nfa_hci_cb.hci_state = NFA_HCI_STATE_WAIT_RSP;
                nfa_hci_cb.pipe_in_use = pipe_id;

                /* Remember the app that is sending Command APDU on
                ** the pipe for routing the response APDU later to it
                */
                p_pipe_cmdrsp_info->pipe_user = p_send_apdu->hci_handle;
                p_pipe_cmdrsp_info->rsp_timeout = p_send_apdu->rsp_timeout;
                max_wait_time += p_send_apdu->rsp_timeout;
                /* Response APDU/ETSI_HCI_EVT_WTX is expected
                   ** within specified interval of time */

                nfa_sys_start_timer (&(p_pipe_cmdrsp_info->rsp_timer),
                                         NFA_HCI_RSP_TIMEOUT_EVT,
                                         max_wait_time);
                LOG(ERROR) << StringPrintf ("nfa_hci_api_send_apdu (): Wait time[0x%02x]",
                                   max_wait_time);
                p_pipe_cmdrsp_info->rsp_buf_size = p_send_apdu->rsp_apdu_buf_size;
                p_pipe_cmdrsp_info->p_rsp_buf    = p_send_apdu->p_rsp_apdu_buf;
            }
            else
            {
                LOG(ERROR) << StringPrintf ("nfa_hci_api_send_apdu (): Sending APDU on Pipe[0x%02x] failed",
                                   pipe_id);
            }
        }
    }
    else
    {
        LOG(DEBUG) << StringPrintf(
            "nfa_hci_api_send_apdu (): Host[0x%02x] inactive",
            p_send_apdu->host_id);
    }
    return true;
}

/*******************************************************************************
**
** Function         nfa_hci_api_abort_apdu
**
** Description      action function to abort APDU commad
**
** Returns          TRUE, if the abort APDU is processed
**                  FALSE, if event is queued for processing later
**
*******************************************************************************/
static bool nfa_hci_api_abort_apdu (tNFA_HCI_EVENT_DATA *p_evt_data)
{
    NFC_HDR                     *p_msg;
    uint8_t                     pipe_id = 0;;
    tNFC_STATUS                 status = NFA_STATUS_FAILED;
    bool                        send_abort_ntf = FALSE;
    bool                        host_reseting = FALSE;
    tNFA_HCI_EVT_DATA           evt_data;
    tNFA_HCI_DYN_PIPE           *p_pipe;
    tNFA_HCI_PIPE_STATE         pipe_state = NFA_HCI_PIPE_CLOSED;
    tNFA_HCI_EVENT_DATA         *p_queue_evt_data;
    tNFA_HCI_API_ABORT_APDU_EVT  *p_abort_apdu = &p_evt_data->abort_apdu;
    tNFA_HCI_PIPE_CMDRSP_INFO   *p_pipe_cmdrsp_info = nullptr;
    tNFA_HCI_APDU_PIPE_REG_INFO *p_apdu_pipe_reg_info = nullptr;

    host_reseting = nfa_hciu_is_host_reseting (p_abort_apdu->host_id);

    if (host_reseting)
    {
        /* Host reset will automatically abort the APDU Command if it was sent on the APDU
        ** pipe and so no need to send ABORT_EVT on the APDU pipe. If the APDU Command is
        ** not yet sent but waiting in hci_host_reset_api_q queue to be sent then just
        ** flush the command from the queue so it will never be sent
        */
        p_msg = (NFC_HDR *) GKI_getfirst (&nfa_hci_cb.hci_host_reset_api_q);

        while (p_msg != nullptr)
        {
            /* Check if the buffered API request is for Sending APDU command */
            if (p_msg->event == NFA_HCI_API_SEND_APDU_EVT)
            {
                p_queue_evt_data = (tNFA_HCI_EVENT_DATA *) p_msg;

                if (  (p_queue_evt_data->send_apdu.hci_handle == p_abort_apdu->hci_handle)
                    &&(p_queue_evt_data->send_apdu.host_id == p_abort_apdu->host_id)  )
                {
                    /* It is the same app that sent the APDU command is now requesting it to abort */
                    GKI_remove_from_queue (&nfa_hci_cb.hci_host_reset_api_q, p_msg);
                    GKI_freebuf (p_msg);

                    break;
                }
            }
            p_msg = (NFC_HDR *) GKI_getnext (p_msg);
        }
    }

    if ((p_pipe = nfa_hciu_find_dyn_apdu_pipe_for_host (p_abort_apdu->host_id)) != nullptr)
    {
        /* APDU pipe on APDU gate/APDU Generic purpose gate */
        pipe_id    = p_pipe->pipe_id;
        pipe_state = p_pipe->pipe_state;

        if (!host_reseting && p_pipe->dest_gate == NFA_HCI_APDU_GATE)
        {
            /* To send Abort event on the APDU pipe */
            send_abort_ntf = TRUE;
        }
    }
    else
    {
      LOG(ERROR) << StringPrintf ("nfa_hci_api_abort_apdu (): APDU pipe not available start creation");
      if (IS_PROP_HOST(p_abort_apdu->host_id)) {
        nfa_hci_notify_w4_atr_timeout();
        return true;
      }
    }

    if (pipe_id != NFA_HCI_INVALID_PIPE)
    {
        p_pipe_cmdrsp_info = nfa_hciu_get_pipe_cmdrsp_info (pipe_id);
        p_apdu_pipe_reg_info = nfa_hciu_find_apdu_pipe_registry_info_for_host (p_abort_apdu->host_id);
    }

    if (p_apdu_pipe_reg_info == nullptr) {
        LOG(DEBUG) << StringPrintf(
            "nfa_hci_api_abort_apdu (): Pipe [0x%02x] Info not available",
            pipe_id);
    }
    if ((p_pipe_cmdrsp_info == nullptr) || (pipe_state != NFA_HCI_PIPE_OPENED)) {
        LOG(DEBUG) << StringPrintf(
            "nfa_hci_api_abort_apdu (): APDU Pipe [0x%02x] is closed or Info "
            "not "
            "available",
            pipe_id);

        nfa_hci_notify_w4_atr_timeout();
        return true;
    }
#if 0
    else if (  (!apdu_dropped)
             &&(  (!p_pipe_cmdrsp_info->w4_rsp_apdu_evt)
                ||(p_pipe_cmdrsp_info->pipe_user != p_abort_apdu->hci_handle)  )  )
    {
        NFA_TRACE_WARNING1 ("nfa_hci_api_abort_apdu(): No APDU sent by app on Pipe[0x%02x]",
                             pipe_id);
    }
#endif
    else if (p_pipe_cmdrsp_info->msg_rx_len > 0)
    {
        /* Too late to Abort command-APDU as one or more fragment of response APDU is already
        ** received. NFA_HCI_RSP_APDU_RCVD_EVT will be reported after all fragments of response
        ** APDU is received or on timeout. Now, report Abort operation failed
        */
        LOG(DEBUG) << StringPrintf(
            "nfa_hci_api_abort_apdu(): Too late to Abort as C-APDU is "
            "processed");
    }
    else
    {
        /* No response APDU is received, release Rsp APDU buffer so that if any
        ** Rsp APDU is received later for the command APDU sent, it will be dropped
        */
        p_pipe_cmdrsp_info->p_rsp_buf       = nullptr;
        p_pipe_cmdrsp_info->pipe_user       = p_abort_apdu->hci_handle;
        p_pipe_cmdrsp_info->rsp_buf_size    = 0;
        p_pipe_cmdrsp_info->w4_rsp_apdu_evt = false;

        /* Stop timer that was started to wait for Response APDU */
        nfa_sys_stop_timer (&(p_pipe_cmdrsp_info->rsp_timer));

        if (send_abort_ntf)
        {
          /* APDU sent on APDU pipe connected to APDU Gate, Send ABORT event on the APDU pipe */
          status = nfa_hciu_send_msg (pipe_id, NFA_HCI_EVENT_TYPE, NFA_HCI_EVT_ABORT, 0, 0);
          if (status == NFA_STATUS_OK)
          {
            /* Restart timer to wait for EVT_ATR for the EVT_ABORT sent */
            p_pipe_cmdrsp_info->rsp_timeout = p_abort_apdu->rsp_timeout;

            nfa_sys_start_timer (&(p_pipe_cmdrsp_info->rsp_timer),
                  NFA_HCI_RSP_TIMEOUT_EVT,
                   (p_abort_apdu->rsp_timeout + NFA_HCI_EVT_SW_PROC_LATENCY));
            /* EVT_ATR is expected */
            p_pipe_cmdrsp_info->w4_atr_evt = true;

            return true;
          }
        }
        else
        {
            /* APDU sent on APDU pipe connected to general purpose APDU gate/static APDU pipe
            ** (or) APDU is dropped before it was sent on APDU pipe
            */
            status = NFA_STATUS_OK;
        }
    }
    evt_data.apdu_aborted.atr_len = 0x00;
    evt_data.apdu_aborted.p_atr = nullptr;
    evt_data.apdu_aborted.status  = status;
    evt_data.apdu_aborted.host_id = p_abort_apdu->host_id;

    /* Send NFA_HCI_APDU_ABORTED_EVT to notify status */
    nfa_hciu_send_to_app (NFA_HCI_APDU_ABORTED_EVT, &evt_data, p_abort_apdu->hci_handle);
    return true;
}

/*******************************************************************************
**
** Function         nfa_hci_api_add_prop_host_info
**
** Description      This api is used to fill data for specified Host Index.
**
** Returns          None
**
*******************************************************************************/
static void nfa_hci_api_add_prop_host_info(uint8_t propHostIndx) {
  uint8_t NFA_HCI_PROP_HOST_GATE_LIST[] = {
      NFA_HCI_CARD_RF_A_GATE, NFA_HCI_LOOP_BACK_GATE,
      NFA_HCI_IDENTITY_MANAGEMENT_GATE, NFA_HCI_APDU_GATE};
  if ((!nfa_hciu_check_pipe_between_gates(NFA_HCI_ID_MNGMNT_APP_GATE,
                                          propHostIndx,
                                          NFA_HCI_IDENTITY_MANAGEMENT_GATE))) {
        LOG(DEBUG) << StringPrintf("nfa_hci_api_add_prop_host_info");
        nfa_hciu_add_pipe_to_gate(NFA_HCI_DEFAULT_ID_MANAGEMENT_PIPE,
                                  NFA_HCI_ID_MNGMNT_APP_GATE, propHostIndx,
                                  NFA_HCI_IDENTITY_MANAGEMENT_GATE);
        tNFA_HCI_DYN_PIPE* p_pipe =
            nfa_hciu_find_pipe_by_pid(NFA_HCI_DEFAULT_ID_MANAGEMENT_PIPE);
        if (p_pipe) {
            p_pipe->pipe_state = NFA_HCI_PIPE_OPENED;
            nfa_hciu_reset_apdu_pipe_registry_info_of_host(p_pipe->dest_host);
            nfa_hciu_update_gate_list_of_host(
                p_pipe->dest_host, sizeof(NFA_HCI_PROP_HOST_GATE_LIST),
                NFA_HCI_PROP_HOST_GATE_LIST);
    }
  }
}
/*******************************************************************************
 **
 ** Function         nfa_hci_handle_control_evt
 **
 ** Description      handle conn evt  from NFC layer.
 **                  Based on the evt  the rsp timer
 **                  is restarted for specified time.
 **
 ** Returns
 **
 *******************************************************************************/
void nfa_hci_handle_control_evt(tNFC_CONN_EVT event,
                               __attribute__((unused))tNFC_CONN* p_data){
    tNFA_HCI_PIPE_CMDRSP_INFO *p_pipe_cmdrsp_info = nullptr;
    tNFA_HCI_DYN_PIPE           *p_pipe = nullptr;

    p_pipe = nfa_hciu_find_dyn_apdu_pipe_for_host (NFA_HCI_FIRST_PROP_HOST);
    if(p_pipe != nullptr)
      p_pipe_cmdrsp_info = nfa_hciu_get_pipe_cmdrsp_info (p_pipe->pipe_id);

    if (p_pipe_cmdrsp_info != nullptr && (p_pipe_cmdrsp_info->w4_cmd_rsp ||
      p_pipe_cmdrsp_info->w4_rsp_apdu_evt))
    {
      nfa_sys_stop_timer(&p_pipe_cmdrsp_info->rsp_timer);
      if(event == NFC_HCI_FC_STARTED_EVT)
      {
        /* More processing time needed for APDU sent to UICC */
        nfa_sys_start_timer (&(p_pipe_cmdrsp_info->rsp_timer),
                          NFA_HCI_RSP_TIMEOUT_EVT, p_pipe_cmdrsp_info->rsp_timeout + NFA_HCI_EVT_SW_PROC_LATENCY);
      } else if(event == NFC_HCI_FC_STOPPED_EVT)
      {
        /* More processing time needed for APDU sent to UICC */
        nfa_sys_start_timer (&(p_pipe_cmdrsp_info->rsp_timer),
                          NFA_HCI_RSP_TIMEOUT_EVT, p_pipe_cmdrsp_info->rsp_timeout * p_nfa_hci_cfg->max_wtx_count);
      }
    }
}

/*******************************************************************************
 **
 ** Function         is_ese_apdu_pipe_required
 **
 ** Description      Checks if eSE APDU pipe needs to be created or not.
 **
 ** Returns          TRUE, if given nfcee_id is eSE & APDU pipe shall be created
 **                  for eSE otherwise false
 **
 *******************************************************************************/
inline bool is_ese_apdu_pipe_required(uint8_t nfcee_id) {
  if (nfcee_id == NFA_HCI_FIRST_PROP_HOST &&
      nfa_hci_cb.se_apdu_gate_support == NFC_EE_TYPE_ESE) {
    return true;
  }
  return false;
}

/*******************************************************************************
 **
 ** Function         is_euicc_apdu_pipe_required
 **
 ** Description      Checks if eUICC APDU pipe needs to be created or not.
 **
 ** Returns          TRUE, if given nfcee_id is eUICC & APDU pipe shall be
 **                  created for eUICC otherwise false
 **
 *******************************************************************************/
inline bool is_euicc_apdu_pipe_required(uint8_t nfcee_id) {
  if (IS_PROP_EUICC_HOST(nfcee_id) &&
      (nfa_hci_cb.se_apdu_gate_support == NFC_EE_TYPE_EUICC_WITH_APDUPIPE19 ||
       nfa_hci_cb.se_apdu_gate_support == NFC_EE_TYPE_EUICC)) {
    return true;
  }
  return false;
}
/*******************************************************************************
 **
 ** Function         nfa_hci_is_apdu_pipe_required
 **
 ** Description      It checks APDU pipe creation is needed for
 **                  given NFCEEs.
 **
 ** Returns          TRUE, if APDU pipe creation is needed for give nfcee
 **                  otherwise false.
 **
 *******************************************************************************/
bool nfa_hci_is_apdu_pipe_required(uint8_t nfcee_id) {
    if ((nfa_hci_cb.se_apdu_gate_support != NFC_EE_TYPE_NONE) &&
        (is_ese_apdu_pipe_required(nfcee_id) ||
         is_euicc_apdu_pipe_required(nfcee_id))) {
      return true;
    }
    return false;
}

/*******************************************************************************
**
** Function         nfa_hci_get_apdu_enabled_host
**
** Description      Find NFCEE ID for which power link control state to be reset
**
** Returns          NFCEE ID for which apdu pipe is created
**
*******************************************************************************/
uint8_t nfa_hci_get_apdu_enabled_host(void) {
    uint8_t mNumEe = NFA_HCI_MAX_HOST_IN_NETWORK;
    tNFA_EE_INFO eeInfo[NFA_HCI_MAX_HOST_IN_NETWORK] = {};
    memset(&eeInfo, 0, mNumEe * sizeof(tNFA_EE_INFO));
    NFA_EeGetInfo(&mNumEe, eeInfo);

    for (uint8_t xx = 0; xx < mNumEe; xx++) {
      uint8_t nfceeid = eeInfo[xx].ee_handle & ~NFA_HANDLE_GROUP_EE;
      if ((is_ese_apdu_pipe_required(nfceeid) ||
           is_euicc_apdu_pipe_required(nfceeid)) &&
          eeInfo[xx].ee_status == NFA_EE_STATUS_ACTIVE) {
        LOG(DEBUG) << StringPrintf("%s Nfcee %d is active", __func__, nfceeid);
        return nfceeid;
      }
    }

    return NFA_HCI_FIRST_PROP_HOST;
}

/*******************************************************************************
 **
 ** Function         nfa_hci_is_power_link_required
 **
 ** Description      Checks if power link command is required to send as per
 **                  enabled/available NFCEEs.
 **
 ** Returns          TRUE, if eSE is enabled or  if eSE is disabled &
 **                  eUICC is enabled otherwise false
 **
 *******************************************************************************/
bool nfa_hci_is_power_link_required(uint8_t source_host) {
    if (!IS_PROP_HOST(source_host)) return false;
    uint8_t mNumEe = NFA_HCI_MAX_HOST_IN_NETWORK;
    tNFA_EE_INFO eeInfo[NFA_HCI_MAX_HOST_IN_NETWORK] = {};
    memset(&eeInfo, 0, mNumEe * sizeof(tNFA_EE_INFO));

    NFA_EeGetInfo(&mNumEe, eeInfo);
    for (int yy = 0; yy < mNumEe; yy++) {
      uint8_t nfceeid = eeInfo[yy].ee_handle & ~NFA_HANDLE_GROUP_EE;
      if (nfceeid == source_host) {
        if (is_ese_apdu_pipe_required(source_host) ||
            is_euicc_apdu_pipe_required(source_host)) {
            return true;
        }
      }
  }
  return false;
}
/*******************************************************************************
 **
 ** Function         nfa_hci_handle_clear_all_pipe_cmd
 **
 ** Description      handle clear all pipe cmd from host
 **
 ** Returns          None
 **
 *******************************************************************************/
static void nfa_hci_handle_clear_all_pipe_cmd(uint8_t source_host) {
  LOG(DEBUG) << StringPrintf(
      "nfa_hci_handle_clear_all_pipe_cmd pipe:%d source host", source_host);
  tNFA_HCI_DYN_PIPE* p_pipe = nullptr;
  tNFA_HCI_PIPE_CMDRSP_INFO* p_pipe_cmdrsp_info = nullptr;
  tNFA_STATUS status = NFA_STATUS_FAILED;
  nfa_hciu_add_host_resetting(source_host, NFCEE_HCI_NOTIFY_ALL_PIPE_CLEARED);
  if (nfa_hci_cb.hci_state == NFA_HCI_STATE_WAIT_RSP ||
      nfa_hci_cb.hci_state == NFA_HCI_STATE_IDLE) {
      if (nfa_hci_cb.w4_rsp_evt == true) nfa_sys_stop_timer(&nfa_hci_cb.timer);
      if ((p_pipe = nfa_hciu_find_dyn_apdu_pipe_for_host(source_host)) !=
          nullptr) {
        p_pipe_cmdrsp_info = nfa_hciu_get_pipe_cmdrsp_info(p_pipe->pipe_id);
        if (p_pipe_cmdrsp_info != nullptr) {
            if (p_pipe_cmdrsp_info->w4_atr_evt)
                    nfa_sys_stop_timer(&p_pipe_cmdrsp_info->rsp_timer);
            else if (p_pipe_cmdrsp_info->w4_rsp_apdu_evt ||
                     p_pipe_cmdrsp_info->w4_cmd_rsp)
                    nfa_sys_stop_timer(&p_pipe_cmdrsp_info->rsp_timer);
        }
      }
      /*nfa_hci_cb.hci_state = NFA_HCI_STATE_WAIT_NETWK_ENABLE;*/
      nfa_hciu_remove_all_pipes_from_host(source_host);

      nfa_nv_co_write((uint8_t*)&nfa_hci_cb.cfg, sizeof(nfa_hci_cb.cfg),
                      DH_NV_BLOCK);
      nfa_hci_cb.nv_write_needed = false;
      /*Trigger pipe creation*/
      nfa_hci_cb.curr_nfcee = source_host;
      if (nfa_hci_is_power_link_required(source_host)) {
        if (nfcFL.eseFL._NCI_NFCEE_PWR_LINK_CMD)
            NFC_NfceePLConfig(source_host, 0x03);
      }
      status = NFC_NfceeModeSet(source_host, NFC_MODE_ACTIVATE);
      LOG(DEBUG) << StringPrintf(
          "nfa_hci_handle_clear_all_pipe_cmd pipe:%d source host", status);
      if (status == NFA_STATUS_REFUSED) {
        /*Mode set ntf pending , pipe will be created on recieving ntf*/
      } else if (status != NFA_STATUS_OK) {
        nfa_hciu_clear_host_resetting(nfa_hci_cb.curr_nfcee,
                                      NFCEE_HCI_NOTIFY_ALL_PIPE_CLEARED);
        if (!nfa_hci_check_set_apdu_pipe_ready_for_next_host()) {
            nfa_hci_handle_pending_host_reset();
        }
      }
  } else {
      if (nfa_hci_is_power_link_required(source_host)) {
        if ((p_pipe = nfa_hciu_find_dyn_apdu_pipe_for_host(source_host)) !=
            nullptr) {
          if (nfcFL.eseFL._NCI_NFCEE_PWR_LINK_CMD)
            NFC_NfceePLConfig(source_host, 0x03);
        }
      }
      nfa_hciu_remove_all_pipes_from_host(source_host);
  }
}

#endif
