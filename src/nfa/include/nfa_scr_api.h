/******************************************************************************
 *
 *  Copyright 2019 NXP
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
 ******************************************************************************/

/******************************************************************************
 *
 *  This is the public interface file for NFA Smart Card Reader's application
 *  layer for (ETSI/POS/SCR/mPOS).
 *
 ******************************************************************************/

#ifndef NFA_SCR_API_H
#define NFA_SCR_API_H

#include "nfa_api.h"
#include "nfa_scr_int.h"
/*******************************************************************************
** SCR Definitions
*******************************************************************************/

/******************************************************************************
**
** Function         NFA_ScrSetReaderMode
**
** Description      This API shall be called by application to set current
**                  state of the Smart Card(ETSI/POS) Reader along with
*callback.
**                  Note: if mode is false then p_cback must be nullptr
**
**Parameters        Reader mode, JNI callback to receive reader events
**
** Returns:
**                  NFA_STATUS_OK if successfully initiated,
**                  NFA_STATUS_BUSY if RF transaction is going on
**                  NFA_STATUS_FAILED otherwise
**
******************************************************************************/
extern tNFA_STATUS NFA_ScrSetReaderMode(bool mode, tNFA_SCR_CBACK* p_cback);

/******************************************************************************
**
** Function         NFA_ScrGetReaderMode
**
** Description      This API shall be called by application to get current
**                  mode of the Smart Card(ETSI/POS) Reader.
**
** Returns:         True if SCR started else false
**
******************************************************************************/
extern bool NFA_ScrGetReaderMode();

#endif /* NFA_SCR_API_H */
