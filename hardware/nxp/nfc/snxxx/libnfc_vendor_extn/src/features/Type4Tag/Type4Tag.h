/*
 * Copyright 2025 NXP
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef T4T_H
#define T4T_H

#include <stdint.h>
#include "phNfcTypes.h"
#include "phNxp_NfcEeprom.h"
/**
 * @brief Manager class to handle the mPOS operations and state management.
 */
class Type4Tag {
  public:
  /**
   * @brief Get the singleton instance of Type4Tag.
   * @return Type4Tag* Pointer to the instance.
   */
  static Type4Tag *getInstance();
  /**
   * @brief This function shall be called as part of hal core_initialized().
   * It enables T4T settings based on the config option.
   * @param void
   * @return returns NFCSTATUS_SUCCESS if T4T is enabled, else respective error
   * code returned by lower layer.
   */
  NFCSTATUS enable();
  /**
   * @brief This function shall be called from NFC HAL after receiving
   * CORE_INIT_RSP. It updates the field:'number logical channel supported'
   * in CORE_INIT_RSP received from NFCC.
   * @param p_len Length of the CORE_INIT_RSP
   * @param pData Pointer to the CORE_INIT_RSP
   * @return returns NFCSTATUS_SUCCESS
   */
  NFCSTATUS updatePropParam(uint16_t dataLen, uint8_t *pData);
    /**
   * @brief Release all resources.
   * @return None
   *
   */
  static inline void finalize() {
    if (sInstance != nullptr) {
      delete (sInstance);
      sInstance = nullptr;
    }
  }
  Type4Tag(const Type4Tag &) = delete;            /* Deleted copy constructor */
  Type4Tag &operator=(const Type4Tag &) = delete; /* Deleted assignment operator */

  private:
  static Type4Tag *sInstance; /* Singleton instance of Type4Tag */
  uint8_t cmdType4TagNfceeCfg;
  phNxpNci_EEPROM_info_t mEepromInfo;
  ~Type4Tag();
  Type4Tag();
};
#endif