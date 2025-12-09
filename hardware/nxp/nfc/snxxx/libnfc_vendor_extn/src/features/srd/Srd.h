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

#ifndef SRD_H
#define SRD_H

#include "IEventHandler.h"
#include "PalThreadMutex.h"
#include <iostream>
#include <stdint.h>
#include <string>
#include <vector>

/**
 * @brief Enum representing various states of the Secure Reader (SCR) process.
 */
typedef enum {
  /* Initial state, no operation in progress */
  SRD_STATE_INIT = 0,
  SRD_STATE_IDLE,
  SRD_STATE_W4_PWR_LINK_ALWAYS_ON_RSP,
  SRD_STATE_W4_MODE_SET_RSP,
  SRD_STATE_W4_MODE_SET_NTF,
  SRD_STATE_W4_HCI_CONNECTIVITY_EVT,
  SRD_STATE_W4_DISCOVER_MAP_RSP,
  SRD_STATE_W4_RF_DISCOVERY_RSP,
  SRD_STATE_W4_HCI_EVENT,
  SRD_STATE_W4_PWR_LINK_PWR_ON_RSP,
  SRD_STATE_STOP_IN_PROGRESS,
  SRD_STATE_STOP_W4_DISCOVERY_STOP_RSP,
  SRD_STATE_STOP_W4_DISCOVER_MAP_RSP,
  SRD_STATE_W4_SELECT_RSP,
  SRD_STATE_MAX,
} SrdState_t;
#define NFC_WRITE(pData, len)                                                  \
  NfcExtensionWriter::getInstance()->write(pData, len)

typedef enum {
  SRD_START_EVT,
  SRD_STOP_EVT,
  SRD_UNKOWN_EVT,
} SrdEvent_t;

/**
 * @brief Manager class to handle the mPOS operations and state management.
 */
class Srd {
public:
  /**
   * @brief Get the singleton instance of Srd.
   * @return Mpos* Pointer to the instance.
   */
  static Srd *getInstance();
  void Initilize();
  Srd(const Srd &) = delete;            /* Deleted copy constructor */
  Srd &operator=(const Srd &) = delete; /* Deleted assignment operator */

  /**
   * @brief Process NCI response/notification for Srd.
   * @param p_len Length of the data
   * @param pData Pointer to the data
   * @return returns NFCSTATUS_EXTN_FEATURE_SUCCESS, if it is vendor specific
   * feature and handled it internaly otherwise NFCSTATUS_EXTN_FEATURE_FAILURE.
   */
  NFCSTATUS processSrdNciRspNtf(std::vector<uint8_t> &pRspBuffer);

  /**
   * @brief Send Mode set command for Srd.
   * @param p_len Length of the data
   * @param pData Pointer to the data
   * @return returns NFCSTATUS_EXTN_FEATURE_SUCCESS, if it is vendor specific
   * feature and handled it internaly otherwise NFCSTATUS_EXTN_FEATURE_FAILURE.
   */
  NFCSTATUS sendModeSetCmd();
  /**
   * @brief Api to int the Srd resources.
   */
  void intSrd();
  /**
   * @brief Api to stop the Srd sequence.
   */
  NFCSTATUS stopSrd();
  NFCSTATUS activeSe();
  NFCSTATUS deactiveSe();
  void srdSndDiscoverMapCmd();
  void updateState(SrdState_t state);
  std::string scrStateToString(SrdState_t state);
  void processDiscoverMapRsp(const std::vector<uint8_t> &pRspHciEvtData);
  NFCSTATUS processRfDiscoverRsp(const std::vector<uint8_t> &pRspHciEvtData);
  bool processCoreGenErNtf(const std::vector<uint8_t> &pRspHciEvtData);
  NFCSTATUS processModeSetRsp(const std::vector<uint8_t> &pRspBuffer);
  NFCSTATUS processPwrLinkRsp(const std::vector<uint8_t> &pRspBuffer);
  NFCSTATUS processModeSetNtf(const std::vector<uint8_t> &pRspBuffer);
  NFCSTATUS processExtnsWrite(uint16_t *dataLen, uint8_t *pData);
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
  static Srd *instance; /* Singleton instance of Srd */
  static const uint8_t T2T_RF_PROTOCOL = 0x02;
  static constexpr std::array<uint8_t, 5> sHaltCmd = {0x00, 0x00, 0x02, 0x50, 0x00};
  static constexpr std::array<uint8_t, 6> sHaltCreditNtf = {0x60, 0x06, 0x03, 0x01, 0x00, 0x01};
  bool checkAndProcessHaltCommand(uint16_t *dataLen, uint8_t *pData);
  bool checkAndHandleRfDiscNtf(const std::vector<uint8_t> &pRspBuffer);
  SrdEvent_t checkSrdEvent(const std::vector<uint8_t> &pDataPacket);
  SrdEvent_t parseandCheckSrdEvent(const std::vector<uint8_t> &pRspHciEvtData);
  bool isTimeoutEvent(const std::vector<uint8_t> &pRspHciEvtData);
  std::vector<uint8_t> mCmdBuffer, mRspBuffer, mDiscStopResp;
  void notifySRDActionEvt(uint8_t ntfType);
  NFCSTATUS processDiscStopResp(const std::vector<uint8_t> &pRspBuffer);
  void sendDefaultDiscoverMapCmd();
  NFCSTATUS processPwrLinkDefRsp(const std::vector<uint8_t> &pRspBuffer);
  bool checkStopDiscCmd(uint16_t *dataLen, uint8_t *pData);
  NFCSTATUS processDefaultDiscMapRsp(const std::vector<uint8_t> &pRspBuffer);
  void processSRDEvent(const std::vector<uint8_t> &pRspHciEvtData,
                       SrdEvent_t srdEvent);
  bool notifySRDStopNtfEvt(const std::vector<uint8_t> &pRspHciEvtData);
  Srd();
  ~Srd();
  SrdState_t mState;
  SrdState_t mPrevState;
  bool sIsSrdSupported = false;
  bool srdStopReceived = false;
  static constexpr uint8_t NCI_SDR_MODE_NTF_PL_LEN = 0x02;
  static constexpr uint8_t SRD_MODE_NTF_SUB_GID_OID = 0x23;
  static constexpr uint8_t ACTIVE_SE = 0x21;
  static constexpr uint8_t DEACTIVE_SE = 0x22;
  static constexpr uint8_t SRD_MODE_NTF_START_SRD_DISCOVERY = 0x00;
  static constexpr uint8_t SRD_MODE_NTF_SRD_TIMED_OUT = 0x01;
  static constexpr uint8_t SRD_MODE_NTF_SRD_FEATURE_NOT_SUPPORTED = 0x02;
  static constexpr uint8_t SRD_MODE_NTF_SRD_TRANSACTION_STOP = 0x03;
};
#endif // SRD_H
