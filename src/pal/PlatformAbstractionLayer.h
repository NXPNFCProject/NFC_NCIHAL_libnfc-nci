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

#ifndef PLATFORM_ABSTRACTION_LAYER_H
#define PLATFORM_ABSTRACTION_LAYER_H

#include <cstdint>
#include <phNfcStatus.h>
#include <phNxpLog.h>
#include <phOsalNfc_Timer.h>

/** \addtogroup PAL_API_INTERFACE
 *  @brief  interface to perform the platform independent functionality.
 *  @{
 */
class PlatformAbstractionLayer {
public:
  PlatformAbstractionLayer(const PlatformAbstractionLayer &) = delete;
  PlatformAbstractionLayer &
  operator=(const PlatformAbstractionLayer &) = delete;
  /*
   * Timer callback interface which will be called once registered timer
   * time out expires.
   *        TimerId  - Timer Id for which callback is called.
   *        pContext - Parameter to be passed to the callback function
   */
  typedef void (*pPal_TimerCallbck_t)(uint32_t timerId, void *pContext);

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
   * @brief  Allocates some memoryAllocates some memory
   *
   * @param[in] dwSize   Size, in uint32_t, to be allocated
   *
   * @return            NON-NULL value:  The memory is successfully allocated ;
   *                    the return value is a pointer to the allocated memory
   * location NULL:The operation is not successful.
   *
   */
  void *palMalloc(size_t dwSize);

  /**
   * @brief                Copies the values stored in the source memory to the
   *                       values stored in the destination memory.
   *
   * @param[in] pDest     Pointer to the Destination Memory
   * @param[in] pSrc      Pointer to the Source Memory
   * @param[in] dwSize    Number of bytes to be copied.
   *
   * @return    void
   */
  void palMemcpy(void *pDest, size_t destSize, const void *pSrc,
                 size_t srcSize);

  /**
   * @brief       Creates a timer which shall call back the specified function
   *              when the timer expires. Fails if OSAL module is not
   *              initialized or timers are already occupied
   *
   *
   * @param void
   *
   * @return    TimerId value of PH_OSALNFC_TIMER_ID_INVALID indicates that
   *            timer is not created
   */
  uint32_t palTimerCreate(void);

  /**
   * @brief           Starts the requested, already created, timer.
   *                  If the timer is already running, timer stops and restarts
   *                  with the new timeout value and new callback function in case
   *                  any ??????
   *                  Creates a timer which shall call back the specified function
   *                  when the timer expires
   *
   * @param           dwTimerId - valid timer ID obtained during timer creation
   *                  dwRegTimeCnt - requested timeout in milliseconds
   *                  pApplication_callback - application callback interface to be
   *                                          called when timer expires
   *                  pContext - caller context, to be passed to the application
   *                             callback function
   *
   * @return          NFC status:
   *                  NFCSTATUS_SUCCESS - the operation was successful
   *                  NFCSTATUS_NOT_INITIALISED - OSAL Module is not initialized
   *                  NFCSTATUS_INVALID_PARAMETER - invalid parameter passed to
   *                                                the function
   *                  PH_OSALNFC_TIMER_START_ERROR - timer could not be created
   *                                                 due to system error
   *
   **/
  NFCSTATUS palTimerStart(uint32_t dwTimerId, uint32_t dwRegTimeCnt,
                          pPal_TimerCallbck_t pApplication_callback,
                          void *pContext);
  /**
   *
   *
   * @brief           Stops already started timer
   *                  Allows to stop running timer. In case timer is stopped,
   *                  timer callback will not be notified any more
   *
   * @param           dwTimerId - valid timer ID obtained during timer creation
   *
   * @return           NFC status:
   *                  NFCSTATUS_SUCCESS - the operation was successful
   *                  NFCSTATUS_NOT_INITIALISED - OSAL Module is not initialized
   *                  NFCSTATUS_INVALID_PARAMETER - invalid parameter passed to
   *                                                the function
   *                  PH_OSALNFC_TIMER_STOP_ERROR - timer could not be stopped due
   *                                                to system error
   *
   **/
  NFCSTATUS palTimerStop(uint32_t dwTimerId);

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