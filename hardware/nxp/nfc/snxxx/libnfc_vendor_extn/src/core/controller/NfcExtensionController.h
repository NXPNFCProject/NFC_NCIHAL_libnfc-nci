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

#ifndef NFC_EXTENSION_CONTROLLER_H
#define NFC_EXTENSION_CONTROLLER_H

#include "IEventHandler.h"
#include "NfcExtensionConstants.h"
#include <cstdint>
#include <unordered_map>

/** \addtogroup CONTROLLER_API_INTERFACE
 *  @brief  interface to controller to handle the event based on current state.
 *  @{
 */

/**
 * @brief Defines the type of the Handler
 * \note define as per NCI specification aligned with SSG
 *
 */
enum class HandlerType {
  /**
   * @brief indicates default handler
   */
  DEFAULT,
  /**
   * @brief indicates lxdebug feature handler
   */
  LxDebug = 0x01,
  /**
   * @brief indicates mpos feature handler
   */
  MPOS = 0x0A,
  /**
   * @brief indicates QTAG feature handler
   */
  QTAG = 0x03,
  /**
   * @brief indicates FW feature handler
   */
  FW = 0x06,
  /**
   * @brief indicate Transit config & RF Register feature handler
   *
   */
  TRANSIT = 0x07,
  // TODO: Align on ROW Sub GID and OID usage
  /**
   * @brief indicates t4t feature handler
   */
  T4T = 15,
  /**
   * @brief indicates mpos feature handler
   */
  SRD = 0x02,
};

class NfcExtensionController {
public:
  /**
   * @brief Get the singleton of this object.
   * @return Reference to this object.
   *
   */
  static NfcExtensionController *getInstance();

  /**
   * @brief Add the event handler and it's type to handler's list of controller.
   * @return void
   *
   */
  void addEventHandler(HandlerType handlerType,
                       std::shared_ptr<IEventHandler> eventHandler);

  /**
   * @brief reverts the current handler and sets the default event handler
   *        to controller.
   * @return void
   *
   */
  void revertToDefaultHandler();

  /**
   * @brief Switches to event handler requested by features.
   * @return void
   *
   */
  void switchEventHandler(HandlerType handlerType);

  /**
   * @brief Gets the current event handler type based on active feature.
   * @return returns the handler of active feature else returns
   *         default handler type.
   *
   */
  HandlerType getEventHandlerType() { return mCurrentHandlerType; }

  /**
   * @brief Gets the current event handler state.
   * @return returns the handler state of active feature
   *
   */
  HandlerState getEventHandlerState() { return mCurrentHandlerState; }

  /**
   * @brief Gets the current Nfc HAL state.
   * @return returns NfcHalState
   *
   */
  NfcHalState getNfcHalState() { return mNfcHalState; }

  /**
   * @brief Gets the current event handler.
   * @return returns a std::shared_ptr<IEventHandler>
   */
  std::shared_ptr<IEventHandler> getCurrentEventHandler() {
    return mIEventHandler;
  }

  /**
   * @brief Updates the Nfc Hal state.
   * @return void
   *
   */
  void updateNfcHalState(int);

  /**
   * @brief Handle HAL events(error/failure).
   * @return returns NFCSTATUS_EXTN_FEATURE_SUCCESS, if it is vendor specific
   * feature and handled by extension library otherwise
   * NFCSTATUS_EXTN_FEATURE_FAILURE.
   *
   */
  NFCSTATUS onHandleHalEvent(uint8_t);

  /**
   * @brief Init the controller with stack,  data callbacks and default event
   * handler.
   * @return void
   *
   */
  void init();

  /**
   * @brief handles the vendor NCI message by delegating the message to
   *        current active event handler
   * @return returns NFCSTATUS_EXTN_FEATURE_SUCCESS, if it is vendor specific
   * feature and handled by extension library otherwise
   * NFCSTATUS_EXTN_FEATURE_FAILURE.
   *
   */
  NFCSTATUS handleVendorNciMessage(uint16_t dataLen, const uint8_t *pData);

  /**
   * @brief handles the vendor NCI response/Notification by delegating the
   * message to current active event handler
   * @return returns NFCSTATUS_EXTN_FEATURE_SUCCESS, if it is vendor specific
   * feature and handled by extension library otherwise
   * NFCSTATUS_EXTN_FEATURE_FAILURE.
   *
   */
  NFCSTATUS handleVendorNciRspNtf(uint16_t dataLen, uint8_t *pData);

  /**
   * @brief Updates the write complete status.
   * @return void
   *
   */
  void onWriteCompletion(uint8_t status);

  /**
   * @brief indicates the exclusive control given to vendor extension library.
   *        vendor extension library responsible for giving back the control
   *        with in certain time bound.
   * @return void
   *
   */
  void halControlGranted();

  /**
   * @brief indicates that HAL Exclusive request to LibNfc
   *        timed out
   * @return void
   *
   */
  void halRequestControlTimedout();

  /**
   * @brief indicates that write response time out for the
   *        NCI packet sent to controller
   * @return void
   *
   */
  void writeRspTimedout();

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
    if (sNfcExtensionController != nullptr) {
      delete (sNfcExtensionController);
      sNfcExtensionController = nullptr;
    }
  }

private:
  static NfcExtensionController *sNfcExtensionController; // singleton object
  std::shared_ptr<IEventHandler> mIEventHandler;
  std::shared_ptr<IEventHandler> mDefaultEventHandler;
  // TODO: check, if there are any better than using map as it takes up memory
  std::unordered_map<HandlerType, std::shared_ptr<IEventHandler>> mHandlers;
  HandlerState mCurrentHandlerState;
  HandlerType mCurrentHandlerType;
  NfcHalState mNfcHalState;
  std::unordered_map<int, NfcHalState> mNfcHalStateMap = {
      {0, NfcHalState::CLOSE},
      {1, NfcHalState::OPEN},
      {2, NfcHalState::MIN_OPEN}};

  /**
   * @brief Initialize member variables.
   * @return None
   *
   */
  NfcExtensionController();

  /**
   * @brief Release all resources.
   * @return None
   *
   */
  ~NfcExtensionController();

  /**
   * @brief Gets the current event handler based on active feature.
   * @return returns the handler of active feature else returns
   *         default handler.
   *
   */
  std::shared_ptr<IEventHandler> getEventHandler();
};
/** @}*/
#endif // NFC_EXTENSION_CONTROLLER_H
