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

#include <android-base/stringprintf.h>
#include <base/logging.h>
#include <nfa_srd_int.h>
#include "NfcAdaptation.h"
#include "nfa_dm_int.h"
#include "nfa_hci_defs.h"
#include "nfc_api.h"
#include "nfc_config.h"
#if (NXP_EXTNS == TRUE && NXP_SRD == TRUE)
using android::base::StringPrintf;
extern bool nfc_debug_enabled;
ThreadCondVar srdDeactivateEvtCb;
ThreadCondVar srdDiscoverymapEvt;
tNFA_HCI_EVT_DATA p_evtdata;

/*Local static function declarations*/
void nfa_srd_process_hci_evt(tNFA_HCI_EVT event, tNFA_HCI_EVT_DATA* p_data);
void nfa_srd_deactivate_req_evt(tNFC_DISCOVER_EVT event,
                                tNFC_DISCOVER* evt_data);
void nfa_srd_discovermap_cb(tNFC_DISCOVER_EVT event, tNFC_DISCOVER* p_data);

/******************************************************************************
 **  Constants
 *****************************************************************************/
#define NFC_NUM_INTERFACE_MAP 3
#define NFC_SWP_RD_NUM_INTERFACE_MAP 2

/*******************************************************************************
**
** Function         nfa_srd_start
**
** Description      Process to send discover map command with RF interface.
**
** Returns          None
**
*******************************************************************************/
void* nfa_srd_start(void*) {
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf(" nfa_srd_start: Enter");
  tNFA_STATUS status = NFA_STATUS_FAILED;
  tNCI_DISCOVER_MAPS* p_intf_mapping = nullptr;

  const tNCI_DISCOVER_MAPS
      nfc_interface_mapping_mfc[NFC_SWP_RD_NUM_INTERFACE_MAP] = {
          {NCI_PROTOCOL_T2T, NCI_INTERFACE_MODE_POLL, NCI_INTERFACE_FRAME},
          {NCI_PROTOCOL_ISO_DEP, NCI_INTERFACE_MODE_POLL, NCI_INTERFACE_FRAME}};
  p_intf_mapping = (tNCI_DISCOVER_MAPS*)nfc_interface_mapping_mfc;

  if (p_intf_mapping != nullptr &&
      (nfa_dm_cb.disc_cb.disc_state == NFA_DM_RFST_IDLE ||
       nfa_dm_cb.disc_cb.disc_state == NFA_DM_RFST_POLL_ACTIVE)) {
    srdDiscoverymapEvt.lock();
    (void)NFC_DiscoveryMap(NFC_SWP_RD_NUM_INTERFACE_MAP, p_intf_mapping,
                           nfa_srd_discovermap_cb);
    srdDiscoverymapEvt.wait();
    status = srd_t.rsp_status;
  }
  if (status == NFC_STATUS_OK) {
    srd_t.srd_state = ENABLE;
  }
  /* notify NFA_HCI_EVENT_RCVD_EVT to the application */
  nfa_hciu_send_to_apps_handling_connectivity_evts(NFA_HCI_EVENT_RCVD_EVT,
                                                   &p_evtdata);

  pthread_exit(NULL);
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf(" nfa_srd_start:Exit");
}

/*******************************************************************************
**
** Function         nfa_srd_stop
**
** Description      Process to reset the default settings for discover map
**                  command.
**
** Returns          None
**
*******************************************************************************/
void* nfa_srd_stop(void*) {
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf(" nfa_srd_stop:Enter");
  tNCI_DISCOVER_MAPS* p_intf_mapping = nullptr;

  const tNCI_DISCOVER_MAPS
      nfc_interface_mapping_default[NFC_NUM_INTERFACE_MAP] = {
        /* Protocols that use Frame Interface do not need to be included in
           the interface mapping */
        {NCI_PROTOCOL_ISO_DEP, NCI_INTERFACE_MODE_POLL_N_LISTEN,
         NCI_INTERFACE_ISO_DEP}
#if (NFC_RW_ONLY == FALSE)
        , /* this can not be set here due to 2079xB0 NFCC issues */
        {NCI_PROTOCOL_NFC_DEP, NCI_INTERFACE_MODE_POLL_N_LISTEN,
         NCI_INTERFACE_NFC_DEP}
#endif
        , /* This mapping is for Felica on DH  */
        {NCI_PROTOCOL_T3T, NCI_INTERFACE_MODE_LISTEN, NCI_INTERFACE_FRAME},
      };

  p_intf_mapping = (tNCI_DISCOVER_MAPS*)nfc_interface_mapping_default;

  if (!(nfa_dm_cb.disc_cb.disc_state == NFA_DM_RFST_IDLE)) {
    srdDeactivateEvtCb.lock();
    if (NFA_STATUS_OK == NFA_StopRfDiscovery()) {
      if (nfa_dm_cb.disc_cb.disc_state == NFA_DM_RFST_POLL_ACTIVE) {
        srd_t.wait_for_deact_ntf = true;
      }
    }
    srdDeactivateEvtCb.wait();
  }
  if (p_intf_mapping != nullptr &&
      (nfa_dm_cb.disc_cb.disc_state == NFA_DM_RFST_IDLE ||
       nfa_dm_cb.disc_cb.disc_state == NFA_DM_RFST_POLL_ACTIVE)) {
    srdDiscoverymapEvt.lock();
    (void)NFC_DiscoveryMap(NFC_NUM_INTERFACE_MAP, p_intf_mapping,
                           nfa_srd_discovermap_cb);
    srdDiscoverymapEvt.wait();
  }
  if (srd_t.srd_state == TIMEOUT) {
    nfa_hciu_send_to_apps_handling_connectivity_evts(NFA_SRD_EVT_TIMEOUT,
                                                     &p_evtdata);
  } else {
    nfa_hciu_send_to_apps_handling_connectivity_evts(NFA_HCI_EVENT_RCVD_EVT,
                                                     &p_evtdata);
  }
  srd_t.srd_state = DISABLE;
  pthread_exit(NULL);
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf(" nfa_srd_stop:Exit");
}
/*******************************************************************************
 **
 ** Function:        nfa_srd_discovermap_cb
 **
 ** Description:     callback for Discover Map command
 **
 ** Returns:         void
 **
 *******************************************************************************/
void nfa_srd_discovermap_cb(tNFC_DISCOVER_EVT event, tNFC_DISCOVER* p_data) {
  switch (event) {
    case NFC_MAP_DEVT:
      if (p_data) srd_t.rsp_status = p_data->status;
      srdDiscoverymapEvt.signal();
      break;
  }
  return;
}
/*******************************************************************************
**
** Function         srdCallback
**
** Description      callback for Srd specific HCI events
**
** Returns          none
**
*******************************************************************************/
void nfa_srd_process_hci_evt(tNFA_HCI_EVT event, tNFA_HCI_EVT_DATA* p_data) {
  pthread_t SrdThread;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  p_evtdata = *p_data;
  switch (event) {
    case NFA_SRD_START_EVT:
      if (pthread_create(&SrdThread, &attr, nfa_srd_start, nullptr) < 0) {
        DLOG_IF(ERROR, nfc_debug_enabled)
            << StringPrintf("SrdThread start creation failed");
      }
      break;
    case NFA_SRD_STOP_EVT:
      if (pthread_create(&SrdThread, &attr, nfa_srd_stop, nullptr) < 0) {
        DLOG_IF(ERROR, nfc_debug_enabled)
            << StringPrintf("SrdThread stop creation failed");
      }
      break;
    case NFA_SRD_FEATURE_NOT_SUPPORT_EVT:
      memset(&p_evtdata, 0, sizeof(p_evtdata));
      nfa_hciu_send_to_apps_handling_connectivity_evts(
          NFA_SRD_FEATURE_NOT_SUPPORT_EVT, &p_evtdata);
      break;
    default:
      DLOG_IF(INFO, nfc_debug_enabled)
          << StringPrintf("%s Unknown Event!!", __func__);
      break;
  }
  pthread_attr_destroy(&attr);
}
/*******************************************************************************
**
** Function         nfa_srd_deactivate_req_evt
**
** Description      callback for Srd deactivate response
**
** Returns          None
**
*******************************************************************************/
void nfa_srd_deactivate_req_evt(tNFC_DISCOVER_EVT event,
                                tNFC_DISCOVER* evt_data) {
  DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf("%s :Enter event:0x%2x", __func__, event);

  if (srd_t.srd_state == FEATURE_NOT_SUPPORTED) {
    DLOG_IF(ERROR, nfc_debug_enabled)
        << StringPrintf("%s : SRD Feature not supported", __func__);
    return;
  }

  switch (event) {
    case NFA_DM_RF_DEACTIVATE_RSP:
      if (!srd_t.wait_for_deact_ntf &&
          evt_data->deactivate.type == NFA_DEACTIVATE_TYPE_IDLE) {
        srd_t.rsp_status = evt_data->deactivate.status;
        srdDeactivateEvtCb.signal();
      }
      break;
    case NFA_DM_RF_DEACTIVATE_NTF:
      /*Handle the scenario where tag is present and stop req is trigger*/
      if (srd_t.wait_for_deact_ntf &&
          (evt_data->deactivate.type == NFA_DEACTIVATE_TYPE_IDLE)) {
        srd_t.rsp_status = evt_data->deactivate.status;
        srdDeactivateEvtCb.signal();
        srd_t.wait_for_deact_ntf = false;
      }
      break;

    default:
      DLOG_IF(INFO, nfc_debug_enabled)
          << StringPrintf("%s Unknown Event!!", __func__);
      break;
  }
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s :Exit ", __func__);
}
/*******************************************************************************
**
** Function         nfa_srd_check_hci_evt
**
** Description      This function shall be called to check whether SRD HCI EVT
**                   or not.
**
** Returns          TRUE/FALSE
**
*******************************************************************************/
bool nfa_srd_check_hci_evt(tNFA_HCI_EVT_DATA* evt_data) {
  static const uint8_t TAG_SRD_AID = 0x81;
  static const uint8_t TAG_SRD_EVT_DATA = 0x82;
  static const uint8_t INDEX1_FIELD_VALUE = 0xA0;
  static const uint8_t INDEX2_FIELD_VALUE = 0xFE;
  uint8_t inst = evt_data->rcvd_evt.evt_code;
  uint8_t* p_data = evt_data->rcvd_evt.p_evt_buf;
  bool is_require = false;
  uint8_t aid[NFC_MAX_AID_LEN] = {0xA0, 0x00, 0x00, 0x03, 0x96, 0x54,
                                  0x53, 0x00, 0x00, 0x00, 0x01, 0x01,
                                  0x80, 0x00, 0x00, 0x01};
  uint8_t len_aid = 0;

  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s :Enter ", __func__);
  if (inst == NFA_HCI_EVT_TRANSACTION && *p_data++ == TAG_SRD_AID) {
    len_aid = *p_data++;
    if (memcmp(p_data, aid, len_aid) == 0) {
      p_data += len_aid;
      if (*p_data == TAG_SRD_EVT_DATA) {
        if (*(p_data + 3) == INDEX1_FIELD_VALUE &&
            *(p_data + 4) == INDEX2_FIELD_VALUE) {
          if (srd_t.srd_state == FEATURE_NOT_SUPPORTED) {
            nfa_srd_process_hci_evt(NFA_SRD_FEATURE_NOT_SUPPORT_EVT, evt_data);
            is_require = true;
          } else if (*(p_data + 6) == 0x01) {
            if (srd_t.srd_state == ENABLE) {
              DLOG_IF(INFO, nfc_debug_enabled)
                  << StringPrintf("%s :SRD Enabled ", __func__);
            } else {
              nfa_srd_process_hci_evt(NFA_SRD_START_EVT, evt_data);
              is_require = true;
            }
          } else if (*(p_data + 6) == 0x00) {
            if (srd_t.srd_state == DISABLE) {
              DLOG_IF(INFO, nfc_debug_enabled)
                  << StringPrintf("%s :SRD Disabled ", __func__);
            } else {
              is_require = true;
              nfa_srd_process_hci_evt(NFA_SRD_STOP_EVT, evt_data);
            }
          }
        }
      }
    }
  }
  DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf("%s :Exit %d", __func__, is_require);
  return is_require;
}
/*******************************************************************************
**
** Function         nfa_srd_timeout_ntf
**
** Description      This function handles SRD TIMEOUT NTF
**
** Returns          void
**
*******************************************************************************/
void nfa_srd_timeout_ntf() {
  if (srd_t.srd_state == TIMEOUT || srd_t.srd_state == FEATURE_NOT_SUPPORTED) {
    return;
  }
  srd_t.srd_state = TIMEOUT;
  memset(&p_evtdata, 0, sizeof(p_evtdata));
  nfa_srd_process_hci_evt(NFA_SRD_STOP_EVT, &p_evtdata);
}
/*******************************************************************************
**
** Function         nfa_srd_init
**
** Description      Initalize the SRD module
**
** Returns          void
**
*******************************************************************************/
void nfa_srd_init() {
  memset(&srd_t, 0, sizeof(srd_t));
  memset(&p_evtdata, 0, sizeof(p_evtdata));
  uint16_t SRD_DISABLE_MASK = 0xFFFF;
  uint16_t isFeatureDisable = 0x0000;
  std::vector<uint8_t> srd_config;
  DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf(" %s Enter : srd_state 0x%2x", __func__, srd_t.srd_state);

  if ((NfcConfig::hasKey(NAME_NXP_SRD_TIMEOUT))) {
    srd_config = NfcConfig::getBytes(NAME_NXP_SRD_TIMEOUT);
    if (srd_config.size() > 0) {
      isFeatureDisable = ((srd_config[1] << 8) & SRD_DISABLE_MASK);
      isFeatureDisable = (isFeatureDisable | srd_config[0]);
    }
  }
  if (!(NfcConfig::hasKey(NAME_NXP_SRD_TIMEOUT)) || !isFeatureDisable) {
    /* NXP_SRD_TIMEOUT value not found in config file. */
    srd_t.srd_state = FEATURE_NOT_SUPPORTED;
  }
  DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf(" %s Exit : srd_state 0x%2x", __func__, srd_t.srd_state);
}
/*******************************************************************************
**
** Function         nfa_srd_deInit
**
** Description      De-Initalize the SRD module
**
** Returns          void
**
*******************************************************************************/
void nfa_srd_deInit() {}

/*******************************************************************************
**
** Function         nfa_srd_get_state
**
** Description      The API calls to get the srd current state.
**
** Returns          void
**
*******************************************************************************/
int nfa_srd_get_state(){
  return srd_t.srd_state;
}
#endif
