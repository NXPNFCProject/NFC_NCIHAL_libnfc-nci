/******************************************************************************
 *
 *  Copyright 2019-2020, 2022 NXP
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
 *  This is the public interface file for NFA Secure Reader's application
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
** Description      This API shall be called by application to set state
**                  of the Secure Reader along with callback.
**                  Note: if mode is false then p_cback must be nullptr
**
** Parameters       mode: Requested Reder operation,
**                  p_cback: JNI callback to receive reader events
**                  type: Type of the reader requested, by default MPOS
**
** Returns:
**                  NFA_STATUS_OK if successfully initiated,
**                  NFA_STATUS_FAILED otherwise
**
******************************************************************************/
tNFA_STATUS NFA_ScrSetReaderMode(bool mode, tNFA_SCR_CBACK* p_cback,
        uint8_t type = NFA_SCR_MPOS);

/******************************************************************************
**
** Function         NFA_ScrGetReaderMode
**
** Description      This API shall be called to check whether the Secure
**                  reader is started or not.
**
** Returns:         TRUE if Secure Reader is started else FALSE
**
******************************************************************************/
extern bool NFA_ScrGetReaderMode();

#endif /* NFA_SCR_API_H */
