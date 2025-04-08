/**
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
 **/
#ifndef NFCEE_STATE_MONITOR_H
#define NFCEE_STATE_MONITOR_H

#include "IEventHandler.h"
#include "NciStateMonitor.h"
#include <map>
#include <phNfcStatus.h>
#include <stdint.h>
#include <vector>

class NfceeStateMonitor {
public:
  static const uint8_t NCI_MODE_SET_NTF_REASON_CODE_INDEX = 3;
  static const uint8_t NCI_EE_STATUS_NTF_REASON_CODE_INDEX = 4;
  static const uint8_t NCI_EE_STATUS_NTF_EE_INDEX = 3;
  static const uint8_t MAX_ESE_RECOVERY_COUNT = 5;

  friend class NciStateMonitor;
  static NfceeStateMonitor *getInstance();

  NfceeStateMonitor(const NfceeStateMonitor &) = delete;
  NfceeStateMonitor &operator=(const NfceeStateMonitor &) = delete;

private:
  /**
   * @brief This API handle the NFCEE_MODE_SET_NTF for eSE.
   *
   * @param NFCEE_MODE_SET_NTF
   */
  NFCSTATUS processNfceeModeSetNtf(vector<uint8_t> &nfceeModeSetNtf);

  /**
   * @brief This API map proprietary error NFCEE_STATUS_NTF with
   * UNRECOVERABLE_ERROR NFCEE_STATUS_NTF for eSE & eUICC.
   *
   * @param NFCEE_STATUS_NTF
   */
  NFCSTATUS processNfceeStatusNtf(vector<uint8_t> &nfceeStatusNtf);

  /**
   * @brief Set the currentEE on receiving ESE Mode set Cmd.
   *
   * @param flag
   */
  void setCurrentEE(uint8_t ee);

  /**
   * @brief Determines whether error recovery is required for a given
   *        error code
   *
   * @param eeErrorCode The error code to be evaluated
   * @return true if recovery is required, false otherwise
   */
  bool isEeRecoveryRequired(uint8_t eeErrorCode);

  static NfceeStateMonitor *instance;

  NfceeStateMonitor();
  ~NfceeStateMonitor();
  uint8_t currentEE;
  uint8_t eseRecoveryCount;
};
#endif // NFCEE_STATE_MONITOR_H
