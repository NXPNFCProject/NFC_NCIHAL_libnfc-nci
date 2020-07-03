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
#include <nfa_api.h>
#include "nfa_dm_int.h"
#include <nfa_wlc_int.h>

/*******************************************************************************
**
** Function         NFA_WlcInitSubsystem
**
** Description      Attaches wlc subsystem
**
** Returns          NFA_STATUS_OK if successful
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_WlcInitSubsystem(tNFA_WLC_EVENT_CBACK* p_cback);

/*******************************************************************************
**
** Function         NFA_WlcDeInitSubsystem
**
** Description      remove attached subsystem
**
** Returns          none
**
*******************************************************************************/
void NFA_WlcDeInitSubsystem();

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
tNFA_STATUS NFA_WlcUpdateDiscovery(tNFA_WLC_DISC_REQ requestOperation);

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
extern tNFA_STATUS NFA_WlcRfIntfExtStart(tNFA_RF_INTF_EXT_PARAMS* p_params);

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
extern tNFA_STATUS NFA_WlcRfIntfExtStop(tNFA_RF_INTF_EXT_PARAMS* p_params);

#endif