#include "nfc_integration_fuzzer_impl.h"

#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <fuzzer/FuzzedDataProvider.h>

#include "nfa_ce_api.h"
#include "nfa_ee_api.h"
#include "nfa_p2p_api.h"
#include "nfa_rw_api.h"
#include "nfa_sys.h"
#include "nfc_api.h"
#include "nfc_int.h"
#include "nfc_task_helpers.h"
#include "rw_int.h"

extern uint32_t g_tick_count;
extern tRW_CB rw_cb;

FuzzedDataProvider* g_fuzzed_data;

static bool g_saw_event = false;
static tNFA_EE_DISCOVER_REQ g_ee_info;

void fuzz_cback(tRW_EVENT event, tRW_DATA *p_rw_data) {
  (void)event;
  (void)p_rw_data;
}
constexpr int32_t kMaxFramesSize =
    USHRT_MAX - NFC_HDR_SIZE - NCI_MSG_OFFSET_SIZE - NCI_DATA_HDR_SIZE - 3;

static void nfa_dm_callback(uint8_t event, tNFA_DM_CBACK_DATA*) {
  g_saw_event = true;
  LOG(INFO) << android::base::StringPrintf("nfa_dm_callback got event %d",
                                           event);
}

static void nfa_conn_callback(uint8_t event, tNFA_CONN_EVT_DATA*) {
  LOG(INFO) << android::base::StringPrintf("nfa_conn_callback got event %d",
                                           event);
  g_saw_event = true;
}

static void nfa_ee_callback(tNFA_EE_EVT event, tNFA_EE_CBACK_DATA* p_data) {
  switch (event) {
    case NFA_EE_DISCOVER_REQ_EVT: {
      memcpy(&g_ee_info, &p_data->discover_req, sizeof(g_ee_info));
      break;
    }
  }
}

// From packages/apps/Nfc/nci/jni/PeerToPeer.cpp
#define LLCP_DATA_LINK_TIMEOUT 2000

void nfc_process_timer_evt(void);
void nfa_p2p_callback(tNFA_P2P_EVT, tNFA_P2P_EVT_DATA*) {}

static tNFC_PROTOCOL GetProtocol(const Protocol& protocol) {
  switch (protocol.value()) {
    case Protocol::FUZZER_PROTOCOL_UNKNOWN: {
      return NFC_PROTOCOL_UNKNOWN;
    }
    case Protocol::FUZZER_PROTOCOL_T1T: {
      return NFC_PROTOCOL_T1T;
    }
    case Protocol::FUZZER_PROTOCOL_T2T: {
      return NFC_PROTOCOL_T2T;
    }
    case Protocol::FUZZER_PROTOCOL_T3T: {
      return NFC_PROTOCOL_T3T;
    }
    case Protocol::FUZZER_PROTOCOL_T5T: {
      return NFC_PROTOCOL_T5T;
    }
    case Protocol::FUZZER_PROTOCOL_ISO_DEP: {
      return NFC_PROTOCOL_ISO_DEP;
    }
    case Protocol::FUZZER_PROTOCOL_NFC_DEP: {
      return NFC_PROTOCOL_NFC_DEP;
    }
    case Protocol::FUZZER_PROTOCOL_MIFARE: {
      return NFC_PROTOCOL_MIFARE;
    }
    case Protocol::FUZZER_PROTOCOL_ISO15693: {
      return NFC_PROTOCOL_ISO15693;
    }
    case Protocol::FUZZER_PROTOCOL_B_PRIME: {
      return NFC_PROTOCOL_B_PRIME;
    }
    case Protocol::FUZZER_PROTOCOL_KOVIO: {
      return NFC_PROTOCOL_KOVIO;
    }
  }
}

static tNFC_DISCOVERY_TYPE GetDiscovery(const DiscoveryType& type) {
  switch (type.value()) {
    case DiscoveryType::FUZZER_DISCOVERY_TYPE_POLL_A: {
      return NFC_DISCOVERY_TYPE_POLL_A;
    }
    case DiscoveryType::FUZZER_DISCOVERY_TYPE_POLL_B: {
      return NFC_DISCOVERY_TYPE_POLL_B;
    }
    case DiscoveryType::FUZZER_DISCOVERY_TYPE_POLL_F: {
      return NFC_DISCOVERY_TYPE_POLL_F;
    }
    case DiscoveryType::FUZZER_DISCOVERY_TYPE_POLL_V: {
      return NFC_DISCOVERY_TYPE_POLL_V;
    }
    case DiscoveryType::FUZZER_DISCOVERY_TYPE_POLL_A_ACTIVE: {
      return NFC_DISCOVERY_TYPE_POLL_A_ACTIVE;
    }
    case DiscoveryType::FUZZER_DISCOVERY_TYPE_POLL_F_ACTIVE: {
      return NFC_DISCOVERY_TYPE_POLL_F_ACTIVE;
    }
    case DiscoveryType::FUZZER_DISCOVERY_TYPE_LISTEN_A: {
      return NFC_DISCOVERY_TYPE_LISTEN_A;
    }
    case DiscoveryType::FUZZER_DISCOVERY_TYPE_LISTEN_B: {
      return NFC_DISCOVERY_TYPE_LISTEN_B;
    }
    case DiscoveryType::FUZZER_DISCOVERY_TYPE_LISTEN_F: {
      return NFC_DISCOVERY_TYPE_LISTEN_F;
    }
    case DiscoveryType::FUZZER_DISCOVERY_TYPE_LISTEN_A_ACTIVE: {
      return NFC_DISCOVERY_TYPE_LISTEN_A_ACTIVE;
    }
    case DiscoveryType::FUZZER_DISCOVERY_TYPE_LISTEN_F_ACTIVE: {
      return NFC_DISCOVERY_TYPE_LISTEN_F_ACTIVE;
    }
    case DiscoveryType::FUZZER_DISCOVERY_TYPE_LISTEN_ISO15693: {
      return NFC_DISCOVERY_TYPE_LISTEN_ISO15693;
    }
    case DiscoveryType::FUZZER_DISCOVERY_TYPE_POLL_B_PRIME: {
      return NFC_DISCOVERY_TYPE_POLL_B_PRIME;
    }
    case DiscoveryType::FUZZER_DISCOVERY_TYPE_POLL_KOVIO: {
      return NFC_DISCOVERY_TYPE_POLL_KOVIO;
    }
    case DiscoveryType::FUZZER_DISCOVERY_TYPE_LISTEN_B_PRIME: {
      return NFC_DISCOVERY_TYPE_LISTEN_B_PRIME;
    }
  }
}

std::vector<uint8_t> SerializeTechParameters(
    const tNFC_RF_TECH_PARAMS& params) {
  std::vector<uint8_t> vec;

  switch (params.mode) {
    case NCI_DISCOVERY_TYPE_POLL_A: {
      const tNFC_RF_PA_PARAMS* pa = &params.param.pa;
      vec.push_back(pa->sens_res[0]);
      vec.push_back(pa->sens_res[1]);
      vec.push_back(pa->nfcid1_len);
      vec.insert(vec.end(), pa->nfcid1, pa->nfcid1 + pa->nfcid1_len);

      // sel_rsp of 0 is the same as not having it, so we just always send it
      vec.push_back(1);
      vec.push_back(pa->sel_rsp);

      vec.push_back(pa->hr_len);
      vec.insert(vec.end(), pa->hr, pa->hr + pa->hr_len);
      break;
    }
    default: {
      abort();
    }
  }

  return vec;
}

// Serialize an NFC Activation event back to the spec wire format
std::vector<uint8_t> SerializeNfcActivate(
    const tNFC_ACTIVATE_DEVT& activate, uint8_t buff_size, uint8_t num_buff,
    const std::string& rf_tech_param_buffer,
    const std::string& intf_param_buffer) {
  std::vector<uint8_t> packet;
  packet.push_back(activate.rf_disc_id);
  packet.push_back(activate.intf_param.type);
  packet.push_back(activate.protocol);
  packet.push_back(activate.rf_tech_param.mode);
  packet.push_back(buff_size);
  packet.push_back(num_buff);

  std::vector<uint8_t> tech_parameters(
      rf_tech_param_buffer.begin(),
      rf_tech_param_buffer
          .end());  // = SerializeTechParameters(activate.rf_tech_param);
  if (tech_parameters.size() > 256) {
    tech_parameters.resize(256);
  }
  packet.push_back(tech_parameters.size());
  packet.insert(packet.end(), tech_parameters.begin(), tech_parameters.end());

  packet.push_back(activate.data_mode);
  packet.push_back(activate.tx_bitrate);
  packet.push_back(activate.rx_bitrate);

  std::vector<uint8_t> activation_parameters(intf_param_buffer.begin(),
                                             intf_param_buffer.end());
  if (activation_parameters.size() > 256) {
    activation_parameters.resize(256);
  }

  packet.push_back(activation_parameters.size());
  packet.insert(packet.end(), activation_parameters.begin(),
                activation_parameters.end());
  return packet;
}

void DoRfManageIntfActivated(const RfManageIntfActivated& activated) {
  // The event we want to generate
  tNFC_ACTIVATE_DEVT activate_event = {};
  activate_event.rf_disc_id = activated.rf_discovery_id();
  activate_event.protocol = GetProtocol(activated.rf_protocol());
  activate_event.data_mode = GetDiscovery(activated.data_mode());
  activate_event.tx_bitrate = activated.tx_bitrate();
  activate_event.rx_bitrate = activated.rx_bitrate();
  uint8_t buff_size = activated.buff_size();
  uint8_t num_buff = activated.num_buff();

  std::vector<uint8_t> packet = SerializeNfcActivate(
      activate_event, buff_size, num_buff, activated.rf_tech_param_buffer(),
      activated.intf_param_buffer());

  g_fake_hal->SimulatePacketArrival(NCI_MT_NTF, 0, NCI_GID_RF_MANAGE,
                                    NCI_MSG_RF_INTF_ACTIVATED, packet.data(),
                                    packet.size());
}

void DoRfManagementNtf(const RfManagementNtf& ntf) {
  switch (ntf.opcode_case()) {
    case RfManagementNtf::kIntfActivated: {
      DoRfManageIntfActivated(ntf.intf_activated());
      break;
    }
    case RfManagementNtf::OPCODE_NOT_SET: {
      break;
    }
  }
}

void DoMtNtf(const MtNtf& ntf) {
  switch (ntf.gid_case()) {
    case MtNtf::kRfManage: {
      DoRfManagementNtf(ntf.rf_manage());
      break;
    }
    case MtNtf::GID_NOT_SET: {
      break;
    }
  }
}

void DoStructuredPacket(const SimulateStructuredPacket& packet) {
  switch (packet.packet_case()) {
    case SimulateStructuredPacket::kNtf: {
      DoMtNtf(packet.ntf());
      break;
    }
    case SimulateStructuredPacket::PACKET_NOT_SET: {
      break;
    }
  }
}

void DoPacket(const SimulatePacketArrival& packet) {
  uint8_t mt = packet.mt();
  uint8_t pbf = packet.pbf();
  uint8_t gid = packet.gid();
  uint8_t opcode = static_cast<uint8_t>(packet.opcode());
  bool need_flush = false;
  if (mt == NCI_MT_DATA) {
    // The gid field will be used as the connection ID. We should handle this
    // a lot better but for now we let the existing gid enum get interpreted
    // as a connection ID.
    // gid = 0;
    opcode = 0;
  }

  g_fake_hal->SimulatePacketArrival(mt, pbf, gid, opcode,
                                    (unsigned char*)packet.packet().data(),
                                    packet.packet().size());
  if (need_flush) {
    DoAllTasks(false);
  }
}

void NfcIntegrationFuzzer::DoOneCommand(
    std::vector<std::vector<uint8_t>>& bytes_container,
    const Command& command) {
  switch (command.command_case()) {
    case Command::kSimulatePacketArrival: {
      DoPacket(command.simulate_packet_arrival());
      break;
    }
    case Command::kSimulateHalEvent: {
      g_fake_hal->SimulateHALEvent(command.simulate_hal_event().hal_event(),
                                   command.simulate_hal_event().hal_status());
      break;
    }
    case Command::kSimulateStructuredPacket: {
      DoStructuredPacket(command.simulate_structured_packet());
      break;
    }
    case Command::kSendRawFrame: {
      std::vector<uint8_t> frame(
          command.send_raw_frame().data(),
          command.send_raw_frame().data() + command.send_raw_frame().size());
      uint16_t frameSize =
          frame.size() <= kMaxFramesSize ? frame.size() : kMaxFramesSize;
      NFA_SendRawFrame(frame.data(), frameSize,
                       /*presence check start delay*/ 0);
      break;
    }
    case Command::kDoNciMessages: {
      nfc_process_nci_messages();
      break;
    }
    case Command::kDoNfaTasks: {
      nfc_process_nfa_messages();
      break;
    }
    case Command::kSimulateTimerEvent: {
      nfc_process_timer_evt();
      break;
    }
    case Command::kSimulateQuickTimerEvent: {
      nfc_process_quick_timer_evt();
      break;
    }
    case Command::kSelect: {
      NFA_Select(command.select().rf_select_id(),
                 GetProtocol(command.select().protocol()),
                 command.select().rf_interface());
      break;
    }
    case Command::kConfigureUiccListenTech: {
      if (g_ee_info.num_ee > 0) {
        uint8_t handle = command.configure_uicc_listen_tech().ee_handle();
        handle = g_ee_info.ee_disc_info[handle % g_ee_info.num_ee].ee_handle;
        NFA_CeConfigureUiccListenTech(
            handle, command.configure_uicc_listen_tech().tech_mask());
        NFA_EeClearDefaultTechRouting(handle, 0xFF);
        NFA_EeSetDefaultTechRouting(handle, 0xFF, 0, 0, 0, 0, 0);
      }
      break;
    }
    case Command::kRegisterT3T: {
      uint8_t nfcid2[NCI_RF_F_UID_LEN] = {};
      uint8_t t3tPmm[NCI_T3T_PMM_LEN] = {};
      NFA_CeRegisterFelicaSystemCodeOnDH(0, nfcid2, t3tPmm, nfa_conn_callback);
      const uint8_t SYS_CODE_PWR_STATE_HOST = 0x01;
      NFA_EeAddSystemCodeRouting(0, NCI_DH_ID, SYS_CODE_PWR_STATE_HOST);
      break;
    }
    case Command::kStartRfDiscovery: {
      NFA_StartRfDiscovery();
      break;
    }
    case Command::kStopRfDiscovery: {
      NFA_StopRfDiscovery();
      break;
    }
    case Command::kSetIsoListenTech: {
      NFA_CeSetIsoDepListenTech(
          fuzzed_data_.ConsumeIntegralInRange<uint8_t>(0, 0xFF));
      NFA_CeRegisterAidOnDH(nullptr, 0, nfa_conn_callback);
      break;
    }
    case Command::kRwFormatTag: {
      NFA_RwFormatTag();
      break;
    }
    case Command::kRwPresenceCheck: {
      NFA_RwPresenceCheck(command.rw_presence_check().option());
      break;
    }
    case Command::kRwSetTagReadOnly: {
      NFA_RwSetTagReadOnly(command.rw_set_tag_read_only());
      break;
    }
    case Command::kEeUpdateNow: {
      NFA_EeUpdateNow();
      break;
    }
    case Command::kEeAddAidRouting: {
      uint8_t handle = command.ee_add_aid_routing().ee_handle();
      if (g_ee_info.num_ee) {
        handle = g_ee_info.ee_disc_info[handle % g_ee_info.num_ee].ee_handle;
      }
      std::vector<uint8_t> aid(command.ee_add_aid_routing().aid().data(),
                               command.ee_add_aid_routing().aid().data() +
                                   command.ee_add_aid_routing().aid().size());
      tNFA_EE_PWR_STATE power_state =
          command.ee_add_aid_routing().power_state();
      uint8_t aidInfo = command.ee_add_aid_routing().aid_info();
      NFA_EeAddAidRouting(handle, aid.size(), aid.data(), power_state, aidInfo);
      break;
    }
    case Command::kReadNdef: {
      NFA_RwReadNDef();
      break;
    }
    case Command::kDetectNdef: {
      NFA_RwDetectNDef();
      break;
    }
    case Command::kWriteNdef: {
      bytes_container.emplace_back(command.write_ndef().size() % 1024);
      NFA_RwWriteNDef(bytes_container.back().data(),
                      bytes_container.back().size());
      break;
    }
    case Command::kP2PRegisterServer: {
      NFA_P2pSetLLCPConfig(LLCP_MAX_MIU, LLCP_OPT_VALUE, LLCP_WAITING_TIME,
                           LLCP_LTO_VALUE, 0, 0, LLCP_DELAY_RESP_TIME,
                           LLCP_DATA_LINK_TIMEOUT,
                           LLCP_DELAY_TIME_TO_SEND_FIRST_PDU);
      NFA_P2pRegisterServer(
          command.p2p_register_server().server_sap() & 0xFF, NFA_P2P_DLINK_TYPE,
          (char*)command.p2p_register_server().service_name().c_str(),
          nfa_p2p_callback);
      break;
    }
    case Command::kP2PAcceptConn: {
      NFA_P2pAcceptConn(command.p2p_accept_conn().handle() & 0xFF,
                        command.p2p_accept_conn().miu() & 0xFFFF,
                        command.p2p_accept_conn().rw() & 0xFF);
      break;
    }
    case Command::kP2PRegisterClient: {
      NFA_P2pRegisterClient(NFA_P2P_DLINK_TYPE, nfa_p2p_callback);
      break;
    }
    case Command::kP2PDeregister: {
      NFA_P2pDeregister(command.p2p_deregister());
      break;
    }
    case Command::kP2PConnectBySap: {
      NFA_P2pConnectBySap(command.p2p_connect_by_sap().handle(),
                          command.p2p_connect_by_sap().dsap(),
                          command.p2p_connect_by_sap().miu(),
                          command.p2p_connect_by_sap().rw());
      break;
    }
    case Command::kP2PConnectByName: {
      NFA_P2pConnectByName(
          command.p2p_connect_by_name().client_handle(),
          (char*)command.p2p_connect_by_name().service_name().c_str(),
          command.p2p_connect_by_name().miu(),
          command.p2p_connect_by_name().rw());
      break;
    }
    case Command::kP2PSendUi: {
      std::vector<uint8_t> buffer(command.p2p_send_ui().data().data(),
                                  command.p2p_send_ui().data().data() +
                                      command.p2p_send_ui().data().size());
      NFA_P2pSendUI(command.p2p_send_ui().handle(),
                    command.p2p_send_ui().dsap(), buffer.size(), buffer.data());
      break;
    }
    case Command::kP2PDisconnect: {
      NFA_P2pDisconnect(command.p2p_disconnect().handle(),
                        command.p2p_disconnect().flush());
      break;
    }
    case Command::kPauseP2P: {
      NFA_PauseP2p();
      break;
    }
    case Command::kResumeP2P: {
      NFA_ResumeP2p();
      break;
    }
    case Command::kP2PReadData: {
      std::vector<uint8_t> buffer((size_t)command.p2p_read_data().length() %
                                  1024);
      bool is_more_data = false;
      unsigned int actual_size = 0;
      NFA_P2pReadData(command.p2p_read_data().handle(), buffer.size(),
                      &actual_size, buffer.data(), &is_more_data);
      break;
    }
    case Command::kP2PSendData: {
      std::vector<uint8_t> buffer(command.p2p_send_data().data().data(),
                                  command.p2p_send_data().data().data() +
                                      command.p2p_send_data().data().size());
      NFA_P2pSendData(command.p2p_send_data().handle(), buffer.size(),
                      buffer.data());
      break;
    }
    case Command::COMMAND_NOT_SET: {
      break;
    }
  }
}

NfcIntegrationFuzzer::NfcIntegrationFuzzer(const Session* session)
    : session_(session),
      fuzzed_data_(
          reinterpret_cast<const uint8_t*>(session->data_provider().data()),
          session->data_provider().size()) {
  g_fuzzed_data = &fuzzed_data_;
}

bool NfcIntegrationFuzzer::Setup() {
  g_tick_count = 0;
  memset(&g_ee_info, 0, sizeof(g_ee_info));
  NFA_Init(&fuzzed_hal_entry);

  rw_cb.p_cback = &fuzz_cback;
  NFA_Enable(nfa_dm_callback, nfa_conn_callback);
  DoAllTasks(false);

  NFA_EeRegister(nfa_ee_callback);
  NFA_EeSetDefaultProtoRouting(NFC_DH_ID, NFA_PROTOCOL_MASK_ISO_DEP, 0, 0, 0, 0,
                               0);

  DoPacket(session_->setup_packet());
  g_saw_event = false;
  DoAllTasks(false);
  if (!g_saw_event) {
    return false;
  }

  NFA_EnableListening();
  NFA_EnablePolling(NFA_TECHNOLOGY_MASK_A | NFA_TECHNOLOGY_MASK_B |
                    NFA_TECHNOLOGY_MASK_F | NFA_TECHNOLOGY_MASK_V |
                    NFA_TECHNOLOGY_MASK_B_PRIME | NFA_TECHNOLOGY_MASK_A_ACTIVE |
                    NFA_TECHNOLOGY_MASK_F_ACTIVE | NFA_TECHNOLOGY_MASK_KOVIO);

  NFA_EnableDtamode(static_cast<tNFA_eDtaModes>(session_->dta_mode()));
  NFA_StartRfDiscovery();

  DoAllTasks(false);
  return true;
}

void NfcIntegrationFuzzer::RunCommands() {
  for (const Command& command : session_->commands()) {
    DoOneCommand(bytes_container_, command);
  }
}

void NfcIntegrationFuzzer::TearDown() {
  // Do any remaining tasks including pending timers
  DoAllTasks(true);

  // Issue a disable command then clear pending tasks
  // and timers again
  NFA_Disable(false);
  DoAllTasks(true);
}
