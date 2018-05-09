/******************************************************************************
 *
 *  Copyright (C) 2010-2014 Broadcom Corporation
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
 *  Copyright (C) 2015-2018 NXP Semiconductors
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

/******************************************************************************
 *
 *  This is the private interface file for the NFA HCI.
 *
 ******************************************************************************/
#ifndef NFA_HCI_INT_H
#define NFA_HCI_INT_H

#include <string>
#include "nfa_hci_api.h"
#include "nfa_sys.h"
#include "nfa_ee_api.h"

extern uint8_t HCI_LOOPBACK_DEBUG;

/* NFA HCI DEBUG states */
#define NFA_HCI_DEBUG_ON 0x01
#define NFA_HCI_DEBUG_OFF 0x00

/*****************************************************************************
**  Constants and data types
*****************************************************************************/

#define NFA_HCI_HOST_ID_UICC0 0x02 /* Host ID for UICC 0 */

#define NFA_HCI_HOST_ID_UICC0_NCI2 0x80 /* Host ID for UICC 0 */
/* Lost host specific gate */
#define NFA_HCI_LAST_HOST_SPECIFIC_GATE 0xEF

#define NFA_HCI_SESSION_ID_LEN 8 /* HCI Session ID length */

#if (NXP_EXTNS == TRUE)
/* HCI Host Type length */
#define NFA_HCI_HOST_TYPE_LEN 2
/* HCI controller ETSI Version 9 */
#define NFA_HCI_CONTROLLER_VERSION_9 9
/* HCI controller ETSI Version 12 */
#define NFA_HCI_CONTROLLER_VERSION_12 12
/* Host ID for UICC 1 */
#define NFA_HCI_HOST_ID_UICC1 0x81
/* Host ID for ESE    */
#define NFA_HCI_HOST_ID_ESE 0xC0
/* Bit Mask to indicate session ID poll is done or not*/
#define NFA_HCI_SESSION_ID_MASK 0x0100
/* Bit mask to indicate ETSI12 config done or not*/
#define NFA_HCI_NFCEE_CONFIG_MASK 0x0200
/* event to set the nfcee config bit*/
#define NFA_HCI_SET_CONFIG_EVENT 0x01
/* event to clear the nfcee config bit*/
#define NFA_HCI_CLEAR_CONFIG_EVENT 0x02
#endif

/* HCI SW Version number                       */
#define NFA_HCI_VERSION_SW 0x090000
/* HCI HW Version number                       */
#define NFA_HCI_VERSION_HW 0x000000
#define NFA_HCI_VENDOR_NAME \
  "HCI" /* Vendor Name                                 */
/* Model ID                                    */
#define NFA_HCI_MODEL_ID 00
/* HCI Version                                 */
#define NFA_HCI_VERSION 90

/* NFA HCI states */
/* HCI is disabled  */
#define NFA_HCI_STATE_DISABLED 0x00
/* HCI performing Initialization sequence */
#define NFA_HCI_STATE_STARTUP 0x01
/* HCI is waiting for initialization of other host in the network */
#define NFA_HCI_STATE_WAIT_NETWK_ENABLE 0x02
/* HCI is waiting to handle api commands  */
#define NFA_HCI_STATE_IDLE 0x03
/* HCI is waiting for response to command sent */
#define NFA_HCI_STATE_WAIT_RSP 0x04
/* Removing all pipes prior to removing the gate */
#define NFA_HCI_STATE_REMOVE_GATE 0x05
/* Removing all pipes and gates prior to deregistering the app */
#define NFA_HCI_STATE_APP_DEREGISTER 0x06
#define NFA_HCI_STATE_RESTORE 0x07 /* HCI restore */
/* HCI is waiting for initialization of other host in the network after restore
 */
#define NFA_HCI_STATE_RESTORE_NETWK_ENABLE 0x08
#if (NXP_EXTNS == TRUE)
/* HCI is waiting for NFCEE initialization */
#define NFA_HCI_STATE_NFCEE_ENABLE 0x09
#define NFA_HCI_STATE_EE_RECOVERY 0x0A
#endif

#if (NXP_EXTNS == TRUE)
#define NFA_HCI_MAX_RSP_WAIT_TIME 0x0C
/* After the reception of WTX, maximum response timeout value is 30 sec */
#define NFA_HCI_CHAIN_PKT_RSP_TIMEOUT 30000
/* Wait time to give response timeout to application if WTX not received*/
#define NFA_HCI_WTX_RESP_TIMEOUT 3000
/* time out for wired mode response after RF deativation */
#define NFA_HCI_DWP_RSP_WAIT_TIMEOUT 2000
/* time out for wired session aborted(0xE6 ntf) due to SWP switched to UICC*/
#define NFA_HCI_DWP_SESSION_ABORT_TIMEOUT 5000
/* delay between session ID poll to check if the reset host is initilized or not
 */
#define NFA_HCI_SESSION_ID_POLL_DELAY 50
/* retry count for session ID poll*/
#define NFA_HCI_MAX_SESSION_ID_RETRY_CNT 0x0A
/* NFCEE disc timeout default value in sec*/
#define NFA_HCI_NFCEE_DISC_TIMEOUT 0x02
/*  NXP specific events */
/* Event to read the number of NFCEE configured in NFCC*/
#define NFA_HCI_GET_NUM_NFCEE_CONFIGURED 0xF1
/* Event to read the session ID of all the Secure Element*/
#define NFA_HCI_READ_SESSIONID 0xF2
/* Event to start ETSI 12 configuration*/
#define NFA_HCI_INIT_NFCEE_CONFIG 0xF3
/* NFCEE ETSI 12 configuration complete*/
#define NFA_HCI_NFCEE_CONFIG_COMPLETE 0xF9
#endif

typedef uint8_t tNFA_HCI_STATE;

/* NFA HCI PIPE states */
#define NFA_HCI_PIPE_CLOSED 0x00 /* Pipe is closed */
#define NFA_HCI_PIPE_OPENED 0x01 /* Pipe is opened */

typedef uint8_t tNFA_HCI_COMMAND;
typedef uint8_t tNFA_HCI_RESPONSE;

/* NFA HCI Internal events */
enum {
  NFA_HCI_API_REGISTER_APP_EVT =
      NFA_SYS_EVT_START(NFA_ID_HCI), /* Register APP with HCI */
  NFA_HCI_API_DEREGISTER_APP_EVT,    /* Deregister an app from HCI */
  NFA_HCI_API_GET_APP_GATE_PIPE_EVT, /* Get the list of gate and pipe associated
                                        to the application */
  NFA_HCI_API_ALLOC_GATE_EVT, /* Allocate a dyanmic gate for the application */
  NFA_HCI_API_DEALLOC_GATE_EVT, /* Deallocate a previously allocated gate to the
                                   application */
  NFA_HCI_API_GET_HOST_LIST_EVT,   /* Get the list of Host in the network */
  NFA_HCI_API_GET_REGISTRY_EVT,    /* Get a registry entry from a host */
  NFA_HCI_API_SET_REGISTRY_EVT,    /* Set a registry entry on a host */
  NFA_HCI_API_CREATE_PIPE_EVT,     /* Create a pipe between two gates */
  NFA_HCI_API_OPEN_PIPE_EVT,       /* Open a pipe */
  NFA_HCI_API_CLOSE_PIPE_EVT,      /* Close a pipe */
  NFA_HCI_API_DELETE_PIPE_EVT,     /* Delete a pipe */
  NFA_HCI_API_ADD_STATIC_PIPE_EVT, /* Add a static pipe */
  NFA_HCI_API_SEND_CMD_EVT,        /* Send command via pipe */
  NFA_HCI_API_SEND_RSP_EVT,        /* Application Response to a command */
#if (NXP_EXTNS == TRUE)
  NFA_HCI_API_CONFIGURE_EVT, /* Configure NFCEE as per ETSI 12 standards */
#endif
  NFA_HCI_API_SEND_EVENT_EVT, /* Send event via pipe */

  NFA_HCI_RSP_NV_READ_EVT,  /* Non volatile read complete event */
  NFA_HCI_RSP_NV_WRITE_EVT, /* Non volatile write complete event */
  NFA_HCI_RSP_TIMEOUT_EVT,  /* Timeout to response for the HCP Command packet */
  NFA_HCI_CHECK_QUEUE_EVT
#if (NXP_EXTNS == TRUE)
  ,
  NFA_HCI_SESSION_ID_POLL_DELAY_TIMEOUT_EVT /*timeout event to read session id
                                               on timeout*/
  ,
  NFA_HCI_NFCEE_DISCOVER_TIMEOUT_EVT /*timeout event for waiting for all
                                        configured nfcee to be discovered*/
#endif
};

#define NFA_HCI_FIRST_API_EVENT NFA_HCI_API_REGISTER_APP_EVT
#define NFA_HCI_LAST_API_EVENT NFA_HCI_API_SEND_EVENT_EVT

/* Internal event structures.
**
** Note, every internal structure starts with a NFC_HDR and an app handle
*/

/* data type for NFA_HCI_API_REGISTER_APP_EVT */
typedef struct {
  NFC_HDR hdr;
  tNFA_HANDLE hci_handle;
  char app_name[NFA_MAX_HCI_APP_NAME_LEN + 1];
  tNFA_HCI_CBACK* p_cback;
  bool b_send_conn_evts;
} tNFA_HCI_API_REGISTER_APP;

/* data type for NFA_HCI_API_DEREGISTER_APP_EVT */
typedef struct {
  NFC_HDR hdr;
  tNFA_HANDLE hci_handle;
  char app_name[NFA_MAX_HCI_APP_NAME_LEN + 1];
} tNFA_HCI_API_DEREGISTER_APP;

/* data type for NFA_HCI_API_GET_APP_GATE_PIPE_EVT */
typedef struct {
  NFC_HDR hdr;
  tNFA_HANDLE hci_handle;
} tNFA_HCI_API_GET_APP_GATE_PIPE;

/* data type for NFA_HCI_API_ALLOC_GATE_EVT */
typedef struct {
  NFC_HDR hdr;
  tNFA_HANDLE hci_handle;
  uint8_t gate;
} tNFA_HCI_API_ALLOC_GATE;

/* data type for NFA_HCI_API_DEALLOC_GATE_EVT */
typedef struct {
  NFC_HDR hdr;
  tNFA_HANDLE hci_handle;
  uint8_t gate;
} tNFA_HCI_API_DEALLOC_GATE;

/* data type for NFA_HCI_API_GET_HOST_LIST_EVT */
typedef struct {
  NFC_HDR hdr;
  tNFA_HANDLE hci_handle;
  tNFA_STATUS status;
} tNFA_HCI_API_GET_HOST_LIST;

/* data type for NFA_HCI_API_GET_REGISTRY_EVT */
typedef struct {
  NFC_HDR hdr;
  tNFA_HANDLE hci_handle;
  uint8_t pipe;
  uint8_t reg_inx;
} tNFA_HCI_API_GET_REGISTRY;

/* data type for NFA_HCI_API_SET_REGISTRY_EVT */
typedef struct {
  NFC_HDR hdr;
  tNFA_HANDLE hci_handle;
  uint8_t pipe;
  uint8_t reg_inx;
  uint8_t size;
  uint8_t data[NFA_MAX_HCI_CMD_LEN];
} tNFA_HCI_API_SET_REGISTRY;

/* data type for NFA_HCI_API_CREATE_PIPE_EVT */
typedef struct {
  NFC_HDR hdr;
  tNFA_HANDLE hci_handle;
  tNFA_STATUS status;
  uint8_t source_gate;
  uint8_t dest_host;
  uint8_t dest_gate;
} tNFA_HCI_API_CREATE_PIPE_EVT;

/* data type for NFA_HCI_API_OPEN_PIPE_EVT */
typedef struct {
  NFC_HDR hdr;
  tNFA_HANDLE hci_handle;
  tNFA_STATUS status;
  uint8_t pipe;
} tNFA_HCI_API_OPEN_PIPE_EVT;

/* data type for NFA_HCI_API_CLOSE_PIPE_EVT */
typedef struct {
  NFC_HDR hdr;
  tNFA_HANDLE hci_handle;
  tNFA_STATUS status;
  uint8_t pipe;
} tNFA_HCI_API_CLOSE_PIPE_EVT;

/* data type for NFA_HCI_API_DELETE_PIPE_EVT */
typedef struct {
  NFC_HDR hdr;
  tNFA_HANDLE hci_handle;
  tNFA_STATUS status;
  uint8_t pipe;
} tNFA_HCI_API_DELETE_PIPE_EVT;

/* data type for NFA_HCI_API_ADD_STATIC_PIPE_EVT */
typedef struct {
  NFC_HDR hdr;
  tNFA_HANDLE hci_handle;
  tNFA_STATUS status;
  uint8_t host;
  uint8_t gate;
  uint8_t pipe;
} tNFA_HCI_API_ADD_STATIC_PIPE_EVT;

/* data type for NFA_HCI_API_SEND_EVENT_EVT */
typedef struct {
  NFC_HDR hdr;
  tNFA_HANDLE hci_handle;
  uint8_t pipe;
  uint8_t evt_code;
  uint16_t evt_len;
  uint8_t* p_evt_buf;
  uint16_t rsp_len;
  uint8_t* p_rsp_buf;
#if (NXP_EXTNS == TRUE)
  uint32_t rsp_timeout;
#else
  uint16_t rsp_timeout;
#endif
} tNFA_HCI_API_SEND_EVENT_EVT;

#if (NXP_EXTNS == TRUE)
/* data type for tNFA_HCI_API_CONFIGURE_EVT */
typedef struct {
  NFC_HDR hdr;
  uint16_t config_nfcee_event;
} tNFA_HCI_API_CONFIGURE_EVT;

typedef uint16_t tNFA_CONFIG_STATE;
#endif

/* data type for NFA_HCI_API_SEND_CMD_EVT */
typedef struct {
  NFC_HDR hdr;
  tNFA_HANDLE hci_handle;
  uint8_t pipe;
  uint8_t cmd_code;
  uint16_t cmd_len;
  uint8_t data[NFA_MAX_HCI_CMD_LEN];
} tNFA_HCI_API_SEND_CMD_EVT;

/* data type for NFA_HCI_RSP_NV_READ_EVT */
typedef struct {
  NFC_HDR hdr;
  uint8_t block;
  uint16_t size;
  tNFA_STATUS status;
} tNFA_HCI_RSP_NV_READ_EVT;

/* data type for NFA_HCI_RSP_NV_WRITE_EVT */
typedef struct {
  NFC_HDR hdr;
  tNFA_STATUS status;
} tNFA_HCI_RSP_NV_WRITE_EVT;

/* data type for NFA_HCI_API_SEND_RSP_EVT */
typedef struct {
  NFC_HDR hdr;
  tNFA_HANDLE hci_handle;
  uint8_t pipe;
  uint8_t response;
  uint8_t size;
  uint8_t data[NFA_MAX_HCI_RSP_LEN];
} tNFA_HCI_API_SEND_RSP_EVT;

/* common data type for internal events */
typedef struct {
  NFC_HDR hdr;
  tNFA_HANDLE hci_handle;
} tNFA_HCI_COMM_DATA;

/* union of all event data types */
typedef union {
  NFC_HDR hdr;
  tNFA_HCI_COMM_DATA comm;

  /* API events */
  tNFA_HCI_API_REGISTER_APP app_info; /* Register/Deregister an application */
  tNFA_HCI_API_GET_APP_GATE_PIPE get_gate_pipe_list; /* Get the list of gates
                                                        and pipes created for
                                                        the application */
  tNFA_HCI_API_ALLOC_GATE
      gate_info; /* Allocate a dynamic gate to the application */
  tNFA_HCI_API_DEALLOC_GATE
      gate_dealloc; /* Deallocate the gate allocated to the application */
  tNFA_HCI_API_CREATE_PIPE_EVT create_pipe;         /* Create a pipe */
  tNFA_HCI_API_OPEN_PIPE_EVT open_pipe;             /* Open a pipe */
  tNFA_HCI_API_CLOSE_PIPE_EVT close_pipe;           /* Close a pipe */
  tNFA_HCI_API_DELETE_PIPE_EVT delete_pipe;         /* Delete a pipe */
  tNFA_HCI_API_ADD_STATIC_PIPE_EVT add_static_pipe; /* Add a static pipe */
  tNFA_HCI_API_GET_HOST_LIST
      get_host_list; /* Get the list of Host in the network */
  tNFA_HCI_API_GET_REGISTRY get_registry; /* Get a registry entry on a host */
  tNFA_HCI_API_SET_REGISTRY set_registry; /* Set a registry entry on a host */
  tNFA_HCI_API_SEND_CMD_EVT send_cmd;     /* Send a event on a pipe to a host */
  tNFA_HCI_API_SEND_RSP_EVT
      send_rsp; /* Response to a command sent on a pipe to a host */
  tNFA_HCI_API_SEND_EVENT_EVT send_evt; /* Send a command on a pipe to a host */

  /* Internal events */
  tNFA_HCI_RSP_NV_READ_EVT nv_read;   /* Read Non volatile data */
  tNFA_HCI_RSP_NV_WRITE_EVT nv_write; /* Write Non volatile data */

#if (NXP_EXTNS == TRUE)
  tNFA_HCI_API_CONFIGURE_EVT
      config_info; /* Configuration of NFCEE for ETSI12 */
#endif
} tNFA_HCI_EVENT_DATA;

/*****************************************************************************
**  control block
*****************************************************************************/

/* Dynamic pipe control block */
typedef struct {
  uint8_t pipe_id;                /* Pipe ID */
  tNFA_HCI_PIPE_STATE pipe_state; /* State of the Pipe */
  uint8_t local_gate;             /* local gate id */
  uint8_t dest_host; /* Peer host to which this pipe is connected */
  uint8_t dest_gate; /* Peer gate to which this pipe is connected */
} tNFA_HCI_DYN_PIPE;

/* Dynamic gate control block */
typedef struct {
  uint8_t gate_id;        /* local gate id */
  tNFA_HANDLE gate_owner; /* NFA-HCI handle assigned to the application which
                             owns the gate */
  uint32_t pipe_inx_mask; /* Bit 0 == pipe inx 0, etc */
} tNFA_HCI_DYN_GATE;

/* Admin gate control block */
typedef struct {
  tNFA_HCI_PIPE_STATE pipe01_state; /* State of Pipe '01' */
  uint8_t
      session_id[NFA_HCI_SESSION_ID_LEN]; /* Session ID of the host network */
} tNFA_ADMIN_GATE_INFO;

/* Link management gate control block */
typedef struct {
  tNFA_HCI_PIPE_STATE pipe00_state; /* State of Pipe '00' */
  uint16_t rec_errors;              /* Receive errors */
} tNFA_LINK_MGMT_GATE_INFO;

/* Identity management gate control block */
typedef struct {
  uint32_t pipe_inx_mask;  /* Bit 0 == pipe inx 0, etc */
  uint16_t version_sw;     /* Software version number */
  uint16_t version_hw;     /* Hardware version number */
  uint8_t vendor_name[20]; /* Vendor name */
  uint8_t model_id;        /* Model ID */
  uint8_t hci_version;     /* HCI Version */
} tNFA_ID_MGMT_GATE_INFO;

#if (NXP_EXTNS == TRUE)
#define NFA_HCI_FL_CONN_PIPE 0x01
#define NFA_HCI_FL_APDU_PIPE 0x02
#define NFA_HCI_FL_OTHER_PIPE 0x04
#define NFA_HCI_CONN_UICC_PIPE 0x0A
#define NFA_HCI_CONN_ESE_PIPE 0x16
#define NFA_HCI_APDU_PIPE 0x19
#define NFA_HCI_CONN_UICC2_PIPE 0x23 /*Connectivity pipe no of UICC2*/
#define NFA_HCI_INIT_MAX_RETRY 20
#endif
/* NFA HCI control block */
typedef struct {
  tNFA_HCI_STATE hci_state;   /* state of the HCI */
  uint8_t num_nfcee;          /* Number of NFCEE ID Discovered */
  tNFA_EE_INFO ee_info[NFA_HCI_MAX_HOST_IN_NETWORK];    /*NFCEE ID Info*/
  uint8_t num_ee_dis_req_ntf; /* Number of ee discovery request ntf received */
  uint8_t num_hot_plug_evts;  /* Number of Hot plug events received after ee
                                 discovery disable ntf */
  uint8_t inactive_host[NFA_HCI_MAX_HOST_IN_NETWORK]; /* Inactive host in the
                                                         host network */
  uint8_t reset_host[NFA_HCI_MAX_HOST_IN_NETWORK]; /* List of host reseting */
  bool b_low_power_mode;  /* Host controller in low power mode */
  bool b_hci_netwk_reset; /* Command sent to reset HCI Network */
  bool w4_hci_netwk_init; /* Wait for other host in network to initialize */
  TIMER_LIST_ENT timer;   /* Timer to avoid indefinitely waiting for response */
  uint8_t conn_id;        /* Connection ID */
  uint8_t buff_size;      /* Connection buffer size */
  bool nv_read_cmplt;     /* NV Read completed */
  bool nv_write_needed;   /* Something changed - NV write is needed */
  bool assembling;        /* Set true if in process of assembling a message  */
  bool assembly_failed;   /* Set true if Insufficient buffer to Reassemble
                             incoming message */
  bool w4_rsp_evt;        /* Application command sent on HCP Event */
  tNFA_HANDLE
      app_in_use; /* Index of the application that is waiting for response */
  uint8_t local_gate_in_use;  /* Local gate currently working with */
  uint8_t remote_gate_in_use; /* Remote gate currently working with */
  uint8_t remote_host_in_use; /* The remote host to which a command is sent */
  uint8_t pipe_in_use;        /* The pipe currently working with */
  uint8_t param_in_use;      /* The registry parameter currently working with */
  tNFA_HCI_COMMAND cmd_sent; /* The last command sent */
  bool ee_disc_cmplt;        /* EE Discovery operation completed */
  bool ee_disable_disc;      /* EE Discovery operation is disabled */
  uint16_t msg_len;     /* For segmentation - length of the combined message */
  uint16_t max_msg_len; /* Maximum reassembled message size */
  uint8_t msg_data[NFA_MAX_HCI_EVENT_LEN]; /* For segmentation - the combined
                                              message data */
  uint8_t* p_msg_data; /* For segmentation - reassembled message */
#if (NXP_EXTNS == TRUE)
  uint8_t assembling_flags; /* the flags to keep track of assembling status*/
  uint8_t assembly_failed_flags; /* the flags to keep track of failed assembly*/
  uint8_t* p_evt_data;           /* For segmentation - reassembled event data */
  uint16_t evt_len; /* For segmentation - length of the combined event data */
  uint16_t max_evt_len; /* Maximum reassembled message size */
  uint8_t evt_data[NFA_MAX_HCI_EVENT_LEN]; /* For segmentation - the combined
                                              event data */
  uint8_t type_evt;   /* Instruction type of incoming message */
  uint8_t inst_evt;   /* Instruction of incoming message */
  uint8_t type_msg;   /* Instruction type of incoming message */
  uint8_t inst_msg;   /* Instruction of incoming message */
  uint8_t host_count; /* no of Hosts ETSI 12 compliant */
  uint8_t host_id[NFA_HCI_MAX_NO_HOST_ETSI12]; /* Host id ETSI 12 compliant */
  uint8_t host_controller_version; /* no of host controller version */
  uint8_t current_nfcee;           /* current Nfcee under execution  */
  bool IsHciTimerChanged;
  bool IsWiredSessionAborted;
  uint32_t hciResponseTimeout;
  bool IsChainedPacket;
  bool bIsHciResponseTimedout;
  uint16_t hci_packet_len;
  bool IsEventAbortSent;
  bool IsLastEvtAbortFailed;
  bool w4_nfcee_enable;
  bool IsApduPipeStatusNotCorrect;
  tNFA_HCI_EVENT_SENT evt_sent;
  struct {
    tNFA_CONFIG_STATE
        config_nfcee_state;   /*state change during config nfcee handling*/
    uint8_t session_id_retry; /*retry count for session ID*/
    tNFA_HANDLE host_cb[NFA_HCI_MAX_HOST_IN_NETWORK]; /*host_cb which stores
                                                         information on config
                                                         complete ,session ID
                                                         poll and other
                                                         information*/
    bool discovery_stopped; /*discovery stopped during config nfcee process*/
    bool nfc_init_state;    /*handling clear all pipe*/
  } nfcee_cfg;

  uint32_t max_hci_session_id_read_count; /*Count for maximum  session id retry
                                             value */
  uint32_t
      max_nfcee_disc_timeout; /*Config file timeout value for all the NFCEE to
                                 be discovered */
  tNFA_EE_INFO hci_ee_info[NFA_HCI_MAX_HOST_IN_NETWORK];
#endif
  uint8_t type; /* Instruction type of incoming message */
  uint8_t inst; /* Instruction of incoming message */

  BUFFER_Q hci_api_q;            /* Buffer Q to hold incoming API commands */
  BUFFER_Q hci_host_reset_api_q; /* Buffer Q to hold incoming API commands to a
                                    host that is reactivating */
  tNFA_HCI_CBACK* p_app_cback[NFA_HCI_MAX_APP_CB]; /* Callback functions
                                                      registered by the
                                                      applications */
  uint16_t rsp_buf_size; /* Maximum size of APDU buffer */
  uint8_t* p_rsp_buf;    /* Buffer to hold response to sent event */
  struct                 /* Persistent information for Device Host */
      {
    char reg_app_names[NFA_HCI_MAX_APP_CB][NFA_MAX_HCI_APP_NAME_LEN + 1];

    tNFA_HCI_DYN_GATE dyn_gates[NFA_HCI_MAX_GATE_CB];
    tNFA_HCI_DYN_PIPE dyn_pipes[NFA_HCI_MAX_PIPE_CB];

    bool b_send_conn_evts[NFA_HCI_MAX_APP_CB];
    tNFA_ADMIN_GATE_INFO admin_gate;
    tNFA_LINK_MGMT_GATE_INFO link_mgmt_gate;
    tNFA_ID_MGMT_GATE_INFO id_mgmt_gate;
#if (NXP_EXTNS == TRUE)
    uint8_t retry_cnt;
#endif
  } cfg;

} tNFA_HCI_CB;

/*****************************************************************************
**  External variables
*****************************************************************************/

/* NFA HCI control block */
extern tNFA_HCI_CB nfa_hci_cb;

/*****************************************************************************
**  External functions
*****************************************************************************/

/* Functions in nfa_hci_main.c
*/
extern void nfa_hci_init(void);
extern void nfa_hci_proc_nfcc_power_mode(uint8_t nfcc_power_mode);
extern void nfa_hci_dh_startup_complete(void);
extern void nfa_hci_startup_complete(tNFA_STATUS status);
extern void nfa_hci_startup(void);
extern void nfa_hci_restore_default_config(uint8_t* p_session_id);
extern void nfa_hci_enable_one_nfcee(void);
#if (NXP_EXTNS == TRUE)
extern void nfa_hci_release_transcieve();
extern void nfa_hci_network_enable(void);
extern tNFA_STATUS nfa_hciu_reset_session_id(tNFA_VSC_CBACK* p_cback);
extern tNFA_STATUS nfa_hciu_send_raw_cmd(uint8_t param_len, uint8_t* p_data,
                                         tNFA_VSC_CBACK* p_cback);
extern bool nfa_hciu_check_nfcee_poll_done(uint8_t host_id);
extern bool nfa_hciu_check_nfcee_config_done(uint8_t host_id);
extern void nfa_hciu_set_nfceeid_config_mask(uint8_t event, uint8_t host_id);
extern void nfa_hciu_set_nfceeid_poll_mask(uint8_t event, uint8_t host_id);
extern bool nfa_hciu_check_any_host_reset_pending();
extern tNFA_STATUS nfa_hci_api_config_nfcee(uint8_t hostId);
extern tNFA_STATUS nfa_hci_getApduAndConnectivity_PipeStatus();
void nfa_hci_handle_clear_all_pipes_evt(uint8_t source_host);
#endif

/* Action functions in nfa_hci_act.c
*/
extern void nfa_hci_check_pending_api_requests(void);
extern void nfa_hci_check_api_requests(void);
extern void nfa_hci_handle_admin_gate_cmd(uint8_t* p_data);
extern void nfa_hci_handle_admin_gate_rsp(uint8_t* p_data, uint8_t data_len);
extern void nfa_hci_handle_admin_gate_evt();
extern void nfa_hci_handle_link_mgm_gate_cmd(uint8_t* p_data);
extern void nfa_hci_handle_dyn_pipe_pkt(uint8_t pipe, uint8_t* p_data,
                                        uint16_t data_len);
extern void nfa_hci_handle_pipe_open_close_cmd(tNFA_HCI_DYN_PIPE* p_pipe);
extern void nfa_hci_api_dealloc_gate(tNFA_HCI_EVENT_DATA* p_evt_data);
extern void nfa_hci_api_deregister(tNFA_HCI_EVENT_DATA* p_evt_data);

/* Utility functions in nfa_hci_utils.c
*/
extern tNFA_HCI_DYN_GATE* nfa_hciu_alloc_gate(uint8_t gate_id,
                                              tNFA_HANDLE app_handle);
extern tNFA_HCI_DYN_GATE* nfa_hciu_find_gate_by_gid(uint8_t gate_id);
extern tNFA_HCI_DYN_GATE* nfa_hciu_find_gate_by_owner(tNFA_HANDLE app_handle);
extern tNFA_HCI_DYN_GATE* nfa_hciu_find_gate_with_nopipes_by_owner(
    tNFA_HANDLE app_handle);
extern tNFA_HCI_DYN_PIPE* nfa_hciu_find_pipe_by_pid(uint8_t pipe_id);
extern tNFA_HCI_DYN_PIPE* nfa_hciu_find_pipe_by_owner(tNFA_HANDLE app_handle);
extern tNFA_HCI_DYN_PIPE* nfa_hciu_find_active_pipe_by_owner(
    tNFA_HANDLE app_handle);
extern tNFA_HCI_DYN_PIPE* nfa_hciu_find_pipe_on_gate(uint8_t gate_id);
extern tNFA_HANDLE nfa_hciu_get_gate_owner(uint8_t gate_id);
extern bool nfa_hciu_check_pipe_between_gates(uint8_t local_gate,
                                              uint8_t dest_host,
                                              uint8_t dest_gate);
extern bool nfa_hciu_is_active_host(uint8_t host_id);
extern bool nfa_hciu_is_host_reseting(uint8_t host_id);
extern bool nfa_hciu_is_no_host_resetting(void);
extern tNFA_HCI_DYN_PIPE* nfa_hciu_find_active_pipe_on_gate(uint8_t gate_id);
extern tNFA_HANDLE nfa_hciu_get_pipe_owner(uint8_t pipe_id);
extern uint8_t nfa_hciu_count_open_pipes_on_gate(tNFA_HCI_DYN_GATE* p_gate);
extern uint8_t nfa_hciu_count_pipes_on_gate(tNFA_HCI_DYN_GATE* p_gate);

extern tNFA_HCI_RESPONSE nfa_hciu_add_pipe_to_gate(uint8_t pipe,
                                                   uint8_t local_gate,
                                                   uint8_t dest_host,
                                                   uint8_t dest_gate);
extern tNFA_HCI_RESPONSE nfa_hciu_add_pipe_to_static_gate(uint8_t local_gate,
                                                          uint8_t pipe_id,
                                                          uint8_t dest_host,
                                                          uint8_t dest_gate);

extern tNFA_HCI_RESPONSE nfa_hciu_release_pipe(uint8_t pipe_id);
extern void nfa_hciu_release_gate(uint8_t gate);
extern void nfa_hciu_remove_all_pipes_from_host(uint8_t host);
extern uint8_t nfa_hciu_get_allocated_gate_list(uint8_t* p_gate_list);

extern void nfa_hciu_send_to_app(tNFA_HCI_EVT event, tNFA_HCI_EVT_DATA* p_evt,
                                 tNFA_HANDLE app_handle);
extern void nfa_hciu_send_to_all_apps(tNFA_HCI_EVT event,
                                      tNFA_HCI_EVT_DATA* p_evt);
extern void nfa_hciu_send_to_apps_handling_connectivity_evts(
    tNFA_HCI_EVT event, tNFA_HCI_EVT_DATA* p_evt);

extern tNFA_STATUS nfa_hciu_send_close_pipe_cmd(uint8_t pipe);
extern tNFA_STATUS nfa_hciu_send_delete_pipe_cmd(uint8_t pipe);
extern tNFA_STATUS nfa_hciu_send_clear_all_pipe_cmd(void);
extern tNFA_STATUS nfa_hciu_send_open_pipe_cmd(uint8_t pipe);
extern tNFA_STATUS nfa_hciu_send_get_param_cmd(uint8_t pipe, uint8_t index);
extern tNFA_STATUS nfa_hciu_send_create_pipe_cmd(uint8_t source_gate,
                                                 uint8_t dest_host,
                                                 uint8_t dest_gate);
extern tNFA_STATUS nfa_hciu_send_set_param_cmd(uint8_t pipe, uint8_t index,
                                               uint8_t length, uint8_t* p_data);
extern tNFA_STATUS nfa_hciu_send_msg(uint8_t pipe_id, uint8_t type,
                                     uint8_t instruction, uint16_t pkt_len,
                                     uint8_t* p_pkt);

extern std::string nfa_hciu_instr_2_str(uint8_t type);
extern std::string nfa_hciu_get_event_name(uint16_t event);
extern std::string nfa_hciu_get_state_name(uint8_t state);
extern char* nfa_hciu_get_type_inst_names(uint8_t pipe, uint8_t type,
                                          uint8_t inst, char* p_buff);
extern std::string nfa_hciu_evt_2_str(uint8_t pipe_id, uint8_t evt);

#endif /* NFA_HCI_INT_H */
