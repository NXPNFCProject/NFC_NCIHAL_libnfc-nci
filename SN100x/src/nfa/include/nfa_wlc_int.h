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
#include <nfa_sys.h>
#include "nfa_dm_int.h"
#include "nfc_api.h"
#include "nfc_int.h"
/* NFCC supports WLC Feature */
#define WLC_FEATURE_SUPPORTED_EVT 1
/* Status of RF interface extension start */
#define WLC_RF_INTF_EXT_START_EVT 2
/* Status of RF interface extension stop */
#define WLC_RF_INTF_EXT_STOP_EVT 3
/*Notify WLC Listener detected*/
#define WLC_TAG_DETECTED_EVT 4

typedef void(tNFA_WLC_EVENT_CBACK)(uint8_t event, tNFA_CONN_EVT_DATA* p_data);
extern const tNFA_SYS_REG nfa_wlc_sys_reg;

typedef struct {
  tNFA_DM_WLC_DATA wlcData;
  tNFA_WLC_EVENT_CBACK* p_wlc_evt_cback;
    bool isRfIntfExtStarted;   /*RF interface Extension start status*/
} wlc_t;
extern wlc_t wlc_cb;
/*WLC events*/
enum {
  NFA_WLC_OP_REQUEST_EVT = NFA_SYS_EVT_START(NFA_ID_WLC),
  NFA_WLC_MAX_EVT
};

enum { SET, RESET };
typedef uint8_t tNFA_WLC_DISC_REQ;

typedef struct {
  uint8_t rfIntfExtType;
  uint8_t* data;
  uint16_t dataLen;
} tNFA_RF_INTF_EXT_PARAMS;

typedef struct {
  tNFA_WLC_EVENT_CBACK* p_cback;
  tNFA_WLC_DISC_REQ discReq;
  tNFA_RF_INTF_EXT_PARAMS rfIntfExt;
} tNFA_WLC_OP_PARAMS;

enum {
  NFA_WLC_OP_INIT,
  NFA_WLC_OP_UPDATE_DISCOVERY,
  NFA_WLC_OP_RF_INTF_EXT_START,
  NFA_WLC_OP_RF_INTF_EXT_STOP
};
typedef uint8_t tNFA_WLC_OP;

/* data type for NFA_WLC_op_req_EVT */
typedef struct {
  NFC_HDR hdr;
  tNFA_WLC_OP op; /* NFA WLC operation */
  tNFA_WLC_OP_PARAMS param;
} tNFA_WLC_OPERATION;

/* union of all data types */
typedef union {
  /* GKI event buffer header */
  NFC_HDR hdr;
  tNFA_WLC_OPERATION op_req;
} tNFA_WLC_MSG;

/* type definition for action functions */
typedef bool (*tNFA_WLC_ACTION)(tNFA_WLC_MSG* p_data);

/*******************************************************************************
**
** Function         WlcHandleOpReq
**
** Description      Handler for tNFA_WLC_OP request event, operation request
**
** Returns          true if caller should free p_data
**                  false if caller does not need to free p_data
**
*******************************************************************************/
bool WlcHandleOpReq(tNFA_WLC_MSG* p_data);

/*******************************************************************************
**
** Function         registerDmCallback
**
** Description      Registers WLC callback for DM events
**
** Returns          none
**
*******************************************************************************/
void registerWlcCallbackToDm();
#endif