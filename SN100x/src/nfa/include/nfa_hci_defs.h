/******************************************************************************
 *
 *  Copyright (C) 2009-2014 Broadcom Corporation
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
 *  Copyright 2018-2019 NXP
 *
 ******************************************************************************/
/******************************************************************************
 *
 *  This file contains the NFA HCI related definitions from the
 *  specification.
 *
 ******************************************************************************/

#ifndef NFA_HCI_DEFS_H
#define NFA_HCI_DEFS_H

/* Static gates */
#define NFA_HCI_LOOP_BACK_GATE 0x04
#define NFA_HCI_IDENTITY_MANAGEMENT_GATE 0x05

#define NFA_HCI_FIRST_HOST_SPECIFIC_GENERIC_GATE 0x10
#if(NXP_EXTNS == TRUE)
#define NFA_HCI_LAST_HOST_SPECIFIC_GENERIC_GATE 0xEF
#endif
#define NFA_HCI_FIRST_PROP_GATE 0xF0
#define NFA_HCI_LAST_PROP_GATE 0xFF
#if(NXP_EXTNS == TRUE)
#define NFA_HCI_CARD_RF_A_GATE              0x23
#endif
/* Generic Gates */
#define NFA_HCI_CONNECTIVITY_GATE 0x41

/* Proprietary Gates */
#define NFA_HCI_PROP_GATE_FIRST 0xF0
#define NFA_HCI_PROP_GATE_LAST 0xFF
#if(NXP_EXTNS == TRUE)
/* Host defined Dynamic gates */
#define NFA_HCI_APDU_GATE                   0x30

#define NFA_HCI_ID_MNGMNT_APP_GATE          0x10

#define NFA_HCI_GEN_PURPOSE_APDU_APP_GATE   0xF0

#define NFA_HCI_GEN_PURPOSE_APDU_GATE       0xF0

#define NFA_HCI_APDU_APP_GATE               0x11
#endif
/* Static pipes */
#define NFA_HCI_LINK_MANAGEMENT_PIPE 0x00
#define NFA_HCI_ADMIN_PIPE 0x01

/* Dynamic pipe range */
#define NFA_HCI_FIRST_DYNAMIC_PIPE 0x02
#define NFA_HCI_LAST_DYNAMIC_PIPE 0x6F

/* host_table */
#define NFA_HCI_HOST_CONTROLLER 0x00
#if(NXP_EXTNS == TRUE)
#define NFA_HCI_DH_HOST 0x01
#define NFA_HCI_UICC_HOST 0x02
#endif

/* Type of instruction */
#define NFA_HCI_COMMAND_TYPE 0x00
#define NFA_HCI_EVENT_TYPE 0x01
#define NFA_HCI_RESPONSE_TYPE 0x02

/* Chaining bit value */
#define NFA_HCI_MESSAGE_FRAGMENTATION 0x00
#define NFA_HCI_NO_MESSAGE_FRAGMENTATION 0x01

/* NFA HCI commands */

/* Commands for all gates */
#define NFA_HCI_ANY_SET_PARAMETER 0x01
#define NFA_HCI_ANY_GET_PARAMETER 0x02
#define NFA_HCI_ANY_OPEN_PIPE 0x03
#define NFA_HCI_ANY_CLOSE_PIPE 0x04

/* Admin gate commands */
#define NFA_HCI_ADM_CREATE_PIPE 0x10
#define NFA_HCI_ADM_DELETE_PIPE 0x11
#define NFA_HCI_ADM_NOTIFY_PIPE_CREATED 0x12
#define NFA_HCI_ADM_NOTIFY_PIPE_DELETED 0x13
#define NFA_HCI_ADM_CLEAR_ALL_PIPE 0x14
#define NFA_HCI_ADM_NOTIFY_ALL_PIPE_CLEARED 0x15

/* Connectivity gate command */
#define NFA_HCI_CON_PRO_HOST_REQUEST 0x10

/* NFA HCI responses */
#define NFA_HCI_ANY_OK 0x00
#define NFA_HCI_ANY_E_NOT_CONNECTED 0x01
#define NFA_HCI_ANY_E_CMD_PAR_UNKNOWN 0x02
#define NFA_HCI_ANY_E_NOK 0x03
#define NFA_HCI_ADM_E_NO_PIPES_AVAILABLE 0x04
#define NFA_HCI_ANY_E_REG_PAR_UNKNOWN 0x05
#define NFA_HCI_ANY_E_PIPE_NOT_OPENED 0x06
#define NFA_HCI_ANY_E_CMD_NOT_SUPPORTED 0x07
#define NFA_HCI_ANY_E_INHIBITED 0x08
#define NFA_HCI_ANY_E_TIMEOUT 0x09
#define NFA_HCI_ANY_E_REG_ACCESS_DENIED 0x0A
#define NFA_HCI_ANY_E_PIPE_ACCESS_DENIED 0x0B

/* NFA HCI Events */
#define NFA_HCI_EVT_HCI_END_OF_OPERATION 0x01
#define NFA_HCI_EVT_POST_DATA 0x02
#define NFA_HCI_EVT_HOT_PLUG 0x03
#if(NXP_EXTNS == TRUE)
#define NFA_HCI_EVT_INIT_COMPLETED 0x04
#endif
/* NFA HCI Connectivity gate Events */
#define NFA_HCI_EVT_CONNECTIVITY 0x10
#define NFA_HCI_EVT_TRANSACTION 0x12
#define NFA_HCI_EVT_OPERATION_ENDED 0x13
#if(NXP_EXTNS == TRUE)
/* Connectivity App gate registry identifier*/
#define NFA_HCI_UI_STATE_INDEX 0x01
#endif
/* Host controller Admin gate registry identifiers */
#define NFA_HCI_SESSION_IDENTITY_INDEX 0x01
#if(NXP_EXTNS == TRUE)
#define NFA_HCI_MAX_PIPE_INDEX 0x02
#endif
#define NFA_HCI_WHITELIST_INDEX 0x03
#define NFA_HCI_HOST_LIST_INDEX 0x04
#if(NXP_EXTNS == TRUE)
/* ETSI HCI APDU gate events */
#define NFA_HCI_EVT_C_APDU                  0x10
#define NFA_HCI_EVT_ABORT                   0x11
#define NFA_HCI_EVT_END_OF_APDU_TRANSACTION 0x21

/* ETSI HCI APDU gate Registry identifiers */
#define NFA_HCI_MAX_C_APDU_SIZE_INDEX       0x01
#define NFA_HCI_MAX_WAIT_TIME_INDEX         0x02

/* ETSI HCI APDU app gate events */
#define NFA_HCI_EVT_R_APDU                  0x10
#define NFA_HCI_EVT_WTX                     0x11
#define NFA_HCI_EVT_ATR                     0x12

#define NFA_HCI_EVT_ATR_TIMEOUT             0x64 /* 100 msec Max EVT_ATR respond time */

/*Host controller Admin gate Rel 12 */
#define NFA_HCI_HOST_ID_INDEX  0x05
#define NFA_HCI_HOST_TYPE_INDEX 0x06
#define NFA_HCI_HOST_TYPE_LIST_INDEX 0x07

/*HOST_TYPE registry value coding types*/
#define NFA_HCI_HOST_CONTROLLER_TYPE 0x0000
#define NFA_HCI_DH_HOST_TYPE 0x0100
#define NFA_HCI_UICC_HOST_TYPE 0x0200
#define NFA_HCI_ESE_HOST_TYPE 0x0300
/* Host controller and DH Link management gate registry identifier */
#define NFA_HCI_REC_ERROR_INDEX 0x02
#endif


/* DH Identity management gate registry identifier */
#define NFA_HCI_VERSION_SW_INDEX 0x01
#define NFA_HCI_VERSION_HW_INDEX 0x03
#define NFA_HCI_VENDOR_NAME_INDEX 0x04
#define NFA_HCI_MODEL_ID_INDEX 0x05
#define NFA_HCI_HCI_VERSION_INDEX 0x02
#define NFA_HCI_GATES_LIST_INDEX 0x06
#if(NXP_EXTNS == TRUE)
#define NFA_HCI_MAX_CURRENT_INDEX 0x08
#define NFA_HCI_EVT_UNKNOWN 0xFF
#endif
#endif /* NFA_HCI_DEFS_H */
