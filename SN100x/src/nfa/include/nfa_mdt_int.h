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
#pragma once
#include <nfa_sys.h>
#include "nfa_hci_int.h"

/* Event to start MDT */
#define NFA_MDT_START_EVT 1
/* Event to stop MDT*/
#define NFA_MDT_STOP_EVT 2

#define NCI_STATUS_MDT_TIMEOUT 0xE2

#define NFA_MDT_PROCESS_EVT(event, evt_data) \
  { nfa_mdt_deactivate_req_evt(event, evt_data); }

struct {
  int mdt_state;
  tNFA_STATUS rsp_status;
  bool wait_for_deact_ntf;
} mdt_t;

enum { DISABLE, ENABLE, TIMEOUT, FEATURE_NOT_SUPPORTED = 0xFF };

/* Action & utility functions in nfa_mdt_main.cc */
extern void nfa_mdt_deactivate_req_evt(tNFC_DISCOVER_EVT event,
                                       tNFC_DISCOVER* evt_data);
bool nfa_mdt_check_hci_evt(tNFA_HCI_EVT_DATA* evt_data);
void nfa_mdt_timeout_ntf();
void nfa_mdt_init();
void nfa_mdt_deInit();
int nfa_mdt_get_state();
#endif
