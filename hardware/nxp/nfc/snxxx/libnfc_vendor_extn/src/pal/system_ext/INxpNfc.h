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
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <android/binder_interface_utils.h>
#ifdef BINDER_STABILITY_SUPPORT
#include <android/binder_stability.h>
#endif  // BINDER_STABILITY_SUPPORT

namespace aidl {
namespace vendor {
namespace nxp {
namespace nxpnfc_aidl {
class INxpNfcDelegator;

class INxpNfc : public ::ndk::ICInterface {
public:
  typedef INxpNfcDelegator DefaultDelegator;
  static const char* descriptor;
  INxpNfc();
  virtual ~INxpNfc();

  static inline const int32_t version = 1;
  static inline const std::string hash = "ad23ef9377549c53428e3795ebd2d3079246bee9";
  static constexpr uint32_t TRANSACTION_getVendorParam = FIRST_CALL_TRANSACTION + 0;
  static constexpr uint32_t TRANSACTION_resetEse = FIRST_CALL_TRANSACTION + 1;
  static constexpr uint32_t TRANSACTION_setNxpTransitConfig = FIRST_CALL_TRANSACTION + 2;
  static constexpr uint32_t TRANSACTION_setVendorParam = FIRST_CALL_TRANSACTION + 3;

  static std::shared_ptr<INxpNfc> fromBinder(const ::ndk::SpAIBinder& binder){ return nullptr;};
  static binder_status_t writeToParcel(AParcel* parcel, const std::shared_ptr<INxpNfc>& instance);
  static binder_status_t readFromParcel(const AParcel* parcel, std::shared_ptr<INxpNfc>* instance);
  static bool setDefaultImpl(const std::shared_ptr<INxpNfc>& impl);
  static const std::shared_ptr<INxpNfc>& getDefaultImpl();
  virtual ::ndk::ScopedAStatus getVendorParam(const std::string& in_key, std::string* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus resetEse(int64_t in_resetType, bool* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus setNxpTransitConfig(const std::string& in_transitConfValue, bool* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus setVendorParam(const std::string& in_key, const std::string& in_value, bool* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus getInterfaceVersion(int32_t* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus getInterfaceHash(std::string* _aidl_return) = 0;
private:
  static std::shared_ptr<INxpNfc> default_impl;
};
class INxpNfcDefault : public INxpNfc {
public:
  ::ndk::ScopedAStatus getVendorParam(const std::string& in_key, std::string* _aidl_return) override;
  ::ndk::ScopedAStatus resetEse(int64_t in_resetType, bool* _aidl_return) override;
  ::ndk::ScopedAStatus setNxpTransitConfig(const std::string& in_transitConfValue, bool* _aidl_return) override;
  ::ndk::ScopedAStatus setVendorParam(const std::string& in_key, const std::string& in_value, bool* _aidl_return) override;
  ::ndk::ScopedAStatus getInterfaceVersion(int32_t* _aidl_return) override;
  ::ndk::ScopedAStatus getInterfaceHash(std::string* _aidl_return) override;
  ::ndk::SpAIBinder asBinder() override;
  bool isRemote() override;
};
}  // namespace nxpnfc_aidl
}  // namespace nxp
}  // namespace vendor
}  // namespace aidl