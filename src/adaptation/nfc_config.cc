/*
 * Copyright 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "nfc_config.h"
#include "NfcAdaptation.h"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/parseint.h>
#include <android-base/strings.h>

#include <config.h>
#include <cutils/properties.h>

using namespace ::std;
using namespace ::android::base;
#define PATH_TRANSIT_CONF "/data/nfc/libnfc-nxpTransit.conf"
namespace {

std::string getPathInfoIfFileExists(const string& configName) {
  const vector<string> search_path = {"/odm/etc/", "/vendor/etc/",
                                      "/product/etc/", "/etc/"};
  for (string path : search_path) {
    path.append(configName);
    struct stat file_stat;
    if (stat(path.c_str(), &file_stat) != 0) continue;
    if (S_ISREG(file_stat.st_mode)) return path;
  }
  return "";
}

std::string findConfigPath() {
  char valueStr[PROPERTY_VALUE_MAX] = {0};
  const string defaultConfigFileName = "libnfc-nci.conf";
  string config_file_name = "libnfc-nci";
  // update config file based on system property
  int len = property_get("persist.nfc.config_file_name", valueStr, "");
  if (len > 0) {
    config_file_name = config_file_name + "_" + valueStr + ".conf";
  } else {
    config_file_name = defaultConfigFileName;
  }
  LOG(INFO) << __func__
            << " nci config file name = " << config_file_name.c_str();

  string filePath = getPathInfoIfFileExists(config_file_name);
  if (filePath.length() == 0) {
    LOG(INFO) << __func__ << " Unable to find nci Config file - "
              << config_file_name.c_str() << " . Using Default Config file.";
    filePath = getPathInfoIfFileExists(defaultConfigFileName);
  }

  return filePath;
}

}  // namespace

void NfcConfig::loadConfig() {
  string config_path = findConfigPath();
  CHECK(config_path != "");
  config_.parseFromFile(config_path);
  struct stat file_stat;
  /* Read Transit configs if available */
  if (stat(PATH_TRANSIT_CONF, &file_stat) == 0)
    config_.parseFromFile(PATH_TRANSIT_CONF);
  /* Read vendor specific configs */
  NfcAdaptation& theInstance = NfcAdaptation::GetInstance();
  std::map<std::string, ConfigValue> configMap;
  theInstance.GetVendorConfigs(configMap);
  theInstance.GetNxpConfigs(configMap);
  for (auto config : configMap) {
    config_.addConfig(config.first, config.second);
  }
}

NfcConfig::NfcConfig() { loadConfig(); }

NfcConfig& NfcConfig::getInstance() {
  static NfcConfig theInstance;
  if (theInstance.config_.isEmpty()) {
    theInstance.loadConfig();
  }
  return theInstance;
}

bool NfcConfig::hasKey(const std::string& key) {
  return getInstance().config_.hasKey(key);
}

std::string NfcConfig::getString(const std::string& key) {
  return getInstance().config_.getString(key);
}

std::string NfcConfig::getString(const std::string& key,
                                 std::string default_value) {
  if (hasKey(key)) return getString(key);
  return default_value;
}

unsigned NfcConfig::getUnsigned(const std::string& key) {
  return getInstance().config_.getUnsigned(key);
}

unsigned NfcConfig::getUnsigned(const std::string& key,
                                unsigned default_value) {
  if (hasKey(key)) return getUnsigned(key);
  return default_value;
}

std::vector<uint8_t> NfcConfig::getBytes(const std::string& key) {
  return getInstance().config_.getBytes(key);
}

void NfcConfig::clear() { getInstance().config_.clear(); }