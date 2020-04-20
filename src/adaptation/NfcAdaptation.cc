/******************************************************************************
 *
 *  Copyright (C) 1999-2012 Broadcom Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
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
/******************************************************************************
 *
 *  The original Work has been changed by NXP Semiconductors.
 *
 *  Copyright (C) 2015-2018 NXP Semiconductors
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
#include "NfcAdaptation.h"
#include <android-base/stringprintf.h>
#include <android/hardware/nfc/1.0/types.h>
#include <android/hardware/nfc/1.1/INfc.h>
#include <android/hardware/nfc/1.2/INfc.h>
#include <vendor/nxp/nxpnfclegacy/1.0/INxpNfcLegacy.h>
#include <vendor/nxp/nxpnfclegacy/1.0/types.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <cutils/properties.h>
#include <hidl/LegacySupport.h>
#include <hwbinder/ProcessState.h>
#include <vector>
#include "debug_nfcsnoop.h"
#include "nfa_api.h"
#include "nfa_rw_api.h"
#include "nfc_config.h"
#include "nfc_int.h"
#include "nfc_target.h"


using android::OK;
using android::sp;
using android::status_t;

using android::base::StringPrintf;
using android::hardware::ProcessState;
using android::hardware::Return;
using android::hardware::Void;
using android::hardware::nfc::V1_0::INfc;
using android::hardware::nfc::V1_1::PresenceCheckAlgorithm;
using INfcV1_1 = android::hardware::nfc::V1_1::INfc;
using INfcV1_2 = android::hardware::nfc::V1_2::INfc;
using NfcVendorConfigV1_1 = android::hardware::nfc::V1_1::NfcConfig;
using NfcVendorConfigV1_2 = android::hardware::nfc::V1_2::NfcConfig;
using android::hardware::nfc::V1_1::INfcClientCallback;
using android::hardware::hidl_vec;
using vendor::nxp::nxpnfc::V1_0::INxpNfc;
using android::hardware::configureRpcThreadpool;
using ::android::hardware::hidl_death_recipient;
using ::android::wp;
using ::android::hidl::base::V1_0::IBase;
using vendor::nxp::nxpnfclegacy::V1_0::NxpNfcHalConfig;

extern bool nfc_debug_enabled;

extern void GKI_shutdown();
extern void verify_stack_non_volatile_store();
extern void delete_stack_non_volatile_store(bool forceDelete);

NfcAdaptation* NfcAdaptation::mpInstance = nullptr;
ThreadMutex NfcAdaptation::sLock;
android::Mutex sIoctlMutex;
sp<INxpNfcLegacy> NfcAdaptation::mHalNxpNfcLegacy;
sp<INxpNfc> NfcAdaptation::mHalNxpNfc;
sp<INfc> NfcAdaptation::mHal;
sp<INfcV1_1> NfcAdaptation::mHal_1_1;
sp<INfcV1_2> NfcAdaptation::mHal_1_2;
INfcClientCallback* NfcAdaptation::mCallback;
tHAL_NFC_CBACK* NfcAdaptation::mHalCallback = nullptr;
tHAL_NFC_DATA_CBACK* NfcAdaptation::mHalDataCallback = nullptr;
ThreadCondVar NfcAdaptation::mHalOpenCompletedEvent;

#if (NXP_EXTNS == TRUE)
ThreadCondVar NfcAdaptation::mHalCoreResetCompletedEvent;
ThreadCondVar NfcAdaptation::mHalCoreInitCompletedEvent;
ThreadCondVar NfcAdaptation::mHalInitCompletedEvent;
#define SIGNAL_NONE 0
#define SIGNAL_SIGNALED 1
static uint8_t isSignaled = SIGNAL_NONE;
static uint8_t evt_status;
#endif

bool nfc_debug_enabled = false;
std::string nfc_storage_path;
uint8_t appl_dta_mode_flag = 0x00;
bool isDownloadFirmwareCompleted = false;

extern tNFA_DM_CFG nfa_dm_cfg;
extern tNFA_PROPRIETARY_CFG nfa_proprietary_cfg;
extern tNFA_HCI_CFG nfa_hci_cfg;
extern uint8_t nfa_ee_max_ee_cfg;
extern bool nfa_poll_bail_out_mode;

// Whitelist for hosts allowed to create a pipe
// See ADM_CREATE_PIPE command in the ETSI test specification
// ETSI TS 102 622, section 6.1.3.1
static std::vector<uint8_t> host_whitelist;

namespace {
void initializeGlobalDebugEnabledFlag() {
  nfc_debug_enabled =
      (NfcConfig::getUnsigned(NAME_NFC_DEBUG_ENABLED, 0) != 0) ? true : false;

  char valueStr[PROPERTY_VALUE_MAX] = {0};
  int len = property_get("nfc.debug_enabled", valueStr, "");
  if (len > 0) {
    // let Android property override .conf variable
    unsigned debug_enabled = 0;
    sscanf(valueStr, "%u", &debug_enabled);
    nfc_debug_enabled = (debug_enabled == 0) ? false : true;
  }

  DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf("%s: level=%u", __func__, nfc_debug_enabled);
}
}  // namespace

class NfcClientCallback : public INfcClientCallback {
 public:
  NfcClientCallback(tHAL_NFC_CBACK* eventCallback,
                    tHAL_NFC_DATA_CBACK dataCallback) {
    mEventCallback = eventCallback;
    mDataCallback = dataCallback;
  };
  virtual ~NfcClientCallback() = default;
  Return<void> sendEvent_1_1(
      ::android::hardware::nfc::V1_1::NfcEvent event,
      ::android::hardware::nfc::V1_0::NfcStatus event_status) override {
    mEventCallback((uint8_t)event, (tHAL_NFC_STATUS)event_status);
    return Void();
  };
  Return<void> sendEvent(
      ::android::hardware::nfc::V1_0::NfcEvent event,
      ::android::hardware::nfc::V1_0::NfcStatus event_status) override {
    mEventCallback((uint8_t)event, (tHAL_NFC_STATUS)event_status);
    return Void();
  };
  Return<void> sendData(
      const ::android::hardware::nfc::V1_0::NfcData& data) override {
    ::android::hardware::nfc::V1_0::NfcData copy = data;
    mDataCallback(copy.size(), &copy[0]);
    return Void();
  };

 private:
  tHAL_NFC_CBACK* mEventCallback;
  tHAL_NFC_DATA_CBACK* mDataCallback;
};

class NfcDeathRecipient : public hidl_death_recipient {
 public:
  android::sp<android::hardware::nfc::V1_0::INfc> mNfcDeathHal;
  NfcDeathRecipient(android::sp<android::hardware::nfc::V1_0::INfc> &mHal) {
    mNfcDeathHal = mHal;
  }

  virtual void serviceDied(
      uint64_t /* cookie */,
      const wp<::android::hidl::base::V1_0::IBase>& /* who */) {
    ALOGE("NfcDeathRecipient::serviceDied - Nfc service died");
    mNfcDeathHal->unlinkToDeath(this);
    mNfcDeathHal = nullptr;
    abort();
  }
};

/*******************************************************************************
**
** Function:    NfcAdaptation::NfcAdaptation()
**
** Description: class constructor
**
** Returns:     none
**
*******************************************************************************/
NfcAdaptation::NfcAdaptation() {
  mNfcHalDeathRecipient = new NfcDeathRecipient(mHal);
  mCurrentIoctlData = NULL;
  memset(&mHalEntryFuncs, 0, sizeof(mHalEntryFuncs));
}

/*******************************************************************************
**
** Function:    NfcAdaptation::~NfcAdaptation()
**
** Description: class destructor
**
** Returns:     none
**
*******************************************************************************/
NfcAdaptation::~NfcAdaptation() { mpInstance = nullptr; }

/*******************************************************************************
**
** Function:    NfcAdaptation::GetInstance()
**
** Description: access class singleton
**
** Returns:     pointer to the singleton object
**
*******************************************************************************/
NfcAdaptation& NfcAdaptation::GetInstance() {
  AutoThreadMutex a(sLock);

  if (!mpInstance) {
    mpInstance = new NfcAdaptation;
    mpInstance->InitializeHalDeviceContext();
  }
  return *mpInstance;
}

void NfcAdaptation::GetNxpConfigs(
    std::map<std::string, ConfigValue>& configMap) {
  NxpAdaptationConfig mNxpAdaptationConfig;
  HalGetNxpConfig(mNxpAdaptationConfig);
  configMap.emplace(
      NAME_NXP_ESE_LISTEN_TECH_MASK,
      ConfigValue(mNxpAdaptationConfig.ese_listen_tech_mask));
  configMap.emplace(
      NAME_NXP_DEFAULT_NFCEE_DISC_TIMEOUT,
      ConfigValue(mNxpAdaptationConfig.default_nfcee_disc_timeout));
  configMap.emplace(
      NAME_NXP_DEFAULT_NFCEE_TIMEOUT,
      ConfigValue(mNxpAdaptationConfig.default_nfcee_timeout));
  configMap.emplace(
      NAME_NXP_ESE_WIRED_PRT_MASK,
      ConfigValue(mNxpAdaptationConfig.ese_wired_prt_mask));
  configMap.emplace(
      NAME_NXP_UICC_WIRED_PRT_MASK,
      ConfigValue(mNxpAdaptationConfig.uicc_wired_prt_mask));
  configMap.emplace(
      NAME_NXP_WIRED_MODE_RF_FIELD_ENABLE,
      ConfigValue(mNxpAdaptationConfig.wired_mode_rf_field_enable));
  configMap.emplace(
      NAME_AID_BLOCK_ROUTE,
      ConfigValue(mNxpAdaptationConfig.aid_block_route));

  configMap.emplace(
      NAME_NXP_ESE_POWER_DH_CONTROL,
      ConfigValue(mNxpAdaptationConfig.esePowerDhControl));
  configMap.emplace(NAME_NXP_SWP_RD_TAG_OP_TIMEOUT,
                    ConfigValue(mNxpAdaptationConfig.tagOpTimeout));
  configMap.emplace(
      NAME_NXP_LOADER_SERICE_VERSION,
      ConfigValue(mNxpAdaptationConfig.loaderServiceVersion));
  configMap.emplace(
      NAME_NXP_DEFAULT_NFCEE_DISC_TIMEOUT,
      ConfigValue(mNxpAdaptationConfig.defaultNfceeDiscTimeout));
  configMap.emplace(NAME_NXP_DUAL_UICC_ENABLE,
                    ConfigValue(mNxpAdaptationConfig.dualUiccEnable));
  configMap.emplace(
      NAME_NXP_CE_ROUTE_STRICT_DISABLE,
      ConfigValue(mNxpAdaptationConfig.ceRouteStrictDisable));
  configMap.emplace(
      NAME_OS_DOWNLOAD_TIMEOUT_VALUE,
      ConfigValue(mNxpAdaptationConfig.osDownloadTimeoutValue));
  configMap.emplace(
      NAME_DEFAULT_AID_ROUTE,
      ConfigValue(mNxpAdaptationConfig.defaultAidRoute));
  configMap.emplace(
      NAME_DEFAULT_AID_PWR_STATE,
      ConfigValue(mNxpAdaptationConfig.defaultAidPwrState));
  configMap.emplace(
      NAME_DEFAULT_ISODEP_PWR_STATE,
      ConfigValue(mNxpAdaptationConfig.defaultRoutePwrState));
  configMap.emplace(
      NAME_DEFAULT_OFFHOST_PWR_STATE,
      ConfigValue(mNxpAdaptationConfig.defaultOffHostPwrState));
  configMap.emplace(
      NAME_NXP_JCOPDL_AT_BOOT_ENABLE,
      ConfigValue(mNxpAdaptationConfig.jcopDlAtBootEnable));
  configMap.emplace(
      NAME_NXP_DEFAULT_NFCEE_TIMEOUT,
      ConfigValue(mNxpAdaptationConfig.defaultNfceeTimeout));
  configMap.emplace(NAME_NXP_NFC_CHIP,
                    ConfigValue(mNxpAdaptationConfig.nxpNfcChip));
  configMap.emplace(
      NAME_NXP_CORE_SCRN_OFF_AUTONOMOUS_ENABLE,
      ConfigValue(mNxpAdaptationConfig.coreScrnOffAutonomousEnable));
  configMap.emplace(
      NAME_NXP_P61_JCOP_DEFAULT_INTERFACE,
      ConfigValue(mNxpAdaptationConfig.p61JcopDefaultInterface));
  configMap.emplace(NAME_NXP_AGC_DEBUG_ENABLE,
                    ConfigValue(mNxpAdaptationConfig.agcDebugEnable));
  configMap.emplace(
      NAME_DEFAULT_NFCF_PWR_STATE,
      ConfigValue(mNxpAdaptationConfig.felicaCltPowerState));
  configMap.emplace(
      NAME_NXP_HCEF_CMD_RSP_TIMEOUT_VALUE,
      ConfigValue(mNxpAdaptationConfig.cmdRspTimeoutValue));
  configMap.emplace(
      NAME_CHECK_DEFAULT_PROTO_SE_ID,
      ConfigValue(mNxpAdaptationConfig.checkDefaultProtoSeId));
  configMap.emplace(
      NAME_NXP_NFCC_PASSIVE_LISTEN_TIMEOUT,
      ConfigValue(mNxpAdaptationConfig.nfccPassiveListenTimeout));
  configMap.emplace(
      NAME_NXP_NFCC_STANDBY_TIMEOUT,
      ConfigValue(mNxpAdaptationConfig.nfccStandbyTimeout));
  configMap.emplace(NAME_NXP_WM_MAX_WTX_COUNT,
                    ConfigValue(mNxpAdaptationConfig.wmMaxWtxCount));
  configMap.emplace(
      NAME_NXP_NFCC_RF_FIELD_EVENT_TIMEOUT,
      ConfigValue(mNxpAdaptationConfig.nfccRfFieldEventTimeout));
  configMap.emplace(
      NAME_NXP_ALLOW_WIRED_IN_MIFARE_DESFIRE_CLT,
      ConfigValue(mNxpAdaptationConfig.allowWiredInMifareDesfireClt));
  configMap.emplace(
      NAME_NXP_DWP_INTF_RESET_ENABLE,
      ConfigValue(mNxpAdaptationConfig.dwpIntfResetEnable));
  configMap.emplace(
      NAME_NXPLOG_HAL_LOGLEVEL,
      ConfigValue(mNxpAdaptationConfig.nxpLogHalLoglevel));
  configMap.emplace(
      NAME_NXPLOG_EXTNS_LOGLEVEL,
      ConfigValue(mNxpAdaptationConfig.nxpLogExtnsLogLevel));
  configMap.emplace(
      NAME_NXPLOG_TML_LOGLEVEL,
      ConfigValue(mNxpAdaptationConfig.nxpLogTmlLogLevel));
  configMap.emplace(
      NAME_NXPLOG_FWDNLD_LOGLEVEL,
      ConfigValue(mNxpAdaptationConfig.nxpLogFwDnldLogLevel));
  configMap.emplace(
      NAME_NXPLOG_NCIX_LOGLEVEL,
      ConfigValue(mNxpAdaptationConfig.nxpLogNcixLogLevel));
  configMap.emplace(
      NAME_NXPLOG_NCIR_LOGLEVEL,
      ConfigValue(mNxpAdaptationConfig.nxpLogNcirLogLevel));
  configMap.emplace(NAME_NFA_CONFIG_FORMAT,
                    ConfigValue(mNxpAdaptationConfig.scrCfgFormat));
  configMap.emplace(NAME_ETSI_READER_ENABLE,
                    ConfigValue(mNxpAdaptationConfig.etsiReaderEnable));
  configMap.emplace(NAME_DEFAULT_TECH_ABF_ROUTE,
                    ConfigValue(mNxpAdaptationConfig.techAbfRoute));
  configMap.emplace(NAME_DEFAULT_TECH_ABF_PWR_STATE,
                    ConfigValue(mNxpAdaptationConfig.techAbfPwrState));
  configMap.emplace(NAME_WTAG_SUPPORT,
                    ConfigValue(mNxpAdaptationConfig.wTagSupport));
  configMap.emplace(NAME_DEFAULT_T4TNFCEE_AID_POWER_STATE,
                    ConfigValue(mNxpAdaptationConfig.t4tNfceePwrState));
  if (mNxpAdaptationConfig.scrResetEmvco.len) {
    std::vector scrResetEmvcoCmd(
        mNxpAdaptationConfig.scrResetEmvco.cmd,
        mNxpAdaptationConfig.scrResetEmvco.cmd +
            mNxpAdaptationConfig.scrResetEmvco.len);
    configMap.emplace(NAME_NXP_PROP_RESET_EMVCO_CMD,
                      ConfigValue(scrResetEmvcoCmd));
  }
}

void NfcAdaptation::GetVendorConfigs(
    std::map<std::string, ConfigValue>& configMap) {
  NfcVendorConfigV1_2 configValue;
  if (mHal_1_2) {
    mHal_1_2->getConfig_1_2(
        [&configValue](NfcVendorConfigV1_2 config) { configValue = config; });
  } else if (mHal_1_1) {
    mHal_1_1->getConfig([&configValue](NfcVendorConfigV1_1 config) {
      configValue.v1_1 = config;
      configValue.defaultIsoDepRoute = 0x00;
    });
  }

  if (mHal_1_1 || mHal_1_2) {
    std::vector<uint8_t> nfaPropCfg = {
        configValue.v1_1.nfaProprietaryCfg.protocol18092Active,
        configValue.v1_1.nfaProprietaryCfg.protocolBPrime,
        configValue.v1_1.nfaProprietaryCfg.protocolDual,
        configValue.v1_1.nfaProprietaryCfg.protocol15693,
        configValue.v1_1.nfaProprietaryCfg.protocolKovio,
        configValue.v1_1.nfaProprietaryCfg.protocolMifare,
        configValue.v1_1.nfaProprietaryCfg.discoveryPollKovio,
        configValue.v1_1.nfaProprietaryCfg.discoveryPollBPrime,
        configValue.v1_1.nfaProprietaryCfg.discoveryListenBPrime};
    configMap.emplace(NAME_NFA_PROPRIETARY_CFG, ConfigValue(nfaPropCfg));
    configMap.emplace(NAME_NFA_POLL_BAIL_OUT_MODE,
                      ConfigValue(configValue.v1_1.nfaPollBailOutMode ? 1 : 0));
    configMap.emplace(NAME_DEFAULT_OFFHOST_ROUTE,
                      ConfigValue(configValue.v1_1.defaultOffHostRoute));
    if (configValue.offHostRouteUicc.size() != 0) {
      configMap.emplace(NAME_OFFHOST_ROUTE_UICC,
                        ConfigValue(configValue.offHostRouteUicc));
    }
    if (configValue.offHostRouteEse.size() != 0) {
      configMap.emplace(NAME_OFFHOST_ROUTE_ESE,
                        ConfigValue(configValue.offHostRouteEse));
    }
    configMap.emplace(NAME_DEFAULT_ISODEP_ROUTE,
                          ConfigValue(configValue.defaultIsoDepRoute));
    configMap.emplace(NAME_DEFAULT_ROUTE,
                      ConfigValue(configValue.v1_1.defaultRoute));
    configMap.emplace(NAME_DEFAULT_NFCF_ROUTE,
                      ConfigValue(configValue.v1_1.defaultOffHostRouteFelica));
    configMap.emplace(NAME_DEFAULT_SYS_CODE_ROUTE,
                      ConfigValue(configValue.v1_1.defaultSystemCodeRoute));
    configMap.emplace(
        NAME_DEFAULT_SYS_CODE_PWR_STATE,
        ConfigValue(configValue.v1_1.defaultSystemCodePowerState));
    configMap.emplace(NAME_OFF_HOST_SIM_PIPE_ID,
                      ConfigValue(configValue.v1_1.offHostSIMPipeId));
    configMap.emplace(NAME_OFF_HOST_ESE_PIPE_ID,
                      ConfigValue(configValue.v1_1.offHostESEPipeId));
    configMap.emplace(NAME_ISO_DEP_MAX_TRANSCEIVE,
                      ConfigValue(configValue.v1_1.maxIsoDepTransceiveLength));
    if (configValue.v1_1.hostWhitelist.size() != 0) {
      configMap.emplace(NAME_DEVICE_HOST_WHITE_LIST,
                        ConfigValue(configValue.v1_1.hostWhitelist));
    }
    /* For Backwards compatibility */
    if (configValue.v1_1.presenceCheckAlgorithm ==
        PresenceCheckAlgorithm::ISO_DEP_NAK) {
      configMap.emplace(NAME_PRESENCE_CHECK_ALGORITHM,
                        ConfigValue((uint32_t)NFA_RW_PRES_CHK_ISO_DEP_NAK));
    } else {
      configMap.emplace(
          NAME_PRESENCE_CHECK_ALGORITHM,
          ConfigValue((uint32_t)configValue.v1_1.presenceCheckAlgorithm));
    }
  }
}

/*******************************************************************************
**
** Function:    NfcAdaptation::Initialize()
**
** Description: class initializer
**
** Returns:     none
**
*******************************************************************************/
void NfcAdaptation::Initialize() {
  const char* func = "NfcAdaptation::Initialize";
  const char* argv[] = {"libnfc_nci"};
  // Init log tag
  base::CommandLine::Init(1, argv);

  // Android already logs thread_id, proc_id, timestamp, so disable those.
  logging::SetLogItems(false, false, false, false);
  initializeGlobalDebugEnabledFlag();

  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: enter", func);

  nfc_storage_path = NfcConfig::getString(NAME_NFA_STORAGE, "/data/nfc");

  if (NfcConfig::hasKey(NAME_NFA_DM_CFG)) {
    std::vector<uint8_t> dm_config = NfcConfig::getBytes(NAME_NFA_DM_CFG);
    if (dm_config.size() > 0) nfa_dm_cfg.auto_detect_ndef = dm_config[0];
    if (dm_config.size() > 1) nfa_dm_cfg.auto_read_ndef = dm_config[1];
    if (dm_config.size() > 2) nfa_dm_cfg.auto_presence_check = dm_config[2];
    if (dm_config.size() > 3) nfa_dm_cfg.presence_check_option = dm_config[3];
    // NOTE: The timeout value is not configurable here because the endianess
    // of a byte array is ambiguous and needlessly difficult to configure.
    // If this value needs to be configgurable, a numeric config option should
    // be used.
  }

  if (NfcConfig::hasKey(NAME_NFA_MAX_EE_SUPPORTED)) {
    nfa_ee_max_ee_cfg = NfcConfig::getUnsigned(NAME_NFA_MAX_EE_SUPPORTED);
    DLOG_IF(INFO, nfc_debug_enabled)
         << StringPrintf("%s: Overriding NFA_EE_MAX_EE_SUPPORTED to use %d",
                         func, nfa_ee_max_ee_cfg);
  }

  if (NfcConfig::hasKey(NAME_NFA_POLL_BAIL_OUT_MODE)) {
    nfa_poll_bail_out_mode =
        NfcConfig::getUnsigned(NAME_NFA_POLL_BAIL_OUT_MODE);
    DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: Overriding NFA_POLL_BAIL_OUT_MODE to use %d", func,
          nfa_poll_bail_out_mode);
  }

  if (NfcConfig::hasKey(NAME_NFA_PROPRIETARY_CFG)) {
    std::vector<uint8_t> p_config =
        NfcConfig::getBytes(NAME_NFA_PROPRIETARY_CFG);
    if (p_config.size() > 0)
      nfa_proprietary_cfg.pro_protocol_18092_active = p_config[0];
    if (p_config.size() > 1)
      nfa_proprietary_cfg.pro_protocol_b_prime = p_config[1];
    if (p_config.size() > 2)
      nfa_proprietary_cfg.pro_protocol_dual = p_config[2];
    if (p_config.size() > 3)
      nfa_proprietary_cfg.pro_protocol_15693 = p_config[3];
    if (p_config.size() > 4)
      nfa_proprietary_cfg.pro_protocol_kovio = p_config[4];
    if (p_config.size() > 5) nfa_proprietary_cfg.pro_protocol_mfc = p_config[5];
    if (p_config.size() > 6)
      nfa_proprietary_cfg.pro_discovery_kovio_poll = p_config[6];
    if (p_config.size() > 7)
      nfa_proprietary_cfg.pro_discovery_b_prime_poll = p_config[7];
    if (p_config.size() > 8)
      nfa_proprietary_cfg.pro_discovery_b_prime_listen = p_config[8];
  }
  // Configure whitelist of HCI host ID's
  // See specification: ETSI TS 102 622, section 6.1.3.1
  if (NfcConfig::hasKey(NAME_DEVICE_HOST_WHITE_LIST)) {
    host_whitelist = NfcConfig::getBytes(NAME_DEVICE_HOST_WHITE_LIST);
    nfa_hci_cfg.num_whitelist_host = host_whitelist.size();
    nfa_hci_cfg.p_whitelist = &host_whitelist[0];
  }


  verify_stack_non_volatile_store();
  if (NfcConfig::hasKey(NAME_PRESERVE_STORAGE) &&
      NfcConfig::getUnsigned(NAME_PRESERVE_STORAGE) == 1) {
    DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: preserve stack NV store", __func__);
  } else {
    delete_stack_non_volatile_store(false);
  }

  GKI_init();
  GKI_enable();
  GKI_create_task((TASKPTR)NFCA_TASK, BTU_TASK, (int8_t*)"NFCA_TASK", 0, 0,
                  (pthread_cond_t*)nullptr, nullptr);
  {
    AutoThreadMutex guard(mCondVar);
    GKI_create_task((TASKPTR)Thread, MMI_TASK, (int8_t*)"NFCA_THREAD", 0, 0,
                    (pthread_cond_t*)nullptr, nullptr);
    mCondVar.wait();
  }

  debug_nfcsnoop_init();
  configureRpcThreadpool(2, false);
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: exit", func);
}
#if (NXP_EXTNS == TRUE)
/*******************************************************************************
**
** Function:    NfcAdaptation::MinInitialize()
**
** Description: class initializer
**
** Returns:     none
**
*******************************************************************************/
void NfcAdaptation::MinInitialize() {
  const char* func = "NfcAdaptation::MinInitialize";
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: enter", func);
  GKI_init();
  GKI_enable();
  GKI_create_task((TASKPTR)NFCA_TASK, BTU_TASK, (int8_t*)"NFCA_TASK", 0, 0,
                  (pthread_cond_t*)nullptr, nullptr);
  {
    AutoThreadMutex guard(mCondVar);
    GKI_create_task((TASKPTR)Thread, MMI_TASK, (int8_t*)"NFCA_THREAD", 0, 0,
                    (pthread_cond_t*)nullptr, nullptr);
    mCondVar.wait();
  }

  InitializeHalDeviceContext();
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: exit", func);
}
#endif


void NfcAdaptation::FactoryReset() {
  if (mHal_1_2 != nullptr) {
    mHal_1_2->factoryReset();
  } else if (mHal_1_1 != nullptr) {
    mHal_1_1->factoryReset();
  }
}

void NfcAdaptation::DeviceShutdown() {
  if (mHal_1_2 != nullptr) {
    mHal_1_2->closeForPowerOffCase();
  } else if (mHal_1_1 != nullptr) {
    mHal_1_1->closeForPowerOffCase();
  }
}

/*******************************************************************************
**
** Function:    NfcAdaptation::Finalize()
**
** Description: class finalizer
**
** Returns:     none
**
*******************************************************************************/
void NfcAdaptation::Finalize() {
  const char* func = "NfcAdaptation::Finalize";
  AutoThreadMutex a(sLock);
  /* Releasing if ioctl mutex is in locked state */
  if (!sIoctlMutex.tryLock()) {
      DLOG_IF(INFO, nfc_debug_enabled)
          << StringPrintf("%s: Ioctl found in locked state", __func__);
  }
  sIoctlMutex.unlock();
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: enter", func);
  GKI_shutdown();

  NfcConfig::clear();

  mCallback = nullptr;
  memset(&mHalEntryFuncs, 0, sizeof(mHalEntryFuncs));
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: exit", func);
  delete this;
}

/*******************************************************************************
**
** Function:    NfcAdaptation::Dump
**
** Description: Native support for dumpsys function.
**
** Returns:     None.
**
*******************************************************************************/
void NfcAdaptation::Dump(int fd) { debug_nfcsnoop_dump(fd); }

/*******************************************************************************
**
** Function:    NfcAdaptation::signal()
**
** Description: signal the CondVar to release the thread that is waiting
**
** Returns:     none
**
*******************************************************************************/
void NfcAdaptation::signal() { mCondVar.signal(); }

/*******************************************************************************
**
** Function:    NfcAdaptation::NFCA_TASK()
**
** Description: NFCA_TASK runs the GKI main task
**
** Returns:     none
**
*******************************************************************************/
uint32_t NfcAdaptation::NFCA_TASK(__attribute__((unused)) uint32_t arg) {
  const char* func = "NfcAdaptation::NFCA_TASK";
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: enter", func);
  GKI_run(0);
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: exit", func);
  return 0;
}

/*******************************************************************************
**
** Function:    NfcAdaptation::Thread()
**
** Description: Creates work threads
**
** Returns:     none
**
*******************************************************************************/
uint32_t NfcAdaptation::Thread(__attribute__((unused)) uint32_t arg) {
  const char* func = "NfcAdaptation::Thread";
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: enter", func);

  {
    ThreadCondVar CondVar;
    AutoThreadMutex guard(CondVar);
    GKI_create_task((TASKPTR)nfc_task, NFC_TASK, (int8_t*)"NFC_TASK", 0, 0,
                    (pthread_cond_t*)CondVar, (pthread_mutex_t*)CondVar);
    CondVar.wait();
  }

  NfcAdaptation::GetInstance().signal();

  GKI_exit_task(GKI_get_taskid());
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: exit", func);
  return 0;
}

/*******************************************************************************
**
** Function:    NfcAdaptation::GetHalEntryFuncs()
**
** Description: Get the set of HAL entry points.
**
** Returns:     Functions pointers for HAL entry points.
**
*******************************************************************************/
tHAL_NFC_ENTRY* NfcAdaptation::GetHalEntryFuncs() { return &mHalEntryFuncs; }

/*******************************************************************************
**
** Function:    NfcAdaptation::InitializeHalDeviceContext
**
** Description: Ask the generic Android HAL to find the Broadcom-specific HAL.
**
** Returns:     None.
**
*******************************************************************************/
void NfcAdaptation::InitializeHalDeviceContext() {
  const char* func = "NfcAdaptation::InitializeHalDeviceContext";

  mHalEntryFuncs.initialize = HalInitialize;
  mHalEntryFuncs.terminate = HalTerminate;
  mHalEntryFuncs.open = HalOpen;
  mHalEntryFuncs.close = HalClose;
  mHalEntryFuncs.core_initialized = HalCoreInitialized;
  mHalEntryFuncs.write = HalWrite;
#if (NXP_EXTNS == TRUE)
  mHalEntryFuncs.ioctl = HalIoctl;
#endif
  mHalEntryFuncs.prediscover = HalPrediscover;
  mHalEntryFuncs.control_granted = HalControlGranted;
  mHalEntryFuncs.power_cycle = HalPowerCycle;
  mHalEntryFuncs.get_max_ee = HalGetMaxNfcee;
  mHalEntryFuncs.spiDwpSync = HalSpiDwpSync;
  mHalEntryFuncs.RelForceDwpOnOffWait = HalRelForceDwpOnOffWait;
  mHalEntryFuncs.HciInitUpdateState = HalHciInitUpdateState;
  mHalEntryFuncs.setEseState = HalsetEseState;
  mHalEntryFuncs.getchipType = HalgetchipType;
  mHalEntryFuncs.setNfcServicePid = HalsetNfcServicePid;
  mHalEntryFuncs.getEseState = HalgetEseState;
  mHalEntryFuncs.GetCachedNfccConfig = HalGetCachedNfccConfig;
  mHalEntryFuncs.nciTransceive = HalNciTransceive;

  LOG(INFO) << StringPrintf("%s: Try INfcV1_1::getService()", func);
  mHal = mHal_1_1 = mHal_1_2 = INfcV1_2::tryGetService();
  if (mHal_1_2 == nullptr) {
    mHal = mHal_1_1 = INfcV1_1::tryGetService();
    if (mHal_1_1 == nullptr) {
      LOG(INFO) << StringPrintf("%s: Try INfc::getService()", func);
      mHal = INfc::tryGetService();
    }
  }
  //LOG_FATAL_IF(mHal == nullptr, "Failed to retrieve the NFC HAL!");
  if (mHal == nullptr) {
    LOG(INFO) << StringPrintf ( "Failed to retrieve the NFC HAL!");
  }else {
    LOG(INFO) << StringPrintf("%s: INfc::getService() returned %p (%s)", func, mHal.get(),
          (mHal->isRemote() ? "remote" : "local"));
  }
  mHal->linkToDeath(mNfcHalDeathRecipient,0);
  LOG(INFO) << StringPrintf("%s: INxpNfc::getService()", func);
  mHalNxpNfc = INxpNfc::tryGetService();
  if(mHalNxpNfc == nullptr) {
    LOG(INFO) << StringPrintf ( "Failed to retrieve the NXPNFC HAL!");
  } else {
    LOG(INFO) << StringPrintf("%s: INxpNfc::getService() returned %p (%s)", func, mHalNxpNfc.get(),
          (mHalNxpNfc->isRemote() ? "remote" : "local"));
  }

  LOG(INFO) << StringPrintf("%s: INxpNfcLegacy::getService()", func);
  mHalNxpNfcLegacy = INxpNfcLegacy::tryGetService();
  if(mHalNxpNfcLegacy == nullptr) {
    LOG(INFO) << StringPrintf ( "Failed to retrieve the NXPNFC Legacy HAL!");
  } else {
    LOG(INFO) << StringPrintf("%s: INxpNfcLegacy::getService() returned %p (%s)", func, mHalNxpNfcLegacy.get(),
          (mHalNxpNfcLegacy->isRemote() ? "remote" : "local"));
  }

}

/*******************************************************************************
**
** Function:    NfcAdaptation::HalInitialize
**
** Description: Not implemented because this function is only needed
**              within the HAL.
**
** Returns:     None.
**
*******************************************************************************/
void NfcAdaptation::HalInitialize() {
  const char* func = "NfcAdaptation::HalInitialize";
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s", func);
}

/*******************************************************************************
**
** Function:    NfcAdaptation::HalTerminate
**
** Description: Not implemented because this function is only needed
**              within the HAL.
**
** Returns:     None.
**
*******************************************************************************/
void NfcAdaptation::HalTerminate() {
  const char* func = "NfcAdaptation::HalTerminate";
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s", func);
}

/*******************************************************************************
**
** Function:    NfcAdaptation::HalOpen
**
** Description: Turn on controller, download firmware.
**
** Returns:     None.
**
*******************************************************************************/
void NfcAdaptation::HalOpen(tHAL_NFC_CBACK* p_hal_cback,
                            tHAL_NFC_DATA_CBACK* p_data_cback) {
  const char* func = "NfcAdaptation::HalOpen";
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s", func);
  mCallback = new NfcClientCallback(p_hal_cback, p_data_cback);
  if (mHal_1_1 != nullptr) {
    mHal_1_1->open_1_1(mCallback);
  } else {
    mHal->open(mCallback);
  }
}
/*******************************************************************************
**
** Function:    NfcAdaptation::HalClose
**
** Description: Turn off controller.
**
** Returns:     None.
**
*******************************************************************************/
void NfcAdaptation::HalClose() {
  const char* func = "NfcAdaptation::HalClose";
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s", func);
  mHal->close();
}

/*******************************************************************************
**
** Function:    NfcAdaptation::HalDeviceContextCallback
**
** Description: Translate generic Android HAL's callback into Broadcom-specific
**              callback function.
**
** Returns:     None.
**
*******************************************************************************/
void NfcAdaptation::HalDeviceContextCallback(nfc_event_t event,
                                             nfc_status_t event_status) {
  const char* func = "NfcAdaptation::HalDeviceContextCallback";
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: event=%u", func, event);
  if (mHalCallback) mHalCallback(event, (tHAL_NFC_STATUS)event_status);
}

/*******************************************************************************
**
** Function:    NfcAdaptation::HalDeviceContextDataCallback
**
** Description: Translate generic Android HAL's callback into Broadcom-specific
**              callback function.
**
** Returns:     None.
**
*******************************************************************************/
void NfcAdaptation::HalDeviceContextDataCallback(uint16_t data_len,
                                                 uint8_t* p_data) {
  const char* func = "NfcAdaptation::HalDeviceContextDataCallback";
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: len=%u", func, data_len);
  if (mHalDataCallback) mHalDataCallback(data_len, p_data);
}

/*******************************************************************************
**
** Function:    NfcAdaptation::HalWrite
**
** Description: Write NCI message to the controller.
**
** Returns:     None.
**
*******************************************************************************/
void NfcAdaptation::HalWrite(uint16_t data_len, uint8_t* p_data) {
  const char* func = "NfcAdaptation::HalWrite";
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s", func);
  ::android::hardware::nfc::V1_0::NfcData data;
  data.setToExternal(p_data, data_len);
  mHal->write(data);
}

#if (NXP_EXTNS == TRUE)
/*******************************************************************************
**
** Function:    IoctlCallback
**
** Description: Callback from HAL stub for IOCTL api invoked.
**              Output data for IOCTL is sent as argument
**
** Returns:     None.
**
*******************************************************************************/
void IoctlCallback(::android::hardware::nfc::V1_0::NfcData outputData) {
  const char* func = "IoctlCallback";
  nfc_nci_ExtnOutputData_t* pOutData =
      (nfc_nci_ExtnOutputData_t*)&outputData[0];
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s Ioctl Type=%llu", func, (unsigned long long)pOutData->ioctlType);
  NfcAdaptation* pAdaptation = (NfcAdaptation*)pOutData->context;
  /*Output Data from stub->Proxy is copied back to output data
   * This data will be sent back to libnfc*/
  memcpy(&pAdaptation->mCurrentIoctlData->out, &outputData[0],
         sizeof(nfc_nci_ExtnOutputData_t));
}
/*******************************************************************************
**
** Function:    NfcAdaptation::HalIoctl
**
** Description: Calls ioctl to the Nfc driver.
**              If called with a arg value of 0x01 than wired access requested,
**              status of the requst would be updated to p_data.
**              If called with a arg value of 0x00 than wired access will be
**              released, status of the requst would be updated to p_data.
**              If called with a arg value of 0x02 than current p61 state would
*be
**              updated to p_data.
**
** Returns:     -1 or 0.
**
*******************************************************************************/
int NfcAdaptation::HalIoctl(long arg, void* p_data) {
  const char* func = "NfcAdaptation::HalIoctl";
  ::android::hardware::nfc::V1_0::NfcData data;
  sIoctlMutex.lock();
  nfc_nci_IoctlInOutData_t* pInpOutData = (nfc_nci_IoctlInOutData_t*)p_data;
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s arg=%ld", func, arg);
  pInpOutData->inp.context = &NfcAdaptation::GetInstance();
  NfcAdaptation::GetInstance().mCurrentIoctlData = pInpOutData;
  data.setToExternal((uint8_t*)pInpOutData, sizeof(nfc_nci_IoctlInOutData_t));
  if (arg == HAL_NFC_IOCTL_SET_TRANSIT_CONFIG) {
    /*Insert Transit config at the end of IOCTL data as transit buffer also
    needs to be part of NfcData(hidl_vec)*/
    std::vector<uint8_t> tempStdVec(data);
    tempStdVec.insert(
        tempStdVec.end(), pInpOutData->inp.data.transitConfig.val,
        pInpOutData->inp.data.transitConfig.val +
            (pInpOutData->inp.data.transitConfig.len));
    data = tempStdVec;
  }
  if(mHalNxpNfc != nullptr)
      mHalNxpNfc->ioctl(arg, data, IoctlCallback);
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s Ioctl Completed for Type=%llu", func, (unsigned long long)pInpOutData->out.ioctlType);
  sIoctlMutex.unlock();
  return (pInpOutData->out.result);
}

/*******************************************************************************
**
** Function:    NfcAdaptation::HalGetFwDwnldFlag
**
** Description: Get FW Download Flag.
**
** Returns:     SUCESS or FAIL.
**
*******************************************************************************/
int NfcAdaptation::HalGetFwDwnldFlag(__attribute__((unused)) uint8_t* fwDnldRequest) {
  const char* func = "NfcAdaptation::HalGetFwDwnldFlag";
  int status = NFA_STATUS_FAILED;
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s : Dummy", func);
  /*FIXME: Additional IOCTL type can be added for this
   * Instead of extra function pointer
   * This is required for a corner case or delayed FW download after SPI session
   * is completed
   * This shall be fixed with a better design later*/
  /*
  if (mHalDeviceContext)
  {
      status = mHalDeviceContext->check_fw_dwnld_flag(mHalDeviceContext,
  fwDnldRequest);
  }*/
  return status;
}
#endif

/*******************************************************************************
**
** Function:    NfcAdaptation::HalCoreInitialized
**
** Description: Adjust the configurable parameters in the controller.
**
** Returns:     None.
**
*******************************************************************************/
void NfcAdaptation::HalCoreInitialized(uint16_t data_len,
                                       uint8_t* p_core_init_rsp_params) {
  const char* func = "NfcAdaptation::HalCoreInitialized";
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s", func);
  hidl_vec<uint8_t> data;
  data.setToExternal(p_core_init_rsp_params, data_len);
  mHal->coreInitialized(data);
}

/*******************************************************************************
**
** Function:    NfcAdaptation::HalPrediscover
**
** Description:     Perform any vendor-specific pre-discovery actions (if
**                  needed) If any actions were performed true will be returned,
**                  and HAL_PRE_DISCOVER_CPLT_EVT will notify when actions are
**                  completed.
**
** Returns:         true if vendor-specific pre-discovery actions initialized
**                  false if no vendor-specific pre-discovery actions are
**                  needed.
**
*******************************************************************************/
bool NfcAdaptation::HalPrediscover() {
  const char* func = "NfcAdaptation::HalPrediscover";
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s", func);
  bool retval = false;
  if(mHal != nullptr)
  {
      mHal->prediscover();
  }
  /*FIXME: Additional IOCTL type can be added for this
   * Instead of extra function pointer
   * prediscover in HAL is empty dummy function
   */
  /*if (mHalDeviceContext)
  {
      retval = mHalDeviceContext->pre_discover (mHalDeviceContext);
  }*/
  return retval;
}

/*******************************************************************************
**
** Function:        HAL_NfcControlGranted
**
** Description:     Grant control to HAL control for sending NCI commands.
**                  Call in response to HAL_REQUEST_CONTROL_EVT.
**                  Must only be called when there are no NCI commands pending.
**                  HAL_RELEASE_CONTROL_EVT will notify when HAL no longer
**                  needs control of NCI.
**
** Returns:         void
**
*******************************************************************************/
void NfcAdaptation::HalControlGranted() {
  const char* func = "NfcAdaptation::HalControlGranted";
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s", func);
  mHal->controlGranted();
}

/*******************************************************************************
**
** Function:    NfcAdaptation::HalPowerCycle
**
** Description: Turn off and turn on the controller.
**
** Returns:     None.
**
*******************************************************************************/
void NfcAdaptation::HalPowerCycle() {
  const char* func = "NfcAdaptation::HalPowerCycle";
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s", func);
  mHal->powerCycle();
}

/*******************************************************************************
**
** Function:    NfcAdaptation::HalGetMaxNfcee
**
** Description: Turn off and turn on the controller.
**
** Returns:     None.
**
*******************************************************************************/
uint8_t NfcAdaptation::HalGetMaxNfcee() {
  const char* func = "NfcAdaptation::HalPowerCycle";
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s", func);
  return nfa_ee_max_ee_cfg;
}

/*******************************************************************************
**
** Function:    NfcAdaptation::DownloadFirmware
**
** Description: Download firmware patch files.
**
** Returns:     None.
**
*******************************************************************************/
bool NfcAdaptation::DownloadFirmware() {
  const char* func = "NfcAdaptation::DownloadFirmware";
  isDownloadFirmwareCompleted = false;
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: enter", func);
#if (NXP_EXTNS == TRUE)
  static uint8_t cmd_reset_nci[] = {0x20, 0x00, 0x01, 0x00};
  static uint8_t cmd_init_nci[] = {0x20, 0x01, 0x00};
  static uint8_t cmd_init_nci2_0[] = {0x20,0x01,0x02,0x00,0x00};
  static uint8_t cmd_reset_nci_size = sizeof(cmd_reset_nci) / sizeof(uint8_t);
  static uint8_t cmd_init_nci_size = sizeof(cmd_init_nci) / sizeof(uint8_t);
  if(NFA_GetNCIVersion() == NCI_VERSION_2_0)
  {
      cmd_init_nci_size = sizeof(cmd_init_nci2_0) / sizeof(uint8_t);
  }
  uint8_t p_core_init_rsp_params;
  evt_status = NFC_STATUS_FAILED;
#endif
  HalInitialize();

  mHalOpenCompletedEvent.lock();
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: try open HAL", func);
  HalOpen(HalDownloadFirmwareCallback, HalDownloadFirmwareDataCallback);
  mHalOpenCompletedEvent.wait();
  mHalOpenCompletedEvent.unlock();
#if (NXP_EXTNS == TRUE)
  /* Send a CORE_RESET and CORE_INIT to the NFCC. This is required because when
   * calling
   * HalCoreInitialized, the HAL is going to parse the conf file and send NCI
   * commands
   * to the NFCC. Hence CORE-RESET and CORE-INIT have to be sent prior to this.
   */
  isSignaled = SIGNAL_NONE;
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: send CORE_RESET", func);
  HalWrite(cmd_reset_nci_size, cmd_reset_nci);
  mHalCoreResetCompletedEvent.lock();
  if (SIGNAL_NONE == isSignaled) {
    mHalCoreResetCompletedEvent.wait();
  }
  isSignaled = SIGNAL_NONE;
  mHalCoreResetCompletedEvent.unlock();
  if (evt_status == NFC_STATUS_FAILED) {
    goto TheEnd;
  }
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: send CORE_INIT", func);
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: send CORE_INIT NCI Version : %d", func, NFA_GetNCIVersion());
  if(NFA_GetNCIVersion() == NCI_VERSION_2_0)
    HalWrite(cmd_init_nci_size, cmd_init_nci2_0);
  else
    HalWrite(cmd_init_nci_size, cmd_init_nci);
  mHalCoreInitCompletedEvent.lock();
  if (SIGNAL_NONE == isSignaled) {
    mHalCoreInitCompletedEvent.wait();
  }
  isSignaled = SIGNAL_NONE;
  mHalCoreInitCompletedEvent.unlock();
  if (evt_status == NFC_STATUS_FAILED) {
    goto TheEnd;
  }
      isSignaled = SIGNAL_NONE;
      DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: send CORE_RESET", func);
      HalWrite(cmd_reset_nci_size, cmd_reset_nci);
      mHalCoreResetCompletedEvent.lock();
      if (SIGNAL_NONE == isSignaled) {
        mHalCoreResetCompletedEvent.wait();
      }
      isSignaled = SIGNAL_NONE;
      mHalCoreResetCompletedEvent.unlock();
      if (evt_status == NFC_STATUS_FAILED) {
        goto TheEnd;
      }
      DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: send CORE_INIT", func);
      if(NFA_GetNCIVersion() == NCI_VERSION_2_0)
        HalWrite(cmd_init_nci_size, cmd_init_nci2_0);
      else
        HalWrite(cmd_init_nci_size, cmd_init_nci);
      mHalCoreInitCompletedEvent.lock();
      if (SIGNAL_NONE == isSignaled) {
        mHalCoreInitCompletedEvent.wait();
      }
      isSignaled = SIGNAL_NONE;
      mHalCoreInitCompletedEvent.unlock();
      if (evt_status == NFC_STATUS_FAILED) {
        goto TheEnd;
      }
      DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: try init HAL", func);
      isSignaled = SIGNAL_NONE;
      mHalInitCompletedEvent.lock();
      HalCoreInitialized(sizeof(uint8_t), &p_core_init_rsp_params);
      if (SIGNAL_NONE == isSignaled) {
        mHalInitCompletedEvent.wait();
      }
      isSignaled = SIGNAL_NONE;
      mHalInitCompletedEvent.unlock();

TheEnd:
  isSignaled = SIGNAL_NONE;
#endif
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: try close HAL", func);
  HalClose();

  HalTerminate();
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: exit", func);

  return isDownloadFirmwareCompleted;
}

/*******************************************************************************
**
** Function:    NfcAdaptation::HalDownloadFirmwareCallback
**
** Description: Receive events from the HAL.
**
** Returns:     None.
**
*******************************************************************************/
void NfcAdaptation::HalDownloadFirmwareCallback(nfc_event_t event,
                                                __attribute__((unused))
                                                nfc_status_t event_status) {
  const char* func = "NfcAdaptation::HalDownloadFirmwareCallback";
  (void)(event_status);
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: event=0x%X", func, event);
  switch (event) {
    case HAL_NFC_OPEN_CPLT_EVT: {
      DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: HAL_NFC_OPEN_CPLT_EVT", func);
      if (event_status == HAL_NFC_STATUS_OK) isDownloadFirmwareCompleted = true;
      mHalOpenCompletedEvent.signal();
      break;
    }
    case HAL_NFC_POST_INIT_CPLT_EVT: {
      DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: HAL_NFC_POST_INIT_CPLT_EVT", func);
#if (NXP_EXTNS == TRUE)
      isSignaled = SIGNAL_SIGNALED;
#endif
      mHalInitCompletedEvent.signal();
      break;
    }
    case HAL_NFC_CLOSE_CPLT_EVT: {
      DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s: HAL_NFC_CLOSE_CPLT_EVT", func);
#if (NXP_EXTNS == TRUE)
      isSignaled = SIGNAL_SIGNALED;
#endif
      break;
    }
  }
}

/*******************************************************************************
**
** Function:    NfcAdaptation::HalDownloadFirmwareDataCallback
**
** Description: Receive data events from the HAL.
**
** Returns:     None.
**
*******************************************************************************/
void NfcAdaptation::HalDownloadFirmwareDataCallback(__attribute__((unused))
                                                    uint16_t data_len,
                                                    __attribute__((unused))
                                                    uint8_t* p_data) {
#if (NXP_EXTNS == TRUE)
  isSignaled = SIGNAL_SIGNALED;
  if (data_len > 3) {
    evt_status = NFC_STATUS_OK;
    if (p_data[0] == 0x40 && p_data[1] == 0x00) {
      mHalCoreResetCompletedEvent.signal();
    } else if (p_data[0] == 0x40 && p_data[1] == 0x01) {
      mHalCoreInitCompletedEvent.signal();
    }
  } else {
    evt_status = NFC_STATUS_FAILED;
    mHalCoreResetCompletedEvent.signal();
    mHalCoreInitCompletedEvent.signal();
  }
#endif
}

/*******************************************************************************
**
** Function:    ThreadMutex::ThreadMutex()
**
** Description: class constructor
**
** Returns:     none
**
*******************************************************************************/
ThreadMutex::ThreadMutex() {
  pthread_mutexattr_t mutexAttr;

  pthread_mutexattr_init(&mutexAttr);
  pthread_mutex_init(&mMutex, &mutexAttr);
  pthread_mutexattr_destroy(&mutexAttr);
}

/*******************************************************************************
**
** Function:    ThreadMutex::~ThreadMutex()
**
** Description: class destructor
**
** Returns:     none
**
*******************************************************************************/
ThreadMutex::~ThreadMutex() { pthread_mutex_destroy(&mMutex); }

/*******************************************************************************
**
** Function:    ThreadMutex::lock()
**
** Description: lock kthe mutex
**
** Returns:     none
**
*******************************************************************************/
void ThreadMutex::lock() { pthread_mutex_lock(&mMutex); }

/*******************************************************************************
**
** Function:    ThreadMutex::unblock()
**
** Description: unlock the mutex
**
** Returns:     none
**
*******************************************************************************/
void ThreadMutex::unlock() { pthread_mutex_unlock(&mMutex); }

/*******************************************************************************
**
** Function:    ThreadCondVar::ThreadCondVar()
**
** Description: class constructor
**
** Returns:     none
**
*******************************************************************************/
ThreadCondVar::ThreadCondVar() {
  pthread_condattr_t CondAttr;

  pthread_condattr_init(&CondAttr);
  pthread_cond_init(&mCondVar, &CondAttr);

  pthread_condattr_destroy(&CondAttr);
}

/*******************************************************************************
**
** Function:    ThreadCondVar::~ThreadCondVar()
**
** Description: class destructor
**
** Returns:     none
**
*******************************************************************************/
ThreadCondVar::~ThreadCondVar() { pthread_cond_destroy(&mCondVar); }

/*******************************************************************************
**
** Function:    ThreadCondVar::wait()
**
** Description: wait on the mCondVar
**
** Returns:     none
**
*******************************************************************************/
void ThreadCondVar::wait() {
  pthread_cond_wait(&mCondVar, *this);
  pthread_mutex_unlock(*this);
}

/*******************************************************************************
**
** Function:    ThreadCondVar::signal()
**
** Description: signal the mCondVar
**
** Returns:     none
**
*******************************************************************************/
void ThreadCondVar::signal() {
  AutoThreadMutex a(*this);
  pthread_cond_signal(&mCondVar);
}

/*******************************************************************************
**
** Function:    AutoThreadMutex::AutoThreadMutex()
**
** Description: class constructor, automatically lock the mutex
**
** Returns:     none
**
*******************************************************************************/
AutoThreadMutex::AutoThreadMutex(ThreadMutex& m) : mm(m) { mm.lock(); }

/*******************************************************************************
**
** Function:    AutoThreadMutex::~AutoThreadMutex()
**
** Description: class destructor, automatically unlock the mutex
**
** Returns:     none
**
*******************************************************************************/
AutoThreadMutex::~AutoThreadMutex() { mm.unlock(); }

/***************************************************************************
**
** Function         initializeGlobalAppDtaMode.
**
** Description      initialize Dta App Mode flag.
**
** Returns          None.
**
***************************************************************************/
void initializeGlobalAppDtaMode() {
  appl_dta_mode_flag = 0x01;
  ALOGD("%s: DTA Enabled", __func__);
}

/***************************************************************************
**
** Function         NfcAdaptation::setEseState
**
** Description      This function is called for to update ese P61 state.
**
** Returns          None.
**
***************************************************************************/
uint32_t NfcAdaptation::HalsetEseState(tNxpEseState ESEstate) {
  const char* func = "NfcAdaptation::HalsetEseState";
  uint32_t status = NFA_STATUS_FAILED;
  uint8_t ret = 0;

  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s : Enter", func);

  if (mHalNxpNfcLegacy != nullptr) {
    ret = mHalNxpNfcLegacy->setEseState((NxpNfcHalEseState)ESEstate);
    if(ret){
      ALOGE("NfcAdaptation::setEseState mHalNxpNfcLegacy completed");
      status = NFA_STATUS_OK;
    } else {
      ALOGE("NfcAdaptation::setEseState mHalNxpNfcLegacy failed");
    }
  }
  return status;
}


/*******************************************************************************
 **
 ** Function         phNxpNciHal_getchipType
 **
 ** Description      Gets the chipType from hal which is already configured
 **                  during init time.
 **
 ** Returns          chipType
 *******************************************************************************/
uint8_t NfcAdaptation::HalgetchipType() {
  const char* func = "NfcAdaptation::HalgetchipType";
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s : Enter", func);
  return mHalNxpNfcLegacy->getchipType();
}



/***************************************************************************
**
** Function         NfcAdaptation::setNfcServicePid
**
** Description      This function request to pn54x driver to
**                  update NFC service process ID for signalling.
**
** Returns          0 if api call success, else -1
***************************************************************************/
uint16_t NfcAdaptation::HalsetNfcServicePid(uint64_t NfcNxpServicePid) {
  const char* func = "NfcAdaptation::HalsetNfcServicePid";
  uint16_t status = NFA_STATUS_FAILED;

  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s : Enter", func);

  if (mHalNxpNfcLegacy != nullptr) {
    status = mHalNxpNfcLegacy->setNfcServicePid(NfcNxpServicePid);

    if(status != NFA_STATUS_FAILED){
      ALOGE("NfcAdaptation::setNfcServicePid mHalNxpNfcLegacy completed");
    } else {
      ALOGE("NfcAdaptation::setNfcServicePid mHalNxpNfcLegacy failed");
    }
  }

  return status;
}

/***************************************************************************
**
** Function         NfcAdaptation::getEseState
**
** Description      This function is called for to get ese state.
**
** Returns          Status.
**
***************************************************************************/
uint32_t NfcAdaptation::HalgetEseState() {
  const char* func = "NfcAdaptation::HalgetEseState";
  uint32_t status = NFA_STATUS_FAILED;


  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s : Enter", func);

  if (mHalNxpNfcLegacy != nullptr) {
    status = mHalNxpNfcLegacy->getEseState();
    if(status != NFA_STATUS_FAILED){
      ALOGE("NfcAdaptation::getEseState mHalNxpNfcLegacy completed");
      status = NFA_STATUS_OK;
    } else {
      ALOGE("NfcAdaptation::getEseState mHalNxpNfcLegacy failed");
    }
  }

  return status;
}

/***************************************************************************
**
** Function         NfcAdaptation::HalSpiDwpSync
**
** Description      This function is called for to update ese P61 state.
**
** Returns          None.
**
***************************************************************************/
uint16_t NfcAdaptation::HalSpiDwpSync(uint32_t level) {
  const char* func = "NfcAdaptation::HalSpiDwpSync";
  uint16_t ret = 0;

  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s : Enter", func);

  if (mHalNxpNfcLegacy != nullptr) {
    ret = mHalNxpNfcLegacy->spiDwpSync(level);
  }

  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s : Exit", func);

  return ret;
}

/***************************************************************************
**
** Function         NfcAdaptation::HalRelForceDwpOnOffWait
**
** Description      This function is called for to update ese P61 state.
**
** Returns          None.
**
***************************************************************************/
uint16_t NfcAdaptation::HalRelForceDwpOnOffWait(uint32_t level) {
  const char* func = "NfcAdaptation::HalRelForceDwpOnOffWait";
  uint16_t ret = 0;

  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s : Enter", func);

  if (mHalNxpNfcLegacy != nullptr) {
    ret = mHalNxpNfcLegacy->RelForceDwpOnOffWait(level);
  }

  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s : Exit", func);

  return ret;
}

/***************************************************************************
**
** Function         NfcAdaptation::HalHciInitUpdateState
**
** Description      This function is called for to update ese P61 state.
**
** Returns          None.
**
***************************************************************************/
int32_t NfcAdaptation::HalHciInitUpdateState(tNFC_HCI_INIT_STATUS HciStatus) {
  const char* func = "NfcAdaptation::HalHciInitUpdateState";
  uint16_t ret = 0;

  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s : Enter", func);

  if (mHalNxpNfcLegacy != nullptr) {
    ret = mHalNxpNfcLegacy->hciInitUpdateState((NfcHciInitStatus)HciStatus);
  }

  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s : Exit", func);

  return ret;
}

/*******************************************************************************
 ** Function         HalGetProperty_cb
 **
 ** Description      This is a callback for HalGetProperty. It shall be called
 **                  from HAL to return the value of requested property.
 **
 ** Parameters       ::android::hardware::hidl_string
 **
 ** Return           void
 *********************************************************************/
static void HalGetCachedNfccConfig_cb(NxpNciCfgInfo CfgInfo) {
    memcpy(&(NfcAdaptation::GetInstance().AdapCfgInfo),&CfgInfo,sizeof(NxpNciCfgInfo));

  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("isGetcfg = %d",NfcAdaptation::GetInstance().AdapCfgInfo.isGetcfg);
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("total_duration = %d",NfcAdaptation::GetInstance().AdapCfgInfo.total_duration[0]);
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("total_duration_len = %d",NfcAdaptation::GetInstance().AdapCfgInfo.total_duration_len);
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("atr_req_gen_bytes = %d",NfcAdaptation::GetInstance().AdapCfgInfo.atr_req_gen_bytes[0]);
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("atr_req_gen_bytes_len = %d",NfcAdaptation::GetInstance().AdapCfgInfo.atr_req_gen_bytes_len);
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("atr_res_gen_bytes = %d",NfcAdaptation::GetInstance().AdapCfgInfo.atr_res_gen_bytes[0]);
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("atr_res_gen_bytes_len = %d",NfcAdaptation::GetInstance().AdapCfgInfo.atr_res_gen_bytes_len);
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("pmid_wt = %d",NfcAdaptation::GetInstance().AdapCfgInfo.pmid_wt[0]);
  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("pmid_wt_len = %d",NfcAdaptation::GetInstance().AdapCfgInfo.pmid_wt_len);

  return;
}

/***************************************************************************
**
** Function         NfcAdaptation::GetCachedNfccConfig
**
** Description      This function is called for to update ese P61 state.
**
** Returns          void.
**
***************************************************************************/
void NfcAdaptation::HalGetCachedNfccConfig(tNxpNci_getCfg_info_t *nxpNciAtrInfo) {
  const char* func = "NfcAdaptation::HalGetCachedNfccConfig";

  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s : Enter", func);

  if (mHalNxpNfcLegacy != nullptr) {
     mHalNxpNfcLegacy->getCachedNfccConfig(HalGetCachedNfccConfig_cb);
  }

  memcpy(nxpNciAtrInfo , &(NfcAdaptation::GetInstance().AdapCfgInfo) , sizeof(tNxpNci_getCfg_info_t));

  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s : Exit", func);
}


/*******************************************************************************
 ** Function         HalGetNxpConfig_cb
 **
 ** Description      This is a callback for HalGetNxpConfig. It shall be called
 **                  from HAL to return the value of Nxp Config.
 **
 ** Parameters       ::vendor::nxp::nxpnfclegacy::V1_0::NxpNfcHalConfig
 **
 ** Return           void
 *********************************************************************/
static void HalGetNxpConfig_cb(const NxpNfcHalConfig& ConfigData) {

  memcpy(&(NfcAdaptation::GetInstance().mNxpNfcHalConfig), &ConfigData, sizeof(NxpNfcHalConfig));
  return;
}

/***************************************************************************
**
** Function         NfcAdaptation::HalGetNxpConfig
**
** Description      This function is called for to get Nxp Config.
**
** Returns          void.
**
***************************************************************************/
void NfcAdaptation::HalGetNxpConfig(NxpAdaptationConfig& NfcConfigData) {
  const char* func = "NfcAdaptation::getNxpConfig";

  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s : Enter", func);

  if (mHalNxpNfcLegacy != nullptr) {
    mHalNxpNfcLegacy->getNxpConfig(HalGetNxpConfig_cb);
    memcpy(&NfcConfigData , &(NfcAdaptation::GetInstance().mNxpNfcHalConfig) , sizeof(NxpAdaptationConfig));
  }
}

/*******************************************************************************
 ** Function         HalNciTransceive_cb
 **
 ** Description      This is a callback for HalGetProperty. It shall be called
 **                  from HAL to return the value of requested property.
 **
 ** Parameters       ::android::hardware::hidl_string
 **
 ** Return           void
 *********************************************************************/
static void HalNciTransceive_cb(NxpNciExtnResp out) {
    memcpy(&(NfcAdaptation::GetInstance().mNciResp),&out,sizeof(NxpNciExtnResp));
  return;
}

/***************************************************************************
**
** Function         NfcAdaptation::HalNciTransceive
**
** Description      This function does tarnsceive of nci command
**
** Returns          NfcStatus.
**
***************************************************************************/
uint32_t NfcAdaptation::HalNciTransceive(phNxpNci_Extn_Cmd_t* NciCmd,phNxpNci_Extn_Resp_t* NciResp) {
  const char* func = "NfcAdaptation::nciTransceive";
  NxpNciExtnCmd inNciCmd;
  uint32_t status = 0;

  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s : Enter", func);

  memcpy(&inNciCmd,NciCmd,sizeof(NxpNciExtnCmd));

  if (mHalNxpNfcLegacy != nullptr) {
     mHalNxpNfcLegacy->nciTransceive(inNciCmd,HalNciTransceive_cb);
     status = (NfcAdaptation::GetInstance().mNciResp).status;
     memcpy(NciResp , &(NfcAdaptation::GetInstance().mNciResp) , sizeof(phNxpNci_Extn_Resp_t));
  }

  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf("%s : Exit", func);
  return status;

}
