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
#ifndef CONFIG_HANDLER_H
#define CONFIG_HANDLER_H

#include <PlatformExtension.h>
#include <phNxpConfig.h>
#include <map>
#include <string>
/**
 * @brief This class is responsible for supporting config property related
 * features in upgrade device.
 * 1. Supporting legacy NXP proprietary configs - Read the legacy config and map
 *    with equivalent AOSP config
 * 2. Configs added in next generation HAL will added to AOSP config
 *
 *  @{
 */
using namespace std;

class ConfigHandler {
 public:
  ConfigHandler(const ConfigHandler&) = delete;
  ConfigHandler& operator=(const ConfigHandler&) = delete;

  /**
   * @brief Get the singleton of this object.
   * @return Reference to this object.
   *
   */
  static ConfigHandler* getInstance();

  /**
   * @brief updates the vendor specific config into libNfc config map
   * \Note loads the vendor configuration, converts as per AOSP standards
   *        and adds/replaces the config and it's value
   * @param pConfigMap config map pointer received from libNfc
   * @return None
   *
   */
  void updateConfig(map<string, ConfigValue>* pConfigMap);
  /**
   * @brief Releases all the resources
   * @return None
   *
   */
  static inline void finalize() {
    if (sConfigHandler != nullptr) {
      delete (sConfigHandler);
      sConfigHandler = nullptr;
    }
  }

 private:
  static ConfigHandler* sConfigHandler;  // singleton object
  /**
   * @brief Route location values defined by OEM
   *
   */
  enum class OemRouteLoc {
    HOST = 0x00,
    ESE = 0x01,
    UICC = 0x02,
    UICC2 = 0x03,
    EUICC = 0x05,
    EUICC2 = 0x06
  };
  /**
   * @brief Route location values defined by AOSP
   *
   */
  enum class AospRouteLoc {
    HOST = 0x00,
    ESE = 0xC0,
    UICC = 0x80,
    UICC2 = 0x81,
    EUICC = 0xC1,
    EUICC2 = 0xC2,
  };
  /**
   * @brief Global map object from libnfc-nci
   *
   */
  map<string, ConfigValue>* mpConfigMap;

  const uint8_t T3T_CONFIG_VAL = 0x8B;
  /**
   * @brief Initialize member variables.
   * @return None
   *
   */
  ConfigHandler();

  /**
   * @brief Release all resources.
   * @return None
   *
   */
  ~ConfigHandler();

  /**
   * @brief Updates map by adding a aosp conf pair
   * with aosp conf name as key and conf value of oemKey as value.
   * @param oemKey name of the oem config
   * @param aospKey name of the aosp config
   * @param nxpNfcConfig map of configs present libnfc-nxp.conf
   * @return None
   *
   */
  void adaptLegacySameKeyType(string oemKey, string aospKey,
                                  map<string, ConfigValue> nxpNfcConfig);

  /**
   * @brief Updates map by adding a aosp conf pair
   * with aosp conf name as key and all the conf values of oemKeys as a value.
   * @param oemKeys name of the oem configs
   * @param aospKey name of the aosp config
   * @param nxpNfcConfig map of configs present libnfc-nxp.conf
   * @return None
   *
   */
  void adaptLegacyDiffKeyType(vector<string> oemKey, string aospKey,
                               map<string, ConfigValue> nxpNfcConfig);

  /**
   * @brief Adapt oem config by assigning Oem config values
   * to aosp config
   * @param nxpNfcConfig map of configs present libnfc-nxp.conf
   * @return None
   *
   */
  void adaptLegacyConfigs(const map<string, ConfigValue>& nxpNfcConfig);

  /**
   * @brief Maps the oem route configuration to the aosp defined route
   * configuration for the route configurations
   * @param nxpNfcConfig map of configs present libnfc-nxp.conf
   * @return None
   *
   */
  void mapRouteConfig(map<string, ConfigValue> nxpNfcConfig);

  /**
   * @brief parse the string to extract each config
   * @param input string of configs
   * @return map of configs which are successfully parsed
   *
   */
  map<string, ConfigValue> parseFromString(const std::string& input);

  /**
   * @brief Read the content of /vendor/etc/libnfc-nxp.conf
   *
   * @return map key as config name and value as a ConfigValue
   */
  map<string, ConfigValue> loadNfcNxpConfig();

  /**
   * @brief Constructs the routingNameMap which contains
   * oem rout configuration names whose values needs to be
   * mapped and add with aosp route configuration names
   * @return map with oem route conf name as key and corresponding to be
   * mapped aosp conf name as value
   *
   */
  map<string, string> getRouteConfigNameMap();

  /**
   * @brief Construct route configurations value map
   * @return map with #OemRouteLoc as key and corresponding #AospRouteLoc as
   * value
   *
   */
  map<OemRouteLoc, AospRouteLoc> getRouteConfigValueMap();

  /**
   * @brief Maps the given values of #OemRouteLoc to #AospRouteLoc
   * @param oemRouteVals #OemRouteLoc values to be mapped
   * @return vector of mapped #AospRouteLoc values if mapping is success else
   * empty vector
   *
   */
  vector<uint8_t> getMappedRouteValue(vector<uint8_t> oemRouteVals);

  /**
   * @brief map the given unsigned route config value and add it to the
   * #mpConfigMap
   * @param oemRouteVal oem route location which needs to mapped
   * @param aospRouteName name of the aosp route config name to be added in
   * #mpConfigMap
   * @return None
   *
   */
  void mapUnsignedRouteConfig(unsigned oemRouteVal, string aospRouteName);

  /**
   * @brief map the given byte array route config values and add it to the
   * #mpConfigMap
   * @param oemRouteVals oem route locations which need to mapped
   * @param aospRouteName name of the aosp route config name to be added in
   * #mpConfigMap
   * @return None
   *
   */
  void mapByteRouteConfig(vector<uint8_t> oemRouteVals, string aospRouteName);

  /**
   * @brief converts ConfigValue content to string
   * @param configValue Obj of ConfigValue to be converted to string
   * @return content of configValue if conversion is success else
   * empty string is returned
   *
   */
  string getConfigValueStr(ConfigValue configValue);
};
/** @}*/
#endif  // CONFIG_HANDLER_H
