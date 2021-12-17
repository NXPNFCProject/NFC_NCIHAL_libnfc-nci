/******************************************************************************
 *
 *  Copyright 2020-2021 NXP
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

#pragma once
#include <nfa_sys.h>
#include "nfa_hci_int.h"

#if (NXP_EXTNS == TRUE && NXP_SRD == TRUE)
/* Event to start SRD */
#define NFA_SRD_START_EVT 1
/* Event to stop SRD*/
#define NFA_SRD_STOP_EVT 2
/* Event SRD timeout*/
#define NFA_SRD_TIMEOUT_EVT 3

#define NCI_STATUS_SRD_TIMEOUT 0xE2

#define NFA_SRD_PROCESS_EVT(event, evt_data) \
  { nfa_srd_process_evt(event, evt_data); }

typedef uint8_t tNFA_SRD_EVT;

struct {
  int srd_state;
  bool isStopping;
} srd_t;

enum { DISABLE, ENABLE, TIMEOUT, FEATURE_NOT_SUPPORTED = 0xFF };

/* Action & utility functions in nfa_srd_main.cc */
extern void nfa_srd_process_evt(tNFA_SRD_EVT event, tNFA_HCI_EVT_DATA* evt_data);
void nfa_srd_init();
int nfa_srd_get_state();
bool nfa_srd_check_hci_evt(tNFA_HCI_EVT_DATA* evt_data);
void nfa_srd_dm_process_evt(tNFC_DISCOVER_EVT event,
        tNFC_DISCOVER* disc_evtdata);
#endif
