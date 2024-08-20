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

#ifndef NCI_COMMAND_BUILDER_H
#define NCI_COMMAND_BUILDER_H

#include <cstdint>

/** \addtogroup NCI_CMD_BUIDLER_API_INTERFACE
 *  @brief  interface to form and send the NCI command to controller
 *  @{
 */
class NciCommandBuilder {
public:
  NciCommandBuilder(const NciCommandBuilder &) = delete;
  NciCommandBuilder &operator=(const NciCommandBuilder &) = delete;
  /**
   * @brief Defines the data elements for NCI discover map command
   *
   */
  struct {
    uint8_t protocol;
    uint8_t mode;
    uint8_t intf_type;
  } NciDiscoverMaps;

  /**
   * @brief Get the singleton of this object.
   * @return Reference to this object.
   *
   */
  static NciCommandBuilder *getInstance();

  /**
   * @brief      compose and send RF Management DISCOVER MAP command
   * @param[in]  num - number of RF interfaces
   * @param[in]  discMaps - object specifies the interface and protocol mapping
   * @return     Reference to this object.
   *
   */
  uint8_t sendDiscoverMapCmd(uint8_t num, struct NciDiscoverMaps *discMaps);

private:
  static NciCommandBuilder *sNciCommandBuilder; // singleton object
                                                /**
                                                 * @brief Initialize member variables.
                                                 * @return None
                                                 *
                                                 */
  NciCommandBuilder();

  /**
   * @brief Release all resources.
   * @return None
   *
   */
  ~NciCommandBuilder();
};
/** @}*/
#endif // NCI_COMMAND_BUILDER_H