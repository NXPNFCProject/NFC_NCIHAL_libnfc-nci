/******************************************************************************
 *
 *  Copyright 2020-2021 NXP
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

bool nfa_srd_check_hci_evt(tNFA_HCI_EVT_DATA* evt_data);
void nfa_srd_discovermap_cb(tNFC_DISCOVER_EVT event, tNFC_DISCOVER* p_data);
/******************************************************************************
 **  Constants
 *****************************************************************************/
#define NFC_NUM_INTERFACE_MAP 3
#define NFC_SWP_RD_NUM_INTERFACE_MAP 2

/*******************************************************************************
**
** Function         nfa_srd_snd_discover_map
**
** Description      Process to send discover map command with RF interface.
**
** Returns          None
**
*******************************************************************************/
void nfa_srd_snd_discover_map(tNFA_SRD_EVT event) {
  DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf(" %s: Enter : %d", __func__, event);
  tNCI_DISCOVER_MAPS* p_intf_mapping = nullptr;
  uint8_t p_params;

  const tNCI_DISCOVER_MAPS
      nfc_interface_mapping_mfc[NFC_SWP_RD_NUM_INTERFACE_MAP] = {
          {NCI_PROTOCOL_T2T, NCI_INTERFACE_MODE_POLL, NCI_INTERFACE_FRAME},
          {NCI_PROTOCOL_ISO_DEP, NCI_INTERFACE_MODE_POLL, NCI_INTERFACE_FRAME}};

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

  switch (event) {
    case NFA_SRD_START_EVT:
      p_intf_mapping = (tNCI_DISCOVER_MAPS*)nfc_interface_mapping_mfc;
      p_params = NFC_SWP_RD_NUM_INTERFACE_MAP;
      break;
    case NFA_SRD_STOP_EVT:
      p_intf_mapping = (tNCI_DISCOVER_MAPS*)nfc_interface_mapping_default;
      p_params = NFC_NUM_INTERFACE_MAP;
      break;
    }

    if (p_intf_mapping != nullptr) {
      (void)NFC_DiscoveryMap(p_params, p_intf_mapping,
                             nfa_srd_discovermap_cb);
  }
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf(" %s: Exit", __func__);
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
  DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf(" %s: Exit status 0x%2x", __func__, p_data->status);

  switch (event) {
    case NFC_MAP_DEVT:
      if (srd_t.srd_state == TIMEOUT) {
        nfa_hciu_send_to_apps_handling_connectivity_evts(NFA_SRD_EVT_TIMEOUT,
                                                         &p_evtdata);
        srd_t.srd_state = DISABLE;
      } else {
        nfa_hciu_send_to_apps_handling_connectivity_evts(NFA_HCI_EVENT_RCVD_EVT,
                                                         &p_evtdata);
      }
      break;
  }
  if (p_evtdata.rcvd_evt.p_evt_buf != nullptr) {
    GKI_freebuf(p_evtdata.rcvd_evt.p_evt_buf);
    p_evtdata.rcvd_evt.p_evt_buf = nullptr;
  }
  return;
}
/*******************************************************************************
**
** Function         nfa_srd_process_evt
**
** Description      Process SRD events.
**
** Returns          none
**
*******************************************************************************/
void nfa_srd_process_evt(tNFA_SRD_EVT event, tNFA_HCI_EVT_DATA* evt_data) {

    DLOG_IF(INFO, nfc_debug_enabled)
        << StringPrintf("%s Enter: event:%d", __func__,event);

    if (evt_data) {
      if (p_evtdata.rcvd_evt.p_evt_buf != nullptr) {
        GKI_freebuf(p_evtdata.rcvd_evt.p_evt_buf);
        p_evtdata.rcvd_evt.p_evt_buf = nullptr;
      }
      p_evtdata = *evt_data;
      p_evtdata.rcvd_evt.p_evt_buf =
          (uint8_t*)GKI_getbuf(p_evtdata.rcvd_evt.evt_len);
      if (p_evtdata.rcvd_evt.p_evt_buf != nullptr) {
        memcpy(p_evtdata.rcvd_evt.p_evt_buf, evt_data->rcvd_evt.p_evt_buf,
               p_evtdata.rcvd_evt.evt_len);
      } else {
        DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf(
            "%s Error allocating memory (p_evtdata.rcvd_evt.p_evt_buf)",
            __func__);
      }
    }
  switch (event) {
    case NFA_SRD_START_EVT:
      if (nfa_dm_cb.disc_cb.disc_state == NFA_DM_RFST_IDLE ||
          nfa_dm_cb.disc_cb.disc_state == NFA_DM_RFST_POLL_ACTIVE) {
        nfa_srd_snd_discover_map(event);
        srd_t.srd_state = ENABLE;
      }
      break;
    case NFA_SRD_TIMEOUT_EVT:
      if (srd_t.srd_state == TIMEOUT ||
          srd_t.srd_state == FEATURE_NOT_SUPPORTED) {
        return;
      }
      srd_t.srd_state = TIMEOUT;
      if (p_evtdata.rcvd_evt.p_evt_buf != nullptr) {
        GKI_freebuf(p_evtdata.rcvd_evt.p_evt_buf);
        p_evtdata.rcvd_evt.p_evt_buf = nullptr;
      }
      memset(&p_evtdata, 0, sizeof(p_evtdata));
      [[fallthrough]];
    case NFA_SRD_STOP_EVT:
      if (!(nfa_dm_cb.disc_cb.disc_state == NFA_DM_RFST_IDLE)) {
        if (NFA_STATUS_OK != NFA_StopRfDiscovery()) {
          DLOG_IF(INFO, nfc_debug_enabled)
              << StringPrintf("%s Failed Stop Rf!!", __func__);
        } else {
          srd_t.isStopping = true;
        }
      }
      break;

    case NFA_SRD_FEATURE_NOT_SUPPORT_EVT:
      if (p_evtdata.rcvd_evt.p_evt_buf != nullptr) {
        GKI_freebuf(p_evtdata.rcvd_evt.p_evt_buf);
        p_evtdata.rcvd_evt.p_evt_buf = nullptr;
      }
      memset(&p_evtdata, 0, sizeof(p_evtdata));
      nfa_hciu_send_to_apps_handling_connectivity_evts(
          NFA_SRD_FEATURE_NOT_SUPPORT_EVT, &p_evtdata);
      break;
    default:
      DLOG_IF(INFO, nfc_debug_enabled)
          << StringPrintf("%s Unknown Event!!", __func__);
      break;
  }
}
/*******************************************************************************
**
** Function         nfa_srd_dm_process_evt
**
** Description      Process DM events.
**
** Returns          none
**
*******************************************************************************/
void nfa_srd_dm_process_evt(tNFC_DISCOVER_EVT event,
        tNFC_DISCOVER* disc_evtdata) {

    DLOG_IF(INFO, nfc_debug_enabled)
        << StringPrintf("%s Enter: event:%d", __func__,event);

  switch (event) {
      case NFA_DM_RF_DEACTIVATE_RSP:
      case NFA_DM_RF_DEACTIVATE_NTF:
        if (srd_t.isStopping &&
            disc_evtdata->deactivate.type == NFA_DEACTIVATE_TYPE_IDLE) {
          if (nfa_dm_cb.disc_cb.disc_state == NFA_DM_RFST_POLL_ACTIVE &&
              event == NFA_DM_RF_DEACTIVATE_RSP) {
            break;
          }
          nfa_srd_snd_discover_map(NFA_SRD_STOP_EVT);
          if (srd_t.srd_state != TIMEOUT) {
            /*Don't update the state in case of Timeout
             *It need to update after notfiy timeout to
             *upper layer*/
            srd_t.srd_state = DISABLE;
          }
          srd_t.isStopping = false;
        }
        break;
      default:
        DLOG_IF(INFO, nfc_debug_enabled)
            << StringPrintf("%s Unknown Event!!", __func__);
        break;
  }
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
            nfa_srd_process_evt(NFA_SRD_FEATURE_NOT_SUPPORT_EVT, evt_data);
            is_require = true;
          } else if (*(p_data + 6) == 0x01) {
            if (srd_t.srd_state == ENABLE) {
              DLOG_IF(INFO, nfc_debug_enabled)
                  << StringPrintf("%s :SRD Enabled ", __func__);
            } else {
              nfa_srd_process_evt(NFA_SRD_START_EVT, evt_data);
              is_require = true;
            }
          } else if (*(p_data + 6) == 0x00) {
            if (srd_t.srd_state == DISABLE) {
              DLOG_IF(INFO, nfc_debug_enabled)
                  << StringPrintf("%s :SRD Disabled ", __func__);
            } else {
              is_require = true;
              nfa_srd_process_evt(NFA_SRD_STOP_EVT, evt_data);
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
** Function         nfa_srd_init
**
** Description      Initalize the SRD module
**
** Returns          void
**
*******************************************************************************/
void nfa_srd_init() {
  memset(&srd_t, 0, sizeof(srd_t));
  if (p_evtdata.rcvd_evt.p_evt_buf != nullptr) {
    GKI_freebuf(p_evtdata.rcvd_evt.p_evt_buf);
    p_evtdata.rcvd_evt.p_evt_buf = nullptr;
  }
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
