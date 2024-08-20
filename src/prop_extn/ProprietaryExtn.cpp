/**
 *
 *  Copyright 2024 NXP
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
 **/

#include "ProprietaryExtn.h"
#include "NfcExtensionConstants.h"
#include <dlfcn.h>
#include <phNxpLog.h>

// TODO: Check the feasibility of removing or adding this code only for SSG
// release
//  and plan to delete for ROW.
ProprietaryExtn *ProprietaryExtn::sProprietaryExtn;

ProprietaryExtn::ProprietaryExtn() {}

ProprietaryExtn::~ProprietaryExtn() {}

ProprietaryExtn *ProprietaryExtn::getInstance() {
  if (sProprietaryExtn == nullptr) {
    sProprietaryExtn = new ProprietaryExtn();
  }
  return sProprietaryExtn;
}

bool ProprietaryExtn::handleVendorNciMsg(uint16_t dataLen,
                                         const uint8_t *pData) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter dataLen:%d", __func__,
                 dataLen);
  if (fp_prop_extn_snd_vnd_msg != nullptr) {
    return fp_prop_extn_snd_vnd_msg(dataLen, pData);
  }
  return false;
}

bool ProprietaryExtn::handleVendorNciRspNtf(uint16_t dataLen,
                                            const uint8_t *pData) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter dataLen:%d", __func__,
                 dataLen);
  if (fp_prop_extn_snd_vnd_rsp_ntf != nullptr) {
    return fp_prop_extn_snd_vnd_rsp_ntf(dataLen, pData);
  }
  return false;
}

void ProprietaryExtn::onExtWriteComplete(uint8_t status) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter status:%d", __func__,
                 status);
  if (fp_prop_extn_on_write_complete != nullptr) {
    return fp_prop_extn_on_write_complete(status);
  }
}

void ProprietaryExtn::setupPropExtension() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter", __func__);
  p_prop_extn_handle =
      dlopen("/system/vendor/lib64/libnxp_nfc_prop_ext.so", RTLD_NOW);
  if (p_prop_extn_handle == nullptr) {
    NXPLOG_EXTNS_E(
        NXPLOG_ITEM_NXP_GEN_EXTN,
        "%s Error : opening (/system/vendor/lib64/libnxp_nfc_prop_ext.so) !!",
        __func__);
    return;
  }

  if ((fp_prop_extn_init = (fp_prop_extn_init_t)dlsym(
           p_prop_extn_handle, "phNxpProp_LibInit")) == NULL) {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "%s Failed to find phNxpProp_LibInit !!", __func__);
  }

  if ((fp_prop_extn_deinit = (fp_prop_extn_deinit_t)dlsym(
           p_prop_extn_handle, "phNxpProp_LibDeInit")) == NULL) {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Failed to find deInit !!",
                   __func__);
  }

  if ((fp_prop_extn_snd_vnd_msg = (fp_prop_extn_snd_vnd_msg_t)dlsym(
           p_prop_extn_handle, "phNxpProp_HandleVendorNciMsg")) == NULL) {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "%s Failed to find phNxpProp_HandleVendorNciMsg !!",
                   __func__);
  }

  if ((fp_prop_extn_snd_vnd_rsp_ntf = (fp_prop_extn_snd_vnd_rsp_ntf_t)dlsym(
           p_prop_extn_handle, "phNxpProp_HandleVendorNciRspNtf")) == NULL) {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "%s Failed to find phNxpProp_HandleVendorNciRspNtf !!",
                   __func__);
  }

  if ((fp_prop_extn_update_nfc_hal_state = (fp_prop_extn_update_nfc_hal_state_t)
           dlsym(p_prop_extn_handle, "phNxpProp_UpdateNfcHalState")) == NULL) {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "%s Failed to find phNxpProp_UpdateNfcHalState !!",
                   __func__);
  }

  if ((fp_prop_extn_update_rf_state = (fp_prop_extn_update_rf_state_t)dlsym(
           p_prop_extn_handle, "phNxpProp_UpdateRfState")) == NULL) {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "%s Failed to find phNxpProp_UpdateRfState !!", __func__);
  }

  if ((fp_prop_extn_on_write_complete = (fp_prop_extn_on_write_complete_t)dlsym(
           p_prop_extn_handle, "phNxpProp_OnWriteComplete")) == NULL) {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "%s Failed to find phNxpProp_OnWriteComplete !!", __func__);
  }

  if ((fp_prop_extn_on_hal_control_granted =
           (fp_prop_extn_on_hal_control_granted_t)dlsym(
               p_prop_extn_handle, "phNxpProp_OnHalControlGranted")) == NULL) {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "%s Failed to find phNxpProp_OnHalControlGranted !!",
                   __func__);
  }
  if (fp_prop_extn_init != NULL) {
    fp_prop_extn_init();
  }
}