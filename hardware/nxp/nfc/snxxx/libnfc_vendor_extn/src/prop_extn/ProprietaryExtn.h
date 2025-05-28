/**
 *
 *  Copyright 2024-2025 NXP
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

#ifndef PROP_EXTENSION_H
#define PROP_EXTENSION_H

#include <cstdint>
#include <string>
#include <phNfcStatus.h>
class ProprietaryExtn {
public:
  ProprietaryExtn(const ProprietaryExtn &) = delete;
  ProprietaryExtn &operator=(const ProprietaryExtn &) = delete;
  /**
   * @brief Get the singleton of this object.
   * @return Reference to this object.
   *
   */
  static ProprietaryExtn *getInstance();
  void setupPropExtension(std::string propLibPath);

  void init();

  // TODO: Implement to reset and clean up the states
  /**
   * @brief De init the extension library
   *        and resets all it's states
   * @return void
   *
   */
  void deInit();

  /**
   * @brief handles the vendor NCI message
   * @param dataLen length of the NCI packet
   * @param pData pointer of the NCI packet
   * @return returns NFCSTATUS_EXTN_FEATURE_SUCCESS, if it is vendor specific
   * feature and handled by extension library otherwise
   * NFCSTATUS_EXTN_FEATURE_FAILURE.
   *
   */
  NFCSTATUS handleVendorNciMsg(uint16_t dataLen, const uint8_t *pData);

  /**
   * @brief handles the vendor NCI response and notification
   * @param dataLen length of the NCI packet
   * @param pData pointer of the NCI packet
   * @return returns NFCSTATUS_EXTN_FEATURE_SUCCESS, if it is vendor specific
   * feature and handled by extension library otherwise
   * NFCSTATUS_EXTN_FEATURE_FAILURE.
   * \Note Handler implements this function is responsible to
   * stop the response timer.
   *
   */
  NFCSTATUS handleVendorNciRspNtf(uint16_t dataLen, uint8_t *pData);

  /**
   * @brief map the vendor NCI command from PROP to ROW
   * @param dataLen length of the NCI packet
   * @param pData pointer of the NCI packet
   * \Note This conversion needed only for SSG Sub GID & OID defined ROW
   * feature. NXP defined Sub GID & OID ROW feature does not need conversion
   *
   */
  void mapGidOidToGenCmd(uint16_t dataLen, uint8_t *pData);

  /**
   * @brief map the vendor NCI command from ROW to PROP
   * @param dataLen length of the NCI packet
   * @param pData pointer of the NCI packet
   * \Note This conversion needed only for SSG Sub GID & OID defined ROW
   * feature. NXP defined Sub GID & OID ROW feature does not need conversion
   *
   */
  void mapGidOidToPropRspNtf(uint16_t dataLen, uint8_t *pData);

  /**
   * @brief onExtWriteComplete
   * @return void
   *
   */
  void onExtWriteComplete(uint8_t status);

  /**
   * @brief Called by libnfc-nci when NFCC control is granted to HAL.
   * @return void
   *
   */
  void halControlGranted();

  /**
   * @brief Updates the Nfc Hal state to
   *        extension library
   * @return void
   *
   */
  void updateNfcHalState(uint8_t halState);

  /**
   * @brief Updates the Nfc RF state to
   *        extension library
   * @return void
   *
   */
  void updateRfState(uint8_t rfState);

  /**
   * @brief Updates FW dnld status to
   *        extension library
   * @return void
   *
   */
  void updateFwDnlsStatus(uint8_t status);
  /**
   * @brief HAL updates the failure and error
   *        events through this function
   * @param event specifies events
   *        defined in NfcExtensionConstants
   * @return returns NFCSTATUS_EXTN_FEATURE_SUCCESS, if it is vendor specific
   * feature and handled by extension library otherwise
   * NFCSTATUS_EXTN_FEATURE_FAILURE.
   *
   */
  NFCSTATUS handleHalEvent(uint8_t event);
  /**
   * @brief update RF_DISC_CMD, if specific poll/listen feature is enabled
   * with required poll or listen technologies & send to upper layer.
   * @param dataLen length RF_DISC_CMD
   * @param pData
   * @return returns NFCSTATUS_EXTN_FEATURE_SUCCESS, if it is vendor specific
   * feature and handled by extension library otherwise
   * NFCSTATUS_EXTN_FEATURE_FAILURE.
   */
  NFCSTATUS processExtnWrite(uint16_t *dataLen, uint8_t *pData);
  /**
   * @brief Release all resources.
   * @return None
   *
   */
  static inline void finalize() {
    if (sProprietaryExtn != nullptr) {
      delete (sProprietaryExtn);
      sProprietaryExtn = nullptr;
    }
  }

private:
  static ProprietaryExtn *sProprietaryExtn; // singleton object
  /**
   * @brief Initialize member variables.
   * @return None
   *
   */
  ProprietaryExtn();

  /**
   * @brief Release all resources.
   * @return None
   *
   */
  ~ProprietaryExtn();
  typedef void (*fp_prop_extn_init_t)();
  typedef NFCSTATUS (*fp_prop_extn_snd_vnd_msg_t)(uint16_t, const uint8_t *);
  typedef NFCSTATUS (*fp_prop_extn_snd_vnd_rsp_ntf_t)(uint16_t,
                                                      const uint8_t *);
  typedef void (*fp_map_gid_oid_to_gen_cmd_t)(uint16_t, uint8_t *);
  typedef void (*fp_map_gid_oid_to_prop_rsp_ntf_t)(uint16_t, uint8_t *);
  typedef void (*fp_prop_extn_deinit_t)();
  typedef void (*fp_prop_extn_update_nfc_hal_state_t)(uint16_t);
  typedef void (*fp_prop_extn_update_rf_state_t)(uint16_t);
  typedef void (*fp_prop_extn_on_write_complete_t)(uint16_t);
  typedef void (*fp_prop_extn_on_hal_control_granted_t)();
  typedef NFCSTATUS (*fp_prop_extn_handle_hal_event_t)(uint8_t);
  typedef void (*fp_prop_extn_update_fw_dnld_status_t)(uint8_t);
  typedef NFCSTATUS (*fp_prop_extn_process_extn_write_t)(uint16_t *, uint8_t *);
  fp_prop_extn_init_t fp_prop_extn_init = nullptr;
  fp_prop_extn_deinit_t fp_prop_extn_deinit = nullptr;
  fp_prop_extn_snd_vnd_msg_t fp_prop_extn_snd_vnd_msg = nullptr;
  fp_prop_extn_snd_vnd_rsp_ntf_t fp_prop_extn_snd_vnd_rsp_ntf = nullptr;
  fp_map_gid_oid_to_gen_cmd_t fp_map_gid_oid_to_gen_cmd = nullptr;
  fp_map_gid_oid_to_prop_rsp_ntf_t fp_map_gid_oid_to_prop_rsp_ntf = nullptr;
  fp_prop_extn_update_nfc_hal_state_t fp_prop_extn_update_nfc_hal_state =
      nullptr;
  fp_prop_extn_update_rf_state_t fp_prop_extn_update_rf_state = nullptr;
  fp_prop_extn_on_write_complete_t fp_prop_extn_on_write_complete = nullptr;
  fp_prop_extn_on_hal_control_granted_t fp_prop_extn_on_hal_control_granted =
      nullptr;
  fp_prop_extn_handle_hal_event_t fp_prop_extn_handle_hal_event = nullptr;
  fp_prop_extn_update_fw_dnld_status_t fp_prop_extn_update_fw_dnld_status =
      nullptr;
  fp_prop_extn_process_extn_write_t fp_prop_extn_process_extn_write = nullptr;
  void* p_prop_extn_handle = nullptr;
};
#endif // PROP_EXTENSION_H
