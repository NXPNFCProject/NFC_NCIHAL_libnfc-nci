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

#ifndef PLATFORM_BASE_H
#define PLATFORM_BASE_H

#include <NxpFeatures.h>
#include <cstdint>
#include <phNxpLog.h>

using namespace std;

class PlatformBase {
public:
  /**
   * @brief executes the provided function in another thread
   * @return true if thread creation is success else false
   *
   */
  bool palRunInThread(void *func(void *));

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
   * @brief      Compares the values stored in two memory blocks.
   *             This function compares the contents of two memory
   *             regions, byte by byte,
   *
   * @param[in]  pSrc1 Pointer to the first memory block.
   * @param[in]  pSrc2 Pointer to the second memory block.
   * @param[in]  dataSize No of byte to be compared.
   *
   * @return int Result of the memory comparison: 0 for equal,
   *         negative or positive for inequality.
   */
  int palMemcmp(const void *pSrc1, const void *pSrc2, size_t dataSize);

  /**
   * @brief this function is called to set the system properties
   * @param  key - Represent the name of property max 32 characters.
   * @param  value - Represent the value to set for the property max 92
   * characters.
   * @return return 0 : Success the property set successfully.
   *               -1 : Failure There was an error setting the property.
   *
   */
  int palPropertySet(const char *key, const char *value);

  /**
   * @brief this function returns the chip type
   * @param  void
   * @return return the chip type version
   *
   */
  tNFC_chipType palGetChipType();

  /**
   * @brief this function extract the chip type from the core rest ntf
   * @param  msg
   * @param  msg_len
   * @return return the chip type version
   *
   */
  tNFC_chipType processChipType(uint8_t *msg, uint16_t msg_len);

private:

  /**
   * @brief threadStartRoutine for palRunInAThread.
   * @return None
   *
   */
  static void *threadStartRoutine(void *arg);
};
/** @}*/
#endif // PLATFORM_BASE_H
