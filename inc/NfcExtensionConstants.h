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

#ifndef NFC_EXTENSION_CONSTANTS_H
#define NFC_EXTENSION_CONSTANTS_H

#include <cstdint>

/** \addtogroup NFC_EXTENSION_CONSTANTS_INTERFACE
 *  @brief  interface for nfc extension specific constants and error codes.
 *  @{
 */

#define NFC_NXP_GEN_EXT_VERSION_MAJ (0x01) /* MW Major Version */
#define NFC_NXP_GEN_EXT_VERSION_MIN (0x00) /* MW Minor Version */

/**
 * @brief Log name of the Gen extension library
 *
 */
extern const char *NXPLOG_ITEM_NXP_GEN_EXTN;

/**
 * @brief returns UN_SUPPORTED_FEATURE code to framework, if HAL is not
 *        supporting the vendor specific message
 *
 */
constexpr uint8_t UN_SUPPORTED_FEATURE = -1;

/**
 * @brief returns SUPPPORTED_FEATURE code to framework, if HAL
 *        supports the vendor specific message
 *
 */
constexpr uint8_t SUPPPORTED_FEATURE = 0;

/**
 * @brief returns ERROR code to framework, if HAL
 *        does not able to process the request because mPOS on going
 *
 */
constexpr uint8_t ERROR = 1;

/**
 * @brief HAL gives this error code when write
 *        response times out
 *
 */
constexpr uint8_t WRITE_RSP_TIMED_OUT = 2;
/**
 * @brief NCI 2.0 Response Status Codes
 *
 */
constexpr uint8_t RESPONSE_STATUS_OK = 0x00;
constexpr uint8_t RESPONSE_STATUS_FAILED = 0x03;

constexpr uint8_t STATUS_REJECTED = 0x01;
constexpr uint8_t STATUS_INDEX = 0x03;
constexpr uint8_t STATUS_OK = 0x00;
constexpr uint8_t STATUS_SEMANTIC_ERROR = 0x06;

constexpr uint8_t MT_CMD = 0x00;
constexpr uint8_t MT_RSP = 0x01;
constexpr uint8_t MT_NTF = 0x02;

constexpr uint8_t DEFAULT_RESP = 0xFF;
constexpr uint8_t ZERO_PAYLOAD = 0x00;

constexpr uint8_t SUB_GID_OID_INDEX = 3;
constexpr uint8_t SUB_GID_MASK = 0xF0;
constexpr uint8_t SUB_OID_MASK = 0x0F;

constexpr uint8_t NCI_GID_INDEX = 0;
constexpr uint8_t NCI_OID_INDEX = 1;
constexpr uint8_t NCI_GID_MASK = 0x0F;
constexpr uint8_t NCI_OID_MASK = 0xFF;

constexpr uint8_t NCI_PROP_CMD_VAL = 0x2F;
constexpr uint8_t NCI_PROP_RSP_VAL = 0x4F;
constexpr uint8_t NCI_PROP_NTF_VAL = 0x6F;
constexpr uint8_t NCI_PROP_OID_VAL = 0x3E;
constexpr uint8_t NXP_FLUSH_SRAM_AO_TO_FLASH_OID = 0x21;

constexpr uint8_t NCI_PAYLOAD_LEN_INDEX = 2;
constexpr uint8_t MIN_PCK_MSG_LEN = 0x03;

constexpr uint8_t DISABLE_LOG = 0x00;
constexpr uint8_t ENABLE_LOG = 0x01;

/**
 * @brief MAINLINE related constants
 *
 */
constexpr uint8_t MAINLINE_MIN_CMD_LEN = 0x04;

constexpr uint8_t RF_BLK_RAW_CMD_TIMEOUT_SEC = 2;
static const int NXP_EXTNS_WRITE_RSP_TIMEOUT_IN_MS = 2000;
static const int NXP_EXTNS_HAL_REQUEST_CTRL_TIMEOUT_IN_MS = 1000;

/**
 * @brief Defines the state of the Handler
 *
 */
enum class HandlerState {
  /**
   * @brief indicates state has been started
   */
  STARTED,
  /**
   * @brief indicates state has been stopped
   */
  STOPPED
};

/**
 * @brief Defines the state of Nfc HAL module
 *
 */
enum class NfcHalState {
  /**
   * @brief indicates Nfc HAL is closed
   */
  CLOSE,
  /**
   * @brief indicates Nfc HAL is opened fully
   */
  OPEN,
  /**
   * @brief indicates Nfc HAL is partially opened.
   */
  MIN_OPEN
};

/**
 * @brief Defines the RfState
 *
 */
enum class RfState {
  /**
   * @brief indicates RF state is Idle
   */
  IDLE,
  /**
   * @brief indicates RF state is DISCOVER
   */
  DISCOVER,
  /**
   * @brief indicates RF state is FIELD_ON
   */
  FIELD_ON,
  /**
   * @brief indicates RF state is FIELD_OFF
   */
  FIELD_OFF,
  /**
   * @brief indicates RF is in interface activated state
   */
  INTERFACE_ACTIVATED,
  /**
   * @brief indicates RF is in interface deactivated state
   */
  INTERFACE_DEACTIVATED
};
// TODO: Define the error code to send to upper layer, if vendor commands are
// not able to process because of another high priority feature

/** @}*/
#endif // NFC_EXTENSION_CONSTANTS_H
