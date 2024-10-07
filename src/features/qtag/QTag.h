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
#ifndef QTAG_H
#define QTAG_H

#include "IEventHandler.h"
#include <phNfcStatus.h>
#include <stdint.h>

constexpr uint8_t NCI_LEN_RF_CMD = 0x06;

class QTag {
public:
  static QTag *getInstance();

  QTag(const QTag &) = delete;
  QTag &operator=(const QTag &) = delete;

  /**
   * @brief enable or disable QTag based on imput flag.
   *
   * @param flag
   */
  void enableQTag(uint8_t flag);
  /**
   * @brief return mIsQTagEnabled value.
   *
   * @return uint8_t
   */
  uint8_t getQTagStatus();

  /**
   * @brief check if QTag enabled & received RF_INTF_ACTIVATED_NTF
   * for Q_POLL(0x71).
   * @param rfIntfNtf
   * @return true or false
   */
  bool isQTagRfIntfNtf(vector<uint8_t> rfIntfNtf);

  /**
   * @brief if RF_INTF_ACTIVATED_NTF includes proprietary RF technology,
   * update with Type_A RF technology.
   * @param rfIntfNtf
   * @return returns NFCSTATUS_EXTN_FEATURE_SUCCESS, if it is vendor specific
   * RF_INTF_ACTIVATED_NTF and handled by extension library otherwise
   * NFCSTATUS_EXTN_FEATURE_FAILURE.
   */
  NFCSTATUS processIntActivatedNtf(vector<uint8_t> rfIntfNtf);

  /**
   * @brief if mIsQTagEnabled is true then includes proprietary RF polling
   * i.e. Q Poll(0x71) to RF_DISC_CMD.
   * @param rfDiscCmd
   * @return returns NFCSTATUS_EXTN_FEATURE_SUCCESS, if Q_poll is enabled
   * and handled by extension library otherwise
   * NFCSTATUS_EXTN_FEATURE_FAILURE.
   */
  NFCSTATUS processRfDiscCmd(vector<uint8_t> &rfDiscCmd);

private:
  static QTag *instance;

  QTag();
  ~QTag();
  uint8_t mIsQTagEnabled;
};

#endif // QTAG_H
