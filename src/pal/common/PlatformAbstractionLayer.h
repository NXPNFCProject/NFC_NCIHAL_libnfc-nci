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

#ifndef PLATFORM_ABSTRACTION_LAYER_H
#define PLATFORM_ABSTRACTION_LAYER_H

#include "PlatformBase.h"
#include "phNxpConfig.h"
#include "vector"

class PlatformAbstractionLayer : public PlatformBase {
public:
  PlatformAbstractionLayer(const PlatformAbstractionLayer &) = delete;
  PlatformAbstractionLayer &
  operator=(const PlatformAbstractionLayer &) = delete;

  /**
   * @brief Get the singleton of this object.
   * @return Reference to this object.
   *
   */
  static PlatformAbstractionLayer *getInstance();

  /**
   *
   * @brief           This function write the data to NFCC through physical
   *                  interface (e.g. I2C) using the PN7220 driver interface.
   *                  It enqueues the data and writer thread picks and process
   * it. Before sending the data to NFCC, phEMVCoHal_write_ext is called to
   * check if there is any extension processing is required for the NCI packet
   * being sent out.
   *
   * @param[in]       data_len length of the data to be written
   * @param[in]       p_data actual data to be written
   *
   * @return          uint16_t - returns the number of bytes written to
   * controller
   *
   */
  uint16_t palenQueueWrite(const uint8_t *pBuffer, uint16_t wLength);

  /**
   *
   * @brief           This function helps to send the response and notification
   *                  asynchronously to upper layer
   *
   * @param[in]       data_len length of the data to be written
   * @param[in]       p_data actual data to be written
   *
   * @return          uint16_t - SUCCESS or FAILURE
   *
   */
  uint16_t palenQueueRspNtf(const uint8_t *pBuffer, uint16_t wLength);

  /**
   * @brief This function can be used by HAL to request control of
   *        NFCC to libnfc-nci. When control is provided to HAL it is
   *        notified through phNxpNciHal_control_granted.
   * @return void
   *
   */
  void palRequestHALcontrol();

  /**
   * @brief This function can be used by HAL to release the control of
   *        NFCC back to libnfc-nci.
   * @return void
   *
   */
  void palReleaseHALcontrol();

  /**
   * @brief sends the NCI packet to upper layer
   * @param  data_len length of the NCI packet
   * @param  p_data data buffer pointer
   * @return void
   *
   */
  void palSendNfcDataCallback(uint16_t dataLen, const uint8_t *pData);

  /**
   * @brief Read byte array value from the config file.
   * @param name - name of the config param to read.
   * @param pValue - pointer to input buffer.
   * @param bufflen - input buffer length.
   * @param len - out parameter to return the number of bytes read from
   *                      config file, return -1 in case bufflen is not enough.
   * @return 1 if config param name is found in the config file, else 0
   *
   */
  uint8_t palGetNxpByteArrayValue(const char *name, char *pValue, long bufflen,
                                  long *len);

  /**
   * @brief API function for getting a numerical value of a setting
   * @param name - name of the config param to read.
   * @param pValue - pointer to input buffer.
   * @param len - sizeof required pValue
   * @return 1 if config param name is found in the config file, else 0
   */
  uint8_t palGetNxpNumValue(const char *name, void *pValue, unsigned long len);

  /**
   * @brief This function can be called to enable/disable the log levels
   * @param enable 0x01 to enable for 0x00 to disable
   * @return NFASTATUS_SUCCESS if success else NFCSTATUS_FAILED
   */
  uint8_t palEnableDisableDebugLog(uint8_t enable);

  /**
   * @brief Internal write function to run in seprate Thread.
   * @return None
   *
   */
  void enQueueWriteInternal(vector<uint8_t> buffer, uint16_t wLength);

  /**
   * @brief get the nxpnfc hidl/aidl service instance
   * @return None
   *
   */
  void getNxpNfcHal();

private:
  static PlatformAbstractionLayer
      *sPlatformAbstractionLayer; // singleton object
  /**
   * @brief Initialize member variables.
   * @return None
   *
   */
  PlatformAbstractionLayer();

  /**
   * @brief Release all resources.
   * @return None
   *
   */
  ~PlatformAbstractionLayer();
};
/** @}*/
#endif // PLATFORM_ABSTRACTION_LAYER_H
