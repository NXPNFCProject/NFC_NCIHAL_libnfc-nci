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

#ifndef NFC_EXTENSION_WRITER_H
#define NFC_EXTENSION_WRITER_H

#include "PlatformAbstractionLayer.h"
#include <cstdint>
/** \addtogroup NFC_EXTENSION_WRITER_API_INTERFACE
 *  @brief  interface to perform the exclusive and non exclusive write to
 * controller.
 *  @{
 */
class NfcExtensionWriter {
public:
  NfcExtensionWriter(const NfcExtensionWriter &) = delete;
  NfcExtensionWriter &operator=(const NfcExtensionWriter &) = delete;
  /**
   * @brief Get the singleton of this object.
   * @return Reference to this object.
   *
   */
  static NfcExtensionWriter &getInstance();
  /**
   * @brief This function can be used by HAL to request control of
   *        NFCC to libnfc-nci. When control is provided to HAL it is
   *        notified through phNxpNciHal_control_granted.
   * @return void
   *
   */
  void requestHALcontrol();
  /**
   * @brief This function can be used by HAL to release the control of
   *        NFCC back to libnfc-nci.
   * @return void
   *
   */
  void releaseHALcontrol();
  /**
   * @brief This function write the data to NFCC through physical
   *        interface (e.g. I2C) using the NFCC driver interface.
   * @return It returns number of bytes successfully written to NFCC.
   *
   */
  NFCSTATUS write(const uint8_t *pBuffer, uint16_t wLength);
  /**
   * @brief updates the write completeion callback status
   * @param status specifies status of write written to controller
   * @return returns void
   *
   */
  void onWriteComplete(uint8_t status);
  /**
   * @brief handles the control granted callback
   * @return returns void
   *
   */
  void onhalControlGrant();
  /**
   * @brief stops the response timer started for write packet
   *        sent to controller
   * @param void
   * @return returns void
   *
   */
  void stopWriteRspTimer();

private:
  NfcExtensionWriter();
  ~NfcExtensionWriter();
  uint32_t writeRspTimeoutTimerId;
  uint32_t halCtrlTimeoutTimerId;
  void stopHalCtrlTimer();
};

#endif // NFC_EXTENSION_WRITER_H