/*
 *
 *  The original Work has been changed by NXP.
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
 */

#ifndef MPOS_H
#define MPOS_H

#include "IEventHandler.h"
#include <iostream>
#include <stdint.h>
#include <string>
#include <vector>

/**********************************************************************************
 **  Macros
 **********************************************************************************/
#define NAME_NXP_SWP_RD_TAG_OP_TIMEOUT "NXP_SWP_RD_TAG_OP_TIMEOUT"
#define NAME_NXP_DM_DISC_NTF_TIMEOUT "NXP_DM_DISC_NTF_TIMEOUT"
#define NAME_NXP_RDR_REQ_GUARD_TIME "NXP_RDR_REQ_GUARD_TIME"
#define NAME_NXP_PROP_RESET_EMVCO_CMD "NXP_PROP_RESET_EMVCO_CMD"

#define READER_MODE_TAG_OP_DEFAULT_TIMEOUT_IN_MS (20 * 1000)  // 20 Sec
#define READER_MODE_CARD_REMOVE_TIMEOUT_IN_MS (1 * 1000)      //  1 Sec
#define READER_MODE_DISC_NTF_DEFAULT_TIMEOUT_IN_MS (0 * 1000) //  0 Sec

#define EVENT_MPOS_RF_ERROR 0x80   // HCI_TRANSACTION_EVENT parameter type
#define EVENT_MPOS_RF_VERSION 0x00 // HCI_TRANSACTION_EVENT parameter version
#define EVENT_MPOS_RDR_MODE_RESTART 0x04 // EVENT to Restart Reader mode

#define SET_CONFIG_BUFF_MAX_SIZE 0x08

#define NFC_WRITE(pData, len)                                                  \
  NfcExtensionWriter::getInstance().write(pData, len)
#define IS_START_STOP_STATE_REQUEST(state)                                     \
  ((state) == MPOS_STATE_SEND_PROFILE_SELECT_CONFIG_CMD ||                     \
   (state) == MPOS_STATE_SEND_PROFILE_DESELECT_CONFIG_CMD)

constexpr uint8_t NCI_PROP_MPOS_SUB_GIDOID_VAL = 0x5F;
constexpr uint8_t NCI_RDR_MODE_NTF_PL_LEN = 0x02;
constexpr uint8_t NCI_AID_VAL = 0x81;
constexpr uint8_t NCI_AID_LEN = 0x10;
constexpr uint8_t NCI_PARAM_TYPE = 0x82;
constexpr uint8_t NCI_PARAM_TYPE_LEN = 0x03;

/**
 * @brief Enum representing various states of the Secure Reader (SCR) process.
 */
typedef enum {
  /* Initial state, no operation in progress */
  MPOS_STATE_IDLE = 0,
  /* mPOS start request accepted, waiting for HAL granted event */
  MPOS_STATE_START_IN_PROGRESS,
  /* Request HAL control for SCR operation */
  MPOS_STATE_REQUEST_HAL_CONTROL,
  /* Send profile select set config command to NFCC */
  MPOS_STATE_SEND_PROFILE_SELECT_CONFIG_CMD,
  /* Waiting for response of profile select command */
  MPOS_STATE_WAIT_FOR_PROFILE_SELECT_RESPONSE,
  /* Send Discover map command to NFCC */
  MPOS_STATE_SEND_DISCOVER_MAP_CMD,
  /* Waiting for discover map response */
  MPOS_STATE_WAIT_FOR_DISCOVER_MAP_RSP,
  /* Notify upper layer about discover map response */
  MPOS_STATE_NOTIFY_DISCOVER_MAP_RSP,
  /* Waiting for RF discovery response */
  MPOS_STATE_WAIT_FOR_RF_DISCOVERY_RSP,
  /* RF Discovery has been started */
  MPOS_STATE_DISCOVERY_STARTED,
  /* RF Discovery has been restarted */
  MPOS_STATE_DISCOVERY_RESTARTED,
  /* Waiting for interface activated notification */
  MPOS_STATE_WAIT_FOR_INTERFACE_ACTIVATION,
  /* Secure transaction has started */
  MPOS_STATE_TRANSACTION_STARTED,
  /* Request to remove RF discovery */
  MPOS_STATE_RF_DISCOVERY_REQUEST_REMOVE,
  /* Notify upper layer to stop the RF discovery*/
  MPOS_STATE_NOTIFY_STOP_RF_DISCOVERY,
  /* Waiting for RF deactivation */
  MPOS_STATE_WAIT_FOR_RF_DEACTIVATION,
  /* Secure reader has stopped */
  MPOS_STATE_SE_READER_STOPPED,
  /* Send profile deselection set config command */
  MPOS_STATE_SEND_PROFILE_DESELECT_CONFIG_CMD,
  /* Waiting for profile deselect set config response */
  MPOS_STATE_WAIT_FOR_PROFILE_DESELECT_CFG_RSP,
  /* Send default discovery map command */
  MPOS_STATE_SEND_DEFAULT_DISCOVERY_MAP_COMMAND,
  /* Waiting for default discovery map response */
  MPOS_STATE_WAIT_FOR_DEFAULT_DISCOVERY_MAP_RSP,
  /* Release HAL control after operation completion */
  MPOS_STATE_RELEASE_HAL_CONTROL,
  /* Timeout occurred during HAL control request */
  MPOS_STATE_HAL_CONTROL_REQUEST_TIMEOUT,
  /* Timeout waiting for tag in RF discovery */
  MPOS_STATE_NO_TAG_TIMEOUT,
  /* Generic error notification received */
  MPOS_STATE_GENERIC_ERR_NTF,
  /* Unknown state */
  MPOS_STATE_UNKNOWN,
  /* Max State */
  MPOS_STATE_MAX,
} ScrState_t;

/**
 * @brief Enum representing various action events in Secure Reader
 * notifications.
 */
typedef enum {
  ACTION_SE_READER_START_RF_DISCOVERY = 0,     /* Start RF discovery */
  ACTION_SE_READER_START_DEFAULT_RF_DISCOVERY, /* Start default RF discovery */
  ACTION_SE_READER_TAG_DISCOVERY_STARTED,      /* Tag discovery started */
  ACTION_SE_READER_TAG_DISCOVERY_START_FAILED, /* Tag discovery start failed */
  ACTION_SE_READER_TAG_DISCOVERY_RESTARTED,    /* Tag discovery restarted */
  ACTION_SE_READER_TAG_ACTIVATED,              /* Tag activated */
  ACTION_SE_READER_STOP_RF_DISCOVERY,          /* Stop RF discovery */
  ACTION_SE_READER_STOPPED,                    /* RF discovery stopped */
  ACTION_SE_READER_STOP_FAILED,                /* Failed to stop RF discovery */
  ACTION_SE_READER_TAG_TIMEOUT,                /* Tag timeout occurred */
  ACTION_SE_READER_TAG_REMOVE_TIMEOUT,    /* Tag removal timeout occurred */
  ACTION_SE_READER_MULTIPLE_TAG_DETECTED, /* Multiple tags detected */
} ScrNtfActionEvent_t;

/**
 * @brief Manager class to handle the mPOS operations and state management.
 */
class Mpos {
public:
  bool isTimerStarted;    /* Flag to check if timer is started */
  bool isActivatedNfcRcv; /* Flag indicating if NFC activation is received */
  unsigned long tagOperationTimeout; /* Timeout for tag operation */
  IntervalTimer tagOperationTimer;   /* Timer for tag operation */
  IntervalTimer tagRemovalTimer;     /* Timer for tag removal */
  IntervalTimer startStopGuardTimer; /* Timer for start/stop guard */
  uint32_t mTimeoutMaxCount;         /* Timeout for deactivation notification */
  uint32_t RdrReqGuardTimer;         /* Timer for reader request guard */
  ScrState_t lastScrState;           /* Last recorded SCR state */

  /**
   * @brief Get the singleton instance of Mpos.
   * @return Mpos* Pointer to the instance.
   */
  static Mpos *getInstance();

  Mpos(const Mpos &) = delete;            /* Deleted copy constructor */
  Mpos &operator=(const Mpos &) = delete; /* Deleted assignment operator */

  /**
   * @brief Process NCI response/notification for mPOS.
   * @param p_len Length of the data
   * @param pData Pointer to the data
   * @return returns NFCSTATUS_EXTN_FEATURE_SUCCESS, if it is vendor specific
   * feature and handled it internaly otherwise NFCSTATUS_EXTN_FEATURE_FAILURE.
   */
  NFCSTATUS processMposNciRspNtf(const std::vector<uint8_t> &pData);

  /**
   * @brief Process mPOS event based on the current state.
   * @param state Current SCR state.
   * @return returns NFCSTATUS_EXTN_FEATURE_SUCCESS, if it is vendor specific
   * feature and handled it internaly otherwise NFCSTATUS_EXTN_FEATURE_FAILURE.
   */
  NFCSTATUS processMposEvent(ScrState_t state);

  /**
   * @brief Update the current state of the SCR.
   * @param state New state to update.
   */
  void updateState(ScrState_t state);

  /**
   * @brief Check if SE reader restarted.
   * @param pData Pointer to the data
   * @param pLen Length of the data
   * @return true If restart, false otherwise.
   */
  bool isSeReaderRestarted(const std::vector<uint8_t> &pData);

  /**
   * @brief Get the current SCR state.
   * @return ScrState_t Current SCR state.
   */
  ScrState_t getState();

  /**
   * @brief Map the event to the SCR state based on the received data.
   * @param pData the data buffer
   * @return ScrState_t Mapped SCR state.
   */
  ScrState_t getScrState(const std::vector<uint8_t> &pData);

  /**
   * @brief Timerout handler, It will handle the event if tag is not tapped
   * if timer will gets expired.
   * @param sigval val
   */
  static void cardTapTimeoutHandler(union sigval val);

  /**
   * @brief Timerout handler, It will handle the event if tag is not being
   * removed from RF antenna proximity after transaction.
   * @param sigval val
   */
  static void cardRemovalTimeoutHandler(union sigval val);

  /**
   * @brief Timerout handler, It will handle smooth SE read start & stop event.
   * @param sigval val
   */
  static void guardTimeoutHandler(union sigval val);

private:
  static Mpos *instance; /* Singleton instance of Mpos */
  uint32_t mCurrentTimeoutCount;

  Mpos();
  ~Mpos();
  bool isProcessRdrReq(ScrState_t state);
  std::string scrStateToString(ScrState_t state);

  ScrState_t mState;
};
#endif // MPOS_H
