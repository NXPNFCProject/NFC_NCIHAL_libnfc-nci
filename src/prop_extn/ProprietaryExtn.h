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

#ifndef PROP_EXTENSION_H
#define PROP_EXTENSION_H

#include <cstdint>
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
  void setupPropExtension();

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
   * @return returns true, if it is vendor specific feature
   *         and it have to be handled by extension library.
   *         returns false, if it have to be handled by NFC HAL.
   *
   */
  bool handleVendorNciMsg(uint16_t dataLen, const uint8_t *pData);

  /**
   * @brief handles the vendor NCI response and notification
   * @param dataLen length of the NCI packet
   * @param pData pointer of the NCI packet
   * @return returns true, if it is vendor specific feature
   *         and it have to be handled by extension library.
   *         returns false, if it have to be handled by NFC HAL.
   * \Note Handler implements this function is responsible to call
   * base class handleVendorNciRspNtf to stop the response timer.
   *
   */
  bool handleVendorNciRspNtf(uint16_t dataLen, const uint8_t *pData);

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
   * @brief HAL updates the failure and error
   *        events through this function
   * @param event specifies events
   *        defined in NfcExtensionConstants
   * @return void
   *
   */
  void handleHalEvent(uint8_t event);

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
  typedef bool (*fp_prop_extn_snd_vnd_msg_t)(uint16_t, const uint8_t *);
  typedef bool (*fp_prop_extn_snd_vnd_rsp_ntf_t)(uint16_t, const uint8_t *);
  typedef void (*fp_prop_extn_deinit_t)();
  typedef void (*fp_prop_extn_update_nfc_hal_state_t)(uint16_t);
  typedef void (*fp_prop_extn_update_rf_state_t)(uint16_t);
  typedef void (*fp_prop_extn_on_write_complete_t)(uint16_t);
  typedef void (*fp_prop_extn_on_hal_control_granted_t)();
  fp_prop_extn_init_t fp_prop_extn_init = nullptr;
  fp_prop_extn_deinit_t fp_prop_extn_deinit = nullptr;
  fp_prop_extn_snd_vnd_msg_t fp_prop_extn_snd_vnd_msg = nullptr;
  fp_prop_extn_snd_vnd_rsp_ntf_t fp_prop_extn_snd_vnd_rsp_ntf = nullptr;
  fp_prop_extn_update_nfc_hal_state_t fp_prop_extn_update_nfc_hal_state =
      nullptr;
  fp_prop_extn_update_rf_state_t fp_prop_extn_update_rf_state = nullptr;
  fp_prop_extn_on_write_complete_t fp_prop_extn_on_write_complete = nullptr;
  fp_prop_extn_on_hal_control_granted_t fp_prop_extn_on_hal_control_granted =
      nullptr;
  void *p_prop_extn_handle = nullptr;
};
#endif // PROP_EXTENSION_H