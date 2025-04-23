/*
 *
 *  The original Work has been changed by NXP.
 *
 *  Copyright 2025 NXP
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
 */

#ifndef LxDebug_H
#define LxDebug_H

#include "IEventHandler.h"
#include <iostream>
#include <stdint.h>
#include <string>
#include <vector>

typedef enum {
  EFDM_DISABLED = 0x00,
  EFDM_ENABLED,
  EFDM_DISABLED_CONFIG,
} EfdmStatus_t;

class LxDebug {
public:
  /**
   * @brief Get the singleton instance of Mpos.
   * @return LxDebug* Pointer to the instance.
   */
  static LxDebug *getInstance();

  void handleEFDMModeSet(bool mode);
  EfdmStatus_t isFieldDetectEnabledFromConfig();
  bool isEFDMStarted();

  /**
   * @brief if mIsEFDMStarted is true then includes proprietary RF polling
   * i.e. EFDM(0xFF) to RF_DISC_CMD.
   * @param rfDiscCmd
   * @return returns NFCSTATUS_EXTN_FEATURE_SUCCESS, if EFDM is enabled
   * and handled by extension library otherwise
   * NFCSTATUS_EXTN_FEATURE_FAILURE.
   */
  NFCSTATUS processRfDiscCmd(vector<uint8_t> &rfDiscCmd);

  /**
   * @brief Releases all the resources
   * @return None
   *
   */
  static inline void finalize() {
    if (instance != nullptr) {
      delete (instance);
      instance = nullptr;
    }
  }

private:
  static constexpr uint8_t NCI_PARAM_ID_RF_FIELD_INFO = 0x80;
  static LxDebug *instance;
  /* Private constructor to make class singleton*/
  LxDebug() = default;
  /* Private destructor to make class singleton*/
  ~LxDebug() = default;
  LxDebug(const LxDebug &) = delete; /* Deleted copy constructor */
  LxDebug &
  operator=(const LxDebug &) = delete; /* Deleted assignment operator */
  bool mIsEFDMStarted;
  bool updateRFSetConfig(vector<uint8_t> &rfSetConfCmd);
};
#endif // LxDebug_H
