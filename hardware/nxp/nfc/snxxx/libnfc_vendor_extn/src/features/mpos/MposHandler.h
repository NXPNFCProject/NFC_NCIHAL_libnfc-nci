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

#ifndef MPOS_HANDLER_H
#define MPOS_HANDLER_H

#include "IEventHandler.h"
#include <cstdint>
#include <vector>

/**********************************************************************************
 **  Macros
 **********************************************************************************/
#define SE_READER_DEDICATED_MODE_OFF 0x00
#define SE_READER_DEDICATED_MODE_ON 0x01

// Forward declaration
class Mpos;

/** \addtogroup MPOS_EVENT_HANDLER_API_INTERFACE
 *  @brief  interface to perform the Mpos feature functionality.
 *  @{
 */
class MposHandler : public IEventHandler {
public:
  /**
   * @brief handles the vendor NCI message
   * @return returns NFCSTATUS_EXTN_FEATURE_SUCCESS, if it is vendor specific
   * feature and handled by extension library otherwise
   * NFCSTATUS_EXTN_FEATURE_FAILURE.
   *
   */
  NFCSTATUS handleVendorNciMessage(uint16_t dataLen, const uint8_t *pData);

  /**
   * @brief handles the vendor NCI response and notification
   * @return returns NFCSTATUS_EXTN_FEATURE_SUCCESS, if it is vendor specific
   * feature and handled by extension library otherwise
   * NFCSTATUS_EXTN_FEATURE_FAILURE.
   *
   */
  NFCSTATUS handleVendorNciRspNtf(uint16_t dataLen, uint8_t *pData);

  /**
   * @brief on Feature start, Current Handler have to be registered with
   * controller through switchEventHandler interface
   * @return void
   *
   */
  void onFeatureStart() override;

  /**
   * @brief on Feature End, Default Handler have to be registered with
   * controller through switchEventHandler interface
   * @return void
   *
   */
  void onFeatureEnd() override;

  /**
   * @brief This is the first API to be called to start or stop the MposHandler
   * mode <p>Requires {@link android.Manifest.permission#NFC} permission.
   * <li>This api shall be called only Nfcservice is enabled.
   * <li>This api shall be called only when there are no NFC transactions
   * ongoing
   * </ul>
   * @param  on Sets/Resets the MposHandler state.
   * @return whether the update of state is
   *          success or busy or fail.
   *          MposHandler_STATUS_BUSY
   *          MposHandler_STATUS_REJECTED
   *          MposHandler_STATUS_SUCCESS
   */
  int MposHandlerSetReaderMode(bool on);

  /**
   * @brief This is provides the info whether MposHandler mode is activated or
   * not <p>Requires {@link android.Manifest.permission#NFC} permission.
   * <li>This api shall be called only Nfcservice is enabled.
   * <li>This api shall be called only when there are no NFC transactions
   * ongoing
   * </ul>
   * @param void
   * @return TRUE if reader mode is started
   *          FALSE if reader mode is not started
   * @throws IOException If a failure occurred during reader mode set or reset
   */
  bool MposHandlerGetReaderMode();
  MposHandler();
  /**
   * @brief handles the control granted callback
   * @return returns void
   *
   */
  void halControlGranted() override;
  /**
   * @brief handles the control granted callback, if concrete
   *        handler does not handle it
   * \note This function should be light weight and shall not
   * take much time to execute othewise all other processing
   * will be blocked.
   * @return returns void
   *
   */
  void onWriteComplete(uint8_t status) override;
  /**
   * @brief indicates that HAL Exclusive request to LibNfc
   *        timed out
   * @return void
   *
   */
  void onHalRequestControlTimeout() override;
  /**
   * @brief indicates that write response time out for the
   *        NCI packet sent to controller
   *
   * @return void
   *
   */
  void onWriteRspTimeout() override;
  /**
   * @brief Releases all the resources
   * @return None
   *
   */
  ~MposHandler();

private:
  Mpos *mPosMngr = nullptr;
};
/** @}*/
#endif // MPOS_HANDLER_H
