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
 *  This is the main implementation file for the NFA HCI.
 *
 ******************************************************************************/
#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <string.h>

#include "nfa_dm_int.h"
#include "nfa_ee_api.h"
#include "nfa_ee_int.h"
#include "nfa_hci_api.h"
#include "nfa_hci_defs.h"
#include "nfa_hci_int.h"
#include "nfa_nv_co.h"
#if (NXP_EXTNS == TRUE)
#include <config.h>
#include "nfc_config.h"
#include "nfa_scr_int.h"
#endif

using android::base::StringPrintf;

/*****************************************************************************
**  Global Variables
*****************************************************************************/

tNFA_HCI_CB nfa_hci_cb;

#ifndef NFA_HCI_NV_READ_TIMEOUT_VAL
#define NFA_HCI_NV_READ_TIMEOUT_VAL 1000
#endif

#ifndef NFA_HCI_CON_CREATE_TIMEOUT_VAL
#define NFA_HCI_CON_CREATE_TIMEOUT_VAL 1000
#endif

/*****************************************************************************
**  Static Functions
*****************************************************************************/

/* event handler function type */
static bool nfa_hci_evt_hdlr(NFC_HDR* p_msg);

static void nfa_hci_sys_enable(void);
static void nfa_hci_sys_disable(void);
#if(NXP_EXTNS == TRUE)
void nfa_hci_rsp_timeout(void);
#else
static void nfa_hci_rsp_timeout(void);
#endif
static void nfa_hci_conn_cback(uint8_t conn_id, tNFC_CONN_EVT event,
                               tNFC_CONN* p_data);
#if(NXP_EXTNS == TRUE)
static void nfa_hci_timer_cback (TIMER_LIST_ENT *p_tle);
static void nfa_hci_set_receive_buf(tNFA_HCI_PIPE_CMDRSP_INFO  *p_pipe_cmdrsp_info);
static bool nfa_hci_assemble_msg(uint8_t* p_data, uint16_t data_len);
static void nfa_hci_app_notify_evt_transaction(uint8_t pipe, uint8_t* p_data,
        uint16_t data_len);
#else
static void nfa_hci_set_receive_buf(uint8_t pipe);
static void nfa_hci_assemble_msg(uint8_t* p_data, uint16_t data_len);
#endif
static void nfa_hci_handle_nv_read(uint8_t block, tNFA_STATUS status);

/*****************************************************************************
**  Constants
*****************************************************************************/
static const tNFA_SYS_REG nfa_hci_sys_reg = {
    nfa_hci_sys_enable, nfa_hci_evt_hdlr, nfa_hci_sys_disable,
    nfa_hci_proc_nfcc_power_mode};

/*******************************************************************************
**
** Function         nfa_hci_ee_info_cback
**
** Description      Callback function
**
** Returns          None
**
*******************************************************************************/
void nfa_hci_ee_info_cback(tNFA_EE_DISC_STS status) {
  LOG(DEBUG) << StringPrintf("%d", status);

  switch (status) {
    case NFA_EE_DISC_STS_ON:
      if ((!nfa_hci_cb.ee_disc_cmplt) &&
          ((nfa_hci_cb.hci_state == NFA_HCI_STATE_STARTUP) ||
           (nfa_hci_cb.hci_state == NFA_HCI_STATE_RESTORE))) {
        /* NFCEE Discovery is in progress */
        nfa_hci_cb.ee_disc_cmplt = true;
        nfa_hci_cb.num_ee_dis_req_ntf = 0;
        nfa_hci_cb.num_hot_plug_evts = 0;
        nfa_hci_cb.conn_id = 0;
        nfa_hci_startup();
#if(NXP_EXTNS == TRUE)
      } else {
          int xx;
          for (xx = 0; xx < NFA_HCI_MAX_HOST_IN_NETWORK; xx++) {
              if(nfa_hci_cb.reset_host[xx].reset_cfg & NFCEE_UNRECOVERABLE_ERRROR) {
                /* Discovery operation is complete, retrieve discovery result */
                nfa_hci_cb.num_nfcee = NFA_HCI_MAX_HOST_IN_NETWORK;
                NFA_EeGetInfo(&nfa_hci_cb.num_nfcee, nfa_hci_cb.ee_info);
                for (int yy = 0; yy < nfa_hci_cb.num_nfcee; yy++) {
                  nfa_hci_cb.ee_info[yy].hci_enable_state = NFA_HCI_FL_EE_NONE;
                  nfa_hciu_add_host_resetting(
                      (nfa_hci_cb.ee_info[yy].ee_handle & ~NFA_HANDLE_GROUP_EE),
                      NFCEE_REINIT);
                  }
                  LOG(DEBUG) << StringPrintf(
                      " NFCEE_UNRECOVERABLE_ERRROR  reset handling");
                  if (nfa_hci_enable_one_nfcee() == false) {
                  LOG(DEBUG)
                      << StringPrintf("nfa_hci_enable_one_nfcee() failed");
                  }
                  break;
              }
          }
#endif
      }
      break;

    case NFA_EE_DISC_STS_OFF:
      if (nfa_hci_cb.ee_disable_disc) break;
      nfa_hci_cb.ee_disable_disc = true;
      /* Discovery operation is complete, retrieve discovery result */
#if(NXP_EXTNS == TRUE)
      nfa_hci_cb.num_nfcee = NFA_HCI_MAX_HOST_IN_NETWORK;
      NFA_AllEeGetInfo(&nfa_hci_cb.num_nfcee, nfa_hci_cb.ee_info);
#endif
      if ((nfa_hci_cb.hci_state == NFA_HCI_STATE_WAIT_NETWK_ENABLE) ||
          (nfa_hci_cb.hci_state == NFA_HCI_STATE_RESTORE_NETWK_ENABLE)) {
        if ((nfa_hci_cb.num_nfcee <= 1) ||
            (nfa_hci_cb.num_ee_dis_req_ntf == (nfa_hci_cb.num_nfcee - 1)) ||
            (nfa_hci_cb.num_hot_plug_evts == (nfa_hci_cb.num_nfcee - 1))) {
          /* No UICC Host is detected or
           * HOT_PLUG_EVT(s) and or EE DISC REQ Ntf(s) are already received
           * Get Host list and notify SYS on Initialization complete */
          nfa_sys_stop_timer(&nfa_hci_cb.timer);
          if ((nfa_hci_cb.num_nfcee > 1) &&
              (nfa_hci_cb.num_ee_dis_req_ntf != (nfa_hci_cb.num_nfcee - 1))) {
            /* Received HOT PLUG EVT, we will also wait for EE DISC REQ Ntf(s)
             */
            nfa_sys_start_timer(&nfa_hci_cb.timer, NFA_HCI_RSP_TIMEOUT_EVT,
                                p_nfa_hci_cfg->hci_netwk_enable_timeout);
          } else {
            nfa_hci_cb.w4_hci_netwk_init = false;
            nfa_hciu_send_get_param_cmd(NFA_HCI_ADMIN_PIPE,
                                        NFA_HCI_HOST_LIST_INDEX);
          }
        }
#if(NXP_EXTNS == TRUE)
        else {
            nfa_hci_cb.w4_hci_netwk_init = false;
            nfa_hciu_send_get_param_cmd(NFA_HCI_ADMIN_PIPE,
                                        NFA_HCI_HOST_LIST_INDEX);
        }
#endif
      } else if (nfa_hci_cb.num_nfcee <= 1) {
        /* No UICC Host is detected, HCI NETWORK is enabled */
        nfa_hci_cb.w4_hci_netwk_init = false;
      }
      break;

    case NFA_EE_DISC_STS_REQ:
      nfa_hci_cb.num_ee_dis_req_ntf++;

      if (nfa_hci_cb.ee_disable_disc) {
        /* Already received Discovery Ntf */
        if ((nfa_hci_cb.hci_state == NFA_HCI_STATE_WAIT_NETWK_ENABLE) ||
            (nfa_hci_cb.hci_state == NFA_HCI_STATE_RESTORE_NETWK_ENABLE)) {
          /* Received DISC REQ Ntf while waiting for other Host in the network
           * to bootup after DH host bootup is complete */
          if ((nfa_hci_cb.num_ee_dis_req_ntf == (nfa_hci_cb.num_nfcee - 1)) &&
              NFC_GetNCIVersion() < NCI_VERSION_2_0) {
            /* Received expected number of EE DISC REQ Ntf(s) */
            nfa_sys_stop_timer(&nfa_hci_cb.timer);
            nfa_hci_cb.w4_hci_netwk_init = false;
#if(NXP_EXTNS != TRUE)
            nfa_hciu_send_get_param_cmd(NFA_HCI_ADMIN_PIPE,
                                        NFA_HCI_HOST_LIST_INDEX);
#endif
          }
        } else if ((nfa_hci_cb.hci_state == NFA_HCI_STATE_STARTUP) ||
                   (nfa_hci_cb.hci_state == NFA_HCI_STATE_RESTORE)) {
          /* Received DISC REQ Ntf during DH host bootup */
          if (nfa_hci_cb.num_ee_dis_req_ntf == (nfa_hci_cb.num_nfcee - 1)) {
            /* Received expected number of EE DISC REQ Ntf(s) */
            nfa_hci_cb.w4_hci_netwk_init = false;
          }
        }
      }
      break;
    case NFA_EE_MODE_SET_COMPLETE:
        /*received mode set Ntf */
#if(NXP_EXTNS != TRUE)
      if ((nfa_hci_cb.hci_state == NFA_HCI_STATE_WAIT_NETWK_ENABLE) ||
                (nfa_hci_cb.hci_state == NFA_HCI_STATE_RESTORE_NETWK_ENABLE)) {
              /* Discovery operation is complete, retrieve discovery result */
          NFA_EeGetInfo(&nfa_hci_cb.num_nfcee, nfa_hci_cb.ee_info);
          nfa_hci_enable_one_nfcee();
        }
      break;
#else
        if ((nfa_hci_cb.hci_state == NFA_HCI_STATE_WAIT_NETWK_ENABLE) ||
                (nfa_hci_cb.hci_state == NFA_HCI_STATE_RESTORE_NETWK_ENABLE)) {

            if(nfa_hci_cb.next_nfcee_idx < nfa_hci_cb.num_nfcee)
            {
          LOG(DEBUG) << StringPrintf("NFA_EE_MODE_SET_COMPLETE handling 2");
          if (nfa_hci_cb.ee_info[nfa_hci_cb.next_nfcee_idx].hci_enable_state ==
              NFA_HCI_FL_EE_ENABLING) {
            for (uint8_t xx = 0; xx < NFA_HCI_MAX_HOST_IN_NETWORK; xx++) {
              if ((nfa_hci_cb.reset_host[xx].reset_cfg &
                   NFCEE_HCI_NOTIFY_ALL_PIPE_CLEARED) &&
                  (nfa_hci_cb.reset_host[xx].host_id ==
                   nfa_hci_cb.curr_nfcee)) {
                LOG(DEBUG) << StringPrintf(
                    "NFA_EE_MODE_SET_COMPLETE  handling here");
                nfa_hciu_clear_host_resetting(
                    nfa_hci_cb.curr_nfcee, NFCEE_HCI_NOTIFY_ALL_PIPE_CLEARED);
                break;
              }
            }
            nfa_hciu_send_get_param_cmd(NFA_HCI_ADMIN_PIPE,
                                        NFA_HCI_HOST_LIST_INDEX);
            nfa_hci_cb.ee_info[nfa_hci_cb.next_nfcee_idx].hci_enable_state =
                NFA_HCI_FL_EE_ENABLED;
            LOG(DEBUG) << StringPrintf("NFA_EE_MODE_SET_COMPLETE handling 3");
            if (nfa_ee_cb.ecb[nfa_hci_cb.next_nfcee_idx].ee_status ==
                NCI_NFCEE_STS_TRANSMISSION_ERROR) {
              nfa_hciu_clear_host_resetting(
                  nfa_ee_cb.ecb[nfa_hci_cb.next_nfcee_idx].nfcee_id,
                  NFCEE_INIT_COMPLETED);
              nfa_hciu_add_host_resetting(
                  nfa_ee_cb.ecb[nfa_hci_cb.next_nfcee_idx].nfcee_id,
                  NFCEE_UNRECOVERABLE_ERRROR);
            }
          } else if (!nfa_hci_enable_one_nfcee())
            nfa_hci_startup_complete(NFA_STATUS_OK);
            }
        } else {
            int xx;
            LOG(DEBUG) << StringPrintf("NFA_EE_MODE_SET_COMPLETE else case");
            for (xx = 0; xx < NFA_HCI_MAX_HOST_IN_NETWORK; xx++) {
                 if(nfa_hci_cb.reset_host[xx].reset_cfg & NFCEE_HCI_NOTIFY_ALL_PIPE_CLEARED &&
                   (nfa_hci_cb.reset_host[xx].reset_cfg & NFCEE_INIT_COMPLETED)) {
                     nfa_hciu_clear_host_resetting(nfa_hci_cb.curr_nfcee, NFCEE_HCI_NOTIFY_ALL_PIPE_CLEARED);
                    if(nfa_hci_cb.reset_host[xx].reset_cfg & NFCEE_REINIT)
                    {
                      LOG(DEBUG) << StringPrintf(
                          "NFCEE_HCI_NOTIFY_ALL_PIPE_CLEARED () handling "
                          "pending NFCEE_UNRECOVERABLE_ERRROR");
                      nfa_hciu_clear_host_resetting(nfa_hci_cb.curr_nfcee, NFCEE_REINIT);
                      nfa_hciu_clear_host_resetting(nfa_hci_cb.curr_nfcee,
                                                    NFCEE_RECOVERY_IN_PROGRESS);
                      nfa_hci_cb.ee_info[nfa_hci_cb.next_nfcee_idx].hci_enable_state = NFA_HCI_FL_EE_ENABLED;
                      nfa_hciu_check_n_clear_host_resetting(
                          nfa_hci_cb.curr_nfcee, NFCEE_UNRECOVERABLE_ERRROR);
                      nfa_hci_cb.next_nfcee_idx += 1;
                    }
                    nfa_hciu_send_get_param_cmd(NFA_HCI_ADMIN_PIPE, NFA_HCI_HOST_LIST_INDEX);
                    if(!nfa_hci_check_set_apdu_pipe_ready_for_next_host ()) {
                      LOG(DEBUG) << StringPrintf(
                          "NFCEE_HCI_NOTIFY_ALL_PIPE_CLEARED () reset "
                          "handling");
                      nfa_hci_handle_pending_host_reset();
                    }
                    break;
                 } else if (nfa_hci_cb.reset_host[xx].reset_cfg & NFCEE_REINIT) {
                    nfa_hciu_clear_host_resetting(nfa_hci_cb.curr_nfcee, NFCEE_REINIT);
                    nfa_hciu_clear_host_resetting(nfa_hci_cb.curr_nfcee,
                                                  NFCEE_RECOVERY_IN_PROGRESS);
                    nfa_hciu_check_n_clear_host_resetting(
                        nfa_hci_cb.curr_nfcee, NFCEE_UNRECOVERABLE_ERRROR);

                     if(nfa_hci_cb.next_nfcee_idx < nfa_hci_cb.num_nfcee) {
                      nfa_hci_cb.ee_info[nfa_hci_cb.next_nfcee_idx]
                          .hci_enable_state = NFA_HCI_FL_EE_ENABLED;
                      LOG(DEBUG) << StringPrintf(
                          "NFCEE_UNRECOVERABLE_ERRROR reset handling "
                          "nfa_hci_cb.next_nfcee_idx %d  nfa_hci_cb.num_nfcee "
                          "%d",
                          nfa_hci_cb.next_nfcee_idx, nfa_hci_cb.num_nfcee);
                      if (nfa_hci_enable_one_nfcee() == false) {
                        LOG(DEBUG) << StringPrintf(
                            "nfa_hci_enable_one_nfcee() failed");
                       }
                     }
                     if(nfa_hci_cb.next_nfcee_idx == nfa_hci_cb.num_nfcee) {
                       nfa_hciu_send_get_param_cmd(NFA_HCI_ADMIN_PIPE,
                          NFA_HCI_HOST_LIST_INDEX);
                       nfa_hci_handle_pending_host_reset();

                       tNFA_HCI_EVT_DATA evt_data;
                       evt_data.init_completed.status = NFA_STATUS_OK;
                       LOG(DEBUG) << StringPrintf("All NFCEE's Initialized");
                       if ((nfa_ee_cb.ee_flags & NFA_EE_FLAG_RECOVERY) == NFA_EE_FLAG_RECOVERY) {
                         nfa_ee_cb.ee_flags &= ~NFA_EE_FLAG_RECOVERY;
                       }
                       NFA_SCR_PROCESS_EVT(NFA_SCR_ESE_RECOVERY_COMPLETE_EVT, NFA_STATUS_OK);
                       if (nfa_hciu_is_no_host_resetting()) {
                         nfa_hciu_send_to_all_apps(NFA_HCI_INIT_COMPLETED,
                                                   &evt_data);
                       }
                     }
                     break;
                 } else if (nfa_hci_cb.reset_host[xx].reset_cfg &
                                NFCEE_HCI_NOTIFY_ALL_PIPE_CLEARED &&
                            !nfa_hci_cb.se_apdu_gate_support) {
                   /*In case SMB/Wired mode disabled and if we receive clear all
                    * pipe shall clear host reset status on receiving mode set
                    * ntf*/
                   nfa_hciu_clear_host_resetting(
                       nfa_hci_cb.curr_nfcee,
                       NFCEE_HCI_NOTIFY_ALL_PIPE_CLEARED);
                   /*check if any NFCEE init pending if not shall perform
                    * startup complete to put power link status to default */
                   if (!nfa_hci_enable_one_nfcee())
                     nfa_hci_startup_complete(NFA_STATUS_OK);
                 } else if (nfa_hci_cb.reset_host[xx].reset_cfg & NFCEE_INIT_COMPLETED) {
                     if(nfa_hciu_find_dyn_apdu_pipe_for_host (nfa_hci_cb.reset_host[xx].host_id) == nullptr)
                     {
                     LOG(DEBUG) << StringPrintf(
                         "delayed NFCEE_INIT_COMPLETED handling");
                     if (!nfa_hci_check_set_apdu_pipe_ready_for_next_host()) {
                         nfa_hci_handle_pending_host_reset();
                     }
                     }
                     else
                     {
                       nfa_hciu_clear_host_resetting(nfa_hci_cb.reset_host[xx].host_id, NFCEE_INIT_COMPLETED);
                     }
                     break;
                 } else if (nfa_hci_cb.reset_host[xx].reset_cfg & NFCEE_REMOVED_NTF) {
                     LOG(DEBUG) << StringPrintf("NFCEE_REMOVED_NTF handling");
                     nfa_hciu_clear_host_resetting(nfa_hci_cb.curr_nfcee,
                                                   NFCEE_REMOVED_NTF);
                     nfa_hci_handle_pending_host_reset();
                     break;
                 }
            }

        }
        break;
#endif
#if(NXP_EXTNS == TRUE)
    case NFA_EE_UNRECOVERABLE_ERROR:
    case NFA_EE_STATUS_INIT_COMPLETED:
        /*NFCEE recovery in progress*/
    {
        int ee_entry_index = 0;
        while (ee_entry_index < nfa_ee_max_ee_cfg) {
          if (nfa_ee_cb.ecb[ee_entry_index].nfcee_status == NFC_NFCEE_STS_INIT_COMPLETED) {
            LOG(DEBUG) << StringPrintf("NFA_EE_STATUS_NTF received  %x",
                                       nfa_ee_cb.ecb[ee_entry_index].nfcee_id);
            if (!((nfa_hci_cb.hci_state == NFA_HCI_STATE_WAIT_NETWK_ENABLE) ||
              (nfa_hci_cb.hci_state == NFA_HCI_STATE_RESTORE_NETWK_ENABLE))) {
                  LOG(DEBUG) << StringPrintf(
                      "NFA_EE_STATUS_NTF received during IDLE %x",
                      nfa_ee_cb.ecb[ee_entry_index].nfcee_id);
                  if (nfa_hciu_find_dyn_apdu_pipe_for_host(
                          nfa_ee_cb.ecb[ee_entry_index].nfcee_id) == nullptr &&
                      nfa_hci_is_apdu_pipe_required(
                          nfa_ee_cb.ecb[ee_entry_index].nfcee_id)) {
                    nfa_hci_cb.curr_nfcee =
                        nfa_ee_cb.ecb[ee_entry_index].nfcee_id;
                    if (IS_PROP_HOST(nfa_ee_cb.ecb[ee_entry_index].nfcee_id) &&
                        nfa_hci_cb.se_apdu_gate_support) {
                      if (nfa_hci_is_power_link_required(
                              nfa_ee_cb.ecb[ee_entry_index].nfcee_id))
                        NFC_NfceePLConfig(
                            nfa_ee_cb.ecb[ee_entry_index].nfcee_id, 0x03);
                      nfa_hciu_add_host_resetting(
                          nfa_ee_cb.ecb[ee_entry_index].nfcee_id,
                          NFCEE_INIT_COMPLETED);
                      status = NFC_NfceeModeSet(
                          nfa_ee_cb.ecb[ee_entry_index].nfcee_id,
                          NFC_MODE_ACTIVATE);
                      if (status == NFA_STATUS_OK) {
                        break;
                      }
                    } else {
                      /* After NFC Init, If NFCEE_STATUS_NTF received with
                       * status INIT_COMPLETED notify to upper layer to
                       * re-configure the LMRT & Restart RF Discovery.  */
                      tNFA_HCI_EVT_DATA evt_data;
                      evt_data.init_completed.status = NFA_STATUS_OK;
                      if ((nfa_ee_cb.ee_flags & NFA_EE_FLAG_RECOVERY) ==
                          NFA_EE_FLAG_RECOVERY) {
                        nfa_hciu_send_to_all_apps(NFA_HCI_INIT_COMPLETED,
                                                  &evt_data);
                      }
                    }
                  }
            }
            else
            {
              if (nfa_hciu_find_dyn_apdu_pipe_for_host(
                      nfa_ee_cb.ecb[ee_entry_index].nfcee_id) == nullptr &&
                      IS_PROP_HOST(nfa_ee_cb.ecb[ee_entry_index].nfcee_id) &&
                      nfa_hci_cb.se_apdu_gate_support) {
                LOG(DEBUG) << StringPrintf(
                    "NFA_EE_STATUS_NTF received during INIT %x",
                    nfa_ee_cb.ecb[ee_entry_index].nfcee_id);
                nfa_hciu_add_host_resetting(nfa_ee_cb.ecb[ee_entry_index].nfcee_id, NFCEE_INIT_COMPLETED);
              }
            }
          } else if (nfa_ee_check_recovery_required(
                         nfa_ee_cb.ecb[ee_entry_index].nfcee_status)) {
            nfa_ee_cb.ecb[ee_entry_index].nfcee_status = NFC_NFCEE_STS_INIT_STARTED;
            if (!((nfa_hci_cb.hci_state == NFA_HCI_STATE_WAIT_NETWK_ENABLE) ||
              (nfa_hci_cb.hci_state == NFA_HCI_STATE_RESTORE_NETWK_ENABLE))) {
              LOG(DEBUG) << StringPrintf(
                  "NFA_EE_RECOVERY %x", nfa_ee_cb.ecb[ee_entry_index].nfcee_id);
              if (nfa_ee_cb.ecb[ee_entry_index].nfcee_id ==
                  NFA_HCI_FIRST_PROP_HOST) {
                NFA_SCR_PROCESS_EVT(NFA_SCR_ESE_RECOVERY_START_EVT, NFA_STATUS_OK);
              }
              nfa_hci_release_transceive(nfa_ee_cb.ecb[ee_entry_index].nfcee_id,
                                         NFA_STATUS_HCI_UNRECOVERABLE_ERROR);
              if (IS_PROP_EUICC_HOST(nfa_ee_cb.ecb[ee_entry_index].nfcee_id)) {
                nfa_hciu_remove_all_pipes_from_host(
                    nfa_ee_cb.ecb[ee_entry_index].nfcee_id);
              }
              if (nfa_hciu_is_no_host_resetting()) {
                nfa_hciu_add_host_resetting(
                    nfa_ee_cb.ecb[ee_entry_index].nfcee_id,
                    NFCEE_UNRECOVERABLE_ERRROR | NFCEE_RECOVERY_IN_PROGRESS);
                nfa_hci_cb.hci_state = NFA_HCI_STATE_IDLE;
                nfa_hci_cb.curr_nfcee = nfa_ee_cb.ecb[ee_entry_index].nfcee_id;
                nfa_hci_cb.next_nfcee_idx = 0x00;
                NFC_NfceeClearWaitModeSetNtf();
                if(NFC_NfceeDiscover(true) == NFC_STATUS_FAILED) {
                  LOG(DEBUG)
                      << StringPrintf("NFA_EE_RECOVERY unable to perform");
                }
              }
            }
            nfa_hciu_add_host_resetting(nfa_ee_cb.ecb[ee_entry_index].nfcee_id,
                                        NFCEE_UNRECOVERABLE_ERRROR);
            break;
          }
          ee_entry_index++;
        }
        break;
    }
    case NFA_EE_STATUS_NFCEE_REMOVED:
    {
        nfa_hci_cb.num_nfcee = NFA_HCI_MAX_HOST_IN_NETWORK;
        NFA_AllEeGetInfo(&nfa_hci_cb.num_nfcee, nfa_hci_cb.ee_info);
        for (int ee_entry_index = 0; ee_entry_index < NFA_HCI_MAX_HOST_IN_NETWORK; ee_entry_index++) {
            uint8_t nfceeid = nfa_hci_cb.ee_info[ee_entry_index].ee_handle & ~NFA_HANDLE_GROUP_EE;
            if (nfa_hci_cb.ee_info[ee_entry_index].ee_status ==
                    NFA_EE_STATUS_REMOVED && (IS_PROP_HOST(nfceeid))) {
              if (!(nfa_hci_cb.hci_state == NFA_HCI_STATE_WAIT_NETWK_ENABLE ||
                    nfa_hci_cb.hci_state ==
                        NFA_HCI_STATE_RESTORE_NETWK_ENABLE ||
                    nfa_hci_cb.hci_state == NFA_HCI_STATE_STARTUP)) {
                nfa_hciu_add_host_resetting(nfceeid, NFCEE_REMOVED_NTF);
                nfa_hci_release_transceive(nfceeid,
                                           NFA_STATUS_EE_REMOVED_ERROR);
                nfa_hci_cb.hci_state = NFA_HCI_STATE_IDLE;
                nfa_hci_cb.ee_info[ee_entry_index].hci_enable_state =
                    NFA_HCI_FL_EE_ENABLING;
                status = NFC_NfceeModeSet(nfceeid, NFC_MODE_ACTIVATE);
                if (status == NFA_STATUS_OK) {
                  nfa_hci_cb.curr_nfcee = nfceeid;
                  nfa_hci_cb.next_nfcee_idx = ee_entry_index;
                  break;
                }
              }
            }
        }
        break;
    }
        /*received NFCEE error ntf*/
#endif
  }
}

/*******************************************************************************
**
** Function         nfa_hci_init
**
** Description      Initialize NFA HCI
**
** Returns          None
**
*******************************************************************************/
void nfa_hci_init(void) {
  LOG(DEBUG) << __func__;
#if(NXP_EXTNS == TRUE)
  uint8_t xx;
#endif
  /* initialize control block */
  memset(&nfa_hci_cb, 0, sizeof(tNFA_HCI_CB));

  nfa_hci_cb.hci_state = NFA_HCI_STATE_STARTUP;
  nfa_hci_cb.num_nfcee = NFA_HCI_MAX_HOST_IN_NETWORK;
#if(NXP_EXTNS == TRUE)
  nfa_hci_cb.pipe_in_use = NFA_HCI_INVALID_PIPE;
  nfa_hci_cb.m_wtx_count = 0;
  /* initialize timer callback */
  for (xx = 0; xx < NFA_HCI_MAX_PIPE_CB; xx++)
  {
      nfa_hci_cb.dyn_pipe_cmdrsp_info[xx].rsp_timer.p_cback = nfa_hci_timer_cback;
      nfa_hci_cb.dyn_pipe_cmdrsp_info[xx].rsp_timer.param   = (uintptr_t) &nfa_hci_cb.cfg.dyn_pipes[xx].pipe_id;
      nfa_hci_cb.dyn_pipe_cmdrsp_info[xx].rsp_timeout = NFA_HCI_DWP_RSP_WAIT_TIMEOUT;
  }
  nfa_hci_cb.static_pipe[0] = NFA_HCI_LINK_MANAGEMENT_PIPE;
  nfa_hci_cb.static_pipe[1] = NFA_HCI_ADMIN_PIPE;
  for (xx = 0; xx < NFA_HCI_MAX_NUM_STATIC_PIPES; xx++)
  {
      nfa_hci_cb.static_pipe_cmdrsp_info[xx].rsp_timer.p_cback = nfa_hci_timer_cback;
      nfa_hci_cb.static_pipe_cmdrsp_info[xx].rsp_timer.param   = (uintptr_t) &nfa_hci_cb.static_pipe[xx];
  }
#endif
  /* register message handler on NFA SYS */
  nfa_sys_register(NFA_ID_HCI, &nfa_hci_sys_reg);
}

/*******************************************************************************
**
** Function         nfa_hci_is_valid_cfg
**
** Description      Validate hci control block config parameters
**
** Returns          None
**
*******************************************************************************/
bool nfa_hci_is_valid_cfg(void) {
  uint8_t xx, yy, zz;
  tNFA_HANDLE reg_app[NFA_HCI_MAX_APP_CB];
  uint8_t valid_gate[NFA_HCI_MAX_GATE_CB];
  uint8_t app_count = 0;
  uint8_t gate_count = 0;
  uint32_t pipe_inx_mask = 0;

  /* First, see if valid values are stored in app names, send connectivity
   * events flag */
  for (xx = 0; xx < NFA_HCI_MAX_APP_CB; xx++) {
    /* Check if app name is valid with null terminated string */
    if (strlen(&nfa_hci_cb.cfg.reg_app_names[xx][0]) > NFA_MAX_HCI_APP_NAME_LEN)
      return false;

    /* Send Connectivity event flag can be either TRUE or FALSE */
    if ((nfa_hci_cb.cfg.b_send_conn_evts[xx] != true) &&
        (nfa_hci_cb.cfg.b_send_conn_evts[xx] != false))
      return false;

    if (nfa_hci_cb.cfg.reg_app_names[xx][0] != 0) {
      /* Check if the app name is present more than one time in the control
       * block */
      for (yy = xx + 1; yy < NFA_HCI_MAX_APP_CB; yy++) {
        if ((nfa_hci_cb.cfg.reg_app_names[yy][0] != 0) &&
            (!strncmp(&nfa_hci_cb.cfg.reg_app_names[xx][0],
                      &nfa_hci_cb.cfg.reg_app_names[yy][0],
                      strlen(nfa_hci_cb.cfg.reg_app_names[xx])))) {
          /* Two app cannot have the same name , NVRAM is corrupted */
              LOG(DEBUG) << StringPrintf(
                  "nfa_hci_is_valid_cfg (%s)  Reusing: %u",
                  &nfa_hci_cb.cfg.reg_app_names[xx][0], xx);
              return false;
        }
      }
      /* Collect list of hci handle */
      reg_app[app_count++] = (tNFA_HANDLE)(xx | NFA_HANDLE_GROUP_HCI);
    }
  }

  /* Validate Gate Control block */
  for (xx = 0; xx < NFA_HCI_MAX_GATE_CB; xx++) {
    if (nfa_hci_cb.cfg.dyn_gates[xx].gate_id != 0) {
      if (((nfa_hci_cb.cfg.dyn_gates[xx].gate_id != NFA_HCI_LOOP_BACK_GATE) &&
           (nfa_hci_cb.cfg.dyn_gates[xx].gate_id !=
            NFA_HCI_IDENTITY_MANAGEMENT_GATE) &&
           (nfa_hci_cb.cfg.dyn_gates[xx].gate_id <
            NFA_HCI_FIRST_HOST_SPECIFIC_GENERIC_GATE)) ||
          (nfa_hci_cb.cfg.dyn_gates[xx].gate_id > NFA_HCI_LAST_PROP_GATE))
        return false;

      /* Check if the same gate id is present more than once in the control
       * block */
      for (yy = xx + 1; yy < NFA_HCI_MAX_GATE_CB; yy++) {
        if ((nfa_hci_cb.cfg.dyn_gates[yy].gate_id != 0) &&
            (nfa_hci_cb.cfg.dyn_gates[xx].gate_id ==
             nfa_hci_cb.cfg.dyn_gates[yy].gate_id)) {
              LOG(DEBUG) << StringPrintf("nfa_hci_is_valid_cfg  Reusing: %u",
                                         nfa_hci_cb.cfg.dyn_gates[xx].gate_id);
              return false;
        }
      }
      if ((nfa_hci_cb.cfg.dyn_gates[xx].gate_owner & (~NFA_HANDLE_GROUP_HCI)) >=
          NFA_HCI_MAX_APP_CB) {
        LOG(DEBUG) << StringPrintf(
            "nfa_hci_is_valid_cfg  Invalid Gate owner: %u",
            nfa_hci_cb.cfg.dyn_gates[xx].gate_owner);
        return false;
      }
      if (!((nfa_hci_cb.cfg.dyn_gates[xx].gate_id ==
             NFA_HCI_CONNECTIVITY_GATE) ||
            ((nfa_hci_cb.cfg.dyn_gates[xx].gate_id >=
              NFA_HCI_PROP_GATE_FIRST) ||
             (nfa_hci_cb.cfg.dyn_gates[xx].gate_id <=
              NFA_HCI_PROP_GATE_LAST)))) {
        /* The gate owner should be one of the registered application */
        for (zz = 0; zz < app_count; zz++) {
          if (nfa_hci_cb.cfg.dyn_gates[xx].gate_owner == reg_app[zz]) break;
        }
        if (zz == app_count) {
          LOG(DEBUG) << StringPrintf(
              "nfa_hci_is_valid_cfg  Invalid Gate owner: %u",
              nfa_hci_cb.cfg.dyn_gates[xx].gate_owner);
          return false;
        }
      }
      /* Collect list of allocated gates */
      valid_gate[gate_count++] = nfa_hci_cb.cfg.dyn_gates[xx].gate_id;

      /* No two gates can own a same pipe */
      if ((pipe_inx_mask & nfa_hci_cb.cfg.dyn_gates[xx].pipe_inx_mask) != 0)
        return false;
      /* Collect the list of pipes on this gate */
      pipe_inx_mask |= nfa_hci_cb.cfg.dyn_gates[xx].pipe_inx_mask;
    }
  }

  for (xx = 0; (pipe_inx_mask && (xx < NFA_HCI_MAX_PIPE_CB));
       xx++, pipe_inx_mask >>= 1) {
    /* Every bit set in pipe increment mask indicates a valid pipe */
    if (pipe_inx_mask & 1) {
      /* Check if the pipe is valid one */
      if (nfa_hci_cb.cfg.dyn_pipes[xx].pipe_id < NFA_HCI_FIRST_DYNAMIC_PIPE)
        return false;
    }
  }

  if (xx == NFA_HCI_MAX_PIPE_CB) return false;

  /* Validate Gate Control block */
  for (xx = 0; xx < NFA_HCI_MAX_PIPE_CB; xx++) {
    if (nfa_hci_cb.cfg.dyn_pipes[xx].pipe_id != 0) {
      /* Check if pipe id is valid */
      if (nfa_hci_cb.cfg.dyn_pipes[xx].pipe_id < NFA_HCI_FIRST_DYNAMIC_PIPE)
        return false;

      /* Check if pipe state is valid */
      if ((nfa_hci_cb.cfg.dyn_pipes[xx].pipe_state != NFA_HCI_PIPE_OPENED) &&
          (nfa_hci_cb.cfg.dyn_pipes[xx].pipe_state != NFA_HCI_PIPE_CLOSED))
        return false;

      /* Check if local gate on which the pipe is created is valid */
      if ((((nfa_hci_cb.cfg.dyn_pipes[xx].local_gate !=
             NFA_HCI_LOOP_BACK_GATE) &&
            (nfa_hci_cb.cfg.dyn_pipes[xx].local_gate !=
             NFA_HCI_IDENTITY_MANAGEMENT_GATE)) &&
           (nfa_hci_cb.cfg.dyn_pipes[xx].local_gate <
            NFA_HCI_FIRST_HOST_SPECIFIC_GENERIC_GATE)) ||
          (nfa_hci_cb.cfg.dyn_pipes[xx].local_gate > NFA_HCI_LAST_PROP_GATE))
        return false;

      /* Check if the peer gate on which the pipe is created is valid */
      if ((((nfa_hci_cb.cfg.dyn_pipes[xx].dest_gate !=
             NFA_HCI_LOOP_BACK_GATE) &&
            (nfa_hci_cb.cfg.dyn_pipes[xx].dest_gate !=
             NFA_HCI_IDENTITY_MANAGEMENT_GATE)) &&
           (nfa_hci_cb.cfg.dyn_pipes[xx].dest_gate <
            NFA_HCI_FIRST_HOST_SPECIFIC_GENERIC_GATE)) ||
          (nfa_hci_cb.cfg.dyn_pipes[xx].dest_gate > NFA_HCI_LAST_PROP_GATE))
        return false;

      /* Check if the same pipe is present more than once in the control block
       */
      for (yy = xx + 1; yy < NFA_HCI_MAX_PIPE_CB; yy++) {
        if ((nfa_hci_cb.cfg.dyn_pipes[yy].pipe_id != 0) &&
            (nfa_hci_cb.cfg.dyn_pipes[xx].pipe_id ==
             nfa_hci_cb.cfg.dyn_pipes[yy].pipe_id)) {
          LOG(DEBUG) << StringPrintf("nfa_hci_is_valid_cfg  Reusing: %u",
                                     nfa_hci_cb.cfg.dyn_pipes[xx].pipe_id);
          return false;
        }
      }
      /* The local gate should be one of the element in gate control block */
      for (zz = 0; zz < gate_count; zz++) {
        if (nfa_hci_cb.cfg.dyn_pipes[xx].local_gate == valid_gate[zz]) break;
      }
      if (zz == gate_count) {
        LOG(DEBUG) << StringPrintf("nfa_hci_is_valid_cfg  Invalid Gate: %u",
                                   nfa_hci_cb.cfg.dyn_pipes[xx].local_gate);
        return false;
      }
    }
  }

  /* Check if admin pipe state is valid */
  if ((nfa_hci_cb.cfg.admin_gate.pipe01_state != NFA_HCI_PIPE_OPENED) &&
      (nfa_hci_cb.cfg.admin_gate.pipe01_state != NFA_HCI_PIPE_CLOSED))
    return false;

  /* Check if link management pipe state is valid */
  if ((nfa_hci_cb.cfg.link_mgmt_gate.pipe00_state != NFA_HCI_PIPE_OPENED) &&
      (nfa_hci_cb.cfg.link_mgmt_gate.pipe00_state != NFA_HCI_PIPE_CLOSED))
    return false;

  pipe_inx_mask = nfa_hci_cb.cfg.id_mgmt_gate.pipe_inx_mask;
  for (xx = 0; (pipe_inx_mask && (xx < NFA_HCI_MAX_PIPE_CB));
       xx++, pipe_inx_mask >>= 1) {
    /* Every bit set in pipe increment mask indicates a valid pipe */
    if (pipe_inx_mask & 1) {
      /* Check if the pipe is valid one */
      if (nfa_hci_cb.cfg.dyn_pipes[xx].pipe_id < NFA_HCI_FIRST_DYNAMIC_PIPE)
        return false;
      /* Check if the pipe is connected to Identity management gate */
      if (nfa_hci_cb.cfg.dyn_pipes[xx].local_gate !=
          NFA_HCI_IDENTITY_MANAGEMENT_GATE)
        return false;
    }
  }
  if (xx == NFA_HCI_MAX_PIPE_CB) return false;

  return true;
}

/*******************************************************************************
**
** Function         nfa_hci_cfg_default
**
** Description      Configure default values for hci control block
**
** Returns          None
**
*******************************************************************************/
void nfa_hci_restore_default_config(uint8_t* p_session_id) {
  memset(&nfa_hci_cb.cfg, 0, sizeof(nfa_hci_cb.cfg));
  memcpy(nfa_hci_cb.cfg.admin_gate.session_id, p_session_id,
         NFA_HCI_SESSION_ID_LEN);
  nfa_hci_cb.nv_write_needed = true;
}

/*******************************************************************************
**
** Function         nfa_hci_proc_nfcc_power_mode
**
** Description      Restore NFA HCI sub-module
**
** Returns          None
**
*******************************************************************************/
void nfa_hci_proc_nfcc_power_mode(uint8_t nfcc_power_mode) {
  LOG(DEBUG) << StringPrintf("nfcc_power_mode=%d", nfcc_power_mode);

  /* if NFCC power mode is change to full power */
  if (nfcc_power_mode == NFA_DM_PWR_MODE_FULL) {
    nfa_hci_cb.b_low_power_mode = false;
    if (nfa_hci_cb.hci_state == NFA_HCI_STATE_IDLE) {
      nfa_hci_cb.hci_state = NFA_HCI_STATE_RESTORE;
      nfa_hci_cb.ee_disc_cmplt = false;
      nfa_hci_cb.ee_disable_disc = true;
      if (nfa_hci_cb.num_nfcee > 1)
        nfa_hci_cb.w4_hci_netwk_init = true;
      else
        nfa_hci_cb.w4_hci_netwk_init = false;
      nfa_hci_cb.conn_id = 0;
      nfa_hci_cb.num_ee_dis_req_ntf = 0;
      nfa_hci_cb.num_hot_plug_evts = 0;
    } else {
      LOG(ERROR) << StringPrintf("Cannot restore now");
      nfa_sys_cback_notify_nfcc_power_mode_proc_complete(NFA_ID_HCI);
    }
  } else {
    nfa_hci_cb.hci_state = NFA_HCI_STATE_IDLE;
    nfa_hci_cb.w4_rsp_evt = false;
    nfa_hci_cb.conn_id = 0;
    nfa_sys_stop_timer(&nfa_hci_cb.timer);
    nfa_hci_cb.b_low_power_mode = true;
    nfa_sys_cback_notify_nfcc_power_mode_proc_complete(NFA_ID_HCI);
  }
}

/*******************************************************************************
**
** Function         nfa_hci_dh_startup_complete
**
** Description      Initialization of terminal host in HCI Network is completed
**                  Wait for other host in the network to initialize
**
** Returns          None
**
*******************************************************************************/
void nfa_hci_dh_startup_complete(void) {
  if (nfa_hci_cb.w4_hci_netwk_init) {
    if (nfa_hci_cb.hci_state == NFA_HCI_STATE_STARTUP) {
      nfa_hci_cb.hci_state = NFA_HCI_STATE_WAIT_NETWK_ENABLE;
      /* Wait for EE Discovery to complete */
      nfa_sys_start_timer(&nfa_hci_cb.timer, NFA_HCI_RSP_TIMEOUT_EVT,
                          NFA_EE_DISCV_TIMEOUT_VAL);
#if(NXP_EXTNS == TRUE)
      if(!nfa_ee_cb.num_ee_expecting) {
        nfa_hci_cb.num_nfcee = NFA_HCI_MAX_HOST_IN_NETWORK;
        NFA_EeGetInfo(&nfa_hci_cb.num_nfcee, nfa_hci_cb.ee_info);
        if(nfa_hci_cb.num_nfcee == nfa_ee_max_ee_cfg)
          nfa_hciu_send_get_param_cmd(NFA_HCI_ADMIN_PIPE, NFA_HCI_HOST_LIST_INDEX);
      }
#endif
    } else if (nfa_hci_cb.hci_state == NFA_HCI_STATE_RESTORE) {
      nfa_hci_cb.hci_state = NFA_HCI_STATE_RESTORE_NETWK_ENABLE;
      /* No HCP packet to DH for a specified period of time indicates all host
       * in the network is initialized */
      nfa_sys_start_timer(&nfa_hci_cb.timer, NFA_HCI_RSP_TIMEOUT_EVT,
                          p_nfa_hci_cfg->hci_netwk_enable_timeout);
    }
  } else if ((nfa_hci_cb.num_nfcee > 1) &&
             (nfa_hci_cb.num_ee_dis_req_ntf != (nfa_hci_cb.num_nfcee - 1))) {
    if (nfa_hci_cb.hci_state == NFA_HCI_STATE_RESTORE)
      nfa_hci_cb.ee_disable_disc = true;
    /* Received HOT PLUG EVT, we will also wait for EE DISC REQ Ntf(s) */
    nfa_sys_start_timer(&nfa_hci_cb.timer, NFA_HCI_RSP_TIMEOUT_EVT,
                        NFA_EE_DISCV_TIMEOUT_VAL);
  } else {
    /* Received EE DISC REQ Ntf(s) */
#if(NXP_EXTNS == TRUE)
      nfa_hci_cb.num_nfcee = NFA_HCI_MAX_HOST_IN_NETWORK;
      NFA_EeGetInfo(&nfa_hci_cb.num_nfcee, nfa_hci_cb.ee_info);
#endif
    nfa_hciu_send_get_param_cmd(NFA_HCI_ADMIN_PIPE, NFA_HCI_HOST_LIST_INDEX);
  }
}

/*******************************************************************************
**
** Function         nfa_hci_startup_complete
**
** Description      HCI network initialization is completed
**
** Returns          None
**
*******************************************************************************/
void nfa_hci_startup_complete(tNFA_STATUS status) {
  tNFA_HCI_EVT_DATA evt_data;

  LOG(DEBUG) << StringPrintf("Status: %u", status);

  nfa_sys_stop_timer(&nfa_hci_cb.timer);

  if ((nfa_hci_cb.hci_state == NFA_HCI_STATE_RESTORE) ||
      (nfa_hci_cb.hci_state == NFA_HCI_STATE_RESTORE_NETWK_ENABLE)) {
    nfa_ee_proc_hci_info_cback();
    nfa_sys_cback_notify_nfcc_power_mode_proc_complete(NFA_ID_HCI);

  } else
#if(NXP_EXTNS == TRUE)
  if(nfa_hci_cb.hci_state == NFA_HCI_STATE_WAIT_NETWK_ENABLE ||
        nfa_hci_cb.hci_state == NFA_HCI_STATE_STARTUP)
#endif
  {
    evt_data.hci_init.status = status;

    nfa_hciu_send_to_all_apps(NFA_HCI_INIT_EVT, &evt_data);
    nfa_sys_cback_notify_enable_complete(NFA_ID_HCI);
  }

  if (status == NFA_STATUS_OK)
    nfa_hci_cb.hci_state = NFA_HCI_STATE_IDLE;

  else
    nfa_hci_cb.hci_state = NFA_HCI_STATE_DISABLED;
#if(NXP_EXTNS == TRUE)
  if ((nfa_ee_cb.ee_flags & NFA_EE_FLAG_RECOVERY) == NFA_EE_FLAG_RECOVERY) {
    nfa_ee_cb.ee_flags &= ~NFA_EE_FLAG_RECOVERY;
  }
  if (nfcFL.eseFL._NCI_NFCEE_PWR_LINK_CMD) {
    if (nfa_hci_cb.next_nfcee_idx == nfa_hci_cb.num_nfcee) {
      LOG(DEBUG) << StringPrintf("All NFCEEs OK update power link status");
      uint8_t active_nfcee_id = nfa_hci_get_apdu_enabled_host();
      switch (nfa_ee_cb.ese_prv_pwr_cfg) {
        case 0xFF:
          NFC_NfceePLConfig(active_nfcee_id, 0x01);
          break;
        case 0x03:
          /*Already sent as part of recovery*/
          break;
        case 0x02:
        case 0x01:
        case 0x00:
          NFC_NfceePLConfig(active_nfcee_id, nfa_ee_cb.ese_prv_pwr_cfg);
          break;
        default:
          /*Should never be here*/
          LOG(ERROR) << StringPrintf("%s: Invalid Value received!!", __func__);
      }
    }
  }
  nfa_hci_handle_pending_host_reset();
#endif
}

#if(NXP_EXTNS != TRUE)
/*******************************************************************************
**
** Function         nfa_hci_enable_one_nfcee
**
** Description      Enable NFCEE Hosts which are discovered.
**
** Returns          None
**
*******************************************************************************/
void nfa_hci_enable_one_nfcee(void) {
  uint8_t xx;
  uint8_t nfceeid = 0;

  LOG(DEBUG) << StringPrintf("%d", nfa_hci_cb.num_nfcee);

  for (xx = 0; xx < nfa_hci_cb.num_nfcee; xx++) {
    nfceeid = nfa_hci_cb.ee_info[xx].ee_handle & ~NFA_HANDLE_GROUP_EE;
    if (nfa_hci_cb.ee_info[xx].ee_status == NFA_EE_STATUS_INACTIVE) {
      NFC_NfceeModeSet(nfceeid, NFC_MODE_ACTIVATE);
      return;
    }
  }

  if (xx == nfa_hci_cb.num_nfcee) {
    nfa_hciu_send_get_param_cmd(NFA_HCI_ADMIN_PIPE, NFA_HCI_HOST_LIST_INDEX);
  }
}
#else
/*******************************************************************************
 **
 ** Function         nfa_hci_enable_one_nfcee
 **
 ** Description      Enable NFCEE Hosts which are discovered.
 **
 ** Returns          TRUE or FALSE
 **
 *******************************************************************************/
bool nfa_hci_enable_one_nfcee(void) {
    uint8_t xx;
    uint8_t nfceeid = 0;
    bool enable_cmplt = false;
    LOG(DEBUG) << StringPrintf("nfa_hci_enable_one_nfcee () enter");

    tNFA_STATUS status = NFA_STATUS_FAILED;
    for (xx = nfa_hci_cb.next_nfcee_idx; xx < nfa_hci_cb.num_nfcee; xx++) {
    LOG(DEBUG) << StringPrintf("nfa_hci_enable_one_nfcee () %d", xx);
    if (nfa_hci_cb.ee_info[xx].ee_interface[0] != NFA_EE_INTERFACE_HCI_ACCESS) {
      nfceeid = nfa_hci_cb.ee_info[xx].ee_handle & ~NFA_HANDLE_GROUP_EE;
      LOG(DEBUG) << StringPrintf(
          "nfa_hci_enable_one_nfcee () xx= %d nfcee_id = %d "
          "nfa_hci_cb.ee_info[xx].ee_status = %d ",
          xx, nfceeid, nfa_hci_cb.ee_info[xx].ee_status);
      if (nfa_hci_cb.ee_info[xx].ee_status == NFA_EE_STATUS_INACTIVE ||
          nfa_hci_cb.ee_info[xx].ee_status == NFA_EE_STATUS_ACTIVE ||
          nfa_hci_cb.ee_info[xx].ee_status == NFA_EE_STATUS_REMOVED) {
        if (nfceeid != NFA_HCI_FIRST_PROP_HOST) {
          tNFA_EE_ECB* p_cb = nfa_ee_find_ecb(nfceeid);
          if (p_cb != nullptr && p_cb->ee_status == NFA_EE_STATUS_REMOVED) {
                     nfa_hci_cb.next_nfcee_idx = xx + 1;
                     continue;
          }
        }
        if (nfa_hci_cb.ee_info[xx].hci_enable_state == NFA_HCI_FL_EE_ENABLED) {
          if (nfa_hci_check_set_apdu_pipe_ready_for_next_host()) {
                     nfa_hci_cb.next_nfcee_idx = xx + 1;
                     enable_cmplt = true;
                     break;
          }
          nfa_hci_cb.next_nfcee_idx = xx + 1;
        } else if (nfa_hci_cb.ee_info[xx].hci_enable_state ==
                       NFA_HCI_FL_EE_ENABLING ||
                   nfa_hci_cb.ee_info[xx].hci_enable_state ==
                       NFA_HCI_FL_EE_NONE) {
          nfa_hci_cb.ee_info[xx].hci_enable_state = NFA_HCI_FL_EE_ENABLING;
          if (nfa_hci_cb.ee_info[xx].ee_status == NFA_EE_STATUS_ACTIVE) {
                     if (nfa_dm_is_hci_supported()) {
                       nfa_hci_cb.ee_info[xx].hci_enable_state =
                           NFA_HCI_FL_EE_ENABLED;
                       nfa_hciu_clear_host_resetting(nfceeid, NFCEE_REINIT);
                       nfa_hciu_clear_host_resetting(
                           nfceeid, NFCEE_RECOVERY_IN_PROGRESS);
                       nfa_hciu_check_n_clear_host_resetting(
                           nfceeid, NFCEE_UNRECOVERABLE_ERRROR);
                       nfa_hci_cb.next_nfcee_idx = xx + 1;
                       continue;
                     }
          }
          if (nfa_hci_is_power_link_required(nfceeid) &&
              ((nfa_hciu_find_dyn_conn_pipe_for_host(nfceeid) == nullptr) ||
               (nfa_hciu_find_dyn_apdu_pipe_for_host(nfceeid) == nullptr &&
                (nfa_hci_cb.se_apdu_gate_support)))) {
                     if (nfcFL.eseFL._NCI_NFCEE_PWR_LINK_CMD) {
                       status = NFC_NfceePLConfig(nfceeid, 0x03);
                       if (status != NFA_STATUS_OK) {
                         LOG(ERROR) << StringPrintf(
                             "%s: Power link configuration is unsuccessfull",
                             __func__);
                       }
                     }
          }
          status = NFC_NfceeModeSet(nfceeid, NFC_MODE_ACTIVATE);
          if (status == NFA_STATUS_OK) {
                     nfa_hci_cb.curr_nfcee = nfceeid;
                     nfa_hci_cb.next_nfcee_idx = xx;
                     enable_cmplt = true;
                     break;
          }
        }
      }
    }
    }
    return enable_cmplt;
}
#endif
/*******************************************************************************
**
** Function         nfa_hci_startup
**
** Description      Perform HCI startup
**
** Returns          None
**
*******************************************************************************/
void nfa_hci_startup(void) {
  tNFA_STATUS status = NFA_STATUS_FAILED;
  uint8_t target_handle;
  uint8_t count = 0;
  bool found = false;

  if (HCI_LOOPBACK_DEBUG == NFA_HCI_DEBUG_ON) {
    /* First step in initialization is to open the admin pipe */
    nfa_hciu_send_open_pipe_cmd(NFA_HCI_ADMIN_PIPE);
    return;
  }

  /* We can only start up if NV Ram is read and EE discovery is complete */
  if (nfa_hci_cb.nv_read_cmplt && nfa_hci_cb.ee_disc_cmplt &&
      (nfa_hci_cb.conn_id == 0)) {
    if (NFC_GetNCIVersion() >= NCI_VERSION_2_0) {
      NFC_SetStaticHciCback(nfa_hci_conn_cback);
    } else {
      NFA_EeGetInfo(&nfa_hci_cb.num_nfcee, nfa_hci_cb.ee_info);

      while ((count < nfa_hci_cb.num_nfcee) && (!found)) {
        target_handle = (uint8_t)nfa_hci_cb.ee_info[count].ee_handle;

        if (nfa_hci_cb.ee_info[count].ee_interface[0] ==
            NFA_EE_INTERFACE_HCI_ACCESS) {
          found = true;

#if(NXP_EXTNS != TRUE)
          if (nfa_hci_cb.ee_info[count].ee_status == NFA_EE_STATUS_INACTIVE) {
            NFC_NfceeModeSet(target_handle, NFC_MODE_ACTIVATE);
          }
#endif
          if ((status = NFC_ConnCreate(NCI_DEST_TYPE_NFCEE, target_handle,
                                       NFA_EE_INTERFACE_HCI_ACCESS,
                                       nfa_hci_conn_cback)) == NFA_STATUS_OK)
            nfa_sys_start_timer(&nfa_hci_cb.timer, NFA_HCI_RSP_TIMEOUT_EVT,
                                NFA_HCI_CON_CREATE_TIMEOUT_VAL);
          else {
            nfa_hci_cb.hci_state = NFA_HCI_STATE_DISABLED;
            LOG(ERROR) << StringPrintf(
                "nfa_hci_startup - Failed to Create Logical connection. HCI "
                "Initialization/Restore failed");
            nfa_hci_startup_complete(NFA_STATUS_FAILED);
          }
#if(NXP_EXTNS == TRUE)
          if (nfa_hci_cb.ee_info[count].ee_status == NFA_EE_STATUS_INACTIVE) {
            LOG(ERROR) << StringPrintf(
                  "nfa_hci_startup - Enabling one hci ");
            NFC_NfceeModeSet(target_handle, NFC_MODE_ACTIVATE);
          }
#endif
        }
        count++;
      }
      if (!found) {
        LOG(ERROR) << StringPrintf(
            "nfa_hci_startup - HCI ACCESS Interface not discovered. HCI "
            "Initialization/Restore failed");
        nfa_hci_startup_complete(NFA_STATUS_FAILED);
      }
    }
  }
}

/*******************************************************************************
**
** Function         nfa_hci_sys_enable
**
** Description      Enable NFA HCI
**
** Returns          None
**
*******************************************************************************/
static void nfa_hci_sys_enable(void) {
  LOG(DEBUG) << __func__;
  nfa_ee_reg_cback_enable_done(&nfa_hci_ee_info_cback);

  nfa_nv_co_read((uint8_t*)&nfa_hci_cb.cfg, sizeof(nfa_hci_cb.cfg),
                 DH_NV_BLOCK);
  nfa_sys_start_timer(&nfa_hci_cb.timer, NFA_HCI_RSP_TIMEOUT_EVT,
                      NFA_HCI_NV_READ_TIMEOUT_VAL);
#if(NXP_EXTNS == TRUE)
  /* se apdu gate support will be enabled only if terminal type
     set to eSE/eUICC */
  if (NfcConfig::hasKey(NAME_NXP_SE_APDU_GATE_SUPPORT)) {
    nfa_hci_cb.se_apdu_gate_support = NfcConfig::getUnsigned(NAME_NXP_SE_SMB_TERMINAL_TYPE, 0x01);
  } else {
    nfa_hci_cb.se_apdu_gate_support = 0x00;
  }
  LOG(DEBUG) << StringPrintf("%s SE terminal type for APDU gate: %d", __func__,
                             nfa_hci_cb.se_apdu_gate_support);
  nfa_hci_cb.uicc_etsi_support =
      NfcConfig::getUnsigned(NAME_NXP_UICC_ETSI_SUPPORT, 0x00);
#endif
}

/*******************************************************************************
**
** Function         nfa_hci_sys_disable
**
** Description      Disable NFA HCI
**
** Returns          None
**
*******************************************************************************/
static void nfa_hci_sys_disable(void) {
  tNFA_HCI_EVT_DATA evt_data;

  nfa_sys_stop_timer(&nfa_hci_cb.timer);

  if (nfa_hci_cb.conn_id) {
    if (nfa_sys_is_graceful_disable()) {
      /* Tell all applications stack is down */
      if (NFC_GetNCIVersion() == NCI_VERSION_1_0) {
        nfa_hciu_send_to_all_apps(NFA_HCI_EXIT_EVT, &evt_data);
        NFC_ConnClose(nfa_hci_cb.conn_id);
        return;
      }
    }
    nfa_hci_cb.conn_id = 0;
  }

  nfa_hci_cb.hci_state = NFA_HCI_STATE_DISABLED;
  /* deregister message handler on NFA SYS */
  nfa_sys_deregister(NFA_ID_HCI);
}

#if (NXP_EXTNS == TRUE)
/*******************************************************************************
**
** Function         nfa_hci_app_notify_evt_transaction
**
** Description      This function shall be called to notify HCI_EVT_TRANSACTION
**                  received on PIPE which hasn't created during initialization.
**
** Returns          None
**
*******************************************************************************/
static void nfa_hci_app_notify_evt_transaction(uint8_t pipe, uint8_t* p,
        uint16_t pkt_len) {
  tNFA_HCI_EVT_DATA evt_data;
  uint8_t type = ((*p) >> 0x06) & 0x03;
  uint8_t inst = (*p++ & 0x3F);
  if (pkt_len != 0) pkt_len--;
  if (type == NFA_HCI_EVENT_TYPE) {
    if (inst == NFA_HCI_EVT_TRANSACTION) {
      evt_data.rcvd_evt.pipe = pipe;
      evt_data.rcvd_evt.evt_code = inst;
      evt_data.rcvd_evt.evt_len = pkt_len;
      evt_data.rcvd_evt.p_evt_buf = p;
      /* notify NFA_HCI_EVENT_RCVD_EVT to the application */
      nfa_hciu_send_to_apps_handling_connectivity_evts(NFA_HCI_EVENT_RCVD_EVT,
              &evt_data);
    }
  }
}
#endif
/*******************************************************************************
**
** Function         nfa_hci_conn_cback
**
** Description      This function Process event from NCI
**
** Returns          None
**
*******************************************************************************/
static void nfa_hci_conn_cback(uint8_t conn_id, tNFC_CONN_EVT event,
                               tNFC_CONN* p_data) {
  uint8_t* p;
  NFC_HDR* p_pkt = (NFC_HDR*)p_data->data.p_data;
  uint8_t chaining_bit;
  uint8_t pipe;
  uint16_t pkt_len;
  const uint8_t MAX_BUFF_SIZE = 100;
  char buff[MAX_BUFF_SIZE];
  LOG(DEBUG) << StringPrintf("%s State: %u  Cmd: %u", __func__,
                             nfa_hci_cb.hci_state, event);
#if(NXP_EXTNS == TRUE)
  tNFA_HCI_PIPE_CMDRSP_INFO *p_pipe_cmdrsp_info = nullptr;
#endif

  if (event == NFC_CONN_CREATE_CEVT) {
    nfa_hci_cb.conn_id = conn_id;
    nfa_hci_cb.buff_size = p_data->conn_create.buff_size;

    if (nfa_hci_cb.hci_state == NFA_HCI_STATE_STARTUP) {
#if(NXP_EXTNS == TRUE)
      nfa_hciu_alloc_gate(NFA_HCI_CONNECTIVITY_GATE, NFA_HCI_APP_HANDLE_NONE);
      nfa_hciu_alloc_gate (NFA_HCI_GEN_PURPOSE_APDU_APP_GATE, NFA_HCI_APP_HANDLE_NONE);
      nfa_hciu_alloc_gate (NFA_HCI_ID_MNGMNT_APP_GATE, NFA_HCI_APP_HANDLE_NONE);
      nfa_hciu_alloc_gate (NFA_HCI_APDU_APP_GATE, NFA_HCI_APP_HANDLE_NONE);
      nfa_hciu_alloc_gate (NFA_HCI_APDU_GATE, NFA_HCI_APP_HANDLE_NONE);

      /* Set flag waiting for EVT_ATR on APDU Pipes connected to different UICC host */
      //nfa_hciu_set_server_apdu_host_not_ready (p_gate);
#else
      nfa_hci_cb.w4_hci_netwk_init = true;
      nfa_hciu_alloc_gate(NFA_HCI_CONNECTIVITY_GATE, 0);
#endif
    }

    if (nfa_hci_cb.cfg.admin_gate.pipe01_state == NFA_HCI_PIPE_CLOSED) {
      /* First step in initialization/restore is to open the admin pipe */
      nfa_hciu_send_open_pipe_cmd(NFA_HCI_ADMIN_PIPE);
    } else {
      /* Read session id, to know DH session id is correct */
      nfa_hciu_send_get_param_cmd(NFA_HCI_ADMIN_PIPE,
                                  NFA_HCI_SESSION_IDENTITY_INDEX);
    }
  } else if (event == NFC_CONN_CLOSE_CEVT) {
    nfa_hci_cb.conn_id = 0;
    nfa_hci_cb.hci_state = NFA_HCI_STATE_DISABLED;
    /* deregister message handler on NFA SYS */
    nfa_sys_deregister(NFA_ID_HCI);
  }
#if(NXP_EXTNS == TRUE)
    else if(event == NFC_HCI_FC_STARTED_EVT || event  == NFC_HCI_FC_STOPPED_EVT) {
    nfa_hci_handle_control_evt(event , p_data);
  }
#endif
  if ((event != NFC_DATA_CEVT) || (p_pkt == nullptr)) return;

  if ((nfa_hci_cb.hci_state == NFA_HCI_STATE_WAIT_NETWK_ENABLE) ||
      (nfa_hci_cb.hci_state == NFA_HCI_STATE_RESTORE_NETWK_ENABLE)) {
    /* Received HCP Packet before timeout, Other Host initialization is not
     * complete */
    nfa_sys_stop_timer(&nfa_hci_cb.timer);
    if (nfa_hci_cb.w4_hci_netwk_init)
      nfa_sys_start_timer(&nfa_hci_cb.timer, NFA_HCI_RSP_TIMEOUT_EVT,
                          p_nfa_hci_cfg->hci_netwk_enable_timeout);
  }

  p = (uint8_t*)(p_pkt + 1) + p_pkt->offset;
  pkt_len = p_pkt->len;

  if (pkt_len < 1) {
    LOG(ERROR) << StringPrintf("Insufficient packet length! Dropping :%u bytes",
                               pkt_len);
    /* release GKI buffer */
    GKI_freebuf(p_pkt);
    return;
  }

  chaining_bit = ((*p) >> 0x07) & 0x01;
  pipe = (*p++) & 0x7F;
  if (pkt_len != 0) pkt_len--;
#if(NXP_EXTNS == TRUE)

  p_pipe_cmdrsp_info = nfa_hciu_get_pipe_cmdrsp_info (pipe);
  if (!p_pipe_cmdrsp_info)
  {
      /* Cannot find the pipe in the control block */
      if(pipe != NfcConfig::getUnsigned(NAME_OFF_HOST_ESE_PIPE_ID, 0x16)) {
          nfa_hci_app_notify_evt_transaction(pipe, p,pkt_len);
      }
      GKI_freebuf (p_pkt);
      return;
  }
  LOG(DEBUG) << StringPrintf("nfa_hci_conn_cback () nfa_hci_cb 1");
  if (p_pipe_cmdrsp_info->reassembling == false) {
#else
  if (nfa_hci_cb.assembling == false) {
#endif
    if (pkt_len < 1) {
      LOG(ERROR) << StringPrintf(
          "Insufficient packet length! Dropping :%u bytes", pkt_len);
      /* release GKI buffer */
      GKI_freebuf(p_pkt);
      return;
    }

    /* First Segment of a packet */
    nfa_hci_cb.type = ((*p) >> 0x06) & 0x03;
    nfa_hci_cb.inst = (*p++ & 0x3F);
    if (pkt_len != 0) pkt_len--;
#if(NXP_EXTNS == TRUE)
    nfa_hci_cb.msg_len = 0;

    p_pipe_cmdrsp_info->recv_inst_type = nfa_hci_cb.type;
    p_pipe_cmdrsp_info->recv_inst = nfa_hci_cb.inst;
    p_pipe_cmdrsp_info->reassembly_failed = false;
#else
    nfa_hci_cb.assembly_failed = false;
    nfa_hci_cb.msg_len = 0;
#endif
#if(NXP_EXTNS == TRUE)
    if (chaining_bit == NFA_HCI_MESSAGE_FRAGMENTATION) {
      /* Fragmented HCP */
      p_pipe_cmdrsp_info->reassembling = true;
      nfa_hci_set_receive_buf (p_pipe_cmdrsp_info);
      if (!nfa_hci_assemble_msg (p, pkt_len))
          p_pipe_cmdrsp_info->reassembly_failed = true;
    } else {
      /* Non Fragmented HCP */
      if (  (nfa_hci_cb.type == NFA_HCI_EVENT_TYPE)
          &&(p_pipe_cmdrsp_info->w4_rsp_apdu_evt)  )
      {
          /* Response APDU: Use buffer provided
          ** by layer above for collecting response APDU
          */
          nfa_hci_set_receive_buf (p_pipe_cmdrsp_info);
          if (!nfa_hci_assemble_msg (p, pkt_len))
              p_pipe_cmdrsp_info->reassembly_failed = true;
          p = nfa_hci_cb.p_msg_data;
          nfa_sys_stop_timer (&(p_pipe_cmdrsp_info->rsp_timer));
          nfa_hci_cb.hci_state = NFA_HCI_STATE_IDLE;
      }
    }
#else
    if (chaining_bit == NFA_HCI_MESSAGE_FRAGMENTATION) {
      nfa_hci_cb.assembling = true;
      nfa_hci_set_receive_buf(pipe);
      nfa_hci_assemble_msg(p, pkt_len);
    } else {
      if ((pipe >= NFA_HCI_FIRST_DYNAMIC_PIPE) &&
          (nfa_hci_cb.type == NFA_HCI_EVENT_TYPE)) {
        nfa_hci_set_receive_buf(pipe);
        nfa_hci_assemble_msg(p, pkt_len);
        p = nfa_hci_cb.p_msg_data;
      }
    }
#endif
  } else {
#if(NXP_EXTNS == TRUE)
      nfa_hci_cb.type           = p_pipe_cmdrsp_info->recv_inst_type;
      nfa_hci_cb.inst           = p_pipe_cmdrsp_info->recv_inst;
      nfa_hci_cb.msg_len        = p_pipe_cmdrsp_info->msg_rx_len;
      nfa_hci_cb.max_msg_len    = p_pipe_cmdrsp_info->max_msg_rx_len;
      if (p_pipe_cmdrsp_info->reassembly_failed) {
#else
    if (nfa_hci_cb.assembly_failed) {
#endif
      /* If Reassembly failed because of insufficient buffer, just drop the new
       * segmented packets */
      LOG(ERROR) << StringPrintf(
          "Insufficient buffer to Reassemble HCP "
          "packet! Dropping :%u bytes",
          pkt_len);
    } else {
      /* Reassemble the packet */
#if(NXP_EXTNS == TRUE)
          if (!nfa_hci_assemble_msg (p, pkt_len))
              p_pipe_cmdrsp_info->reassembly_failed = TRUE;
#else
      nfa_hci_assemble_msg(p, pkt_len);
#endif
      }

    if (chaining_bit == NFA_HCI_NO_MESSAGE_FRAGMENTATION) {
      /* Just added the last segment in the chain. Reset pointers */
#if(NXP_EXTNS == TRUE)
      p_pipe_cmdrsp_info->reassembling = false;
#else
      nfa_hci_cb.assembling = false;
#endif
      p = nfa_hci_cb.p_msg_data;
      pkt_len = nfa_hci_cb.msg_len;
    }
  }

  LOG(DEBUG) << StringPrintf(
      "nfa_hci_conn_cback Recvd data pipe:%d  %s  chain:%d  assmbl:%d  len:%d",
      (uint8_t)pipe,
      nfa_hciu_get_type_inst_names(pipe, nfa_hci_cb.type, nfa_hci_cb.inst, buff,
                                   MAX_BUFF_SIZE),
      (uint8_t)chaining_bit, (uint8_t)nfa_hci_cb.assembling, p_pkt->len);

  /* If still reassembling fragments, just return */
#if(NXP_EXTNS == TRUE)
  if (!p || p_pipe_cmdrsp_info->reassembling) {
      /* Either no buffer to hold incoming hcp or the current hcp packet
      ** is not the last fragment and will continue to reassemble
      */
      p_pipe_cmdrsp_info->msg_rx_len = nfa_hci_cb.msg_len;
          /* if not last packet, release GKI buffer */
      nfa_sys_start_timer (&(p_pipe_cmdrsp_info->rsp_timer),
        NFA_HCI_RSP_TIMEOUT_EVT, p_pipe_cmdrsp_info->rsp_timeout);
#else
  if (nfa_hci_cb.assembling) {
#endif
    GKI_freebuf(p_pkt);
    return;
  }
#if(NXP_EXTNS == TRUE)
  if (nfa_hci_cb.type == NFA_HCI_EVENT_TYPE) {
          /* Response APDU: stop the timers */
      nfa_sys_stop_timer (&(p_pipe_cmdrsp_info->rsp_timer));
      if(p_pipe_cmdrsp_info->w4_rsp_apdu_evt) {
      /*Clear chaining resp pending once full resp is received*/
        p_pipe_cmdrsp_info->msg_rx_len = 0;
        nfa_hci_cb.hci_state = NFA_HCI_STATE_IDLE;
      }
    }
#endif
  /* If we got a response, cancel the response timer. Also, if waiting for */
  /* a single response, we can go back to idle state                       */
#if(NXP_EXTNS != TRUE)
  if ((nfa_hci_cb.hci_state == NFA_HCI_STATE_WAIT_RSP) &&
      ((nfa_hci_cb.type == NFA_HCI_RESPONSE_TYPE) ||
       (nfa_hci_cb.w4_rsp_evt && (nfa_hci_cb.type == NFA_HCI_EVENT_TYPE)))) {
#else
  if ((nfa_hci_cb.hci_state == NFA_HCI_STATE_WAIT_RSP) &&
          ((nfa_hci_cb.type == NFA_HCI_RESPONSE_TYPE))) {
#endif
      nfa_sys_stop_timer(&nfa_hci_cb.timer);
      nfa_hci_cb.hci_state = NFA_HCI_STATE_IDLE;
    }
#if(NXP_EXTNS == TRUE)
    else if((nfa_hci_cb.type == NFA_HCI_RESPONSE_TYPE && p_pipe_cmdrsp_info->w4_cmd_rsp)) {
      /* Response received to command sent on generic gate pipe */
      nfa_sys_stop_timer (&(p_pipe_cmdrsp_info->rsp_timer));
  }
  LOG(DEBUG) << StringPrintf(
      "nfa_hci_conn_cback Recvd data pipe:%d  Type: %u  Inst: %u  chain:%d ",
      pipe, nfa_hci_cb.type, nfa_hci_cb.inst, chaining_bit);
#endif
  switch (pipe) {
    case NFA_HCI_ADMIN_PIPE:
      /* Check if data packet is a command, response or event */
      if (nfa_hci_cb.type == NFA_HCI_COMMAND_TYPE) {
        nfa_hci_handle_admin_gate_cmd(p, pkt_len);
      } else if (nfa_hci_cb.type == NFA_HCI_RESPONSE_TYPE) {
        nfa_hci_handle_admin_gate_rsp(p, (uint8_t)pkt_len);
      } else if (nfa_hci_cb.type == NFA_HCI_EVENT_TYPE) {
        nfa_hci_handle_admin_gate_evt();
      }
      break;

    case NFA_HCI_LINK_MANAGEMENT_PIPE:
      /* We don't send Link Management commands, we only get them */
      if (nfa_hci_cb.type == NFA_HCI_COMMAND_TYPE)
        nfa_hci_handle_link_mgm_gate_cmd(p, pkt_len);
      break;

    default:
      if (pipe >= NFA_HCI_FIRST_DYNAMIC_PIPE)
        nfa_hci_handle_dyn_pipe_pkt(pipe, p, pkt_len);
      break;
  }
#if(NXP_EXTNS != TRUE)
  if ((nfa_hci_cb.type == NFA_HCI_RESPONSE_TYPE) ||
      (nfa_hci_cb.w4_rsp_evt && (nfa_hci_cb.type == NFA_HCI_EVENT_TYPE))) {
    nfa_hci_cb.w4_rsp_evt = false;
  }
#endif
  /* Send a message to ouselves to check for anything to do */
  p_pkt->event = NFA_HCI_CHECK_QUEUE_EVT;
  p_pkt->len = 0;
  nfa_sys_sendmsg(p_pkt);
}
/*******************************************************************************
**
** Function         nfa_hci_handle_nv_read
**
** Description      handler function for nv read complete event
**
** Returns          None
**
*******************************************************************************/
void nfa_hci_handle_nv_read(uint8_t block, tNFA_STATUS status) {
  uint8_t session_id[NFA_HCI_SESSION_ID_LEN];
  uint8_t default_session[NFA_HCI_SESSION_ID_LEN] = {0xFF, 0xFF, 0xFF, 0xFF,
                                                     0xFF, 0xFF, 0xFF, 0xFF};
  uint8_t reset_session[NFA_HCI_SESSION_ID_LEN] = {0x00, 0x00, 0x00, 0x00,
                                                   0x00, 0x00, 0x00, 0x00};
  uint32_t os_tick;

  if (block == DH_NV_BLOCK) {
    /* Stop timer as NVDATA Read Completed */
    nfa_sys_stop_timer(&nfa_hci_cb.timer);
    nfa_hci_cb.nv_read_cmplt = true;
    if ((status != NFA_STATUS_OK) || (!nfa_hci_is_valid_cfg()) ||
        (!(memcmp(nfa_hci_cb.cfg.admin_gate.session_id, default_session,
                  NFA_HCI_SESSION_ID_LEN))) ||
        (!(memcmp(nfa_hci_cb.cfg.admin_gate.session_id, reset_session,
                  NFA_HCI_SESSION_ID_LEN)))) {
      nfa_hci_cb.b_hci_netwk_reset = true;
      /* Set a new session id so that we clear all pipes later after seeing a
       * difference with the HC Session ID */
      memcpy(&session_id[(NFA_HCI_SESSION_ID_LEN / 2)],
             nfa_hci_cb.cfg.admin_gate.session_id,
             (NFA_HCI_SESSION_ID_LEN / 2));
      os_tick = GKI_get_os_tick_count();
      memcpy(session_id, (uint8_t*)&os_tick, (NFA_HCI_SESSION_ID_LEN / 2));
      nfa_hci_restore_default_config(session_id);
    }
    nfa_hci_startup();
  }
}

/*******************************************************************************
**
** Function         nfa_hci_rsp_timeout
**
** Description      action function to process timeout
**
** Returns          None
**
*******************************************************************************/
void nfa_hci_rsp_timeout() {
  tNFA_HCI_EVT evt = 0;
  tNFA_HCI_EVT_DATA evt_data;
  uint8_t delete_pipe;
#if(NXP_EXTNS == TRUE)
  uint8_t ee_entry_index = 0;
#endif
  LOG(DEBUG) << StringPrintf("State: %u  Cmd: %u", nfa_hci_cb.hci_state,
                             nfa_hci_cb.cmd_sent);

  evt_data.status = NFA_STATUS_FAILED;

  switch (nfa_hci_cb.hci_state) {
    case NFA_HCI_STATE_STARTUP:
    case NFA_HCI_STATE_RESTORE:
      LOG(ERROR) << StringPrintf(
          "nfa_hci_rsp_timeout - Initialization failed!");
      nfa_hci_startup_complete(NFA_STATUS_TIMEOUT);
      break;

    case NFA_HCI_STATE_WAIT_NETWK_ENABLE:
    case NFA_HCI_STATE_RESTORE_NETWK_ENABLE:

      if (nfa_hci_cb.w4_hci_netwk_init) {
        /* HCI Network is enabled */
        nfa_hci_cb.w4_hci_netwk_init = false;
#if(NXP_EXTNS == TRUE)
        nfa_hci_cb.num_nfcee = NFA_HCI_MAX_HOST_IN_NETWORK;
        NFA_EeGetInfo(&nfa_hci_cb.num_nfcee, nfa_hci_cb.ee_info);
#endif
        nfa_hciu_send_get_param_cmd(NFA_HCI_ADMIN_PIPE,
                                    NFA_HCI_HOST_LIST_INDEX);
      } else {
#if(NXP_EXTNS == TRUE)
          if (!nfa_hci_enable_one_nfcee ())
#endif
        nfa_hci_startup_complete(NFA_STATUS_FAILED);
      }
      break;

    case NFA_HCI_STATE_REMOVE_GATE:
      /* Something wrong, NVRAM data could be corrupt */
      if (nfa_hci_cb.cmd_sent == NFA_HCI_ADM_DELETE_PIPE) {
        nfa_hciu_send_clear_all_pipe_cmd();
      } else {
        nfa_hciu_remove_all_pipes_from_host(0);
        nfa_hci_api_dealloc_gate(nullptr);
      }
      break;

    case NFA_HCI_STATE_APP_DEREGISTER:
      /* Something wrong, NVRAM data could be corrupt */
      if (nfa_hci_cb.cmd_sent == NFA_HCI_ADM_DELETE_PIPE) {
        nfa_hciu_send_clear_all_pipe_cmd();
      } else {
        nfa_hciu_remove_all_pipes_from_host(0);
        nfa_hci_api_deregister(nullptr);
      }
      break;

    case NFA_HCI_STATE_WAIT_RSP:
      nfa_hci_cb.hci_state = NFA_HCI_STATE_IDLE;
#if(NXP_EXTNS == TRUE)
      while (ee_entry_index < nfa_ee_max_ee_cfg) {
        nfa_hci_release_transceive(nfa_ee_cb.ecb[ee_entry_index].nfcee_id, NFA_STATUS_TIMEOUT);
        ee_entry_index++;
      }
#endif
      if (nfa_hci_cb.w4_rsp_evt) {
        nfa_hci_cb.w4_rsp_evt = false;
        evt = NFA_HCI_EVENT_RCVD_EVT;
        evt_data.rcvd_evt.pipe = nfa_hci_cb.pipe_in_use;
        evt_data.rcvd_evt.evt_code = 0;
        evt_data.rcvd_evt.evt_len = 0;
        evt_data.rcvd_evt.p_evt_buf = nullptr;
        nfa_hci_cb.rsp_buf_size = 0;
        nfa_hci_cb.p_rsp_buf = nullptr;
#if(NXP_EXTNS != TRUE)
        break;
      }
#endif
      delete_pipe = 0;
      switch (nfa_hci_cb.cmd_sent) {
        case NFA_HCI_ANY_SET_PARAMETER:
          /*
           * As no response to the command sent on this pipe, we may assume the
           * pipe is
           * deleted already and release the pipe. But still send delete pipe
           * command to be safe.
           */
          delete_pipe = nfa_hci_cb.pipe_in_use;
          evt_data.registry.pipe = nfa_hci_cb.pipe_in_use;
          evt_data.registry.data_len = 0;
          evt_data.registry.index = nfa_hci_cb.param_in_use;
          evt = NFA_HCI_SET_REG_RSP_EVT;
          break;

        case NFA_HCI_ANY_GET_PARAMETER:
          /*
           * As no response to the command sent on this pipe, we may assume the
           * pipe is
           * deleted already and release the pipe. But still send delete pipe
           * command to be safe.
           */
          delete_pipe = nfa_hci_cb.pipe_in_use;
          evt_data.registry.pipe = nfa_hci_cb.pipe_in_use;
          evt_data.registry.data_len = 0;
          evt_data.registry.index = nfa_hci_cb.param_in_use;
          evt = NFA_HCI_GET_REG_RSP_EVT;
          break;

        case NFA_HCI_ANY_OPEN_PIPE:
          /*
           * As no response to the command sent on this pipe, we may assume the
           * pipe is
           * deleted already and release the pipe. But still send delete pipe
           * command to be safe.
           */
          delete_pipe = nfa_hci_cb.pipe_in_use;
          evt_data.opened.pipe = nfa_hci_cb.pipe_in_use;
          evt = NFA_HCI_OPEN_PIPE_EVT;
          break;

        case NFA_HCI_ANY_CLOSE_PIPE:
          /*
           * As no response to the command sent on this pipe, we may assume the
           * pipe is
           * deleted already and release the pipe. But still send delete pipe
           * command to be safe.
           */
          delete_pipe = nfa_hci_cb.pipe_in_use;
          evt_data.closed.pipe = nfa_hci_cb.pipe_in_use;
          evt = NFA_HCI_CLOSE_PIPE_EVT;
          break;

        case NFA_HCI_ADM_CREATE_PIPE:
          evt_data.created.pipe = nfa_hci_cb.pipe_in_use;
          evt_data.created.source_gate = nfa_hci_cb.local_gate_in_use;
          evt_data.created.dest_host = nfa_hci_cb.remote_host_in_use;
          evt_data.created.dest_gate = nfa_hci_cb.remote_gate_in_use;
          evt = NFA_HCI_CREATE_PIPE_EVT;
          break;

        case NFA_HCI_ADM_DELETE_PIPE:
          /*
           * As no response to the command sent on this pipe, we may assume the
           * pipe is
           * deleted already. Just release the pipe.
           */
          if (nfa_hci_cb.pipe_in_use <= NFA_HCI_LAST_DYNAMIC_PIPE)
            nfa_hciu_release_pipe(nfa_hci_cb.pipe_in_use);
          evt_data.deleted.pipe = nfa_hci_cb.pipe_in_use;
          evt = NFA_HCI_DELETE_PIPE_EVT;
          break;

        default:
          /*
           * As no response to the command sent on this pipe, we may assume the
           * pipe is
           * deleted already and release the pipe. But still send delete pipe
           * command to be safe.
           */
#if(NXP_EXTNS != TRUE)
            /*dont delete the pipe incase of no response*/
          delete_pipe = nfa_hci_cb.pipe_in_use;
#endif
          break;
      }
#if(NXP_EXTNS == TRUE)
      if(nfa_hci_cb.evt_sent.evt_type == NFA_HCI_EVT_ABORT) {
          evt = NFA_HCI_APDU_ABORTED_EVT;
          evt_data.apdu_aborted.status  = NFA_STATUS_TIMEOUT;
          evt_data.apdu_aborted.atr_len = 0;
          evt_data.apdu_aborted.host_id = nfa_hci_cb.remote_host_in_use;
      }
#endif
      if (delete_pipe && (delete_pipe <= NFA_HCI_LAST_DYNAMIC_PIPE)) {
        nfa_hciu_send_delete_pipe_cmd(delete_pipe);
        nfa_hciu_release_pipe(delete_pipe);
#if(NXP_EXTNS == TRUE)
      }
#endif
    }
      break;
    case NFA_HCI_STATE_DISABLED:
    default:
    LOG(DEBUG) << StringPrintf("Timeout in DISABLED/ Invalid state");
    break;
  }
  if (evt != 0) nfa_hciu_send_to_app(evt, &evt_data, nfa_hci_cb.app_in_use);
}

/*******************************************************************************
**
** Function         nfa_hci_set_receive_buf
**
** Description      Set reassembly buffer for incoming message
**
** Returns          status
**
*******************************************************************************/
#if(NXP_EXTNS == TRUE)
static void nfa_hci_set_receive_buf(tNFA_HCI_PIPE_CMDRSP_INFO  *p_pipe_cmdrsp_info) {
#else
static void nfa_hci_set_receive_buf(uint8_t pipe) {
#endif
#if(NXP_EXTNS == TRUE)
    if (  (nfa_hci_cb.type == NFA_HCI_EVENT_TYPE)
            &&(p_pipe_cmdrsp_info->w4_rsp_apdu_evt)  )
    {
        /* Response APDU */
        if (  (p_pipe_cmdrsp_info->rsp_buf_size)
                &&(p_pipe_cmdrsp_info->p_rsp_buf != nullptr)  )
        {
            /* Buffer provided by layer above for Response APDU */
            nfa_hci_cb.p_msg_data              = p_pipe_cmdrsp_info->p_rsp_buf;
            nfa_hci_cb.max_msg_len          = p_pipe_cmdrsp_info->rsp_buf_size;
            p_pipe_cmdrsp_info->max_msg_rx_len = nfa_hci_cb.max_msg_len;
        }
        else
        {
            nfa_hci_cb.p_msg_data              = nullptr;
            nfa_hci_cb.max_msg_len             = 0;
            p_pipe_cmdrsp_info->max_msg_rx_len = 0;
        }
    }
    else
    {
        nfa_hci_cb.p_msg_data              = nfa_hci_cb.msg_data;
        nfa_hci_cb.max_msg_len             = NFA_MAX_HCI_EVENT_LEN;
        p_pipe_cmdrsp_info->max_msg_rx_len = nfa_hci_cb.max_msg_len;
    }
#else
  if ((pipe >= NFA_HCI_FIRST_DYNAMIC_PIPE) &&
      (nfa_hci_cb.type == NFA_HCI_EVENT_TYPE)) {
    if ((nfa_hci_cb.rsp_buf_size) && (nfa_hci_cb.p_rsp_buf != nullptr)) {
      nfa_hci_cb.p_msg_data = nfa_hci_cb.p_rsp_buf;
      nfa_hci_cb.max_msg_len = nfa_hci_cb.rsp_buf_size;
      return;
    }
  }
  nfa_hci_cb.p_msg_data = nfa_hci_cb.msg_data;
  nfa_hci_cb.max_msg_len = NFA_MAX_HCI_EVENT_LEN;
#endif
}

/*******************************************************************************
 **
 ** Function         nfa_hci_assemble_msg
 **
 ** Description      Reassemble the incoming message
 **
 ** Returns          None
 **
 *******************************************************************************/
#if(NXP_EXTNS == TRUE)
static bool nfa_hci_assemble_msg(uint8_t* p_data, uint16_t data_len) {
    if (nfa_hci_cb.p_msg_data == nullptr)
    {
        LOG(ERROR) << StringPrintf ("nfa_hci_assemble_msg (): No buffer! Dropping :%u bytes",
                data_len);
        return false;
    }
    if ((nfa_hci_cb.msg_len + data_len) > nfa_hci_cb.max_msg_len) {
        /* Fill the buffer as much it can hold */
        memcpy(&nfa_hci_cb.p_msg_data[nfa_hci_cb.msg_len], p_data,
                (nfa_hci_cb.max_msg_len - nfa_hci_cb.msg_len));
        nfa_hci_cb.msg_len = nfa_hci_cb.max_msg_len;
        LOG(ERROR) << StringPrintf(
                "nfa_hci_assemble_msg (): Insufficient buffer to Reassemble HCP "
                "packet! Dropping :%u bytes",
                ((nfa_hci_cb.msg_len + data_len) - nfa_hci_cb.max_msg_len));
        /* Not enaough buffer to reassemble the incoming hcp */
        return false;
    }
    memcpy(&nfa_hci_cb.p_msg_data[nfa_hci_cb.msg_len], p_data, data_len);
    nfa_hci_cb.msg_len += data_len;
    return true;
}
#else
static void nfa_hci_assemble_msg(uint8_t* p_data, uint16_t data_len) {
  if ((nfa_hci_cb.msg_len + data_len) > nfa_hci_cb.max_msg_len) {
    /* Fill the buffer as much it can hold */
    memcpy(&nfa_hci_cb.p_msg_data[nfa_hci_cb.msg_len], p_data,
           (nfa_hci_cb.max_msg_len - nfa_hci_cb.msg_len));
    nfa_hci_cb.msg_len = nfa_hci_cb.max_msg_len;
    /* Set Reassembly failed */
    nfa_hci_cb.assembly_failed = true;
    LOG(ERROR) << StringPrintf(
        "Insufficient buffer to Reassemble HCP "
        "packet! Dropping :%u bytes",
        ((nfa_hci_cb.msg_len + data_len) - nfa_hci_cb.max_msg_len));
  } else {
    memcpy(&nfa_hci_cb.p_msg_data[nfa_hci_cb.msg_len], p_data, data_len);
    nfa_hci_cb.msg_len += data_len;
  }
}
#endif

/*******************************************************************************
**
** Function         nfa_hci_evt_hdlr
**
** Description      Processing all event for NFA HCI
**
** Returns          TRUE if p_msg needs to be deallocated
**
*******************************************************************************/
static bool nfa_hci_evt_hdlr(NFC_HDR* p_msg) {
    LOG(DEBUG) << StringPrintf(
        "nfa_hci_evt_hdlr state: %s (%d) event: %s (0x%04x)",
        nfa_hciu_get_state_name(nfa_hci_cb.hci_state).c_str(),
        nfa_hci_cb.hci_state, nfa_hciu_get_event_name(p_msg->event).c_str(),
        p_msg->event);

    /* If this is an API request, queue it up */
    if ((p_msg->event >= NFA_HCI_FIRST_API_EVENT) &&
        (p_msg->event <= NFA_HCI_LAST_API_EVENT)) {
        GKI_enqueue(&nfa_hci_cb.hci_api_q, p_msg);
    } else {
        tNFA_HCI_EVENT_DATA* p_evt_data = (tNFA_HCI_EVENT_DATA*)p_msg;
        switch (p_msg->event) {
            case NFA_HCI_RSP_NV_READ_EVT:
        nfa_hci_handle_nv_read(p_evt_data->nv_read.block,
                               p_evt_data->nv_read.status);
        break;

      case NFA_HCI_RSP_NV_WRITE_EVT:
        /* NV Ram write completed - nothing to do... */
        break;

      case NFA_HCI_RSP_TIMEOUT_EVT:
        nfa_hci_rsp_timeout();
        break;

      case NFA_HCI_CHECK_QUEUE_EVT:
        if (HCI_LOOPBACK_DEBUG == NFA_HCI_DEBUG_ON) {
          if (p_msg->len != 0) {
            tNFC_CONN nfc_conn;
            nfc_conn.data.p_data = p_msg;
            nfa_hci_conn_cback(0, NFC_DATA_CEVT, &nfc_conn);
            return false;
          }
        }
        break;
    }
    }

  if ((p_msg->event > NFA_HCI_LAST_API_EVENT)) GKI_freebuf(p_msg);

  nfa_hci_check_api_requests();

  if (nfa_hciu_is_no_host_resetting()) nfa_hci_check_pending_api_requests();

  if ((nfa_hci_cb.hci_state == NFA_HCI_STATE_IDLE) &&
      (nfa_hci_cb.nv_write_needed)) {
    nfa_hci_cb.nv_write_needed = false;
    nfa_nv_co_write((uint8_t*)&nfa_hci_cb.cfg, sizeof(nfa_hci_cb.cfg),
                    DH_NV_BLOCK);
  }

  return false;
}

#if(NXP_EXTNS == TRUE)
void nfa_hci_release_transceive(uint8_t host_id, uint8_t status) {
  LOG(DEBUG) << StringPrintf("nfa_hci_release_transcieve ()");
  tNFA_HCI_DYN_PIPE           *p_pipe;
  tNFA_HCI_PIPE_CMDRSP_INFO   *p_pipe_cmdrsp_info = nullptr;
  tNFA_HCI_EVT_DATA           evt_data;
  uint8_t                     cmd_inst;
  uint8_t                     cmd_inst_param;
  tNFA_HCI_DYN_GATE         *p_gate;

  p_pipe = nfa_hciu_find_dyn_apdu_pipe_for_host (host_id);
  if ((p_pipe == nullptr) || (p_pipe->pipe_id == NFA_HCI_INVALID_PIPE)) {
    LOG(ERROR) << StringPrintf(
        "nfa_hci_release_transcieve ():pipe is not valid or NULL ");
    return;
  }
  p_pipe_cmdrsp_info = nfa_hciu_get_pipe_cmdrsp_info(p_pipe->pipe_id);
  if(p_pipe_cmdrsp_info == nullptr) return;

  if (p_pipe_cmdrsp_info->w4_cmd_rsp)
  {
      /* Timeout to command response on host specific generic pipe */
      p_pipe_cmdrsp_info->w4_cmd_rsp = false;

      p_gate = nfa_hciu_find_gate_by_gid (p_pipe->local_gate);

      if (p_gate == nullptr)
      {
          LOG(ERROR) << StringPrintf ("nfa_hci_release_transceive ():Invalid Gate[0x%02x] for pipe[0x%02x] ",
                             p_pipe->local_gate, p_pipe->pipe_id);
          return;
      }

      cmd_inst       = p_pipe_cmdrsp_info->cmd_inst_sent;
      cmd_inst_param = p_pipe_cmdrsp_info->cmd_inst_param_sent;

      if (cmd_inst == NFA_HCI_ANY_OPEN_PIPE)
      {
          /* Tell application */
          evt_data.opened.status  = NFA_STATUS_TIMEOUT;
          evt_data.opened.pipe = p_pipe->pipe_id;

          nfa_hciu_send_to_app (NFA_HCI_OPEN_PIPE_EVT, &evt_data, p_gate->gate_owner);
      }
      else if (cmd_inst == NFA_HCI_ANY_CLOSE_PIPE)
      {
          /* Tell application */
          evt_data.closed.status  = NFA_STATUS_TIMEOUT;
          evt_data.closed.pipe = p_pipe->pipe_id;

          nfa_hciu_send_to_app (NFA_HCI_CLOSE_PIPE_EVT, &evt_data, p_gate->gate_owner);
      }
      else if (cmd_inst == NFA_HCI_ANY_GET_PARAMETER)
      {
          /* Tell application */
          evt_data.registry.status   = NFA_STATUS_TIMEOUT;
          evt_data.registry.pipe  = p_pipe->pipe_id;
          evt_data.registry.index    = cmd_inst_param;

          nfa_hciu_send_to_app (NFA_HCI_GET_REG_RSP_EVT, &evt_data, p_gate->gate_owner);
      }
      else if (cmd_inst == NFA_HCI_ANY_SET_PARAMETER)
      {
          /* Tell application */
          evt_data.registry.status  = NFA_STATUS_TIMEOUT;
          evt_data.registry.pipe = p_pipe->pipe_id;
          evt_data.registry.index   = cmd_inst_param;

          nfa_hciu_send_to_app (NFA_HCI_SET_REG_RSP_EVT, &evt_data, p_gate->gate_owner);
      }
      else
      {
          /* Could be a response to application specific command sent, pass it on */
          evt_data.rsp_rcvd.status   = NFA_STATUS_TIMEOUT;
          evt_data.rsp_rcvd.pipe  = p_pipe->pipe_id;

          nfa_hciu_send_to_app (NFA_HCI_RSP_RCVD_EVT, &evt_data, p_gate->gate_owner);
      }
  }
  else
  {
      /* Timeout in APDU Pipe */
      LOG(DEBUG) << StringPrintf(
          "nfa_hci_release_transceive () pending requests");

      p_pipe_cmdrsp_info->p_rsp_buf    = nullptr;
      p_pipe_cmdrsp_info->rsp_buf_size = 0;

      if (p_pipe_cmdrsp_info->w4_atr_evt)
      {
          /* Timeout to ETSI_HCI_EVT_ATR after ETSI_HCI_EVT_ABORT is sent on the APDU pipe
          ** and so cannot send next command APDU on the pipe till APDU server initialize
          ** and sends ETSI_HCI_EVT_ATR on the pipe
          */
          p_pipe_cmdrsp_info->w4_rsp_apdu_evt = false;

          evt_data.apdu_aborted.status  = status;
          evt_data.apdu_aborted.host_id = p_pipe->dest_host;
          evt_data.apdu_aborted.atr_len = 0;
          /* Send NFA_HCI_APDU_ABORTED_EVT to notify status */
          nfa_hciu_send_to_app (NFA_HCI_APDU_ABORTED_EVT, &evt_data,
                                p_pipe_cmdrsp_info->pipe_user);
      }
      else if (p_pipe_cmdrsp_info->w4_rsp_apdu_evt)
      {
          /* Timeout to Response APDU (ETSI_HCI_EVT_R_APDU) */
          p_pipe_cmdrsp_info->w4_rsp_apdu_evt = false;

          evt_data.apdu_rcvd.status  = status;
          evt_data.apdu_rcvd.p_apdu  = nullptr;
          evt_data.apdu_rcvd.host_id = p_pipe->dest_host;

          /* notify NFA_HCI_RSP_APDU_RCVD_EVT to the application */
          nfa_hciu_send_to_app (NFA_HCI_RSP_APDU_RCVD_EVT, &evt_data,
                                p_pipe_cmdrsp_info->pipe_user);

          /* Release the temporary ownership for APDU Pipe given to the App */
          p_pipe_cmdrsp_info->pipe_user = NFA_HCI_APP_HANDLE_NONE;
      }
  }
  p_pipe_cmdrsp_info->cmd_inst_sent        = 0;
  p_pipe_cmdrsp_info->cmd_inst_param_sent  = 0;
}

/*******************************************************************************
**
** Function         nfa_hci_timer_cback
**
** Description      Process timeout event when timer expires
**
** Returns          None
**
*******************************************************************************/
static void nfa_hci_timer_cback (TIMER_LIST_ENT *p_tle)
{
    uint8_t                     *p_pipe_id;
    uint8_t                     cmd_inst;
    uint8_t                     cmd_inst_param;
    TIMER_LIST_ENT            *p_timer;
    tNFA_HCI_DYN_PIPE         *p_pipe;
    tNFA_HCI_DYN_GATE* p_gate = nullptr;
    tNFA_HCI_EVT_DATA         evt_data;
    tNFA_HCI_PIPE_CMDRSP_INFO *p_pipe_cmdrsp_info = nullptr;

    LOG(DEBUG) << StringPrintf("nfa_hci_timer_cback() state = %d",
                               nfa_hci_cb.hci_state);

    if ((nfa_hci_cb.hci_state == NFA_HCI_STATE_STARTUP)
      || (nfa_hci_cb.hci_state == NFA_HCI_STATE_WAIT_NETWK_ENABLE))
    {
        LOG(ERROR) << StringPrintf ("nfa_hci_timer_cback - Initialization failed!");
#if(NXP_EXTNS == TRUE)
        nfa_hci_startup_complete (NFA_STATUS_OK);
#else
        /* Timeout to Read Registry from APDU gate pipe */
        nfa_hci_startup_complete (NFA_STATUS_FAILED);
#endif
    }
    else
    {
        p_timer   = p_tle;
        p_pipe_id = (uint8_t *) p_timer->param;

        p_pipe = nfa_hciu_find_pipe_by_pid (*p_pipe_id);
        p_pipe_cmdrsp_info = nfa_hciu_get_pipe_cmdrsp_info (*p_pipe_id);
        if (p_pipe_cmdrsp_info == NULL || p_pipe == NULL) {
          LOG(ERROR) << StringPrintf(
              "p_pipe_cmdrsp_info or p_pipe was found NULL");
          return;
        }
        memset (&evt_data, 0, sizeof (evt_data));

        if (p_pipe_cmdrsp_info->w4_cmd_rsp)
        {
            /* Timeout to command response on host specific generic pipe */
            p_pipe_cmdrsp_info->w4_cmd_rsp = false;

            p_gate = nfa_hciu_find_gate_by_gid (p_pipe->local_gate);

            if (p_gate == nullptr)
            {
                LOG(ERROR) << StringPrintf("nfa_hci_timer_cback ():Invalid Gate[0x%02x] for pipe[0x%02x] ",
                                   p_pipe->local_gate, p_pipe->pipe_id);
                return;
            }

            cmd_inst       = p_pipe_cmdrsp_info->cmd_inst_sent;
            cmd_inst_param = p_pipe_cmdrsp_info->cmd_inst_param_sent;

            if (cmd_inst == NFA_HCI_ANY_OPEN_PIPE)
            {
                /* Tell application */
                evt_data.opened.status  = NFA_STATUS_TIMEOUT;
                evt_data.opened.pipe = p_pipe->pipe_id;

                nfa_hciu_send_to_app (NFA_HCI_OPEN_PIPE_EVT, &evt_data, p_gate->gate_owner);
            }
            else if (cmd_inst == NFA_HCI_ANY_CLOSE_PIPE)
            {
                /* Tell application */
                evt_data.closed.status  = NFA_STATUS_TIMEOUT;
                evt_data.closed.pipe = p_pipe->pipe_id;

                nfa_hciu_send_to_app (NFA_HCI_CLOSE_PIPE_EVT, &evt_data, p_gate->gate_owner);
            }
            else if (cmd_inst == NFA_HCI_ANY_GET_PARAMETER)
            {
                /* Tell application */
                evt_data.registry.status   = NFA_STATUS_TIMEOUT;
                evt_data.registry.pipe  = p_pipe->pipe_id;
                evt_data.registry.index    = cmd_inst_param;

                nfa_hciu_send_to_app (NFA_HCI_GET_REG_RSP_EVT, &evt_data, p_gate->gate_owner);
            }
            else if (cmd_inst == NFA_HCI_ANY_SET_PARAMETER)
            {
                /* Tell application */
                evt_data.registry.status  = NFA_STATUS_TIMEOUT;
                evt_data.registry.pipe = p_pipe->pipe_id;
                evt_data.registry.index   = cmd_inst_param;

                nfa_hciu_send_to_app (NFA_HCI_SET_REG_RSP_EVT, &evt_data, p_gate->gate_owner);
            }
            else
            {
                /* Could be a response to application specific command sent, pass it on */
                evt_data.rsp_rcvd.status   = NFA_STATUS_TIMEOUT;
                evt_data.rsp_rcvd.pipe  = p_pipe->pipe_id;

                nfa_hciu_send_to_app (NFA_HCI_RSP_RCVD_EVT, &evt_data, p_gate->gate_owner);
            }
        }
        else
        {
            /* Timeout in APDU Pipe */
            LOG(DEBUG) << StringPrintf(
                "nfa_hci_timer_cback () Timeout on APDU Pipe");

            p_pipe_cmdrsp_info->p_rsp_buf    = nullptr;
            p_pipe_cmdrsp_info->rsp_buf_size = 0;
            /*In case of chaining Rx timeout clear resp len*/
            p_pipe_cmdrsp_info->msg_rx_len = 0;
            if (p_pipe_cmdrsp_info->w4_atr_evt)
            {
                /* Timeout to ETSI_HCI_EVT_ATR after ETSI_HCI_EVT_ABORT is sent on the APDU pipe
                ** and so cannot send next command APDU on the pipe till APDU server initialize
                ** and sends ETSI_HCI_EVT_ATR on the pipe
                */
                p_pipe_cmdrsp_info->w4_atr_evt = false;
                p_pipe_cmdrsp_info->w4_rsp_apdu_evt = false;

                evt_data.apdu_aborted.status  = NFA_STATUS_TIMEOUT;
                evt_data.apdu_aborted.host_id = p_pipe->dest_host;

                /* Send NFA_HCI_APDU_ABORTED_EVT to notify status */
                nfa_hciu_send_to_app (NFA_HCI_APDU_ABORTED_EVT, &evt_data,
                                      p_pipe_cmdrsp_info->pipe_user);
            }
            else if (p_pipe_cmdrsp_info->w4_rsp_apdu_evt)
            {
                /* Timeout to Response APDU (ETSI_HCI_EVT_R_APDU) */
                p_pipe_cmdrsp_info->w4_rsp_apdu_evt = false;
                NFC_FlushData(NFC_HCI_CONN_ID);
                evt_data.apdu_rcvd.status  = NFA_STATUS_TIMEOUT;
                evt_data.apdu_rcvd.p_apdu  = nullptr;
                evt_data.apdu_rcvd.host_id = p_pipe->dest_host;
                nfa_hci_cb.hci_state = NFA_HCI_STATE_IDLE;
                /* notify NFA_HCI_RSP_APDU_RCVD_EVT to the application */
                nfa_hciu_send_to_app (NFA_HCI_RSP_APDU_RCVD_EVT, &evt_data,
                                      p_pipe_cmdrsp_info->pipe_user);

                /* Release the temporary ownership for APDU Pipe given to the App */
                p_pipe_cmdrsp_info->pipe_user = NFA_HCI_APP_HANDLE_NONE;
            }
        }
        //p_pipe_cmdrsp_info->cmd_inst_sent        = 0;
        //p_pipe_cmdrsp_info->cmd_inst_param_sent  = 0;
    }
}
#endif
