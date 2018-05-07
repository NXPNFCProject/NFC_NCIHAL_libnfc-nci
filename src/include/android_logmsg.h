/******************************************************************************
 *
 *  Copyright (C) 2011-2012 Broadcom Corporation
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
 *  The original Work has been changed by NXP Semiconductors.
 *
 *  Copyright (C) 2015 NXP Semiconductors
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
/* Decode NFC packets and print them to ADB log.
* If protocol decoder is not present, then decode packets into hex numbers.
******************************************************************************/

#include "data_types.h"
#include "nfc_types.h"

#define DISP_NCI ProtoDispAdapterDisplayNciPacket
void ProtoDispAdapterDisplayNciPacket(uint8_t* nciPacket, uint16_t nciPacketLen,
                                      bool is_recv);
void ProtoDispAdapterUseRawOutput(bool isUseRaw);
void ScrLog(uint32_t trace_set_mask, const char* fmt_str, ...);
uint8_t* scru_dump_hex(uint8_t* p, char* pTitle, uint32_t len, uint32_t layer,
                       uint32_t type);
void BTDISP_LOCK_LOG();
void BTDISP_UNLOCK_LOG();
void BTDISP_INIT_LOCK();
void BTDISP_UNINIT_LOCK();
void DispHciCmd(NFC_HDR* p_buf);
void DispHciEvt(NFC_HDR* p_buf);
void DispLLCP(NFC_HDR* p_buf, bool is_recv);
void DispHcp(uint8_t* data, uint16_t len, bool is_recv);
void DispSNEP(uint8_t local_sap, uint8_t remote_sap, NFC_HDR* p_buf,
              bool is_first, bool is_rx);
void DispCHO(uint8_t* pMsg, uint32_t MsgLen, bool is_rx);
void DispT3TagMessage(NFC_HDR* p_msg, bool is_rx);
void DispRWT4Tags(NFC_HDR* p_buf, bool is_rx);
void DispCET4Tags(NFC_HDR* p_buf, bool is_rx);
void DispRWI93Tag(NFC_HDR* p_buf, bool is_rx, uint8_t command_to_respond);
void DispNDEFMsg(uint8_t* pMsg, uint32_t MsgLen, bool is_recv);
