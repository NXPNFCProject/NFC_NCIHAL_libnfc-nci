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
/******************************************************************************
 *
 *  Copyright 2018-2020 NXP
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
 ******************************************************************************/

#include "config.h"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/parseint.h>
#include <android-base/strings.h>

using namespace ::std;
using namespace ::android::base;

namespace {

bool parseBytesString(std::string in, std::vector<uint8_t>& out) {
  vector<string> values = Split(in, ":");
  if (values.size() == 0) return false;
  for (const string& value : values) {
    if (value.length() != 2) return false;
    uint8_t tmp = 0;
    string hexified = "0x";
    hexified.append(value);
    if (!ParseUint(hexified.c_str(), &tmp)) return false;
    out.push_back(tmp);
  }
  return true;
}

}  // namespace

ConfigValue::ConfigValue() {
  type_ = UNSIGNED;
  value_unsigned_ = 0;
}

ConfigValue::ConfigValue(std::string value) {
  // Don't allow empty strings
  CHECK(!(value.empty()));
  type_ = STRING;
  value_string_ = value;
  value_unsigned_ = 0;
}

ConfigValue::ConfigValue(unsigned value) {
  type_ = UNSIGNED;
  value_unsigned_ = value;
}

ConfigValue::ConfigValue(std::vector<int8_t> value) {
  CHECK(!(value.empty()));
  type_ = BYTES;
  value_bytes_ = std::vector<uint8_t>(value.begin(), value.end());
  value_unsigned_ = 0;
}

ConfigValue::ConfigValue(std::vector<uint8_t> value) {
  CHECK(!(value.empty()));
  type_ = BYTES;
  value_bytes_ = value;
  value_unsigned_ = 0;
}

ConfigValue::Type ConfigValue::getType() const { return type_; }

std::string ConfigValue::getString() const {
  CHECK(type_ == STRING);
  return value_string_;
};

unsigned ConfigValue::getUnsigned() const {
  CHECK(type_ == UNSIGNED);
  return value_unsigned_;
};

std::vector<uint8_t> ConfigValue::getBytes() const {
  CHECK(type_ == BYTES);
  return value_bytes_;
};

bool ConfigValue::parseFromString(std::string in) {
  if (in.length() > 1 && in[0] == '"' && in[in.length() - 1] == '"') {
    CHECK(in.length() > 2);  // Don't allow empty strings
    type_ = STRING;
    value_string_ = in.substr(1, in.length() - 2);
    return true;
  }

  if (in.length() > 1 && in[0] == '{' && in[in.length() - 1] == '}') {
    CHECK(in.length() >= 4);  // Needs at least one byte
    type_ = BYTES;
    return parseBytesString(in.substr(1, in.length() - 2), value_bytes_);
  }

  unsigned tmp = 0;
  if (ParseUint(in.c_str(), &tmp)) {
    type_ = UNSIGNED;
    value_unsigned_ = tmp;
    return true;
  }

  return false;
}

void ConfigFile::addConfig(const std::string& key, ConfigValue& value) {
  CHECK(!hasKey(key));
  values_.emplace(key, value);
}

#if(NXP_EXTNS == TRUE)
bool ConfigFile::updateConfig(const std::string& key, ConfigValue& value) {
  std::map<std::string, ConfigValue>::iterator it = values_.find(key);
  if (it != values_.end() && isUpdateAllowed(key)) {
    it->second = value;
    return true;
  }
  return false;
}

bool ConfigFile::isUpdateAllowed(const std::string& key) {
  if ((key.compare("P2P_LISTEN_TECH_MASK") == 0) ||
      (key.compare("HOST_LISTEN_TECH_MASK") == 0) ||
      (key.compare("UICC_LISTEN_TECH_MASK") == 0) ||
      (key.compare("POLLING_TECH_MASK") == 0))
    return true;
  return false;
}
#endif

void ConfigFile::parseFromFile(const std::string& file_name) {
  string config;
  bool config_read = ReadFileToString(file_name, &config);
  CHECK(config_read);
  LOG(INFO) << "ConfigFile - Parsing file '" << file_name << "'";
#if(NXP_EXTNS == TRUE)
  cur_file_name_ = file_name;
#endif
  parseFromString(config);
}

void ConfigFile::parseFromString(const std::string& config) {
  stringstream ss(config);
  string line;
  while (getline(ss, line)) {
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

#if(NXP_EXTNS == TRUE)
    if (cur_file_name_.find("nxpTransit") != std::string::npos) {
      if (updateConfig(key, value))
        LOG(INFO) << "ConfigFile Updated - [" << key << "] = " << value_string;
    } else {
      addConfig(key, value);
      LOG(INFO) << "ConfigFile - [" << key << "] = " << value_string;
    }
  }
  cur_file_name_ = "";
#endif
}

bool ConfigFile::hasKey(const std::string& key) {
  return values_.count(key) != 0;
}

ConfigValue& ConfigFile::getValue(const std::string& key) {
  CHECK(values_.find(key) != values_.end());
  auto search = values_.find(key);
  return search->second;
}

std::string ConfigFile::getString(const std::string& key) {
  return getValue(key).getString();
}

unsigned ConfigFile::getUnsigned(const std::string& key) {
  return getValue(key).getUnsigned();
}

std::vector<uint8_t> ConfigFile::getBytes(const std::string& key) {
  return getValue(key).getBytes();
}

bool ConfigFile::isEmpty() { return values_.empty(); }
void ConfigFile::clear() { values_.clear(); }
