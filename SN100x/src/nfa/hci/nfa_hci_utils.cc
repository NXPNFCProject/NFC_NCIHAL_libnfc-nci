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
 *  Copyright 2018-2023 NXP
 *
 ******************************************************************************/

/******************************************************************************
 *
 *  This file contains the utility functions for the NFA HCI.
 *
 ******************************************************************************/
#include <android-base/stringprintf.h>
#include <base/logging.h>
#include <log/log.h>

#include <string>

#include "nfa_dm_int.h"
#include "nfa_hci_api.h"
#include "nfa_hci_defs.h"
#include "nfa_hci_int.h"

using android::base::StringPrintf;

extern bool nfc_debug_enabled;

static void handle_debug_loopback(NFC_HDR* p_buf, uint8_t type,
                                  uint8_t instruction);
uint8_t HCI_LOOPBACK_DEBUG = NFA_HCI_DEBUG_OFF;

/*******************************************************************************
**
** Function         nfa_hciu_find_pipe_by_pid
**
** Description      look for the pipe control block based on pipe id
**
** Returns          pointer to the pipe control block, or NULL if not found
**
*******************************************************************************/
tNFA_HCI_DYN_PIPE* nfa_hciu_find_pipe_by_pid(uint8_t pipe_id) {
  tNFA_HCI_DYN_PIPE* pp = nfa_hci_cb.cfg.dyn_pipes;
  int xx = 0;

  /* Loop through looking for a match */
  for (; xx < NFA_HCI_MAX_PIPE_CB; xx++, pp++) {
    if (pp->pipe_id == pipe_id) return (pp);
  }

  /* If here, not found */
  return (nullptr);
}

/*******************************************************************************
**
** Function         nfa_hciu_find_gate_by_gid
**
** Description      Find the gate control block for the given gate id
**
** Returns          pointer to the gate control block, or NULL if not found
**
*******************************************************************************/
tNFA_HCI_DYN_GATE* nfa_hciu_find_gate_by_gid(uint8_t gate_id) {
  tNFA_HCI_DYN_GATE* pg = nfa_hci_cb.cfg.dyn_gates;
  int xx = 0;

  for (; xx < NFA_HCI_MAX_GATE_CB; xx++, pg++) {
    if (pg->gate_id == gate_id) return (pg);
  }

  return (nullptr);
}

/*******************************************************************************
**
** Function         nfa_hciu_find_gate_by_owner
**
** Description      Find the the first gate control block for the given owner
**
** Returns          pointer to the gate control block, or NULL if not found
**
*******************************************************************************/
tNFA_HCI_DYN_GATE* nfa_hciu_find_gate_by_owner(tNFA_HANDLE app_handle) {
  tNFA_HCI_DYN_GATE* pg = nfa_hci_cb.cfg.dyn_gates;
  int xx = 0;

  for (; xx < NFA_HCI_MAX_GATE_CB; xx++, pg++) {
    if (pg->gate_owner == app_handle) return (pg);
  }

  return (nullptr);
}

/*******************************************************************************
**
** Function         nfa_hciu_find_gate_with_nopipes_by_owner
**
** Description      Find the the first gate control block with no pipes
**                  for the given owner
**
** Returns          pointer to the gate control block, or NULL if not found
**
*******************************************************************************/
tNFA_HCI_DYN_GATE* nfa_hciu_find_gate_with_nopipes_by_owner(
    tNFA_HANDLE app_handle) {
  tNFA_HCI_DYN_GATE* pg = nfa_hci_cb.cfg.dyn_gates;
  int xx = 0;

  for (; xx < NFA_HCI_MAX_GATE_CB; xx++, pg++) {
    if ((pg->gate_owner == app_handle) && (pg->pipe_inx_mask == 0)) return (pg);
  }

  return (nullptr);
}

/*******************************************************************************
**
** Function         nfa_hciu_count_pipes_on_gate
**
** Description      Count the number of pipes on the given gate
**
** Returns          the number of pipes on the gate
**
*******************************************************************************/
uint8_t nfa_hciu_count_pipes_on_gate(tNFA_HCI_DYN_GATE* p_gate) {
  int xx = 0;
  uint32_t mask = 1;
  uint8_t count = 0;

  for (; xx < NFA_HCI_MAX_PIPE_CB; xx++) {
    if (p_gate->pipe_inx_mask & mask) count++;

    mask = mask << 1;
  }

  return (count);
}

/*******************************************************************************
**
** Function         nfa_hciu_count_open_pipes_on_gate
**
** Description      Count the number of opened pipes on the given gate
**
** Returns          the number of pipes in OPENED state on the gate
**
*******************************************************************************/
uint8_t nfa_hciu_count_open_pipes_on_gate(tNFA_HCI_DYN_GATE* p_gate) {
  tNFA_HCI_DYN_PIPE* pp = nfa_hci_cb.cfg.dyn_pipes;
  int xx = 0;
  uint32_t mask = 1;
  uint8_t count = 0;

  for (; xx < NFA_HCI_MAX_PIPE_CB; xx++, pp++) {
    /* For each pipe on this gate, check if it is open */
    if ((p_gate->pipe_inx_mask & mask) &&
        (pp->pipe_state == NFA_HCI_PIPE_OPENED))
      count++;

    mask = mask << 1;
  }

  return (count);
}

/*******************************************************************************
**
** Function         nfa_hciu_get_gate_owner
**
** Description      Find the application that owns a gate
**
** Returns          application handle
**
*******************************************************************************/
tNFA_HANDLE nfa_hciu_get_gate_owner(uint8_t gate_id) {
  tNFA_HCI_DYN_GATE* pg;

  pg = nfa_hciu_find_gate_by_gid(gate_id);
  if (pg == nullptr) return (NFA_HANDLE_INVALID);

  return (pg->gate_owner);
}

/*******************************************************************************
**
** Function         nfa_hciu_get_pipe_owner
**
** Description      Find the application that owns a pipe
**
** Returns          application handle
**
*******************************************************************************/
tNFA_HANDLE nfa_hciu_get_pipe_owner(uint8_t pipe_id) {
  tNFA_HCI_DYN_PIPE* pp;
  tNFA_HCI_DYN_GATE* pg;

  pp = nfa_hciu_find_pipe_by_pid(pipe_id);
  if (pp == nullptr) return (NFA_HANDLE_INVALID);

  pg = nfa_hciu_find_gate_by_gid(pp->local_gate);
  if (pg == nullptr) return (NFA_HANDLE_INVALID);

  return (pg->gate_owner);
}

/*******************************************************************************
**
** Function         nfa_hciu_alloc_gate
**
** Description      Allocate an gate control block
**
** Returns          pointer to the allocated gate, or NULL if cannot allocate
**
*******************************************************************************/
tNFA_HCI_DYN_GATE* nfa_hciu_alloc_gate(uint8_t gate_id,
                                       tNFA_HANDLE app_handle) {
  tNFA_HCI_DYN_GATE* pg;
  int xx;
  uint8_t app_inx = app_handle & NFA_HANDLE_MASK;

  /* First, check if the application handle is valid */
  if ((gate_id != NFA_HCI_CONNECTIVITY_GATE) &&
#if(NXP_EXTNS == TRUE)
      (gate_id != NFA_HCI_ID_MNGMNT_APP_GATE) &&
      (gate_id != NFA_HCI_APDU_APP_GATE) &&
      (gate_id != NFA_HCI_APDU_GATE) &&
#endif
      (gate_id < NFA_HCI_FIRST_PROP_GATE) &&
      (((app_handle & NFA_HANDLE_GROUP_MASK) != NFA_HANDLE_GROUP_HCI) ||
       (app_inx >= NFA_HCI_MAX_APP_CB) ||
       (nfa_hci_cb.p_app_cback[app_inx] == nullptr))) {
    return (nullptr);
  }

  if (gate_id != 0) {
    pg = nfa_hciu_find_gate_by_gid(gate_id);
    if (pg != nullptr) return (pg);
  } else {
    /* If gate_id is 0, we need to assign a free one */
    /* Loop through all possible gate IDs checking if they are already used */
    uint32_t gate_id_index;
    for (gate_id_index = NFA_HCI_FIRST_HOST_SPECIFIC_GENERIC_GATE;
         gate_id_index <= NFA_HCI_LAST_PROP_GATE; gate_id_index++) {
      /* Skip connectivity gate */
      if (gate_id_index == NFA_HCI_CONNECTIVITY_GATE) continue;

      /* Check if the gate is already allocated */
      if (nfa_hciu_find_gate_by_gid(gate_id_index) == nullptr) {
        gate_id = gate_id_index & 0xFF;
        break;
      }
    }
    if (gate_id_index > NFA_HCI_LAST_PROP_GATE) {
      LOG(ERROR) << StringPrintf(
          "nfa_hci_alloc_gate - no free Gate ID: %u  App Handle: 0x%04x",
          gate_id_index, app_handle);
      return (nullptr);
    }
  }

  /* Now look for a free control block */
  for (xx = 0, pg = nfa_hci_cb.cfg.dyn_gates; xx < NFA_HCI_MAX_GATE_CB;
       xx++, pg++) {
    if (pg->gate_id == 0) {
      /* Found a free gate control block */
      pg->gate_id = gate_id;
      pg->gate_owner = app_handle;
      pg->pipe_inx_mask = 0;

      DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf(
          "nfa_hciu_alloc_gate id:%d  app_handle: 0x%04x", gate_id, app_handle);

      nfa_hci_cb.nv_write_needed = true;
      return (pg);
    }
  }

  /* If here, no free gate control block */
  LOG(ERROR) << StringPrintf(
      "nfa_hci_alloc_gate - no CB  Gate ID: %u  App Handle: 0x%04x", gate_id,
      app_handle);
  return (nullptr);
}

/*******************************************************************************
**
** Function         nfa_hciu_send_msg
**
** Description      This function will fragment the given packet, if necessary
**                  and send it on the given pipe.
**
** Returns          status
**
*******************************************************************************/
tNFA_STATUS nfa_hciu_send_msg(uint8_t pipe_id, uint8_t type,
                              uint8_t instruction, uint16_t msg_len,
                              uint8_t* p_msg) {
  NFC_HDR* p_buf;
  uint8_t* p_data;
  bool first_pkt = true;
  uint16_t data_len;
  tNFA_STATUS status = NFA_STATUS_OK;
#if(NXP_EXTNS == TRUE)
  uint16_t max_seg_hcp_pkt_size = nfa_hci_cb.buff_size;
#else
  uint16_t max_seg_hcp_pkt_size;
  if (nfa_hci_cb.buff_size > (NCI_DATA_HDR_SIZE + 2)) {
    max_seg_hcp_pkt_size = nfa_hci_cb.buff_size - NCI_DATA_HDR_SIZE;
  } else {
    android_errorWriteLog(0x534e4554, "124521372");
    return NFA_STATUS_NO_BUFFERS;
  }
#endif
  const uint8_t MAX_BUFF_SIZE = 100;
  char buff[MAX_BUFF_SIZE];

  DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf("nfa_hciu_send_msg pipe_id:%d   %s  len:%d", pipe_id,
                      nfa_hciu_get_type_inst_names(pipe_id, type, instruction,
                                                   buff, MAX_BUFF_SIZE),
                      msg_len);

  if (instruction == NFA_HCI_ANY_GET_PARAMETER)
    nfa_hci_cb.param_in_use = *p_msg;
#if (NXP_EXTNS == TRUE)
  if(pipe_id == NFA_HCI_APDUESE_PIPE)
  {
    nfa_hci_cb.m_wtx_count = 0;
  }
#endif

  while ((first_pkt == true) || (msg_len != 0)) {
#if (NXP_EXTNS == TRUE)
    p_buf = (NFC_HDR*)GKI_getpoolbuf(NFC_WIRED_POOL_ID);
#else
    p_buf = (NFC_HDR*)GKI_getpoolbuf(NFC_RW_POOL_ID);
#endif
    if (p_buf != nullptr) {
      p_buf->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE;

      /* First packet has a 2-byte header, subsequent fragments have a 1-byte
       * header */
      data_len =
          first_pkt ? (max_seg_hcp_pkt_size - 2) : (max_seg_hcp_pkt_size - 1);

      p_data = (uint8_t*)(p_buf + 1) + p_buf->offset;

      /* Last or only segment has "no fragmentation" bit set */
      if (msg_len > data_len) {
        *p_data++ = (NFA_HCI_MESSAGE_FRAGMENTATION << 7) | (pipe_id & 0x7F);
      } else {
        data_len = msg_len;
        *p_data++ = (NFA_HCI_NO_MESSAGE_FRAGMENTATION << 7) | (pipe_id & 0x7F);
      }

      p_buf->len = 1;

      /* Message header only goes in the first segment */
      if (first_pkt) {
        first_pkt = false;
        *p_data++ = (type << 6) | instruction;
        p_buf->len++;
      }
#if(NXP_EXTNS == TRUE)
      if ((data_len != 0) && (p_msg != nullptr)) {
#else
      if (data_len != 0) {
#endif
        memcpy(p_data, p_msg, data_len);

        p_buf->len += data_len;
        if (msg_len >= data_len) {
          msg_len -= data_len;
          p_msg += data_len;
        } else {
          msg_len = 0;
        }
      }

      if (HCI_LOOPBACK_DEBUG == NFA_HCI_DEBUG_ON)
        handle_debug_loopback(p_buf, type, instruction);
      else
        status = NFC_SendData(nfa_hci_cb.conn_id, p_buf);
    } else {
      LOG(ERROR) << StringPrintf("nfa_hciu_send_data_packet no buffers");
      status = NFA_STATUS_NO_BUFFERS;
      break;
    }
  }

  /* Start timer if response to wait for a particular time for the response  */
  if (type == NFA_HCI_COMMAND_TYPE) {
#if(NXP_EXTNS != TRUE)
    nfa_hci_cb.cmd_sent = instruction;

    if (nfa_hci_cb.hci_state == NFA_HCI_STATE_IDLE)
      nfa_hci_cb.hci_state = NFA_HCI_STATE_WAIT_RSP;

    nfa_sys_start_timer(&nfa_hci_cb.timer, NFA_HCI_RSP_TIMEOUT_EVT,
                        p_nfa_hci_cfg->hcp_response_timeout);
#else
      if (  (pipe_id == NFA_HCI_ADMIN_PIPE)
              ||(pipe_id == NFA_HCI_LINK_MANAGEMENT_PIPE))
      {
          nfa_hci_cb.cmd_sent = instruction;
          nfa_hci_cb.evt_sent.evt_type = 0;
          if (nfa_hci_cb.hci_state == NFA_HCI_STATE_IDLE)
              nfa_hci_cb.hci_state = NFA_HCI_STATE_WAIT_RSP;

          nfa_sys_start_timer(&nfa_hci_cb.timer, NFA_HCI_RSP_TIMEOUT_EVT,
                  p_nfa_hci_cfg->hcp_response_timeout);
      }
#endif
  }
#if(NXP_EXTNS == TRUE)
  else if(type == NFA_HCI_EVENT_TYPE)
  {
      nfa_hci_cb.cmd_sent = 0;
      nfa_hci_cb.evt_sent.evt_type = instruction;
  }
#endif
  return status;
}

/*******************************************************************************
**
** Function         nfa_hciu_get_allocated_gate_list
**
** Description      fills in a list of allocated gates
**
** Returns          the number of gates
**
*******************************************************************************/
uint8_t nfa_hciu_get_allocated_gate_list(uint8_t* p_gate_list) {
  tNFA_HCI_DYN_GATE* p_cb;
  int xx;
  uint8_t count = 0;

  for (xx = 0, p_cb = nfa_hci_cb.cfg.dyn_gates; xx < NFA_HCI_MAX_GATE_CB;
       xx++, p_cb++) {
    if (p_cb->gate_id != 0) {
      *p_gate_list++ = p_cb->gate_id;
      count++;
    }
  }

  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("returns: %u", count);

  return (count);
}

/*******************************************************************************
**
** Function         nfa_hciu_alloc_pipe
**
** Description      Allocate a pipe control block
**
** Returns          pointer to the pipe control block, or NULL if
**                  cannot allocate
**
*******************************************************************************/
tNFA_HCI_DYN_PIPE* nfa_hciu_alloc_pipe(uint8_t pipe_id) {
  uint8_t xx;
  tNFA_HCI_DYN_PIPE* pp;

  /* If we already have a pipe of the same ID, release it first it */
  pp = nfa_hciu_find_pipe_by_pid(pipe_id);
  if (pp != nullptr) {
    if (pipe_id > NFA_HCI_LAST_DYNAMIC_PIPE) return pp;
    nfa_hciu_release_pipe(pipe_id);
  }

  /* Look for a free pipe control block */
  for (xx = 0, pp = nfa_hci_cb.cfg.dyn_pipes; xx < NFA_HCI_MAX_PIPE_CB;
       xx++, pp++) {
    if (pp->pipe_id == 0) {
      DLOG_IF(INFO, nfc_debug_enabled)
          << StringPrintf("nfa_hciu_alloc_pipe:%d, index:%d", pipe_id, xx);
      pp->pipe_id = pipe_id;

      nfa_hci_cb.nv_write_needed = true;
      return (pp);
    }
  }

  DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf("nfa_hciu_alloc_pipe:%d, NO free entries !!", pipe_id);
  return (nullptr);
}

/*******************************************************************************
**
** Function         nfa_hciu_release_gate
**
** Description      Remove a generic gate from gate list
**
** Returns          none
**
*******************************************************************************/
void nfa_hciu_release_gate(uint8_t gate_id) {
  tNFA_HCI_DYN_GATE* p_gate = nfa_hciu_find_gate_by_gid(gate_id);

  if (p_gate != nullptr) {
    DLOG_IF(INFO, nfc_debug_enabled)
        << StringPrintf("ID: %d  owner: 0x%04x  pipe_inx_mask: 0x%04x", gate_id,
                        p_gate->gate_owner, p_gate->pipe_inx_mask);

    p_gate->gate_id = 0;
    p_gate->gate_owner = 0;
    p_gate->pipe_inx_mask = 0;

    nfa_hci_cb.nv_write_needed = true;
  } else {
    LOG(WARNING) << StringPrintf("ID: %d  NOT FOUND", gate_id);
  }
}

/*******************************************************************************
**
** Function         nfa_hciu_add_pipe_to_gate
**
** Description      Add pipe to generic gate
**
** Returns          NFA_STATUS_OK, if successfully add the pipe on to the gate
**                  NFA_HCI_ADM_E_NO_PIPES_AVAILABLE, otherwise
**
*******************************************************************************/
tNFA_HCI_RESPONSE nfa_hciu_add_pipe_to_gate(uint8_t pipe_id, uint8_t local_gate,
                                            uint8_t dest_host,
                                            uint8_t dest_gate) {
  tNFA_HCI_DYN_GATE* p_gate;
  tNFA_HCI_DYN_PIPE* p_pipe;
  uint8_t pipe_index;

  p_gate = nfa_hciu_find_gate_by_gid(local_gate);

  if (p_gate != nullptr) {
    /* Allocate a pipe control block */
    p_pipe = nfa_hciu_alloc_pipe(pipe_id);
    if (p_pipe != nullptr) {
      p_pipe->pipe_id = pipe_id;
      p_pipe->pipe_state = NFA_HCI_PIPE_CLOSED;
      p_pipe->dest_host = dest_host;
      p_pipe->dest_gate = dest_gate;
      p_pipe->local_gate = local_gate;

      /* Save the pipe in the gate that it belongs to */
      pipe_index = (uint8_t)(p_pipe - nfa_hci_cb.cfg.dyn_pipes);
      p_gate->pipe_inx_mask |= (uint32_t)(1 << pipe_index);

      DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf(
          "nfa_hciu_add_pipe_to_gate  Gate ID: 0x%02x  Pipe ID: 0x%02x  "
          "pipe_index: %u  App Handle: 0x%08x",
          local_gate, pipe_id, pipe_index, p_gate->gate_owner);
      return (NFA_HCI_ANY_OK);
    }
  }

  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf(
      "nfa_hciu_add_pipe_to_gate: 0x%02x  NOT FOUND", local_gate);

  return (NFA_HCI_ADM_E_NO_PIPES_AVAILABLE);
}

/*******************************************************************************
**
** Function         nfa_hciu_add_pipe_to_static_gate
**
** Description      Add pipe to identity management gate
**
** Returns          NFA_HCI_ANY_OK, if successfully add the pipe on to the gate
**                  NFA_HCI_ADM_E_NO_PIPES_AVAILABLE, otherwise
**
*******************************************************************************/
tNFA_HCI_RESPONSE nfa_hciu_add_pipe_to_static_gate(uint8_t local_gate,
                                                   uint8_t pipe_id,
                                                   uint8_t dest_host,
                                                   uint8_t dest_gate) {
  tNFA_HCI_DYN_PIPE* p_pipe;
  uint8_t pipe_index;

  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf(
      "nfa_hciu_add_pipe_to_static_gate (%u)  Pipe: 0x%02x  Dest Host: 0x%02x  "
      "Dest Gate: 0x%02x)",
      local_gate, pipe_id, dest_host, dest_gate);

  /* Allocate a pipe control block */
  p_pipe = nfa_hciu_alloc_pipe(pipe_id);
  if (p_pipe != nullptr) {
    p_pipe->pipe_id = pipe_id;
    p_pipe->pipe_state = NFA_HCI_PIPE_CLOSED;
    p_pipe->dest_host = dest_host;
    p_pipe->dest_gate = dest_gate;
    p_pipe->local_gate = local_gate;

    /* If this is the ID gate, save the pipe index in the ID gate info     */
    /* block. Note that for loopback, it is enough to just create the pipe */
    if (local_gate == NFA_HCI_IDENTITY_MANAGEMENT_GATE) {
      pipe_index = (uint8_t)(p_pipe - nfa_hci_cb.cfg.dyn_pipes);
      nfa_hci_cb.cfg.id_mgmt_gate.pipe_inx_mask |= (uint32_t)(1 << pipe_index);
    }
    return NFA_HCI_ANY_OK;
  }

  return NFA_HCI_ADM_E_NO_PIPES_AVAILABLE;
}

/*******************************************************************************
**
** Function         nfa_hciu_find_active_pipe_by_owner
**
** Description      Find the first pipe associated with the given app
**
** Returns          pointer to pipe, or NULL if none found
**
*******************************************************************************/
tNFA_HCI_DYN_PIPE* nfa_hciu_find_active_pipe_by_owner(tNFA_HANDLE app_handle) {
  tNFA_HCI_DYN_GATE* pg;
  tNFA_HCI_DYN_PIPE* pp;
  int xx;

  DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf("app_handle:0x%x", app_handle);

  /* Loop through all pipes looking for the owner */
  for (xx = 0, pp = nfa_hci_cb.cfg.dyn_pipes; xx < NFA_HCI_MAX_PIPE_CB;
       xx++, pp++) {
    if ((pp->pipe_id != 0) && (pp->pipe_id >= NFA_HCI_FIRST_DYNAMIC_PIPE) &&
        (pp->pipe_id <= NFA_HCI_LAST_DYNAMIC_PIPE) &&
        (nfa_hciu_is_active_host(pp->dest_host))) {
      if (((pg = nfa_hciu_find_gate_by_gid(pp->local_gate)) != nullptr) &&
          (pg->gate_owner == app_handle))
        return (pp);
    }
  }

  /* If here, not found */
  return (nullptr);
}

/*******************************************************************************
**
** Function         nfa_hciu_check_pipe_between_gates
**
** Description      Check if there is a pipe between specified Terminal host
**                  gate and and the specified UICC gate
**
** Returns          TRUE, if there exists a pipe between the two specified gated
**                  FALSE, otherwise
**
*******************************************************************************/
bool nfa_hciu_check_pipe_between_gates(uint8_t local_gate, uint8_t dest_host,
                                       uint8_t dest_gate) {
  tNFA_HCI_DYN_PIPE* pp;
  int xx;

  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf(
      "Local gate: 0x%02X, Host[0x%02X] "
      "gate: 0x%02X",
      local_gate, dest_host, dest_gate);

  /* Loop through all pipes looking for the owner */
  for (xx = 0, pp = nfa_hci_cb.cfg.dyn_pipes; xx < NFA_HCI_MAX_PIPE_CB;
       xx++, pp++) {
    if ((pp->pipe_id != 0) && (pp->pipe_id >= NFA_HCI_FIRST_DYNAMIC_PIPE) &&
        (pp->pipe_id <= NFA_HCI_LAST_DYNAMIC_PIPE) &&
        (pp->local_gate == local_gate) && (pp->dest_host == dest_host) &&
        (pp->dest_gate == dest_gate)) {
      return true;
    }
  }

  /* If here, not found */
  return false;
}

/*******************************************************************************
**
** Function         nfa_hciu_find_pipe_by_owner
**
** Description      Find the first pipe associated with the given app
**
** Returns          pointer to pipe, or NULL if none found
**
*******************************************************************************/
tNFA_HCI_DYN_PIPE* nfa_hciu_find_pipe_by_owner(tNFA_HANDLE app_handle) {
  tNFA_HCI_DYN_GATE* pg;
  tNFA_HCI_DYN_PIPE* pp;
  int xx;

  DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf("app_handle:0x%x", app_handle);

  /* Loop through all pipes looking for the owner */
  for (xx = 0, pp = nfa_hci_cb.cfg.dyn_pipes; xx < NFA_HCI_MAX_PIPE_CB;
       xx++, pp++) {
    if (pp->pipe_id != 0) {
      if (((pg = nfa_hciu_find_gate_by_gid(pp->local_gate)) != nullptr) &&
          (pg->gate_owner == app_handle))
        return (pp);
    }
  }

  /* If here, not found */
  return (nullptr);
}

/*******************************************************************************
**
** Function         nfa_hciu_find_pipe_on_gate
**
** Description      Find the first pipe associated with the given gate
**
** Returns          pointer to pipe, or NULL if none found
**
*******************************************************************************/
tNFA_HCI_DYN_PIPE* nfa_hciu_find_pipe_on_gate(uint8_t gate_id) {
  tNFA_HCI_DYN_GATE* pg;
  tNFA_HCI_DYN_PIPE* pp;
  int xx;

  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("Gate:0x%x", gate_id);

  /* Loop through all pipes looking for the owner */
  for (xx = 0, pp = nfa_hci_cb.cfg.dyn_pipes; xx < NFA_HCI_MAX_PIPE_CB;
       xx++, pp++) {
    if (pp->pipe_id != 0) {
      if (((pg = nfa_hciu_find_gate_by_gid(pp->local_gate)) != nullptr) &&
          (pg->gate_id == gate_id))
        return (pp);
    }
  }

  /* If here, not found */
  return (nullptr);
}

/*******************************************************************************
**
** Function         nfa_hciu_is_active_host
**
** Description      Check if the host is currently active
**
** Returns          TRUE, if the host is active in the host network
**                  FALSE, if the host is not active in the host network
**
*******************************************************************************/
bool nfa_hciu_is_active_host(uint8_t host_id) {
  uint8_t xx;
#if(NXP_EXTNS == TRUE)
  for (xx = 0; xx < NFA_HCI_MAX_HOST_IN_NETWORK; xx++) {
    if (nfa_hci_cb.active_host[xx] == host_id) return true;
  }
  return false;
#else
  for (xx = 0; xx < NFA_HCI_MAX_HOST_IN_NETWORK; xx++) {
    if (nfa_hci_cb.inactive_host[xx] == host_id) return false;
  }

  return true;
#endif
}

/*******************************************************************************
**
** Function         nfa_hciu_is_host_reseting
**
** Description      Check if the host is currently reseting
**
** Returns          TRUE, if the host is reseting
**                  FALSE, if the host is not reseting
**
*******************************************************************************/
bool nfa_hciu_is_host_reseting(uint8_t host_id) {
  uint8_t xx;

  for (xx = 0; xx < NFA_HCI_MAX_HOST_IN_NETWORK; xx++) {
#if(NXP_EXTNS == TRUE)
    if (nfa_hci_cb.reset_host[xx].host_id == host_id) return true;
#else
    if (nfa_hci_cb.reset_host[xx] == host_id) return true;
#endif
  }

  return false;
}

/*******************************************************************************
**
** Function         nfa_hciu_is_no_host_resetting
**
** Description      Check if no host is reseting
**
** Returns          TRUE, if no host is resetting at this time
**                  FALSE, if one or more host is resetting
**
*******************************************************************************/
bool nfa_hciu_is_no_host_resetting(void) {
  uint8_t xx;

  for (xx = 0; xx < NFA_HCI_MAX_HOST_IN_NETWORK; xx++) {
#if(NXP_EXTNS == TRUE)
    if (nfa_hci_cb.reset_host[xx].host_id != 0) return false;
#else
    if (nfa_hci_cb.reset_host[xx] != 0) return false;
#endif
  }

  return true;
}

/*******************************************************************************
**
** Function         nfa_hciu_find_active_pipe_on_gate
**
** Description      Find the first active pipe associated with the given gate
**
** Returns          pointer to pipe, or NULL if none found
**
*******************************************************************************/
tNFA_HCI_DYN_PIPE* nfa_hciu_find_active_pipe_on_gate(uint8_t gate_id) {
  tNFA_HCI_DYN_GATE* pg;
  tNFA_HCI_DYN_PIPE* pp;
  int xx;

  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("Gate:0x%x", gate_id);

  /* Loop through all pipes looking for the owner */
  for (xx = 0, pp = nfa_hci_cb.cfg.dyn_pipes; xx < NFA_HCI_MAX_PIPE_CB;
       xx++, pp++) {
    if ((pp->pipe_id != 0) && (pp->pipe_id >= NFA_HCI_FIRST_DYNAMIC_PIPE) &&
        (pp->pipe_id <= NFA_HCI_LAST_DYNAMIC_PIPE) &&
        (nfa_hciu_is_active_host(pp->dest_host))) {
      if (((pg = nfa_hciu_find_gate_by_gid(pp->local_gate)) != nullptr) &&
          (pg->gate_id == gate_id))
        return (pp);
    }
  }

  /* If here, not found */
  return (nullptr);
}

/*******************************************************************************
**
** Function         nfa_hciu_release_pipe
**
** Description      remove the specified pipe
**
** Returns          NFA_HCI_ANY_OK, if removed
**                  NFA_HCI_ANY_E_NOK, if otherwise
**
*******************************************************************************/
tNFA_HCI_RESPONSE nfa_hciu_release_pipe(uint8_t pipe_id) {
  tNFA_HCI_DYN_GATE* p_gate;
  tNFA_HCI_DYN_PIPE* p_pipe;
  uint8_t pipe_index;

  DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf("nfa_hciu_release_pipe: %u", pipe_id);

  p_pipe = nfa_hciu_find_pipe_by_pid(pipe_id);
  if (p_pipe == nullptr) return (NFA_HCI_ANY_E_NOK);

  if (pipe_id > NFA_HCI_LAST_DYNAMIC_PIPE) {
    DLOG_IF(INFO, nfc_debug_enabled)
        << StringPrintf("ignore pipe: %d", pipe_id);
    return (NFA_HCI_ANY_E_NOK);
  }

  pipe_index = (uint8_t)(p_pipe - nfa_hci_cb.cfg.dyn_pipes);

  if (p_pipe->local_gate == NFA_HCI_IDENTITY_MANAGEMENT_GATE) {
    /* Remove pipe from ID management gate */
    nfa_hci_cb.cfg.id_mgmt_gate.pipe_inx_mask &= ~(uint32_t)(1 << pipe_index);
  } else {
    p_gate = nfa_hciu_find_gate_by_gid(p_pipe->local_gate);
    if (p_gate == nullptr) {
      /* Mark the pipe control block as free */
      p_pipe->pipe_id = 0;
      return (NFA_HCI_ANY_E_NOK);
    }

    /* Remove pipe from gate */
    p_gate->pipe_inx_mask &= ~(uint32_t)(1 << pipe_index);
  }

  /* Reset pipe control block */
  memset(p_pipe, 0, sizeof(tNFA_HCI_DYN_PIPE));
  nfa_hci_cb.nv_write_needed = true;
  return NFA_HCI_ANY_OK;
}

/*******************************************************************************
**
** Function         nfa_hciu_remove_all_pipes_from_host
**
** Description      remove all the pipes that are connected to a specific host
**
** Returns          None
**
*******************************************************************************/
void nfa_hciu_remove_all_pipes_from_host(uint8_t host) {
  tNFA_HCI_DYN_GATE* pg;
  tNFA_HCI_DYN_PIPE* pp;
  int xx;
  tNFA_HCI_EVT_DATA evt_data;

  DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf("nfa_hciu_remove_all_pipes_from_host (0x%02x)", host);

  /* Remove all pipes from the specified host connected to all generic gates */
  for (xx = 0, pp = nfa_hci_cb.cfg.dyn_pipes; xx < NFA_HCI_MAX_PIPE_CB;
       xx++, pp++) {
    if ((pp->pipe_id == 0) ||
        ((host != 0) && ((pp->dest_host != host) ||
                         (pp->pipe_id > NFA_HCI_LAST_DYNAMIC_PIPE))))
      continue;

    pg = nfa_hciu_find_gate_by_gid(pp->local_gate);
    if (pg != nullptr) {
      evt_data.deleted.status = NFA_STATUS_OK;
      evt_data.deleted.pipe = pp->pipe_id;

      nfa_hciu_send_to_app(NFA_HCI_DELETE_PIPE_EVT, &evt_data, pg->gate_owner);
    }
    nfa_hciu_release_pipe(pp->pipe_id);
  }
}

/*******************************************************************************
**
** Function         nfa_hciu_send_create_pipe_cmd
**
** Description      Create dynamic pipe between the specified gates
**
** Returns          status
**
*******************************************************************************/
tNFA_STATUS nfa_hciu_send_create_pipe_cmd(uint8_t source_gate,
                                          uint8_t dest_host,
                                          uint8_t dest_gate) {
  tNFA_STATUS status;
  uint8_t data[3];

  data[0] = source_gate;
  data[1] = dest_host;
  data[2] = dest_gate;

  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf(
      "nfa_hciu_send_create_pipe_cmd source_gate:%d, dest_host:%d, "
      "dest_gate:%d",
      source_gate, dest_host, dest_gate);

  status = nfa_hciu_send_msg(NFA_HCI_ADMIN_PIPE, NFA_HCI_COMMAND_TYPE,
                             NFA_HCI_ADM_CREATE_PIPE, 3, data);
#if(NXP_EXTNS == TRUE)
  if (status == NFA_STATUS_OK)
  {
      nfa_hci_cb.local_gate_in_use  = source_gate;
      nfa_hci_cb.remote_host_in_use = dest_host;
      nfa_hci_cb.remote_gate_in_use = dest_gate;
  }
#endif
  return status;
}

/*******************************************************************************
**
** Function         nfa_hciu_send_delete_pipe_cmd
**
** Description      Delete the dynamic pipe
**
** Returns          None
**
*******************************************************************************/
tNFA_STATUS nfa_hciu_send_delete_pipe_cmd(uint8_t pipe) {
  tNFA_STATUS status;

  DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf("nfa_hciu_send_delete_pipe_cmd: %d", pipe);

  if (pipe > NFA_HCI_LAST_DYNAMIC_PIPE) {
    DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("ignore pipe: %d", pipe);
    return (NFA_HCI_ANY_E_NOK);
  }
  nfa_hci_cb.pipe_in_use = pipe;

  status = nfa_hciu_send_msg(NFA_HCI_ADMIN_PIPE, NFA_HCI_COMMAND_TYPE,
                             NFA_HCI_ADM_DELETE_PIPE, 1, &pipe);
#if(NXP_EXTNS == TRUE)
  if (status == NFA_STATUS_OK)
  {
      nfa_hci_cb.pipe_in_use = pipe;
  }
#endif
  return status;
}

/*******************************************************************************
**
** Function         nfa_hciu_send_clear_all_pipe_cmd
**
** Description      delete all the dynamic pipe connected to device host,
**                  to close all static pipes connected to device host,
**                  and to set registry values related to static pipes to
**                  theri default values.
**
** Returns          None
**
*******************************************************************************/
tNFA_STATUS nfa_hciu_send_clear_all_pipe_cmd(void) {
  tNFA_STATUS status;
  uint16_t id_ref_data = 0x0102;

  DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf("nfa_hciu_send_clear_all_pipe_cmd");

  status =
      nfa_hciu_send_msg(NFA_HCI_ADMIN_PIPE, NFA_HCI_COMMAND_TYPE,
                        NFA_HCI_ADM_CLEAR_ALL_PIPE, 2, (uint8_t*)&id_ref_data);

  return status;
}

/*******************************************************************************
**
** Function         nfa_hciu_send_open_pipe_cmd
**
** Description      Open a closed pipe
**
** Returns          status
**
*******************************************************************************/
tNFA_STATUS nfa_hciu_send_open_pipe_cmd(uint8_t pipe) {
  tNFA_STATUS status;
#if(NXP_EXTNS == TRUE)
  tNFA_HCI_PIPE_CMDRSP_INFO  *p_pipe_cmdrsp_info = nullptr;
#endif
  nfa_hci_cb.pipe_in_use = pipe;

  status = nfa_hciu_send_msg(pipe, NFA_HCI_COMMAND_TYPE, NFA_HCI_ANY_OPEN_PIPE,
                             0, nullptr);
#if(NXP_EXTNS == TRUE)
  if (status == NFA_STATUS_OK)
  {
      if ((pipe != NFA_HCI_ADMIN_PIPE) && (pipe != NFA_HCI_LINK_MANAGEMENT_PIPE))
      {
          p_pipe_cmdrsp_info = nfa_hciu_get_pipe_cmdrsp_info (pipe);
          if (p_pipe_cmdrsp_info)
          {
              p_pipe_cmdrsp_info->cmd_inst_sent   = NFA_HCI_ANY_OPEN_PIPE;
              p_pipe_cmdrsp_info->w4_cmd_rsp      = true;

              nfa_sys_start_timer (&(p_pipe_cmdrsp_info->rsp_timer),
                                    NFA_HCI_RSP_TIMEOUT_EVT,
                                    p_nfa_hci_cfg->hcp_response_timeout);
          }
      }
  }
#endif
  return status;
}

/*******************************************************************************
**
** Function         nfa_hciu_send_close_pipe_cmd
**
** Description      Close an opened pipe
**
** Returns          status
**
*******************************************************************************/
tNFA_STATUS nfa_hciu_send_close_pipe_cmd(uint8_t pipe) {
  tNFA_STATUS status;
#if(NXP_EXTNS == TRUE)
  tNFA_HCI_PIPE_CMDRSP_INFO  *p_pipe_cmdrsp_info = nullptr;
#endif
  nfa_hci_cb.pipe_in_use = pipe;

  status = nfa_hciu_send_msg(pipe, NFA_HCI_COMMAND_TYPE, NFA_HCI_ANY_CLOSE_PIPE,
                             0, nullptr);
#if(NXP_EXTNS == TRUE)
  if (status == NFA_STATUS_OK)
  {
      if ((pipe != NFA_HCI_ADMIN_PIPE) && (pipe != NFA_HCI_LINK_MANAGEMENT_PIPE))
      {
          p_pipe_cmdrsp_info = nfa_hciu_get_pipe_cmdrsp_info (pipe);
          if (p_pipe_cmdrsp_info)
          {
              p_pipe_cmdrsp_info->cmd_inst_sent   = NFA_HCI_ANY_CLOSE_PIPE;
              p_pipe_cmdrsp_info->w4_cmd_rsp = TRUE;
              nfa_sys_start_timer (&(p_pipe_cmdrsp_info->rsp_timer),
                                    NFA_HCI_RSP_TIMEOUT_EVT,
                                    p_nfa_hci_cfg->hcp_response_timeout);
          }
      }
  }
#endif
  return status;
}

/*******************************************************************************
**
** Function         nfa_hciu_send_get_param_cmd
**
** Description      Read a parameter value from gate registry
**
** Returns          None
**
*******************************************************************************/
tNFA_STATUS nfa_hciu_send_get_param_cmd(uint8_t pipe, uint8_t index) {
  tNFA_STATUS status;
#if(NXP_EXTNS == TRUE)
  tNFA_HCI_PIPE_CMDRSP_INFO  *p_pipe_cmdrsp_info = nullptr;
#endif
  status = nfa_hciu_send_msg(pipe, NFA_HCI_COMMAND_TYPE,
                             NFA_HCI_ANY_GET_PARAMETER, 1, &index);
#if(NXP_EXTNS == TRUE)
  if (status == NFC_STATUS_OK) {
      if ((pipe == NFA_HCI_ADMIN_PIPE) || (pipe == NFA_HCI_LINK_MANAGEMENT_PIPE))
      {
          nfa_hci_cb.param_in_use = index;
      }
      else
      {
          p_pipe_cmdrsp_info = nfa_hciu_get_pipe_cmdrsp_info (pipe);
          if (p_pipe_cmdrsp_info)
          {
              p_pipe_cmdrsp_info->cmd_inst_sent       = NFA_HCI_ANY_GET_PARAMETER;
              p_pipe_cmdrsp_info->cmd_inst_param_sent = index;
              p_pipe_cmdrsp_info->w4_cmd_rsp     = TRUE;

              nfa_sys_start_timer (&(p_pipe_cmdrsp_info->rsp_timer),
                      NFA_HCI_RSP_TIMEOUT_EVT,
                      p_nfa_hci_cfg->hcp_response_timeout);
          }
      }
  }
#else
  if (status == NFC_STATUS_OK) nfa_hci_cb.param_in_use = index;
#endif
  return status;
}

/*******************************************************************************
**
** Function         nfa_hciu_send_set_param_cmd
**
** Description      Set a parameter value in a gate registry
**
** Returns          None
**
*******************************************************************************/
tNFA_STATUS nfa_hciu_send_set_param_cmd(uint8_t pipe, uint8_t index,
                                        uint8_t length, uint8_t* p_data) {
  tNFA_STATUS status;
  uint8_t data[255];
#if(NXP_EXTNS == TRUE)
  tNFA_HCI_PIPE_CMDRSP_INFO  *p_pipe_cmdrsp_info = nullptr;
#endif
  data[0] = index;

  memcpy(&data[1], p_data, length);

  status =
      nfa_hciu_send_msg(pipe, NFA_HCI_COMMAND_TYPE, NFA_HCI_ANY_SET_PARAMETER,
                        (uint16_t)(length + 1), data);
#if(NXP_EXTNS == TRUE)
  if (status == NFC_STATUS_OK) {
      if (  (pipe == NFA_HCI_ADMIN_PIPE)
          ||(pipe == NFA_HCI_LINK_MANAGEMENT_PIPE)  )
      {
          nfa_hci_cb.param_in_use = index;
      }
      else
      {
          p_pipe_cmdrsp_info = nfa_hciu_get_pipe_cmdrsp_info (pipe);

          if (p_pipe_cmdrsp_info)
          {
              p_pipe_cmdrsp_info->cmd_inst_sent       = NFA_HCI_ANY_SET_PARAMETER;
              p_pipe_cmdrsp_info->cmd_inst_param_sent = index;
              p_pipe_cmdrsp_info->w4_cmd_rsp     = TRUE;

              nfa_sys_start_timer (&(p_pipe_cmdrsp_info->rsp_timer),
                                    NFA_HCI_RSP_TIMEOUT_EVT,
                                    p_nfa_hci_cfg->hcp_response_timeout);
          }
      }
  }
#else
  if (status == NFC_STATUS_OK) nfa_hci_cb.param_in_use = index;
#endif
  return status;
}

/*******************************************************************************
**
** Function         nfa_hciu_send_to_app
**
** Description      Send an event back to an application
**
** Returns          none
**
*******************************************************************************/
void nfa_hciu_send_to_app(tNFA_HCI_EVT event, tNFA_HCI_EVT_DATA* p_evt,
                          tNFA_HANDLE app_handle) {
  uint8_t app_inx = app_handle & NFA_HANDLE_MASK;

  /* First, check if the application handle is valid */
  if (((app_handle & NFA_HANDLE_GROUP_MASK) == NFA_HANDLE_GROUP_HCI) &&
      (app_inx < NFA_HCI_MAX_APP_CB)) {
    if (nfa_hci_cb.p_app_cback[app_inx] != nullptr) {
      nfa_hci_cb.p_app_cback[app_inx](event, p_evt);
      return;
    }
  }

  if (app_handle != NFA_HANDLE_INVALID) {
    LOG(WARNING) << StringPrintf(
        "nfa_hciu_send_to_app no callback,  event: 0x%04x  app_handle: 0x%04x",
        event, app_handle);
  }
}

/*******************************************************************************
**
** Function         nfa_hciu_send_to_all_apps
**
** Description      Send an event back to all applications
**
** Returns          none
**
*******************************************************************************/
void nfa_hciu_send_to_all_apps(tNFA_HCI_EVT event, tNFA_HCI_EVT_DATA* p_evt) {
  uint8_t app_inx;

  for (app_inx = 0; app_inx < NFA_HCI_MAX_APP_CB; app_inx++) {
    if (nfa_hci_cb.p_app_cback[app_inx] != nullptr)
      nfa_hci_cb.p_app_cback[app_inx](event, p_evt);
  }
}

/*******************************************************************************
**
** Function         nfa_hciu_send_to_apps_handling_connectivity_evts
**
** Description      Send a connectivity event to all the application interested
**                  in connectivity events
**
** Returns          none
**
*******************************************************************************/
void nfa_hciu_send_to_apps_handling_connectivity_evts(
    tNFA_HCI_EVT event, tNFA_HCI_EVT_DATA* p_evt) {
  uint8_t app_inx;

  for (app_inx = 0; app_inx < NFA_HCI_MAX_APP_CB; app_inx++) {
    if ((nfa_hci_cb.p_app_cback[app_inx] != nullptr) &&
        (nfa_hci_cb.cfg.b_send_conn_evts[app_inx]))

      nfa_hci_cb.p_app_cback[app_inx](event, p_evt);
  }
}

/*******************************************************************************
**
** Function         nfa_hciu_get_response_name
**
** Description      This function returns the error code name.
**
** NOTE             conditionally compiled to save memory.
**
** Returns          pointer to the name
**
*******************************************************************************/
static std::string nfa_hciu_get_response_name(uint8_t rsp_code) {
  switch (rsp_code) {
    case NFA_HCI_ANY_OK:
      return "ANY_OK";
    case NFA_HCI_ANY_E_NOT_CONNECTED:
      return "ANY_E_NOT_CONNECTED";
    case NFA_HCI_ANY_E_CMD_PAR_UNKNOWN:
      return "ANY_E_CMD_PAR_UNKNOWN";
    case NFA_HCI_ANY_E_NOK:
      return "ANY_E_NOK";
    case NFA_HCI_ADM_E_NO_PIPES_AVAILABLE:
      return "ADM_E_NO_PIPES_AVAILABLE";
    case NFA_HCI_ANY_E_REG_PAR_UNKNOWN:
      return "ANY_E_REG_PAR_UNKNOWN";
    case NFA_HCI_ANY_E_PIPE_NOT_OPENED:
      return "ANY_E_PIPE_NOT_OPENED";
    case NFA_HCI_ANY_E_CMD_NOT_SUPPORTED:
      return "ANY_E_CMD_NOT_SUPPORTED";
    case NFA_HCI_ANY_E_INHIBITED:
      return "ANY_E_INHIBITED";
    case NFA_HCI_ANY_E_TIMEOUT:
      return "ANY_E_TIMEOUT";
    case NFA_HCI_ANY_E_REG_ACCESS_DENIED:
      return "ANY_E_REG_ACCESS_DENIED";
    case NFA_HCI_ANY_E_PIPE_ACCESS_DENIED:
      return "ANY_E_PIPE_ACCESS_DENIED";
    default:
      return "UNKNOWN";
  }
}

/*******************************************************************************
**
** Function         nfa_hciu_type_2_str
**
** Description      This function returns the type name.
**
** Returns          pointer to the name
**
*******************************************************************************/
static std::string nfa_hciu_type_2_str(uint8_t type) {
  switch (type) {
    case NFA_HCI_COMMAND_TYPE:
      return "COMMAND";
    case NFA_HCI_EVENT_TYPE:
      return "EVENT";
    case NFA_HCI_RESPONSE_TYPE:
      return "RESPONSE";
    default:
      return "UNKNOWN";
  }
}

/*******************************************************************************
**
** Function         nfa_hciu_instr_2_str
**
** Description      This function returns the instruction name.
**
** Returns          pointer to the name
**
*******************************************************************************/
std::string nfa_hciu_instr_2_str(uint8_t instruction) {
  switch (instruction) {
    case NFA_HCI_ANY_SET_PARAMETER:
      return "ANY_SET_PARAMETER";
    case NFA_HCI_ANY_GET_PARAMETER:
      return "ANY_GET_PARAMETER";
    case NFA_HCI_ANY_OPEN_PIPE:
      return "ANY_OPEN_PIPE";
    case NFA_HCI_ANY_CLOSE_PIPE:
      return "ANY_CLOSE_PIPE";
    case NFA_HCI_ADM_CREATE_PIPE:
      return "ADM_CREATE_PIPE";
    case NFA_HCI_ADM_DELETE_PIPE:
      return "ADM_DELETE_PIPE";
    case NFA_HCI_ADM_NOTIFY_PIPE_CREATED:
      return "ADM_NOTIFY_PIPE_CREATED";
    case NFA_HCI_ADM_NOTIFY_PIPE_DELETED:
      return "ADM_NOTIFY_PIPE_DELETED";
    case NFA_HCI_ADM_CLEAR_ALL_PIPE:
      return "ADM_CLEAR_ALL_PIPE";
    case NFA_HCI_ADM_NOTIFY_ALL_PIPE_CLEARED:
      return "ADM_NOTIFY_ALL_PIPE_CLEARED";
    default:
      return "UNKNOWN";
  }
}

/*******************************************************************************
**
** Function         nfa_hciu_get_event_name
**
** Description      This function returns the event code name.
**
** Returns          pointer to the name
**
*******************************************************************************/
std::string nfa_hciu_get_event_name(uint16_t event) {
  switch (event) {
    case NFA_HCI_API_REGISTER_APP_EVT:
      return "API_REGISTER";
    case NFA_HCI_API_DEREGISTER_APP_EVT:
      return "API_DEREGISTER";
    case NFA_HCI_API_GET_APP_GATE_PIPE_EVT:
      return "API_GET_GATE_LIST";
    case NFA_HCI_API_ALLOC_GATE_EVT:
      return "API_ALLOC_GATE";
    case NFA_HCI_API_DEALLOC_GATE_EVT:
      return "API_DEALLOC_GATE";
    case NFA_HCI_API_GET_HOST_LIST_EVT:
      return "API_GET_HOST_LIST";
    case NFA_HCI_API_GET_REGISTRY_EVT:
      return "API_GET_REG_VALUE";
    case NFA_HCI_API_SET_REGISTRY_EVT:
      return "API_SET_REG_VALUE";
    case NFA_HCI_API_CREATE_PIPE_EVT:
      return "API_CREATE_PIPE";
    case NFA_HCI_API_OPEN_PIPE_EVT:
      return "API_OPEN_PIPE";
    case NFA_HCI_API_CLOSE_PIPE_EVT:
      return "API_CLOSE_PIPE";
    case NFA_HCI_API_DELETE_PIPE_EVT:
      return "API_DELETE_PIPE";
    case NFA_HCI_API_SEND_CMD_EVT:
      return "API_SEND_COMMAND_EVT";
    case NFA_HCI_API_SEND_RSP_EVT:
      return "API_SEND_RESPONSE_EVT";
    case NFA_HCI_API_SEND_EVENT_EVT:
      return "API_SEND_EVENT_EVT";
    case NFA_HCI_RSP_NV_READ_EVT:
      return "NV_READ_EVT";
    case NFA_HCI_RSP_NV_WRITE_EVT:
      return "NV_WRITE_EVT";
    case NFA_HCI_RSP_TIMEOUT_EVT:
      return "RESPONSE_TIMEOUT_EVT";
    case NFA_HCI_CHECK_QUEUE_EVT:
      return "CHECK_QUEUE";
#if(NXP_EXTNS == TRUE)
    case NFA_HCI_API_SEND_APDU_EVT:
      return "API_SEND_APDU_EVT";
    case NFA_HCI_API_ABORT_APDU_EVT:
      return "API_ABORT_APDU_EVT";
#endif
    default:
      return "UNKNOWN";
  }
}

/*******************************************************************************
**
** Function         nfa_hciu_get_state_name
**
** Description      This function returns the state name.
**
** Returns          pointer to the name
**
*******************************************************************************/
std::string nfa_hciu_get_state_name(uint8_t state) {
  switch (state) {
    case NFA_HCI_STATE_DISABLED:
      return "DISABLED";
    case NFA_HCI_STATE_STARTUP:
      return "STARTUP";
    case NFA_HCI_STATE_WAIT_NETWK_ENABLE:
      return "WAIT_NETWK_ENABLE";
    case NFA_HCI_STATE_IDLE:
      return "IDLE";
    case NFA_HCI_STATE_WAIT_RSP:
      return "WAIT_RSP";
    case NFA_HCI_STATE_REMOVE_GATE:
      return "REMOVE_GATE";
    case NFA_HCI_STATE_APP_DEREGISTER:
      return "APP_DEREGISTER";
    case NFA_HCI_STATE_RESTORE:
      return "RESTORE";
    case NFA_HCI_STATE_RESTORE_NETWK_ENABLE:
      return "WAIT_NETWK_ENABLE_AFTER_RESTORE";
    default:
      return "UNKNOWN";
  }
}

/*******************************************************************************
**
** Function         nfa_hciu_get_type_inst_names
**
** Description      This function returns command/response/event name.
**
** Returns          none
**
*******************************************************************************/
char* nfa_hciu_get_type_inst_names(uint8_t pipe, uint8_t type, uint8_t inst,
                                   char* p_buff, const uint8_t max_buff_size) {
  int xx;

  xx = snprintf(p_buff, max_buff_size, "Type: %s [0x%02x] ",
                nfa_hciu_type_2_str(type).c_str(), type);
  if (xx < 0) {
    LOG(ERROR) << StringPrintf("snprintf returned error !");
    return p_buff;
  }

  switch (type) {
    case NFA_HCI_COMMAND_TYPE:
      snprintf(&p_buff[xx], max_buff_size - xx, "Inst: %s [0x%02x] ",
               nfa_hciu_instr_2_str(inst).c_str(), inst);

      break;
    case NFA_HCI_EVENT_TYPE:
      snprintf(&p_buff[xx], max_buff_size - xx, "Evt: %s [0x%02x] ",
               nfa_hciu_evt_2_str(pipe, inst).c_str(), inst);

      break;
    case NFA_HCI_RESPONSE_TYPE:
      snprintf(&p_buff[xx], max_buff_size - xx, "Resp: %s [0x%02x] ",
               nfa_hciu_get_response_name(inst).c_str(), inst);

      break;
    default:
      snprintf(&p_buff[xx], max_buff_size - xx, "Inst: %u ", inst);
      break;
  }
  return p_buff;
}

/*******************************************************************************
**
** Function         nfa_hciu_evt_2_str
**
** Description      This function returns the event name.
**
** Returns          pointer to the name
**
*******************************************************************************/
std::string nfa_hciu_evt_2_str(uint8_t pipe_id, uint8_t evt) {
  tNFA_HCI_DYN_PIPE* p_pipe = nfa_hciu_find_pipe_by_pid(pipe_id);
  if (pipe_id != NFA_HCI_ADMIN_PIPE &&
      pipe_id != NFA_HCI_LINK_MANAGEMENT_PIPE && p_pipe != nullptr &&
      p_pipe->local_gate == NFA_HCI_CONNECTIVITY_GATE) {
    switch (evt) {
      case NFA_HCI_EVT_CONNECTIVITY:
        return "EVT_CONNECTIVITY";
      case NFA_HCI_EVT_TRANSACTION:
        return "EVT_TRANSACTION";
      case NFA_HCI_EVT_OPERATION_ENDED:
        return "EVT_OPERATION_ENDED";
      default:
        return "UNKNOWN";
    }
  }

  switch (evt) {
    case NFA_HCI_EVT_HCI_END_OF_OPERATION:
      return "EVT_END_OF_OPERATION";
    case NFA_HCI_EVT_POST_DATA:
      return "EVT_POST_DATA";
    case NFA_HCI_EVT_HOT_PLUG:
      return "EVT_HOT_PLUG";
    default:
      return "UNKNOWN";
  }
}

static void handle_debug_loopback(NFC_HDR* p_buf, uint8_t type,
                                  uint8_t instruction) {
  uint8_t* p = (uint8_t*)(p_buf + 1) + p_buf->offset;
  static uint8_t next_pipe = 0x10;

  if (type == NFA_HCI_COMMAND_TYPE) {
    switch (instruction) {
      case NFA_HCI_ADM_CREATE_PIPE:
        p[6] = next_pipe++;
        p[5] = p[4];
        p[4] = p[3];
        p[3] = p[2];
        p[2] = 3;
        p[1] = (NFA_HCI_RESPONSE_TYPE << 6) | NFA_HCI_ANY_OK;
        p_buf->len = p_buf->offset + 7;
        break;

      case NFA_HCI_ANY_GET_PARAMETER:
        p[1] = (NFA_HCI_RESPONSE_TYPE << 6) | NFA_HCI_ANY_OK;
        memcpy(&p[2], (uint8_t*)nfa_hci_cb.cfg.admin_gate.session_id,
               NFA_HCI_SESSION_ID_LEN);
        p_buf->len = p_buf->offset + 2 + NFA_HCI_SESSION_ID_LEN;
        break;

      default:
        p[1] = (NFA_HCI_RESPONSE_TYPE << 6) | NFA_HCI_ANY_OK;
        p_buf->len = p_buf->offset + 2;
        break;
    }
  } else if (type == NFA_HCI_RESPONSE_TYPE) {
    GKI_freebuf(p_buf);
    return;
  }

  p_buf->event = NFA_HCI_CHECK_QUEUE_EVT;
  nfa_sys_sendmsg(p_buf);
}
#if(NXP_EXTNS == TRUE)
/*******************************************************************************
**
** Function         nfa_hciu_send_raw_cmd
**
** Description      send raw command from the HCI module
**
** Returns          tNFA_STATUS  NFA_STATUS_OK or NFA_STATUS_FAILED
**
*******************************************************************************/
tNFA_STATUS nfa_hciu_send_raw_cmd(uint8_t param_len, uint8_t* p_data,
                                  tNFA_VSC_CBACK* p_cback) {
  tNFA_STATUS status = NFA_STATUS_OK;
  status = NFA_SendRawVsCommand(param_len, p_data, p_cback);
  if (NFA_STATUS_OK == status) {
    nfa_sys_start_timer(&nfa_hci_cb.timer, NFA_HCI_RSP_TIMEOUT_EVT,
                        p_nfa_hci_cfg->hcp_response_timeout);
  }
  return status;
}



/*******************************************************************************
**
** Function         nfa_hciu_reset_session_id
**
+** Description      reset ESE session ID to FF
**
** Returns          tNFA_STATUS
**
*******************************************************************************/
tNFA_STATUS nfa_hciu_reset_session_id(tNFA_VSC_CBACK* p_cback) {
  tNFA_STATUS status = NFA_STATUS_FAILED;
  uint8_t* pp, *p_start;
  uint8_t cmd_len = 0;
  tNFA_DM_API_SEND_VSC* p_data;
  NFC_HDR* p_cmd;
  uint8_t id_buf[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf("%s: enter", __func__);

  p_data = (tNFA_DM_API_SEND_VSC*)GKI_getbuf(sizeof(tNFA_DM_API_SEND_VSC) +
                                             NXP_NFC_PROP_MAX_CMD_BUF_SIZE);
  if (p_data != nullptr) {
    p_cmd = (NFC_HDR*)p_data;
    p_cmd->offset = sizeof(tNFA_DM_API_SEND_VSC) - NFC_HDR_SIZE;
    pp = (uint8_t*)(p_cmd + 1) + p_cmd->offset;
    NCI_MSG_BLD_HDR0(pp, NCI_MT_CMD, NCI_GID_CORE);
    NCI_MSG_BLD_HDR1(pp, NCI_MSG_CORE_SET_CONFIG);
    p_start = pp;
    pp++;
    UINT8_TO_STREAM(pp, 0x01);
    UINT8_TO_STREAM(pp, NXP_NFC_SET_CONFIG_PARAM_EXT);
    UINT8_TO_STREAM(pp, NXP_NFC_PARAM_SWP_SESSIONID_INT2);
    UINT8_TO_STREAM(pp, NXP_NFC_PARAM_SWP_SESSION_ID_LEN);
    memcpy(pp, id_buf, NXP_NFC_PARAM_SWP_SESSION_ID_LEN);
    pp = pp + NXP_NFC_PARAM_SWP_SESSION_ID_LEN;
    cmd_len = (pp - p_start) - 1; /*skip len byte filed*/
    pp = p_start;
    UINT8_TO_STREAM(pp, cmd_len);
    p_cmd->len = cmd_len + NCI_DATA_HDR_SIZE;
    status = NFC_SendRawVsCommand(p_cmd, p_cback);
  }
  return status;
}

/*******************************************************************************
**
** Function         nfa_hciu_set_server_apdu_host_not_ready
**
** Description      Set flag to indicate APDU cannot be sent on APDU Pipe until
**                  server host indicate that it is ready for APDU processing
**
** Returns          None
**
*******************************************************************************/
void nfa_hciu_set_server_apdu_host_not_ready (tNFA_HCI_DYN_GATE *p_gate)
{
    uint8_t             xx    = 0;
    uint32_t            mask  = 1;
    tNFA_HCI_DYN_PIPE *pp   = nfa_hci_cb.cfg.dyn_pipes;
    DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf ("nfa_hciu_set_server_apdu_host_not_ready enter");

    if(p_gate != nullptr) {
        for ( ; xx < NFA_HCI_MAX_PIPE_CB; xx++, pp++)
        {
            DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf ("nfa_hciu_reset_gate_list_of_host  xx:0x%x", xx);
            /* For each pipe on this gate, check if it is open */
            if ((p_gate->pipe_inx_mask & mask) && (pp->pipe_state == NFA_HCI_PIPE_OPENED))
            {
                nfa_hci_cb.dyn_pipe_cmdrsp_info[xx].w4_atr_evt = true;
            }
            mask = mask << 1;
        }
    }
}

/*******************************************************************************
**
** Function         nfa_hciu_reset_gate_list_of_host
**
** Description      Reset Gate list of the specified host
**
** Returns          None
**
*******************************************************************************/
void nfa_hciu_reset_gate_list_of_host (uint8_t host_id)
{
    uint8_t xx;
    tNFA_HCI_HOST_INFO *p_host;

    DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf ("nfa_hciu_reset_gate_list_of_host () Host:0x%x", host_id);
    for(xx = 0; xx < NFA_HCI_MAX_HOST_IN_NETWORK; xx++) {
        p_host = &nfa_hci_cb.cfg.host[xx];
        if(host_id == p_host->host_id) {
            p_host->num_gates = 0;
            memset (p_host->gate_id, 0, NFA_HCI_MAX_GATE_CB);
            break;
        }
    }
}

/*******************************************************************************
**
** Function         nfa_hciu_update_gate_list_of_host
**
** Description      Update Gate list of the specified host
**
** Returns          None
**
*******************************************************************************/
void nfa_hciu_update_gate_list_of_host (uint8_t host_id, uint8_t num_gates, uint8_t *p_gates)
{
    uint8_t xx;
    tNFA_HCI_HOST_INFO *p_host;

    DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf ("nfa_hciu_update_gate_list_of_host () Host:0x%x Num Gates:0x%x",
                       host_id, num_gates);
    for(xx = 0; xx < NFA_HCI_MAX_HOST_IN_NETWORK; xx++) {
        p_host = &nfa_hci_cb.cfg.host[xx];
        if(  (p_gates)
                &&(num_gates <= NFA_HCI_MAX_GATE_CB)
                &&(host_id == p_host->host_id)
                )
        {
            p_host->num_gates = num_gates;
            memcpy (p_host->gate_id, p_gates, num_gates);
            break;
        }
    }
}

/*******************************************************************************
**
** Function         nfa_hciu_reset_apdu_pipe_registry_info_of_host
**
** Description      Reset registry info of APDU Pipe connected to specified
**                  server host
**
** Returns          None
**
*******************************************************************************/
void nfa_hciu_reset_apdu_pipe_registry_info_of_host (uint8_t host_id)
{
    uint8_t xx;
    tNFA_HCI_APDU_PIPE_REG_INFO *p_apdu_pipe_reg_info;
    tNFA_HCI_HOST_INFO *p_host;
    for(xx = 0; xx < NFA_HCI_MAX_HOST_IN_NETWORK; xx++) {
        p_host = &nfa_hci_cb.cfg.host[xx];
        DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf ("nfa_hciu_reset_apdu_pipe_registry_info_of_host () Host:0x%x", host_id);
        if(host_id == p_host->host_id) {
            p_apdu_pipe_reg_info = &p_host->apdu_pipe_reg_info;

            p_apdu_pipe_reg_info->reg_info_valid    = false;
            p_apdu_pipe_reg_info->max_wait_time     = 0;
            p_apdu_pipe_reg_info->max_cmd_apdu_size = 0;
            break;
        }
    }
}

/*******************************************************************************
**
** Function         nfa_hciu_reset_host_info_list
**
** Description      Reset Host information for all enabled hosts
**
** Returns          None
**
*******************************************************************************/
void nfa_hciu_reset_host_info_list (void)
{
    uint8_t xx = 0;

    for(xx = 0; xx < NFA_HCI_MAX_HOST_IN_NETWORK; xx++) {
        nfa_hciu_reset_apdu_pipe_registry_info_of_host (xx);
        nfa_hciu_reset_gate_list_of_host (xx);
    }
}

/*******************************************************************************
**
** Function         nfa_hciu_find_id_pipe_for_host
**
** Description      Find ID pipe connected to the specified host
**
** Returns          pointer to pipe, or NULL if none found
**
*******************************************************************************/
tNFA_HCI_DYN_PIPE  *nfa_hciu_find_id_pipe_for_host (uint8_t host_id)
{
    uint8_t               xx;
    uint8_t               gate_id;
    tNFA_HCI_DYN_GATE   *pg;
    tNFA_HCI_DYN_PIPE   *pp;

    DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf ("nfa_hciu_find_id_pipe_for_host () Host:0x%x", host_id);

    /* Loop through all pipes looking for the owner */
    for (xx = 0, pp = nfa_hci_cb.cfg.dyn_pipes; xx < NFA_HCI_MAX_PIPE_CB; xx++, pp++)
    {
        if ((pp->pipe_id != 0) && (pp->dest_host == host_id))
        {
            gate_id = pp->local_gate;
            pg   = nfa_hciu_find_gate_by_gid (gate_id);

            if (  (pg != nullptr)
                &&(gate_id == NFA_HCI_ID_MNGMNT_APP_GATE)  )
                return (pp);
        }
    }

    /* If here, not found */
    return (nullptr);
}

/*******************************************************************************
**
** Function         nfa_hciu_find_apdu_pipe_registry_info_for_host
**
** Description      Find registry info for APDU Pipe connected to specified
**                  server host
**
** Returns          pointer to Registry info of APDU Pipe, or NULL if none found
**
*******************************************************************************/
tNFA_HCI_APDU_PIPE_REG_INFO *nfa_hciu_find_apdu_pipe_registry_info_for_host (uint8_t host_id)
{
    tNFA_HCI_HOST_INFO  *p_host;
    uint8_t xx;
    tNFA_HCI_APDU_PIPE_REG_INFO *p_apdu_pipe_reg_info = nullptr;
    for(xx = 0; xx < NFA_HCI_MAX_HOST_IN_NETWORK; xx++) {
        p_host = &nfa_hci_cb.cfg.host[xx];
        DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf ("nfa_hciu_find_apdu_pipe_registry_info_for_host () Host:0x%x", host_id);

        p_host = &nfa_hci_cb.cfg.host[xx];
        if  (host_id == p_host->host_id)
        {
            p_apdu_pipe_reg_info = &nfa_hci_cb.cfg.host[xx].apdu_pipe_reg_info;
            break;
        }
    }

    return p_apdu_pipe_reg_info;
}

/*******************************************************************************
**
** Function         nfa_hciu_find_server_apdu_gate_for_host
**
** Description      Find Server APDU Gate for the specified host
**
** Returns          Gate id, or 0 if none found
**
*******************************************************************************/
uint8_t  nfa_hciu_find_server_apdu_gate_for_host (uint8_t host_id)
{
    tNFA_HCI_HOST_INFO  *p_host;
    int                 xx, yy;
    uint8_t             gate_id = 0;
    bool                gen_purp_apdu_gate_found = false;

    DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf ("nfa_hciu_find_server_apdu_gate_for_host () Host:0x%x", host_id);
    for(xx = 0; xx < NFA_HCI_MAX_HOST_IN_NETWORK; xx++) {
        p_host = &nfa_hci_cb.cfg.host[xx];
        if  (host_id == p_host->host_id)
        {
            /* Loop through all gates of the host */
            for (yy = 0; yy < p_host->num_gates; yy++)
            {
                if (p_host->gate_id[yy] == NFA_HCI_APDU_GATE)
                {
                    gate_id = NFA_HCI_APDU_GATE;
                    break;
                }
                else if (p_host->gate_id[yy] == NFA_HCI_GEN_PURPOSE_APDU_GATE)
                {
                    gen_purp_apdu_gate_found = true;
                }
            }

            if ((gate_id == 0) && (gen_purp_apdu_gate_found))
            {
                /* Only General purpose APDU Gate exist with UICC host */
                gate_id = NFA_HCI_GEN_PURPOSE_APDU_GATE;
            }
            break;
        }
    }
    return (gate_id);
}

/*******************************************************************************
 **
 ** Function         nfa_hciu_update_host_list
 **
 ** Description      Update host list based on the host list reported
 **                  by NFCC
 **
 ** Returns          None
 **
 *******************************************************************************/
void nfa_hciu_update_host_list (uint8_t data_len, uint8_t *p_host_list)
{
    uint8_t   host_id       = 0;
    uint8_t   host_count = 0, xx;
    DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf ("nfa_hciu_update_host_list () reistry entry Host");
    /* Reset the array with default value*/
    while (host_count < NFA_HCI_MAX_HOST_IN_NETWORK) {
        nfa_hci_cb.active_host[host_count] = 0x00;
        host_count++;
    }
    /* Fill with members */
    host_count = 0;
    if(data_len > NFA_HCI_MAX_HOST_IN_NETWORK)
        data_len = NFA_HCI_MAX_HOST_IN_NETWORK;
    /* Collect active host in the Host Network */
    while (host_count < data_len) {
        host_id = (uint8_t)*p_host_list++;
        DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf ("nfa_hciu_update_host_list ()  checking :0x%x ", host_id);
        nfa_hci_cb.active_host[host_count] = host_id;
        for(xx= 0; xx < NFA_HCI_MAX_HOST_IN_NETWORK; xx++)
        {
            if(nfa_hci_cb.cfg.host[xx].host_id == host_id) {
                DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf ("nfa_hciu_update_host_list () already exist in reistry entry Host:0x%x ", host_id);
                break;
            }
        }
        if(xx == NFA_HCI_MAX_HOST_IN_NETWORK) {
            xx = 0;
            while(xx < NFA_HCI_MAX_HOST_IN_NETWORK)
            {
                if(nfa_hci_cb.cfg.host[xx].host_id != 0x00)
                    xx++;
                else  {
                    nfa_hci_cb.cfg.host[xx].host_id = host_id;
                    DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf ("nfa_hciu_update_host_list () found free reistry entry  adding Host:0x%x ", host_id);
                    break;
                }
            }
        }
        host_count++;
    }
    DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf ("nfa_hciu_update_host_list () exit entry Host");
}
/*******************************************************************************
**
** Function         nfa_hciu_get_pipe_cmdrsp_info
**
** Description      Get the reassembly state of the pipe
**
** Returns          returns pointer to reassembly state of pipe
**                          or NULL if not found
**
*******************************************************************************/
tNFA_HCI_PIPE_CMDRSP_INFO *nfa_hciu_get_pipe_cmdrsp_info (uint8_t pipe)
{
    uint8_t                      *ps;
    uint8_t                       xx;
    tNFA_HCI_DYN_PIPE          *pp;
    tNFA_HCI_PIPE_CMDRSP_INFO  *p_pipe_cmdrsp_info = nullptr;

    if (  (pipe >= NFA_HCI_FIRST_DYNAMIC_PIPE)
        &&(pipe <= NFA_HCI_LAST_DYNAMIC_PIPE)  )
    {
        /* Loop through all dynamic pipes looking for the pipe */
        for (xx = 0, pp = nfa_hci_cb.cfg.dyn_pipes; xx < NFA_HCI_MAX_PIPE_CB; xx++, pp++)
        {
            if (pp->pipe_id == pipe)
            {
                p_pipe_cmdrsp_info = &nfa_hci_cb.dyn_pipe_cmdrsp_info[xx];
                break;
            }
        }
    }
    else
    {
        /* Loop through all dynamic pipes looking for the pipe */

        for (xx = 0, ps = nfa_hci_cb.static_pipe; xx < NFA_HCI_MAX_NUM_STATIC_PIPES; xx++, ps++)
        {
            if (*ps == pipe)
            {
                p_pipe_cmdrsp_info = &nfa_hci_cb.static_pipe_cmdrsp_info[xx];
                break;
            }
        }
    }

    return p_pipe_cmdrsp_info;
}

/*******************************************************************************
**
** Function         nfa_hciu_find_dyn_apdu_pipe_for_host
**
** Description      Find dynamic APDU pipe if any connected to the specified host
**
** Returns          pointer to pipe, or NULL if none found
**
*******************************************************************************/
tNFA_HCI_DYN_PIPE  *nfa_hciu_find_dyn_apdu_pipe_for_host (uint8_t host_id)
{
    tNFA_HCI_DYN_GATE   *pg;
    tNFA_HCI_DYN_PIPE   *pp;
    int                 xx;
    uint8_t             gate_id;

    DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf ("nfa_hciu_find_dyn_apdu_pipe_for_host () Host:0x%x", host_id);

    /* Loop through all pipes looking for the destination host */
    for (xx = 0, pp = nfa_hci_cb.cfg.dyn_pipes; xx < NFA_HCI_MAX_PIPE_CB; xx++, pp++)
    {
        if ((pp->pipe_id != 0) && (pp->dest_host == host_id))
        {
            gate_id = pp->local_gate;
            pg   = nfa_hciu_find_gate_by_gid (gate_id);

            if (  (pg != nullptr)
                &&((gate_id == NFA_HCI_APDU_APP_GATE) || (gate_id == NFA_HCI_GEN_PURPOSE_APDU_APP_GATE))  )
                return (pp);
        }
    }

    /* If here, not found */
    return (nullptr);
}

/*******************************************************************************
**
** Function         nfa_hciu_find_dyn_conn_pipe_for_host
**
** Description      Find dynamic Connectivity pipe if any connected to the
** specified host
**
** Returns          pointer to pipe, or NULL if none found
**
*******************************************************************************/
tNFA_HCI_DYN_PIPE  *nfa_hciu_find_dyn_conn_pipe_for_host (uint8_t host_id)
{
    tNFA_HCI_DYN_GATE   *pg;
    tNFA_HCI_DYN_PIPE   *pp;
    int                 xx;
    uint8_t             gate_id;

    DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf ("nfa_hciu_find_dyn_conn_pipe_for_host () Host:0x%x", host_id);

    /* Loop through all pipes looking for the destination host */
    for (xx = 0, pp = nfa_hci_cb.cfg.dyn_pipes; xx < NFA_HCI_MAX_PIPE_CB; xx++, pp++)
    {
        if ((pp->pipe_id != 0) && (pp->dest_host == host_id))
        {
            gate_id = pp->local_gate;
            pg   = nfa_hciu_find_gate_by_gid (gate_id);

            if (  (pg != nullptr)
                &&(gate_id == NFA_HCI_CONNECTIVITY_GATE)  )
                return (pp);
        }
    }

    /* If here, not found */
    return (nullptr);
}

/*******************************************************************************
**
** Function         nfa_hciu_add_host_resetting
**
** Description      Check if the host is currently reseting
**
** Returns          TRUE, if the host is reseting
**                  FALSE, if the host is not reseting
**
*******************************************************************************/
void nfa_hciu_clear_host_resetting(uint8_t host_id, uint8_t reset_type) {
    uint8_t xx;

    for (xx = 0; xx < NFA_HCI_MAX_HOST_IN_NETWORK; xx++) {
      if (nfa_hci_cb.reset_host[xx].host_id == host_id) {
          nfa_hci_cb.reset_host[xx].reset_cfg &= ~reset_type;
          DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf("nfa_hciu_clear_host_resetting () adding host %d %d ",
                  host_id,reset_type);
          if(!nfa_hci_cb.reset_host[xx].reset_cfg)
              nfa_hci_cb.reset_host[xx].host_id = 0x00;
          break;
      }
    }
}
/*******************************************************************************
**
** Function         nfa_hciu_add_host_resetting
**
** Description      Check if the host is currently reseting
**
** Returns          TRUE, if the host is reseting
**                  FALSE, if the host is not reseting
**
*******************************************************************************/
void nfa_hciu_add_host_resetting(uint8_t host_id, uint8_t reset_type) {
  uint8_t xx;

  for (xx = 0; xx < NFA_HCI_MAX_HOST_IN_NETWORK; xx++) {
    if (nfa_hci_cb.reset_host[xx].host_id == host_id) {
        nfa_hci_cb.reset_host[xx].reset_cfg |= reset_type;
        DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf("nfa_hciu_add_host_resetting () adding host %d %d ",
                host_id,reset_type);
        break;
    }
  }
  if(xx == NFA_HCI_MAX_HOST_IN_NETWORK) {
      for (xx = 0; xx < NFA_HCI_MAX_HOST_IN_NETWORK; xx++) {
          if (nfa_hci_cb.reset_host[xx].host_id == 0x00) {
              DLOG_IF(INFO, nfc_debug_enabled)
          << StringPrintf("nfa_hciu_add_host_resetting () adding host %d %d ",
                      host_id,reset_type);
              nfa_hci_cb.reset_host[xx].host_id = host_id;
              nfa_hci_cb.reset_host[xx].reset_cfg |= reset_type;
              break;
          }
      }
  }
}

/*******************************************************************************
**
** Function         nfa_hciu_check_host_resetting
**
** Description      Check whether host recovery in progress
**
** Returns          TRUE, if the host is recovering
**                  FALSE, if the host is not recovering
**
*******************************************************************************/
bool nfa_hciu_check_host_resetting(uint8_t host_id, uint8_t reset_type) {
  uint8_t xx;
  bool isResetting = false;
  for (xx = 0; xx < NFA_HCI_MAX_HOST_IN_NETWORK; xx++) {
    if (nfa_hci_cb.reset_host[xx].host_id == host_id &&
        (nfa_hci_cb.reset_host[xx].reset_cfg & reset_type) == reset_type) {
      isResetting = true;
      DLOG_IF(INFO, nfc_debug_enabled)
          << StringPrintf("nfa_hciu_check_host_resetting () host %d %d ",
                          host_id, reset_type);
      break;
    }
  }
  return isResetting;
}

/*******************************************************************************
**
** Function         nfa_hciu_get_hci_host_id
**
** Description      This function returns hci host id for given nfceeid
**
** Returns          hci host id
**
*******************************************************************************/
uint8_t nfa_hciu_get_hci_host_id(uint8_t nfceeid)
{
  switch(nfceeid)
  {
    case NFA_HCI_FIRST_DYNAMIC_HOST:
      return NFA_HCI_UICC_HOST;
    case NFA_HCI_FIRST_PROP_HOST:
      return NFA_HCI_FIRST_PROP_HOST;
    case NFA_HCI_EUICC_HOST:
      return NFA_HCI_FIRST_PROP_HOST + 1;
    case NFA_HCI_FIRST_DYNAMIC_HOST + 1:
      return NFA_HCI_FIRST_DYNAMIC_HOST + 1;
    case NFA_HCI_FIRST_DYNAMIC_HOST + 2:
      return NFA_HCI_FIRST_DYNAMIC_HOST + 2;
    default:
      return 0x00;
  }
    return 0x00;
}
#endif
