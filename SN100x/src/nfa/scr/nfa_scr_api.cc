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
 *****************************************************************************/

/******************************************************************************
 *  NFA interface for Secure Reader (ETSI/POS/SCR)
 ******************************************************************************/

#include "nfa_scr_api.h"
#include <android-base/stringprintf.h>
#include <base/logging.h>
#include <string.h>

using android::base::StringPrintf;

extern bool nfc_debug_enabled;
/*****************************************************************************
**  APIs
*****************************************************************************/

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
**                  type: Type of th reader requested
**
** Returns:
**                  NFA_STATUS_OK if successfully initiated,
**                  NFA_STATUS_FAILED otherwise
**
******************************************************************************/
tNFA_STATUS NFA_ScrSetReaderMode(bool mode, tNFA_SCR_CBACK* scr_cback, uint8_t type) {
  tNFA_SCR_API_SET_READER_MODE* p_msg;

  DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf("%s: Enter, Mode=%s", __func__, (mode ? "On" : "Off"));

  p_msg = (tNFA_SCR_API_SET_READER_MODE*)GKI_getbuf(
      sizeof(tNFA_SCR_API_SET_READER_MODE));
  if (p_msg != nullptr) {
    p_msg->hdr.event = NFA_SCR_API_SET_READER_MODE_EVT;
    p_msg->type = type;
    p_msg->mode = mode;
    p_msg->scr_cback = scr_cback;

    nfa_sys_sendmsg(p_msg);
    return (NFA_STATUS_OK);
  }
  return (NFA_STATUS_FAILED);
}

/******************************************************************************
**
** Function         NFA_ScrGetReaderMode
**
** Description      This API shall be called to check whether the Secure
**                  Reader is started or not.
**
** Returns:         TRUE if Secure Reader is started else FALSE
**
******************************************************************************/
bool NFA_ScrGetReaderMode() {
  bool isEnabled = true;
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: Enter", __func__);
  if ((nfa_scr_cb.state == NFA_SCR_STATE_STOPPED) ||
          (nfa_scr_cb.state == NFA_SCR_STATE_START_CONFIG)) {
   isEnabled = false;
  }
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("isEnabled =%x", isEnabled);
  return isEnabled;
}
