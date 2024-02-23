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
 *  This is the private interface file for NFA Secure Reader
 *
 ******************************************************************************/
#ifndef NFA_SCR_INT_H
#define NFA_SCR_INT_H

#include <string>
#include "nfa_sys.h"
#include "nfc_api.h"
#include "nfa_ee_api.h"

/*******************************************************************************
**  Constant
*******************************************************************************/
#define NFC_NUM_INTERFACE_MAP 3
#define NFC_SWP_RD_NUM_INTERFACE_MAP 1

#define IS_SCR_APP_REQESTED (nfa_scr_cb.state == NFA_SCR_STATE_START_CONFIG)

#define IS_SCR_START_IN_PROGRESS (nfa_scr_cb.state == NFA_SCR_STATE_START_IN_PROGRESS)

#define IS_SCR_START_SUCCESS (nfa_scr_cb.state == NFA_SCR_STATE_START_SUCCESS)

#define IS_SCR_STOP_IN_PROGRESS                                                \
                           ((nfa_scr_cb.state == NFA_SCR_STATE_STOP_CONFIG) || \
                           (nfa_scr_cb.state == NFA_SCR_STATE_STOP_IN_PROGRESS))

#define IS_SCR_STOP_SUCCESS (nfa_scr_cb.state == NFA_SCR_STATE_STOP_SUCCESS)

#define IS_SCR_ON (nfa_scr_cb.state != NFA_SCR_STATE_STOPPED)

#define IS_STATUS_ERROR(status)                                          \
  {                                                                      \
    if (NFA_STATUS_OK != status) {                                       \
      return nfa_scr_error_handler(NFA_SCR_ERROR_NCI_RSP_STATUS_FAILED); \
    }                                                                    \
  }

#if (NFC_NFCEE_INCLUDED == TRUE)
#define NFA_SCR_IS_ENABLED(status) {               \
  if(nullptr != nfa_scr_cb.scr_evt_cback) {        \
     return status;                                \
  }                                                \
}

/* Check if 610A has been received for START/STOP SCR mode */
#define NFA_SCR_HANDLE_RDR_REQ_NTF(data) {         \
  if(nfa_scr_proc_rdr_req_ntf(data)) {             \
     return;                                       \
  }                                                \
}

#define NFA_SCR_PROCESS_EVT(event,status) {        \
  if(nullptr != nfa_scr_cb.scr_evt_cback) {        \
    if(nfa_scr_is_req_evt(event, status)) {        \
      return;                                      \
    }                                              \
  }                                                \
}
/******************************************************/
#else
#define NFA_SCR_IS_ENABLED(status)
#define NFA_SCR_HANDLE_RDR_REQ_NTF(data)
#define NFA_SCR_PROCESS_EVT(event,status)
#endif

/******************************************************************************
**  Data types: union of all event data types
******************************************************************************/
/* Reader types */
enum {
  NFA_SCR_INVALID,
  NFA_SCR_MPOS, /* MPOS */
};

/* NFA SCR Internal events */
enum {
  /* Events for the exposed APIs invocation, to be used by nfa_scr_api.cc file */
  NFA_SCR_API_SET_READER_MODE_EVT =
      NFA_SYS_EVT_START(NFA_ID_SCR), /* Set Mode of the Secure Reader */
  NFA_SCR_TAG_OP_TIMEOUT_EVT,        /* Tag Operation Time out windows */
  NFA_SCR_RM_CARD_TIMEOUT_EVT,       /* Notify REMOVE_CARD_EVT to app and
                                        start timer again for the same    */
  NFA_SCR_ERROR_REC_TIMEOUT_EVT,
  NFA_SCR_RDR_REQ_GUARD_TIMEOUT_EVT,
};

/* NFA SCR (Reader) events requested from other libnfc-nci modules */
enum {
  NFA_SCR_START_REQ_EVT = NFC_EE_DISC_OP_ADD,  /* SCR Start request */
  NFA_SCR_STOP_REQ_EVT,
  NFA_SCR_APP_START_REQ_EVT,
  NFA_SCR_APP_STOP_REQ_EVT,
  NFA_SCR_CORE_SET_CONF_RSP_EVT,
  NFA_SCR_CORE_GET_CONF_RSP_EVT,
  NFA_SCR_RF_DISCOVER_MAP_RSP_EVT,
  NFA_SCR_RF_DISCOVERY_RSP_EVT,
  NFA_SCR_RF_DEACTIVATE_RSP_EVT,
  NFA_SCR_RF_DEACTIVATE_NTF_EVT,
  NFA_SCR_RF_INTF_ACTIVATED_NTF_EVT,
  NFA_SCR_ESE_RECOVERY_START_EVT,
  NFA_SCR_ESE_RECOVERY_COMPLETE_EVT,
  NFA_SCR_MULTIPLE_TARGET_DETECTED_EVT,
  NFA_SCR_UNKOWN_EVT,
};

/* NFA SCR (Reader) events to be posted to JNI */
typedef enum {
  NFA_SCR_INVALID_EVT = 0x00,
  NFA_SCR_SET_READER_MODE_EVT,
  NFA_SCR_GET_READER_MODE_EVT,
  NFA_SCR_START_SUCCESS_EVT,
  NFA_SCR_START_FAIL_EVT,
  NFA_SCR_RESTART_EVT,
  NFA_SCR_TARGET_ACTIVATED_EVT,
  NFA_SCR_STOP_SUCCESS_EVT,
  NFA_SCR_STOP_FAIL_EVT,
  NFA_SCR_TIMEOUT_EVT,
  NFA_SCR_REMOVE_CARD_EVT,
  NFA_SCR_MULTI_TARGET_DETECTED_EVT,
} tNFA_SCR_EVT;

/* NFA SCR possible errors which can occur in SCR module */
typedef enum {
  NFA_SCR_NO_ERROR = 0x00,
  NFA_SCR_ERROR_SEND_CONF_CMD,          /* Failed to Send SET/GET Config NCI command  */
  NFA_SCR_ERROR_START_RF_DISC,          /* Sending RF_DISCOVER_CMD failed     */
  NFA_SCR_ERROR_NCI_RSP_STATUS_FAILED,  /* Received NFCC_RSP with status failed
                                         */
  NFA_SCR_ERROR_RECOVERY_STARTED,       /* eSE recovery is started            */
  NFA_SCR_ERROR_RECOVERY_COMPLETED,     /* eSE recovery completed             */
  NFA_SCR_ERROR_RECOVERY_TIMEOUT, /* Timeout while waiting for eSE Recovery */
  NFA_SCR_ERROR_APP_STOP_REQ /* Stop API is called when applet is in rdr mode */
} tNFA_SCR_ERROR;

/* NFA SCR (Reader)states to be maintained in libnfc-nci */
typedef enum {
  NFA_SCR_STATE_STOPPED = 0x00,    /* received NFA_SCR_API_SET_READER_MODE_EVT with false */
  /* State during Start Reader request */
  NFA_SCR_STATE_START_CONFIG,      /* received NFA_SCR_APP_START_REQ_EVT from application */
  NFA_SCR_STATE_START_IN_PROGRESS, /* received NFA_SCR_SECURE_READER_REQ_EVT with op = 0  */
  NFA_SCR_STATE_START_SUCCESS,     /* received NFA_SCR_RF_DISCOVERY_RSP_EVT               */
  /* State during Stop Reader request */
  NFA_SCR_STATE_STOP_CONFIG,      /* received NFA_SCR_SECURE_READER_REQ_EVT with op = 1   */
  NFA_SCR_STATE_STOP_IN_PROGRESS, /* received NFA_SCR_SUBSTATE_WAIT_DEACTIVATE_RSP        */
  NFA_SCR_STATE_STOP_SUCCESS,     /* received NFA_SCR_STATE_WAIT_DISC_MAP                 */
} tNFA_SCR_STATE;

/* NFA SCR (Reader)states to be maintained in libnfc-nci */
typedef enum {
  NFA_SCR_SUBSTATE_INVALID = 0x00,
  NFA_SCR_SUBSTATE_WAIT_START_RDR_NTF,
  NFA_SCR_SUBSTATE_WAIT_DEACTIVATE_RSP,
  NFA_SCR_SUBSTATE_WAIT_DEACTIVATE_NTF,
  NFA_SCR_SUBSTATE_WAIT_PROP_SET_CONF_RSP,
  NFA_SCR_SUBSTATE_WAIT_PROP_GET_CONF_RSP,
  NFA_SCR_SUBSTATE_WAIT_DISC_MAP_RSP,
  NFA_SCR_SUBSTATE_WAIT_POLL_RSP,
  NFA_SCR_SUBSTATE_WAIT_ACTIVETED_NTF,
} tNFA_SCR_SUB_STATE;


/*******************************************************************************
**  Data Structures and callback functions
********************************************************************************/
typedef bool(tNFA_SCR_EVT_CBACK)(uint8_t event, uint8_t status);
typedef void(tNFA_SCR_CBACK)(uint8_t event, uint8_t status);
/* Data type for the Set Nfc Forum mode API */
typedef struct {
  NFC_HDR hdr;
} tNFA_SCR_API_SET_NFC_FORUM_MODE;
/* Data type for the Set Reader Mode API */
typedef struct {
  NFC_HDR hdr;
  uint8_t type;
  bool mode;
  tNFA_SCR_CBACK* scr_cback;
} tNFA_SCR_API_SET_READER_MODE;

/******************************************************************************
**  NFA SCR control block structure
******************************************************************************/
typedef struct {
  tNFA_SCR_ERROR error;
  TIMER_LIST_ENT scr_tle;
  TIMER_LIST_ENT scr_rec_tle;
  TIMER_LIST_ENT scr_guard_tle; /* shall be started only if NXP_RDR_REQ_GUARD_TIME is non-ZERO */
  tNFA_SCR_CBACK* scr_cback;
  tNFA_SCR_EVT_CBACK* scr_evt_cback;
  tNFA_GET_CONFIG p_nfa_get_confg;
  uint32_t tag_op_timeout;
  int32_t rdr_req_guard_time;
  uint32_t deact_ntf_timeout;
  uint8_t state;
  uint8_t sub_state;
  uint8_t poll_prof_sel_cfg;
  uint8_t last_scr_req;
  bool rdr_stop_req;
  bool app_stop_req;
  bool wait_for_deact_ntf;
  uint8_t is_timer_started:1;   /* shall be used/set only if NXP_RDR_REQ_GUARD_TIME is non-ZERO */
  uint8_t restart_emvco_poll:1; /* shall be used/set only if NXP_RDR_REQ_GUARD_TIME is non-ZERO */
  uint8_t configure_lpcd;
  /**************************************/
} tNFA_SCR_CB;

/******************************************************************************
**  External variables
******************************************************************************/
/* NFA SCR control block */
extern tNFA_SCR_CB nfa_scr_cb;

/******************************************************************************
**  External functions
******************************************************************************/
/* Action & utility functions in nfa_scr_act.cc */
extern void nfa_scr_set_reader_mode(bool mode, tNFA_SCR_CBACK* scr_cback);
extern void nfa_scr_notify_evt(uint8_t event, uint8_t status = NFA_STATUS_OK);

/* Action & utility functions in nfa_scr_main.cc */
extern bool nfa_scr_is_proc_rdr_req(uint8_t event);
extern bool nfa_scr_is_evt_allowed(uint8_t event);
extern bool nfa_scr_proc_app_start_req();
extern bool nfa_scr_start_polling(uint8_t status);
extern bool nfa_scr_rdr_processed(uint8_t status);
extern bool nfa_scr_trigger_stop_seq(void);
extern bool nfa_scr_handle_act_ntf(uint8_t status);
extern bool nfa_scr_evt_hdlr(NFC_HDR* p_msg);
extern bool nfa_scr_finalize(bool notify = true);
extern void nfa_scr_sys_disable(void);
extern bool nfa_scr_proc_rdr_req_ntf(tNFC_EE_DISCOVER_REQ_REVT* p_cbk);
extern bool nfa_scr_is_req_evt(uint8_t event, uint8_t status);
extern bool nfa_scr_error_handler(tNFA_SCR_ERROR error);
extern void nfa_scr_proc_ntf(bool operation);
extern std::string nfa_scr_get_event_name(uint16_t event);
void set_smr_cback(tNFA_SCR_EVT_CBACK* nfa_smr_cback);

#endif /* NFA_SCR_INT_H */
