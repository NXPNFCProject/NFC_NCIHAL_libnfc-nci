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

#include <ConfigHandler.h>
#include <PlatformAbstractionLayer.h>
#include <android-base/logging.h>
#include <android-base/strings.h>
#include <phNxpLog.h>
#include <stdio.h>
#include <ranges>

using namespace ::android::base;
ConfigHandler* ConfigHandler::sConfigHandler;

ConfigHandler::ConfigHandler() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter", __func__);
}

ConfigHandler::~ConfigHandler() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter", __func__);
}

ConfigHandler* ConfigHandler::getInstance() {
  if (sConfigHandler == nullptr) {
    sConfigHandler = new ConfigHandler();
  }
  return sConfigHandler;
}

map<string, ConfigValue> ConfigHandler::parseFromString(string input) {
  map<string, ConfigValue> configs;
  for (auto lineR : std::views::split(input, '\n')) {
    string line(lineR.begin(), lineR.end());
    line = Trim(line);
    if (line.empty()) continue;
    if (line.at(0) == '#') continue;
    if (line.at(0) == 0) continue;

    auto search = line.find('=');
    CHECK(search != string::npos);

    string key(Trim(line.substr(0, search)));
    string value_string(Trim(line.substr(search + 1, string::npos)));
    ConfigValue value;
    bool value_parsed = value.parseFromString(value_string);
    CHECK(value_parsed);
    configs.emplace(key, value);
  }
  return configs;
}

map<string, ConfigValue> ConfigHandler::loadNfcNxpConfig() {
  string confString = PlatformAbstractionLayer::getInstance()->getVendorParam(
      "libnfc-nxp.conf");
  map<string, ConfigValue> configs;
  if (!confString.empty()) {
    configs = parseFromString(confString);
  } else {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "%s No content from getVendorParam!", __func__);
  }
  return configs;
}

string ConfigHandler::getConfigValueStr(ConfigValue value) {
  string valueStr;
  if (value.getType() == ConfigValue::UNSIGNED) {
    valueStr = value.getUnsigned();
  } else if (value.getType() == ConfigValue::STRING) {
    valueStr = value.getString();
  } else if (value.getType() == ConfigValue::BYTES) {
    vector<uint8_t> values = value.getBytes();
    size_t bufferSize = values.size() * 5 + 1;
    char buffer[bufferSize];
    memset(buffer, 0, bufferSize);
    size_t offset = 0;
    for (uint8_t val : values) {
      offset += snprintf(buffer + offset, bufferSize - offset, "0x%02X ", val);
    }
    PlatformAbstractionLayer::getInstance()->palMemcpy(&valueStr, bufferSize,
                                                       &buffer, bufferSize);
  }
  return valueStr;
}

void ConfigHandler::adaptLegacySameKeyType(
    string oemKey, string aospKey, map<string, ConfigValue> nxpNfcConfig) {
  auto oemPair = nxpNfcConfig.find(oemKey);
  if (oemPair == nxpNfcConfig.end()) {
    oemPair = mpConfigMap->find(oemKey);
  }
  if (oemPair == mpConfigMap->end()) {
    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s %s Conf Not found!", __func__,
                   oemKey.c_str());
    return;
  }

  auto configValue = oemPair->second;
  if (strcmp(oemKey.c_str(), NAME_NXP_T4T_NFCEE_ENABLE) == 0) {
    configValue = ConfigValue(oemPair->second.getUnsigned() & 0x01);
  }

  // single oem config => single aosp config
  // it can be unsigned, string, array etc..
  // ConfigValue can be fetched directly from OemPair
  mpConfigMap->erase(aospKey);
  mpConfigMap->emplace(aospKey, configValue);
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s %s(%s)!", __func__,
                 aospKey.c_str(), getConfigValueStr(oemPair->second).c_str());
}

void ConfigHandler::adaptLegacyDiffKeyType(
    vector<string> oemKeys, string aospKey,
    map<string, ConfigValue> nxpNfcConfig) {
  vector<uint8_t> aospVals;
  for (string oemKey : oemKeys) {
    auto oemPair = nxpNfcConfig.find(oemKey);
    if (oemPair == nxpNfcConfig.end()) {
      oemPair = mpConfigMap->find(oemKey);
    }
    if (oemPair == mpConfigMap->end()) {
      NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s %s Conf Not found!",
                     __func__, oemKey.c_str());
      continue;
    }
    if (oemPair->second.getType() != ConfigValue::UNSIGNED) {
      NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN,
                     "%s Invalid value type '%d' for  %s!", __func__,
                     oemPair->second.getType(), oemKey.c_str());
      continue;
    }
    // multiple oem configs => single aosp config
    // add it to array
    aospVals.push_back(oemPair->second.getUnsigned());
  }
  if (!aospVals.empty()) {
    mpConfigMap->erase(aospKey);
    mpConfigMap->emplace(aospKey, ConfigValue(aospVals));
    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s %s(%s)!", __func__,
                   aospKey.c_str(),
                   getConfigValueStr(ConfigValue(aospVals)).c_str());
  }
}

void ConfigHandler::adaptLegacyConfigs(map<string, ConfigValue> nxpNfcConfig) {
  map<vector<string>, string> updateConfigMap{
      {{NAME_NXP_T4T_NFCEE_ENABLE}, NAME_T4T_NFCEE_ENABLE},
      {{NAME_OFF_HOST_SIM_PIPE_ID, NAME_OFF_HOST_SIM2_PIPE_ID,
        NAME_OFF_HOST_ESIM_PIPE_ID, NAME_OFF_HOST_ESIM2_PIPE_ID},
       NAME_OFF_HOST_SIM_PIPE_IDS},
  };
  for (auto const& [oemConfNames, aospConfName] : updateConfigMap) {
    if (oemConfNames.size() == 1) {
      adaptLegacySameKeyType(oemConfNames[0], aospConfName, nxpNfcConfig);
    } else {
      adaptLegacyDiffKeyType(oemConfNames, aospConfName, nxpNfcConfig);
    }
  }
  auto t3tPair = mpConfigMap->find(NAME_NFA_PROPRIETARY_CFG);
  if (t3tPair != mpConfigMap->end()) {
    vector<uint8_t> t3tValues = t3tPair->second.getBytes();
    // As per .conf byte[9] is T3T ID
    t3tValues.push_back(T3T_CONFIG_VAL);
    t3tPair->second = ConfigValue(t3tValues);
  }
}
map<ConfigHandler::OemRouteLoc, ConfigHandler::AospRouteLoc>
ConfigHandler::getRouteConfigValueMap() {
  map<OemRouteLoc, AospRouteLoc> routeValueMap{
      {OemRouteLoc::HOST, AospRouteLoc::HOST},
      {OemRouteLoc::ESE, AospRouteLoc::ESE},
      {OemRouteLoc::UICC, AospRouteLoc::UICC},
      {OemRouteLoc::UICC2, AospRouteLoc::UICC2},
      {OemRouteLoc::EUICC, AospRouteLoc::EUICC},
      {OemRouteLoc::EUICC2, AospRouteLoc::EUICC2},
  };
  return routeValueMap;
}

vector<uint8_t> ConfigHandler::getMappedRouteValue(
    vector<uint8_t> oemRouteVals) {
  vector<uint8_t> aospRouteVals;
  map<OemRouteLoc, AospRouteLoc> routeValMap = getRouteConfigValueMap();
  for (uint8_t oemRouteVal : oemRouteVals) {
    // Get corresponding enum value
    OemRouteLoc oemRoute = OemRouteLoc(oemRouteVal);
    auto aospRouteValuePair = routeValMap.find(oemRoute);
    if (aospRouteValuePair != routeValMap.end()) {
      aospRouteVals.push_back((uint8_t)aospRouteValuePair->second);
    }
  }
  return aospRouteVals;
}

void ConfigHandler::mapByteRouteConfig(vector<uint8_t> oemRouteVals,
                                       string aospRouteName) {
  vector<uint8_t> aospRouteVals = getMappedRouteValue(oemRouteVals);
  if (!aospRouteVals.empty()) {
    mpConfigMap->erase(aospRouteName);
    mpConfigMap->emplace(aospRouteName, ConfigValue(aospRouteVals));
    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s %s(%s) => (%s)!", __func__,
                   aospRouteName.c_str(),
                   getConfigValueStr(ConfigValue(oemRouteVals)).c_str(),
                   getConfigValueStr(ConfigValue(aospRouteVals)).c_str());
  } else {
    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "%s Corresponding Route Value Not found for'%s'!", __func__,
                   aospRouteName.c_str());
  }
}

void ConfigHandler::mapUnsignedRouteConfig(unsigned oemRouteVal,
                                           string aospRouteName) {
  vector<uint8_t> aospRouteVals =
      getMappedRouteValue({static_cast<uint8_t>(oemRouteVal)});
  if (!aospRouteVals.empty()) {
    mpConfigMap->erase(aospRouteName);
    mpConfigMap->emplace(aospRouteName, ConfigValue(aospRouteVals[0]));
    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s %s(%x) => (%x)!", __func__,
                   aospRouteName.c_str(), oemRouteVal, aospRouteVals[0]);
  } else {
    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "%s Corresponding Route Value Not found for'%s'!", __func__,
                   aospRouteName.c_str());
  }
}

map<string, string> ConfigHandler::getRouteConfigNameMap() {
  map<string, string> routeNameMap{
      {NAME_DEFAULT_ROUTE, NAME_DEFAULT_ROUTE},
      {NAME_DEFAULT_AID_ROUTE, NAME_DEFAULT_ROUTE},
      {NAME_DEFAULT_ISODEP_ROUTE, NAME_DEFAULT_ISODEP_ROUTE},
      {NAME_DEFAULT_FELICA_CLT_ROUTE, NAME_DEFAULT_NFCF_ROUTE},
      {NAME_DEFAULT_MIFARE_CLT_ROUTE, NAME_DEFAULT_OFFHOST_ROUTE},
      {NAME_DEFAULT_SYS_CODE_ROUTE, NAME_DEFAULT_SYS_CODE_ROUTE},
      {NAME_OFFHOST_ROUTE_ESE, NAME_OFFHOST_ROUTE_ESE},
      {NAME_OFFHOST_ROUTE_UICC, NAME_OFFHOST_ROUTE_UICC},
  };
  return routeNameMap;
}

void ConfigHandler::mapRouteConfig(map<string, ConfigValue> nxpNfcConfig) {
  map<string, string> routeNameMap = getRouteConfigNameMap();
  for (auto const& [oemRouteName, aospRouteName] : routeNameMap) {
    auto oemRoutePair = mpConfigMap->find(oemRouteName);
    if (oemRoutePair == mpConfigMap->end()) {
      oemRoutePair = nxpNfcConfig.find(oemRouteName);
    }
    if (oemRoutePair == nxpNfcConfig.end()) {
      NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Route Conf '%s' not found!",
                     __func__, oemRouteName.c_str());
      continue;
    }
    if (oemRoutePair->second.getType() == ConfigValue::UNSIGNED) {
      mapUnsignedRouteConfig(oemRoutePair->second.getUnsigned(), aospRouteName);
    } else if (oemRoutePair->second.getType() == ConfigValue::BYTES) {
      mapByteRouteConfig(oemRoutePair->second.getBytes(), aospRouteName);
    } else {
      NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN,
                     "%s Unkown Route value type '%d' for  %s!", __func__,
                     oemRoutePair->second.getType(), oemRouteName.c_str());
    }
  }
}

void ConfigHandler::updateConfig(map<string, ConfigValue>* pConfigMap) {
  if (pConfigMap != nullptr) {
    mpConfigMap = pConfigMap;
    map<string, ConfigValue> nxpNfcConfig = loadNfcNxpConfig();
    if (nxpNfcConfig.empty()) {
      NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN,
                     "%s Error in loading NfcNxp config!", __func__);
    } else {
      mapRouteConfig(nxpNfcConfig);
      adaptLegacyConfigs(nxpNfcConfig);
    }
  } else {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Invalid config map!",
                   __func__);
  }
}
