// Copyright 2023, The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//! Implementation of the NFCC.

use crate::packets::{nci, rf};
use anyhow::Result;
use core::time::Duration;
use futures::StreamExt;
use log::{debug, error, info, trace, warn};
use pdl_runtime::Packet;
use std::convert::TryFrom;
use std::future::Future;
use std::pin::pin;
use std::time::Instant;
use tokio::sync::mpsc;
use tokio::time;

const NCI_VERSION: nci::NciVersion = nci::NciVersion::Version20;
const MANUFACTURER_ID: u8 = 0x02;
const MANUFACTURER_SPECIFIC_INFORMATION: [u8; 26] =
    [5, 3, 3, 19, 4, 25, 1, 7, 0, 0, 68, 100, 214, 0, 0, 90, 172, 0, 0, 0, 1, 44, 176, 153, 243, 0];

/// Read-only configuration parameters
const PB_ATTRIB_PARAM1: u8 = 0x00;
const LF_T3T_MAX: u8 = 16;
const LLCP_VERSION: u8 = 0x00;

/// Writable configuration parameters with default
/// value defined by the NFCC.
const TOTAL_DURATION: u16 = 1000;
const PA_DEVICES_LIMIT: u8 = 255;
const PB_DEVICES_LIMIT: u8 = 255;
const PF_DEVICES_LIMIT: u8 = 255;
const PV_DEVICES_LIMIT: u8 = 255;
const LA_BIT_FRAME_SDD: u8 = 0x10;
const LA_PLATFORM_CONFIG: u8 = 0x0c;
const LA_SEL_INFO: u8 = 0x60; // Supports ISO-DEP and NFC-DEP.
const LB_SENSB_INFO: u8 = 0x1; // Supports ISO-DEP.
const LB_SFGI: u8 = 0;
const LB_FWI_ADC_FO: u8 = 0x00;
const LF_PROTOCOL_TYPE: u8 = 0x02; // Supports NFC-DEP.
const LI_A_RATS_TB1: u8 = 0x70;
const LI_A_RATS_TC1: u8 = 0x02;

const MAX_LOGICAL_CONNECTIONS: u8 = 2;
const MAX_ROUTING_TABLE_SIZE: u16 = 512;
const MAX_CONTROL_PACKET_PAYLOAD_SIZE: u8 = 255;
const MAX_DATA_PACKET_PAYLOAD_SIZE: u8 = 255;
const NUMBER_OF_CREDITS: u8 = 1;
const MAX_NFCV_RF_FRAME_SIZE: u16 = 512;

/// Time in milliseconds that Casimir waits for poll responses after
/// sending a poll command.
const POLL_RESPONSE_TIMEOUT: u64 = 200;

/// All configuration parameters of the NFCC.
/// The configuration is filled with default values from the specification
/// See [NCI] Table 46: Common Parameters for Discovery Configuration
/// for the format of each parameter and the default value.
#[derive(Clone, Debug, PartialEq, Eq)]
#[allow(missing_docs)]
pub struct ConfigParameters {
    total_duration: u16,
    /// [NCI] Table 47: Values for CON_DISCOVERY_PARAM.
    con_discovery_param: u8,
    power_state: u8,
    pa_bail_out: u8,
    pa_devices_limit: u8,
    pb_afi: u8,
    pb_bail_out: u8,
    pb_attrib_param1: u8,
    /// [NCI] Table 26: Values for PB_SENSB_REQ_PARAM.
    pb_sensb_req_param: u8,
    pb_devices_limit: u8,
    pf_bit_rate: u8,
    pf_bail_out: u8,
    pf_devices_limit: u8,
    pi_b_h_info: Vec<u8>,
    pi_bit_rate: u8,
    pn_nfc_dep_psl: u8,
    pn_atr_req_gen_bytes: Vec<u8>,
    /// [NCI] Table 30: Values for PN_ATR_REQ_CONFIG.
    pn_atr_req_config: u8,
    pv_devices_limit: u8,
    la_bit_frame_sdd: u8,
    la_platform_config: u8,
    /// [NCI] Table 34: LA_SEL_INFO Coding.
    la_sel_info: u8,
    la_nfcid1: Vec<u8>,
    /// [NCI] Table 36: LB_SENSB_INFO Values.
    lb_sensb_info: u8,
    lb_nfcid0: [u8; 4],
    lb_application_data: u32,
    lb_sfgi: u8,
    /// [NCI] Table 37: LB_FWI_ADC_FO Values.
    lb_fwi_adc_fo: u8,
    lb_bit_rate: u8,
    lf_t3t_identifiers_1: [u8; 18],
    lf_t3t_identifiers_2: [u8; 18],
    lf_t3t_identifiers_3: [u8; 18],
    lf_t3t_identifiers_4: [u8; 18],
    lf_t3t_identifiers_5: [u8; 18],
    lf_t3t_identifiers_6: [u8; 18],
    lf_t3t_identifiers_7: [u8; 18],
    lf_t3t_identifiers_8: [u8; 18],
    lf_t3t_identifiers_9: [u8; 18],
    lf_t3t_identifiers_10: [u8; 18],
    lf_t3t_identifiers_11: [u8; 18],
    lf_t3t_identifiers_12: [u8; 18],
    lf_t3t_identifiers_13: [u8; 18],
    lf_t3t_identifiers_14: [u8; 18],
    lf_t3t_identifiers_15: [u8; 18],
    lf_t3t_identifiers_16: [u8; 18],
    lf_t3t_pmm_default: [u8; 8],
    lf_t3t_max: u8,
    lf_t3t_flags: u16,
    lf_t3t_rd_allowed: u8,
    /// [NCI] Table 39: Supported Protocols for Listen F.
    lf_protocol_type: u8,
    li_a_rats_tb1: u8,
    li_a_hist_by: Vec<u8>,
    li_b_h_info_resp: Vec<u8>,
    li_a_bit_rate: u8,
    li_a_rats_tc1: u8,
    ln_wt: u8,
    ln_atr_res_gen_bytes: Vec<u8>,
    ln_atr_res_config: u8,
    pacm_bit_rate: u8,
    /// [NCI] Table 23: RF Field Information Configuration Parameter.
    rf_field_info: u8,
    rf_nfcee_action: u8,
    nfcdep_op: u8,
    /// [NCI] Table 115: LLCP Version Parameter.
    llcp_version: u8,
    /// [NCI] Table 65: Value Field for NFCC Configuration Control.
    nfcc_config_control: u8,
}

/// State of an NFCC logical connection with the DH.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
#[allow(missing_docs)]
pub enum LogicalConnection {
    RemoteNfcEndpoint { rf_discovery_id: u8, rf_protocol_type: nci::RfProtocolType },
}

/// State of the RF Discovery of an NFCC instance.
/// The state WaitForAllDiscoveries is not represented as it is implied
/// by the discovery routine.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
#[allow(missing_docs)]
pub enum RfState {
    Idle,
    Discovery,
    PollActive {
        id: u16,
        rf_interface: nci::RfInterfaceType,
        rf_technology: rf::Technology,
        rf_protocol: rf::Protocol,
    },
    ListenSleep {
        id: u16,
    },
    ListenActive {
        id: u16,
        rf_interface: nci::RfInterfaceType,
        rf_technology: rf::Technology,
        rf_protocol: rf::Protocol,
    },
    WaitForHostSelect,
    WaitForSelectResponse {
        id: u16,
        rf_discovery_id: usize,
        rf_interface: nci::RfInterfaceType,
        rf_technology: rf::Technology,
        rf_protocol: rf::Protocol,
    },
}

/// State of the emulated eSE (ST) NFCEE.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
#[allow(missing_docs)]
pub enum NfceeState {
    Enabled,
    Disabled,
}

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
#[allow(missing_docs)]
pub enum RfMode {
    Poll,
    Listen,
}

/// Poll responses received in the context of RF discovery in active
/// Listen mode.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct RfPollResponse {
    id: u16,
    rf_protocol: rf::Protocol,
    rf_technology: rf::Technology,
    rf_technology_specific_parameters: Vec<u8>,
}

/// State of an NFCC instance.
#[allow(missing_docs)]
pub struct State {
    pub config_parameters: ConfigParameters,
    pub logical_connections: [Option<LogicalConnection>; MAX_LOGICAL_CONNECTIONS as usize],
    pub discover_configuration: Vec<nci::DiscoverConfiguration>,
    pub discover_map: Vec<nci::MappingConfiguration>,
    pub nfcee_state: NfceeState,
    pub rf_state: RfState,
    pub rf_poll_responses: Vec<RfPollResponse>,
    pub rf_activation_parameters: Vec<u8>,
    pub passive_observe_mode: nci::PassiveObserveMode,
    pub start_time: std::time::Instant,
}

/// State of an NFCC instance.
pub struct Controller<'a> {
    id: u16,
    nci_stream: nci::StreamRefMut<'a>,
    nci_writer: nci::Writer,
    rf_rx: mpsc::UnboundedReceiver<rf::RfPacket>,
    rf_tx: mpsc::UnboundedSender<rf::RfPacket>,
    state: State,
}

impl ConfigParameters {
    fn get(&self, id: nci::ConfigParameterId) -> Result<Vec<u8>> {
        match id {
            nci::ConfigParameterId::TotalDuration => Ok(self.total_duration.to_le_bytes().to_vec()),
            nci::ConfigParameterId::ConDiscoveryParam => {
                Ok(self.con_discovery_param.to_le_bytes().to_vec())
            }
            nci::ConfigParameterId::PowerState => Ok(vec![self.power_state]),
            nci::ConfigParameterId::PaBailOut => Ok(vec![self.pa_bail_out]),
            nci::ConfigParameterId::PaDevicesLimit => Ok(vec![self.pa_devices_limit]),
            nci::ConfigParameterId::PbAfi => Ok(vec![self.pb_afi]),
            nci::ConfigParameterId::PbBailOut => Ok(vec![self.pb_bail_out]),
            nci::ConfigParameterId::PbAttribParam1 => Ok(vec![self.pb_attrib_param1]),
            nci::ConfigParameterId::PbSensbReqParam => Ok(vec![self.pb_sensb_req_param]),
            nci::ConfigParameterId::PbDevicesLimit => Ok(vec![self.pb_devices_limit]),
            nci::ConfigParameterId::PfBitRate => Ok(vec![self.pf_bit_rate]),
            nci::ConfigParameterId::PfBailOut => Ok(vec![self.pf_bail_out]),
            nci::ConfigParameterId::PfDevicesLimit => Ok(vec![self.pf_devices_limit]),
            nci::ConfigParameterId::PiBHInfo => Ok(self.pi_b_h_info.clone()),
            nci::ConfigParameterId::PiBitRate => Ok(vec![self.pi_bit_rate]),
            nci::ConfigParameterId::PnNfcDepPsl => Ok(vec![self.pn_nfc_dep_psl]),
            nci::ConfigParameterId::PnAtrReqGenBytes => Ok(self.pn_atr_req_gen_bytes.clone()),
            nci::ConfigParameterId::PnAtrReqConfig => Ok(vec![self.pn_atr_req_config]),
            nci::ConfigParameterId::PvDevicesLimit => Ok(vec![self.pv_devices_limit]),
            nci::ConfigParameterId::LaBitFrameSdd => Ok(vec![self.la_bit_frame_sdd]),
            nci::ConfigParameterId::LaPlatformConfig => Ok(vec![self.la_platform_config]),
            nci::ConfigParameterId::LaSelInfo => Ok(vec![self.la_sel_info]),
            nci::ConfigParameterId::LaNfcid1 => Ok(self.la_nfcid1.clone()),
            nci::ConfigParameterId::LbSensbInfo => Ok(vec![self.lb_sensb_info]),
            nci::ConfigParameterId::LbNfcid0 => Ok(self.lb_nfcid0.to_vec()),
            nci::ConfigParameterId::LbApplicationData => {
                Ok(self.lb_application_data.to_le_bytes().to_vec())
            }
            nci::ConfigParameterId::LbSfgi => Ok(vec![self.lb_sfgi]),
            nci::ConfigParameterId::LbFwiAdcFo => Ok(vec![self.lb_fwi_adc_fo]),
            nci::ConfigParameterId::LbBitRate => Ok(vec![self.lb_bit_rate]),
            nci::ConfigParameterId::LfT3tIdentifiers1 => Ok(self.lf_t3t_identifiers_1.to_vec()),
            nci::ConfigParameterId::LfT3tIdentifiers2 => Ok(self.lf_t3t_identifiers_2.to_vec()),
            nci::ConfigParameterId::LfT3tIdentifiers3 => Ok(self.lf_t3t_identifiers_3.to_vec()),
            nci::ConfigParameterId::LfT3tIdentifiers4 => Ok(self.lf_t3t_identifiers_4.to_vec()),
            nci::ConfigParameterId::LfT3tIdentifiers5 => Ok(self.lf_t3t_identifiers_5.to_vec()),
            nci::ConfigParameterId::LfT3tIdentifiers6 => Ok(self.lf_t3t_identifiers_6.to_vec()),
            nci::ConfigParameterId::LfT3tIdentifiers7 => Ok(self.lf_t3t_identifiers_7.to_vec()),
            nci::ConfigParameterId::LfT3tIdentifiers8 => Ok(self.lf_t3t_identifiers_8.to_vec()),
            nci::ConfigParameterId::LfT3tIdentifiers9 => Ok(self.lf_t3t_identifiers_9.to_vec()),
            nci::ConfigParameterId::LfT3tIdentifiers10 => Ok(self.lf_t3t_identifiers_10.to_vec()),
            nci::ConfigParameterId::LfT3tIdentifiers11 => Ok(self.lf_t3t_identifiers_11.to_vec()),
            nci::ConfigParameterId::LfT3tIdentifiers12 => Ok(self.lf_t3t_identifiers_12.to_vec()),
            nci::ConfigParameterId::LfT3tIdentifiers13 => Ok(self.lf_t3t_identifiers_13.to_vec()),
            nci::ConfigParameterId::LfT3tIdentifiers14 => Ok(self.lf_t3t_identifiers_14.to_vec()),
            nci::ConfigParameterId::LfT3tIdentifiers15 => Ok(self.lf_t3t_identifiers_15.to_vec()),
            nci::ConfigParameterId::LfT3tIdentifiers16 => Ok(self.lf_t3t_identifiers_16.to_vec()),
            nci::ConfigParameterId::LfT3tPmmDefault => Ok(self.lf_t3t_pmm_default.to_vec()),
            nci::ConfigParameterId::LfT3tMax => Ok(vec![self.lf_t3t_max]),
            nci::ConfigParameterId::LfT3tFlags => Ok(self.lf_t3t_flags.to_le_bytes().to_vec()),
            nci::ConfigParameterId::LfT3tRdAllowed => Ok(vec![self.lf_t3t_rd_allowed]),
            nci::ConfigParameterId::LfProtocolType => Ok(vec![self.lf_protocol_type]),
            nci::ConfigParameterId::LiARatsTb1 => Ok(vec![self.li_a_rats_tb1]),
            nci::ConfigParameterId::LiAHistBy => Ok(self.li_a_hist_by.clone()),
            nci::ConfigParameterId::LiBHInfoResp => Ok(self.li_b_h_info_resp.clone()),
            nci::ConfigParameterId::LiABitRate => Ok(vec![self.li_a_bit_rate]),
            nci::ConfigParameterId::LiARatsTc1 => Ok(vec![self.li_a_rats_tc1]),
            nci::ConfigParameterId::LnWt => Ok(vec![self.ln_wt]),
            nci::ConfigParameterId::LnAtrResGenBytes => Ok(self.ln_atr_res_gen_bytes.clone()),
            nci::ConfigParameterId::LnAtrResConfig => Ok(vec![self.ln_atr_res_config]),
            nci::ConfigParameterId::PacmBitRate => Ok(vec![self.pacm_bit_rate]),
            nci::ConfigParameterId::RfFieldInfo => Ok(vec![self.rf_field_info]),
            nci::ConfigParameterId::RfNfceeAction => Ok(vec![self.rf_nfcee_action]),
            nci::ConfigParameterId::NfcdepOp => Ok(vec![self.nfcdep_op]),
            nci::ConfigParameterId::LlcpVersion => Ok(vec![self.llcp_version]),
            nci::ConfigParameterId::NfccConfigControl => Ok(vec![self.nfcc_config_control]),
            _ => Err(anyhow::anyhow!("unknown config parameter ID")),
        }
    }

    fn set(&mut self, id: nci::ConfigParameterId, value: &[u8]) -> Result<()> {
        match id {
            nci::ConfigParameterId::TotalDuration => {
                self.total_duration = u16::from_le_bytes(value.try_into()?);
                Ok(())
            }
            nci::ConfigParameterId::ConDiscoveryParam => {
                self.con_discovery_param = u8::from_le_bytes(value.try_into()?);
                Ok(())
            }
            nci::ConfigParameterId::PowerState => {
                self.power_state = u8::from_le_bytes(value.try_into()?);
                Ok(())
            }
            nci::ConfigParameterId::PaBailOut => {
                self.pa_bail_out = u8::from_le_bytes(value.try_into()?);
                Ok(())
            }
            nci::ConfigParameterId::PaDevicesLimit => {
                self.pa_devices_limit = u8::from_le_bytes(value.try_into()?);
                Ok(())
            }
            nci::ConfigParameterId::PbAfi => {
                self.pb_afi = u8::from_le_bytes(value.try_into()?);
                Ok(())
            }
            nci::ConfigParameterId::PbBailOut => {
                self.pb_bail_out = u8::from_le_bytes(value.try_into()?);
                Ok(())
            }
            nci::ConfigParameterId::PbAttribParam1 => {
                self.pb_attrib_param1 = u8::from_le_bytes(value.try_into()?);
                Ok(())
            }
            nci::ConfigParameterId::PbSensbReqParam => {
                self.pb_sensb_req_param = u8::from_le_bytes(value.try_into()?);
                Ok(())
            }
            nci::ConfigParameterId::PbDevicesLimit => {
                self.pb_devices_limit = u8::from_le_bytes(value.try_into()?);
                Ok(())
            }
            nci::ConfigParameterId::PfBitRate => {
                self.pf_bit_rate = u8::from_le_bytes(value.try_into()?);
                Ok(())
            }
            nci::ConfigParameterId::PfBailOut => {
                self.pf_bail_out = u8::from_le_bytes(value.try_into()?);
                Ok(())
            }
            nci::ConfigParameterId::PfDevicesLimit => {
                self.pf_devices_limit = u8::from_le_bytes(value.try_into()?);
                Ok(())
            }
            nci::ConfigParameterId::PiBHInfo => {
                self.pi_b_h_info = value.to_vec();
                Ok(())
            }
            nci::ConfigParameterId::PiBitRate => {
                self.pi_bit_rate = u8::from_le_bytes(value.try_into()?);
                Ok(())
            }
            nci::ConfigParameterId::PnNfcDepPsl => {
                self.pn_nfc_dep_psl = u8::from_le_bytes(value.try_into()?);
                Ok(())
            }
            nci::ConfigParameterId::PnAtrReqGenBytes => {
                self.pn_atr_req_gen_bytes = value.to_vec();
                Ok(())
            }
            nci::ConfigParameterId::PnAtrReqConfig => {
                self.pn_atr_req_config = u8::from_le_bytes(value.try_into()?);
                Ok(())
            }
            nci::ConfigParameterId::PvDevicesLimit => {
                self.pv_devices_limit = u8::from_le_bytes(value.try_into()?);
                Ok(())
            }
            nci::ConfigParameterId::LaBitFrameSdd => {
                self.la_bit_frame_sdd = u8::from_le_bytes(value.try_into()?);
                Ok(())
            }
            nci::ConfigParameterId::LaPlatformConfig => {
                self.la_platform_config = u8::from_le_bytes(value.try_into()?);
                Ok(())
            }
            nci::ConfigParameterId::LaSelInfo => {
                self.la_sel_info = u8::from_le_bytes(value.try_into()?);
                Ok(())
            }
            nci::ConfigParameterId::LaNfcid1 => {
                self.la_nfcid1 = value.to_vec();
                Ok(())
            }
            nci::ConfigParameterId::LbSensbInfo => {
                self.lb_sensb_info = u8::from_le_bytes(value.try_into()?);
                Ok(())
            }
            nci::ConfigParameterId::LbNfcid0 => {
                self.lb_nfcid0 = value.try_into()?;
                Ok(())
            }
            nci::ConfigParameterId::LbApplicationData => {
                self.lb_application_data = u32::from_le_bytes(value.try_into()?);
                Ok(())
            }
            nci::ConfigParameterId::LbSfgi => {
                self.lb_sfgi = u8::from_le_bytes(value.try_into()?);
                Ok(())
            }
            nci::ConfigParameterId::LbFwiAdcFo => {
                self.lb_fwi_adc_fo = u8::from_le_bytes(value.try_into()?);
                Ok(())
            }
            nci::ConfigParameterId::LbBitRate => {
                self.lb_bit_rate = u8::from_le_bytes(value.try_into()?);
                Ok(())
            }
            nci::ConfigParameterId::LfT3tIdentifiers1 => {
                self.lf_t3t_identifiers_1 = value.try_into()?;
                Ok(())
            }
            nci::ConfigParameterId::LfT3tIdentifiers2 => {
                self.lf_t3t_identifiers_2 = value.try_into()?;
                Ok(())
            }
            nci::ConfigParameterId::LfT3tIdentifiers3 => {
                self.lf_t3t_identifiers_3 = value.try_into()?;
                Ok(())
            }
            nci::ConfigParameterId::LfT3tIdentifiers4 => {
                self.lf_t3t_identifiers_4 = value.try_into()?;
                Ok(())
            }
            nci::ConfigParameterId::LfT3tIdentifiers5 => {
                self.lf_t3t_identifiers_5 = value.try_into()?;
                Ok(())
            }
            nci::ConfigParameterId::LfT3tIdentifiers6 => {
                self.lf_t3t_identifiers_6 = value.try_into()?;
                Ok(())
            }
            nci::ConfigParameterId::LfT3tIdentifiers7 => {
                self.lf_t3t_identifiers_7 = value.try_into()?;
                Ok(())
            }
            nci::ConfigParameterId::LfT3tIdentifiers8 => {
                self.lf_t3t_identifiers_8 = value.try_into()?;
                Ok(())
            }
            nci::ConfigParameterId::LfT3tIdentifiers9 => {
                self.lf_t3t_identifiers_9 = value.try_into()?;
                Ok(())
            }
            nci::ConfigParameterId::LfT3tIdentifiers10 => {
                self.lf_t3t_identifiers_10 = value.try_into()?;
                Ok(())
            }
            nci::ConfigParameterId::LfT3tIdentifiers11 => {
                self.lf_t3t_identifiers_11 = value.try_into()?;
                Ok(())
            }
            nci::ConfigParameterId::LfT3tIdentifiers12 => {
                self.lf_t3t_identifiers_12 = value.try_into()?;
                Ok(())
            }
            nci::ConfigParameterId::LfT3tIdentifiers13 => {
                self.lf_t3t_identifiers_13 = value.try_into()?;
                Ok(())
            }
            nci::ConfigParameterId::LfT3tIdentifiers14 => {
                self.lf_t3t_identifiers_14 = value.try_into()?;
                Ok(())
            }
            nci::ConfigParameterId::LfT3tIdentifiers15 => {
                self.lf_t3t_identifiers_15 = value.try_into()?;
                Ok(())
            }
            nci::ConfigParameterId::LfT3tIdentifiers16 => {
                self.lf_t3t_identifiers_16 = value.try_into()?;
                Ok(())
            }
            nci::ConfigParameterId::LfT3tPmmDefault => {
                self.lf_t3t_pmm_default = value.try_into()?;
                Ok(())
            }
            nci::ConfigParameterId::LfT3tMax => Err(anyhow::anyhow!("read-only config parameter")),
            nci::ConfigParameterId::LfT3tFlags => {
                self.lf_t3t_flags = u16::from_le_bytes(value.try_into()?);
                Ok(())
            }
            nci::ConfigParameterId::LfT3tRdAllowed => {
                self.lf_t3t_rd_allowed = u8::from_le_bytes(value.try_into()?);
                Ok(())
            }
            nci::ConfigParameterId::LfProtocolType => {
                self.lf_protocol_type = u8::from_le_bytes(value.try_into()?);
                Ok(())
            }
            nci::ConfigParameterId::LiARatsTb1 => {
                self.li_a_rats_tb1 = u8::from_le_bytes(value.try_into()?);
                Ok(())
            }
            nci::ConfigParameterId::LiAHistBy => {
                self.li_a_hist_by = value.to_vec();
                Ok(())
            }
            nci::ConfigParameterId::LiBHInfoResp => {
                self.li_b_h_info_resp = value.to_vec();
                Ok(())
            }
            nci::ConfigParameterId::LiABitRate => {
                self.li_a_bit_rate = u8::from_le_bytes(value.try_into()?);
                Ok(())
            }
            nci::ConfigParameterId::LiARatsTc1 => {
                self.li_a_rats_tc1 = u8::from_le_bytes(value.try_into()?);
                Ok(())
            }
            nci::ConfigParameterId::LnWt => {
                self.ln_wt = u8::from_le_bytes(value.try_into()?);
                Ok(())
            }
            nci::ConfigParameterId::LnAtrResGenBytes => {
                self.ln_atr_res_gen_bytes = value.to_vec();
                Ok(())
            }
            nci::ConfigParameterId::LnAtrResConfig => {
                self.ln_atr_res_config = u8::from_le_bytes(value.try_into()?);
                Ok(())
            }
            nci::ConfigParameterId::PacmBitRate => {
                self.pacm_bit_rate = u8::from_le_bytes(value.try_into()?);
                Ok(())
            }
            nci::ConfigParameterId::RfFieldInfo => {
                self.rf_field_info = u8::from_le_bytes(value.try_into()?);
                Ok(())
            }
            nci::ConfigParameterId::RfNfceeAction => {
                self.rf_nfcee_action = u8::from_le_bytes(value.try_into()?);
                Ok(())
            }
            nci::ConfigParameterId::NfcdepOp => {
                self.nfcdep_op = u8::from_le_bytes(value.try_into()?);
                Ok(())
            }
            nci::ConfigParameterId::LlcpVersion => {
                self.llcp_version = u8::from_le_bytes(value.try_into()?);
                Ok(())
            }
            nci::ConfigParameterId::NfccConfigControl => {
                self.nfcc_config_control = u8::from_le_bytes(value.try_into()?);
                Ok(())
            }
            _ => Err(anyhow::anyhow!("unknown config parameter ID")),
        }
    }
}

impl Default for ConfigParameters {
    fn default() -> Self {
        ConfigParameters {
            total_duration: TOTAL_DURATION,
            con_discovery_param: 0x01,
            power_state: 0x02,
            pa_bail_out: 0x00,
            pa_devices_limit: PA_DEVICES_LIMIT,
            pb_afi: 0x00,
            pb_bail_out: 0x00,
            pb_attrib_param1: PB_ATTRIB_PARAM1,
            pb_sensb_req_param: 0x00,
            pb_devices_limit: PB_DEVICES_LIMIT,
            pf_bit_rate: 0x01,
            pf_bail_out: 0x00,
            pf_devices_limit: PF_DEVICES_LIMIT,
            pi_b_h_info: vec![],
            pi_bit_rate: 0x00,
            pn_nfc_dep_psl: 0x00,
            pn_atr_req_gen_bytes: vec![],
            pn_atr_req_config: 0x30,
            pv_devices_limit: PV_DEVICES_LIMIT,
            la_bit_frame_sdd: LA_BIT_FRAME_SDD,
            la_platform_config: LA_PLATFORM_CONFIG,
            la_sel_info: LA_SEL_INFO,
            la_nfcid1: vec![0x08, 0x00, 0x00, 0x00],
            lb_sensb_info: LB_SENSB_INFO,
            lb_nfcid0: [0x08, 0x00, 0x00, 0x00],
            lb_application_data: 0x00000000,
            lb_sfgi: LB_SFGI,
            lb_fwi_adc_fo: LB_FWI_ADC_FO,
            lb_bit_rate: 0x00,
            lf_t3t_identifiers_1: [0; 18],
            lf_t3t_identifiers_2: [0; 18],
            lf_t3t_identifiers_3: [0; 18],
            lf_t3t_identifiers_4: [0; 18],
            lf_t3t_identifiers_5: [0; 18],
            lf_t3t_identifiers_6: [0; 18],
            lf_t3t_identifiers_7: [0; 18],
            lf_t3t_identifiers_8: [0; 18],
            lf_t3t_identifiers_9: [0; 18],
            lf_t3t_identifiers_10: [0; 18],
            lf_t3t_identifiers_11: [0; 18],
            lf_t3t_identifiers_12: [0; 18],
            lf_t3t_identifiers_13: [0; 18],
            lf_t3t_identifiers_14: [0; 18],
            lf_t3t_identifiers_15: [0; 18],
            lf_t3t_identifiers_16: [0; 18],
            lf_t3t_pmm_default: [0xff; 8],
            lf_t3t_max: LF_T3T_MAX,
            lf_t3t_flags: 0x0000,
            lf_t3t_rd_allowed: 0x00,
            lf_protocol_type: LF_PROTOCOL_TYPE,
            li_a_rats_tb1: LI_A_RATS_TB1,
            li_a_hist_by: vec![],
            li_b_h_info_resp: vec![],
            li_a_bit_rate: 0x00,
            li_a_rats_tc1: LI_A_RATS_TC1,
            ln_wt: 10,
            ln_atr_res_gen_bytes: vec![],
            ln_atr_res_config: 0x30,
            pacm_bit_rate: 0x01,
            rf_field_info: 0x00,
            rf_nfcee_action: 0x01,
            // [NCI] Table 101: NFC-DEP Operation Parameter.
            nfcdep_op: 0x1f,
            llcp_version: LLCP_VERSION,
            nfcc_config_control: 0x00,
        }
    }
}

impl State {
    /// Craft the NFCID1 used by this instance in NFC-A poll responses.
    /// Returns a dynamically generated NFCID1 (4 byte long and starts with 08h).
    fn nfcid1(&self) -> Vec<u8> {
        if self.config_parameters.la_nfcid1.len() == 4
            && self.config_parameters.la_nfcid1[0] == 0x08
        {
            vec![0x08, 186, 7, 99] // TODO(hchataing) pseudo random
        } else {
            self.config_parameters.la_nfcid1.clone()
        }
    }

    /// Select the interface to be preferably used for the selected protocol.
    fn select_interface(
        &self,
        mode: RfMode,
        rf_protocol: nci::RfProtocolType,
    ) -> nci::RfInterfaceType {
        for config in self.discover_map.iter() {
            match (mode, config.mode.poll_mode, config.mode.listen_mode) {
                _ if config.rf_protocol != rf_protocol => (),
                (RfMode::Poll, nci::FeatureFlag::Enabled, _)
                | (RfMode::Listen, _, nci::FeatureFlag::Enabled) => return config.rf_interface,
                _ => (),
            }
        }

        // [NCI] 6.2 RF Interface Mapping Configuration
        //
        // The NFCC SHALL set the default mapping of RF Interface to RF Protocols /
        // Modes to the following values:
        //
        // • If the NFCC supports the ISO-DEP RF interface, the NFCC SHALL map the
        //   ISO-DEP RF Protocol to the ISO-DEP RF Interface for Poll Mode and
        //   Listen Mode.
        // • If the NFCC supports the NFC-DEP RF interface, the NFCC SHALL map the
        //   NFC-DEP RF Protocol to the NFC-DEP RF Interface for Poll Mode and
        //   Listen Mode.
        // • If the NFCC supports the NDEF RF interface, the NFCC SHALL map the
        //   NDEF RF Protocol to the NDEF RF Interface for Poll Mode.
        // • Otherwise, the NFCC SHALL map to the Frame RF Interface by default
        match rf_protocol {
            nci::RfProtocolType::IsoDep => nci::RfInterfaceType::IsoDep,
            nci::RfProtocolType::NfcDep => nci::RfInterfaceType::NfcDep,
            nci::RfProtocolType::Ndef if mode == RfMode::Poll => nci::RfInterfaceType::Ndef,
            _ => nci::RfInterfaceType::Frame,
        }
    }

    /// Insert a poll response into the discovery list.
    /// The response is not inserted if the device was already discovered
    /// with the same parameters.
    fn add_poll_response(&mut self, poll_response: RfPollResponse) {
        if !self.rf_poll_responses.contains(&poll_response) {
            self.rf_poll_responses.push(poll_response);
        }
    }
}

impl<'a> Controller<'a> {
    /// Create a new NFCC instance with default configuration.
    pub fn new(
        id: u16,
        nci_stream: nci::StreamRefMut<'a>,
        nci_writer: nci::Writer,
        rf_rx: mpsc::UnboundedReceiver<rf::RfPacket>,
        rf_tx: mpsc::UnboundedSender<rf::RfPacket>,
    ) -> Controller<'a> {
        Controller {
            id,
            nci_stream,
            nci_writer,
            rf_rx,
            rf_tx,
            state: State {
                config_parameters: Default::default(),
                logical_connections: [None; MAX_LOGICAL_CONNECTIONS as usize],
                discover_map: vec![],
                discover_configuration: vec![],
                nfcee_state: NfceeState::Disabled,
                rf_state: RfState::Idle,
                rf_poll_responses: vec![],
                rf_activation_parameters: vec![],
                passive_observe_mode: nci::PassiveObserveMode::Disable,
                start_time: Instant::now(),
            },
        }
    }

    async fn send_control(&mut self, packet: impl Into<nci::ControlPacket>) -> Result<()> {
        self.nci_writer.write(&packet.into().encode_to_vec()?).await
    }

    async fn send_data(&mut self, packet: impl Into<nci::DataPacket>) -> Result<()> {
        self.nci_writer.write(&packet.into().encode_to_vec()?).await
    }

    async fn send_rf(&self, packet: impl Into<rf::RfPacket>) -> Result<()> {
        self.rf_tx.send(packet.into())?;
        Ok(())
    }

    async fn core_reset(&mut self, cmd: nci::CoreResetCommand) -> Result<()> {
        info!("[{}] CORE_RESET_CMD", self.id);
        info!("         ResetType: {:?}", cmd.get_reset_type());

        match cmd.get_reset_type() {
            nci::ResetType::KeepConfig => (),
            nci::ResetType::ResetConfig => self.state.config_parameters = Default::default(),
        }

        for i in 0..MAX_LOGICAL_CONNECTIONS {
            self.state.logical_connections[i as usize] = None;
        }

        self.state.discover_map.clear();
        self.state.discover_configuration.clear();
        self.state.rf_state = RfState::Idle;
        self.state.rf_poll_responses.clear();

        self.send_control(nci::CoreResetResponseBuilder { status: nci::Status::Ok }).await?;

        self.send_control(nci::CoreResetNotificationBuilder {
            trigger: nci::ResetTrigger::ResetCommand,
            config_status: match cmd.get_reset_type() {
                nci::ResetType::KeepConfig => nci::ConfigStatus::ConfigKept,
                nci::ResetType::ResetConfig => nci::ConfigStatus::ConfigReset,
            },
            nci_version: NCI_VERSION,
            manufacturer_id: MANUFACTURER_ID,
            manufacturer_specific_information: MANUFACTURER_SPECIFIC_INFORMATION.to_vec(),
        })
        .await?;

        Ok(())
    }

    async fn core_init(&mut self, _cmd: nci::CoreInitCommand) -> Result<()> {
        info!("[{}] CORE_INIT_CMD", self.id);

        self.send_control(nci::CoreInitResponseBuilder {
            status: nci::Status::Ok,
            nfcc_features: nci::NfccFeatures {
                discovery_frequency_configuration: nci::FeatureFlag::Disabled,
                discovery_configuration_mode: nci::DiscoveryConfigurationMode::DhOnly,
                hci_network_support: nci::FeatureFlag::Enabled,
                active_communication_mode: nci::FeatureFlag::Enabled,
                technology_based_routing: nci::FeatureFlag::Enabled,
                protocol_based_routing: nci::FeatureFlag::Enabled,
                aid_based_routing: nci::FeatureFlag::Enabled,
                system_code_based_routing: nci::FeatureFlag::Enabled,
                apdu_pattern_based_routing: nci::FeatureFlag::Enabled,
                forced_nfcee_routing: nci::FeatureFlag::Enabled,
                battery_off_state: nci::FeatureFlag::Disabled,
                switched_off_state: nci::FeatureFlag::Enabled,
                switched_on_substates: nci::FeatureFlag::Enabled,
                rf_configuration_in_switched_off_state: nci::FeatureFlag::Disabled,
                proprietary_capabilities: 0,
            },
            max_logical_connections: MAX_LOGICAL_CONNECTIONS,
            max_routing_table_size: MAX_ROUTING_TABLE_SIZE,
            max_control_packet_payload_size: MAX_CONTROL_PACKET_PAYLOAD_SIZE,
            max_data_packet_payload_size: MAX_DATA_PACKET_PAYLOAD_SIZE,
            number_of_credits: NUMBER_OF_CREDITS,
            max_nfcv_rf_frame_size: MAX_NFCV_RF_FRAME_SIZE,
            supported_rf_interfaces: vec![
                nci::RfInterface { interface: nci::RfInterfaceType::Frame, extensions: vec![] },
                nci::RfInterface { interface: nci::RfInterfaceType::IsoDep, extensions: vec![] },
                nci::RfInterface { interface: nci::RfInterfaceType::NfcDep, extensions: vec![] },
                nci::RfInterface {
                    interface: nci::RfInterfaceType::NfceeDirect,
                    extensions: vec![],
                },
            ],
        })
        .await?;

        Ok(())
    }

    async fn core_set_config(&mut self, cmd: nci::CoreSetConfigCommand) -> Result<()> {
        info!("[{}] CORE_SET_CONFIG_CMD", self.id);

        let mut invalid_parameters = vec![];
        for parameter in cmd.get_parameters().iter() {
            info!("         Type: {:?}", parameter.id);
            info!("         Value: {:?}", parameter.value);
            match parameter.id {
                nci::ConfigParameterId::Rfu(_) => invalid_parameters.push(parameter.id),
                // TODO(henrichataing):
                // [NCI] 5.2.1 State RFST_IDLE
                // Unless otherwise specified, discovery related configuration
                // defined in Sections 6.1, 6.2, 6.3 and 7.1 SHALL only be set
                // while in IDLE state.
                //
                // Respond with Semantic Error as indicated by
                // [NCI] 3.2.2 Exception Handling for Control Messages
                // An unexpected Command SHALL NOT cause any action by the NFCC.
                // Unless otherwise specified, the NFCC SHALL send a Response
                // with a Status value of STATUS_SEMANTIC_ERROR and no
                // additional fields.
                _ => {
                    if self.state.config_parameters.set(parameter.id, &parameter.value).is_err() {
                        invalid_parameters.push(parameter.id)
                    }
                }
            }
        }

        self.send_control(nci::CoreSetConfigResponseBuilder {
            status: if invalid_parameters.is_empty() {
                // A Status of STATUS_OK SHALL indicate that all configuration parameters
                // have been set to these new values in the NFCC.
                nci::Status::Ok
            } else {
                // If the DH tries to set a parameter that is not applicable for the NFCC,
                // the NFCC SHALL respond with a CORE_SET_CONFIG_RSP with a Status field
                // of STATUS_INVALID_PARAM and including one or more invalid Parameter ID(s).
                // All other configuration parameters SHALL have been set to the new values
                // in the NFCC.
                warn!(
                    "[{}] rejecting unknown configuration parameter ids: {:?}",
                    self.id, invalid_parameters
                );
                nci::Status::InvalidParam
            },
            parameters: invalid_parameters,
        })
        .await?;

        Ok(())
    }

    async fn core_get_config(&mut self, cmd: nci::CoreGetConfigCommand) -> Result<()> {
        info!("[{}] CORE_GET_CONFIG_CMD", self.id);

        let mut valid_parameters = vec![];
        let mut invalid_parameters = vec![];
        for id in cmd.get_parameters() {
            info!("         ID: {:?}", id);
            match self.state.config_parameters.get(*id) {
                Ok(value) => {
                    valid_parameters.push(nci::ConfigParameter { id: *id, value: value.to_vec() })
                }
                Err(_) => invalid_parameters.push(nci::ConfigParameter { id: *id, value: vec![] }),
            }
        }

        self.send_control(if invalid_parameters.is_empty() {
            // If the NFCC is able to respond with all requested parameters, the
            // NFCC SHALL respond with the CORE_GET_CONFIG_RSP with a Status
            // of STATUS_OK.
            nci::CoreGetConfigResponseBuilder {
                status: nci::Status::Ok,
                parameters: valid_parameters,
            }
        } else {
            // If the DH tries to retrieve any parameter(s) that are not available
            // in the NFCC, the NFCC SHALL respond with a CORE_GET_CONFIG_RSP with
            // a Status field of STATUS_INVALID_PARAM, containing each unavailable
            // Parameter ID with a Parameter Len field of value zero.
            nci::CoreGetConfigResponseBuilder {
                status: nci::Status::InvalidParam,
                parameters: invalid_parameters,
            }
        })
        .await?;

        Ok(())
    }

    async fn core_conn_create(&mut self, cmd: nci::CoreConnCreateCommand) -> Result<()> {
        info!("[{}] CORE_CONN_CREATE_CMD", self.id);

        let result: std::result::Result<u8, nci::Status> = (|| {
            // Retrieve an unused connection ID for the logical connection.
            let conn_id = {
                (0..MAX_LOGICAL_CONNECTIONS)
                    .find(|conn_id| self.state.logical_connections[*conn_id as usize].is_none())
                    .ok_or(nci::Status::Rejected)?
            };

            // Check that the selected destination type is supported and validate
            // the destination specific parameters.
            let logical_connection = match cmd.get_destination_type() {
                // If the value of Destination Type is that of a Remote NFC
                // Endpoint (0x02), then only the Destination-specific Parameter
                // with Type 0x00 or proprietary parameters (as defined in Table 16)
                // SHALL be present.
                nci::DestinationType::RemoteNfcEndpoint => {
                    let mut rf_discovery_id: Option<u8> = None;
                    let mut rf_protocol_type: Option<nci::RfProtocolType> = None;

                    for parameter in cmd.get_parameters() {
                        match parameter.id {
                            nci::DestinationSpecificParameterId::RfDiscovery => {
                                rf_discovery_id = parameter.value.first().cloned();
                                rf_protocol_type = parameter
                                    .value
                                    .get(1)
                                    .and_then(|t| nci::RfProtocolType::try_from(*t).ok());
                            }
                            _ => return Err(nci::Status::Rejected),
                        }
                    }

                    LogicalConnection::RemoteNfcEndpoint {
                        rf_discovery_id: rf_discovery_id.ok_or(nci::Status::Rejected)?,
                        rf_protocol_type: rf_protocol_type.ok_or(nci::Status::Rejected)?,
                    }
                }
                nci::DestinationType::NfccLoopback | nci::DestinationType::Nfcee => {
                    return Err(nci::Status::Rejected)
                }
            };

            // The combination of Destination Type and Destination Specific
            // Parameters SHALL uniquely identify a single destination for the
            // Logical Connection.
            if self
                .state
                .logical_connections
                .iter()
                .any(|c| c.as_ref() == Some(&logical_connection))
            {
                return Err(nci::Status::Rejected);
            }

            // Create the connection.
            self.state.logical_connections[conn_id as usize] = Some(logical_connection);

            Ok(conn_id)
        })();

        self.send_control(match result {
            Ok(conn_id) => nci::CoreConnCreateResponseBuilder {
                status: nci::Status::Ok,
                max_data_packet_payload_size: MAX_DATA_PACKET_PAYLOAD_SIZE,
                initial_number_of_credits: 0xff,
                conn_id: nci::ConnId::from_dynamic(conn_id),
            },
            Err(status) => nci::CoreConnCreateResponseBuilder {
                status,
                max_data_packet_payload_size: 0,
                initial_number_of_credits: 0xff,
                conn_id: 0.try_into().unwrap(),
            },
        })
        .await?;

        Ok(())
    }

    async fn core_conn_close(&mut self, cmd: nci::CoreConnCloseCommand) -> Result<()> {
        info!("[{}] CORE_CONN_CLOSE_CMD", self.id);

        let conn_id = match cmd.get_conn_id() {
            nci::ConnId::StaticRf | nci::ConnId::StaticHci => {
                warn!("[{}] core_conn_close called with static conn_id", self.id);
                self.send_control(nci::CoreConnCloseResponseBuilder {
                    status: nci::Status::Rejected,
                })
                .await?;
                return Ok(());
            }
            nci::ConnId::Dynamic(id) => nci::ConnId::to_dynamic(id),
        };

        let status = if conn_id >= MAX_LOGICAL_CONNECTIONS
            || self.state.logical_connections[conn_id as usize].is_none()
        {
            // If there is no connection associated to the Conn ID in the CORE_CONN_CLOSE_CMD, the
            // NFCC SHALL reject the connection closure request by sending a CORE_CONN_CLOSE_RSP
            // with a Status of STATUS_REJECTED.
            nci::Status::Rejected
        } else {
            // When it receives a CORE_CONN_CLOSE_CMD for an existing connection, the NFCC SHALL
            // accept the connection closure request by sending a CORE_CONN_CLOSE_RSP with a Status of
            // STATUS_OK, and the Logical Connection is closed.
            self.state.logical_connections[conn_id as usize] = None;
            nci::Status::Ok
        };

        self.send_control(nci::CoreConnCloseResponseBuilder { status }).await?;

        Ok(())
    }

    async fn core_set_power_sub_state(
        &mut self,
        cmd: nci::CoreSetPowerSubStateCommand,
    ) -> Result<()> {
        info!("[{}] CORE_SET_POWER_SUB_STATE_CMD", self.id);
        info!("         State: {:?}", cmd.get_power_state());

        self.send_control(nci::CoreSetPowerSubStateResponseBuilder { status: nci::Status::Ok })
            .await?;

        Ok(())
    }

    async fn rf_discover_map(&mut self, cmd: nci::RfDiscoverMapCommand) -> Result<()> {
        info!("[{}] RF_DISCOVER_MAP_CMD", self.id);

        self.state.discover_map.clone_from(cmd.get_mapping_configurations());
        self.send_control(nci::RfDiscoverMapResponseBuilder { status: nci::Status::Ok }).await?;

        Ok(())
    }

    async fn rf_set_listen_mode_routing(
        &mut self,
        _cmd: nci::RfSetListenModeRoutingCommand,
    ) -> Result<()> {
        info!("[{}] RF_SET_LISTEN_MODE_ROUTING_CMD", self.id);

        self.send_control(nci::RfSetListenModeRoutingResponseBuilder { status: nci::Status::Ok })
            .await?;

        Ok(())
    }

    async fn rf_get_listen_mode_routing(
        &mut self,
        _cmd: nci::RfGetListenModeRoutingCommand,
    ) -> Result<()> {
        info!("[{}] RF_GET_LISTEN_MODE_ROUTING_CMD", self.id);

        self.send_control(nci::RfGetListenModeRoutingResponseBuilder {
            status: nci::Status::Ok,
            more_to_follow: 0,
            routing_entries: vec![],
        })
        .await?;

        Ok(())
    }

    async fn rf_discover(&mut self, cmd: nci::RfDiscoverCommand) -> Result<()> {
        info!("[{}] RF_DISCOVER_CMD", self.id);
        for config in cmd.get_configurations() {
            info!("         TechMode: {:?}", config.technology_and_mode);
        }

        if self.state.rf_state != RfState::Idle {
            warn!("[{}] rf_discover received in {:?} state", self.id, self.state.rf_state);
            self.send_control(nci::RfDiscoverResponseBuilder {
                status: nci::Status::SemanticError,
            })
            .await?;
            return Ok(());
        }

        self.state.discover_configuration.clone_from(cmd.get_configurations());
        self.state.rf_state = RfState::Discovery;

        self.send_control(nci::RfDiscoverResponseBuilder { status: nci::Status::Ok }).await?;

        Ok(())
    }

    async fn rf_discover_select(&mut self, cmd: nci::RfDiscoverSelectCommand) -> Result<()> {
        info!("[{}] RF_DISCOVER_SELECT_CMD", self.id);
        info!("         DiscoveryID: {:?}", cmd.get_rf_discovery_id());
        info!("         Protocol: {:?}", cmd.get_rf_protocol());
        info!("         Interface: {:?}", cmd.get_rf_interface());

        if self.state.rf_state != RfState::WaitForHostSelect {
            warn!("[{}] rf_discover_select received in {:?} state", self.id, self.state.rf_state);
            self.send_control(nci::RfDiscoverSelectResponseBuilder {
                status: nci::Status::SemanticError,
            })
            .await?;
            return Ok(());
        }

        let rf_discovery_id = match cmd.get_rf_discovery_id() {
            nci::RfDiscoveryId::Rfu(_) => {
                warn!("[{}] rf_discover_select with reserved rf_discovery_id", self.id);
                self.send_control(nci::RfDiscoverSelectResponseBuilder {
                    status: nci::Status::Rejected,
                })
                .await?;
                return Ok(());
            }
            nci::RfDiscoveryId::Id(id) => nci::RfDiscoveryId::to_index(id),
        };

        // If the RF Discovery ID, RF Protocol or RF Interface is not valid,
        // the NFCC SHALL respond with RF_DISCOVER_SELECT_RSP with a Status of
        // STATUS_REJECTED.
        if rf_discovery_id >= self.state.rf_poll_responses.len() {
            warn!("[{}] rf_discover_select with invalid rf_discovery_id", self.id);
            self.send_control(nci::RfDiscoverSelectResponseBuilder {
                status: nci::Status::Rejected,
            })
            .await?;
            return Ok(());
        }

        if cmd.get_rf_protocol() != self.state.rf_poll_responses[rf_discovery_id].rf_protocol.into()
        {
            warn!("[{}] rf_discover_select with invalid rf_protocol", self.id);
            self.send_control(nci::RfDiscoverSelectResponseBuilder {
                status: nci::Status::Rejected,
            })
            .await?;
            return Ok(());
        }

        self.send_control(nci::RfDiscoverSelectResponseBuilder { status: nci::Status::Ok }).await?;

        // Send RF select command to the peer to activate the device.
        // The command has varying parameters based on the activated protocol.
        self.activate_poll_interface(
            rf_discovery_id,
            cmd.get_rf_protocol(),
            cmd.get_rf_interface(),
        )
        .await?;

        Ok(())
    }

    async fn rf_deactivate(&mut self, cmd: nci::RfDeactivateCommand) -> Result<()> {
        info!("[{}] RF_DEACTIVATE_CMD", self.id);
        info!("         Type: {:?}", cmd.get_deactivation_type());

        use nci::DeactivationType::*;

        let (status, mut next_state) = match (self.state.rf_state, cmd.get_deactivation_type()) {
            (RfState::Idle, _) => (nci::Status::SemanticError, RfState::Idle),
            (RfState::Discovery, IdleMode) => (nci::Status::Ok, RfState::Idle),
            (RfState::Discovery, _) => (nci::Status::SemanticError, RfState::Discovery),
            (RfState::PollActive { .. }, IdleMode) => (nci::Status::Ok, RfState::Idle),
            (RfState::PollActive { .. }, SleepMode | SleepAfMode) => {
                (nci::Status::Ok, RfState::WaitForHostSelect)
            }
            (RfState::PollActive { .. }, Discovery) => (nci::Status::Ok, RfState::Discovery),
            (RfState::ListenSleep { .. }, IdleMode) => (nci::Status::Ok, RfState::Idle),
            (RfState::ListenSleep { .. }, _) => (nci::Status::SemanticError, self.state.rf_state),
            (RfState::ListenActive { .. }, IdleMode) => (nci::Status::Ok, RfState::Idle),
            (RfState::ListenActive { id, .. }, SleepMode | SleepAfMode) => {
                (nci::Status::Ok, RfState::ListenSleep { id })
            }
            (RfState::ListenActive { .. }, Discovery) => (nci::Status::Ok, RfState::Discovery),
            (RfState::WaitForHostSelect, IdleMode) => (nci::Status::Ok, RfState::Idle),
            (RfState::WaitForHostSelect, _) => {
                (nci::Status::SemanticError, RfState::WaitForHostSelect)
            }
            (RfState::WaitForSelectResponse { .. }, IdleMode) => (nci::Status::Ok, RfState::Idle),
            (RfState::WaitForSelectResponse { .. }, _) => {
                (nci::Status::SemanticError, self.state.rf_state)
            }
        };

        // Update the state now to prevent interface activation from
        // completing if a remote device is being selected.
        (next_state, self.state.rf_state) = (self.state.rf_state, next_state);

        self.send_control(nci::RfDeactivateResponseBuilder { status }).await?;

        // Deactivate the active RF interface if applicable
        // (next_state is the previous state in this context).
        match next_state {
            RfState::PollActive { .. } | RfState::ListenActive { .. } => {
                info!("[{}] RF_DEACTIVATE_NTF", self.id);
                info!("         Type: {:?}", cmd.get_deactivation_type());
                info!("         Reason: DH_Request");
                self.send_control(nci::RfDeactivateNotificationBuilder {
                    deactivation_type: cmd.get_deactivation_type(),
                    deactivation_reason: nci::DeactivationReason::DhRequest,
                })
                .await?
            }
            _ => (),
        }

        // Deselect the remote device if applicable.
        match next_state {
            RfState::PollActive { id, rf_protocol, rf_technology, .. }
            | RfState::WaitForSelectResponse { id, rf_protocol, rf_technology, .. } => {
                self.send_rf(rf::DeactivateNotificationBuilder {
                    receiver: id,
                    protocol: rf_protocol,
                    technology: rf_technology,
                    sender: self.id,
                    type_: cmd.get_deactivation_type().into(),
                    reason: rf::DeactivateReason::EndpointRequest,
                })
                .await?
            }
            _ => (),
        }

        Ok(())
    }

    async fn nfcee_discover(&mut self, _cmd: nci::NfceeDiscoverCommand) -> Result<()> {
        info!("[{}] NFCEE_DISCOVER_CMD", self.id);

        self.send_control(nci::NfceeDiscoverResponseBuilder {
            status: nci::Status::Ok,
            number_of_nfcees: 1,
        })
        .await?;

        self.send_control(nci::NfceeDiscoverNotificationBuilder {
            nfcee_id: nci::NfceeId::hci_nfcee(0x86),
            nfcee_status: nci::NfceeStatus::Disabled,
            supported_nfcee_protocols: vec![],
            nfcee_information: vec![nci::NfceeInformation {
                r#type: nci::NfceeInformationType::HostId,
                value: vec![0xc0],
            }],
            nfcee_supply_power: nci::NfceeSupplyPower::NfccHasNoControl,
        })
        .await?;

        Ok(())
    }

    async fn nfcee_mode_set(&mut self, cmd: nci::NfceeModeSetCommand) -> Result<()> {
        info!("[{}] NFCEE_MODE_SET_CMD", self.id);
        info!("         NFCEE ID: {:?}", cmd.get_nfcee_id());
        info!("         NFCEE Mode: {:?}", cmd.get_nfcee_mode());

        if cmd.get_nfcee_id() != nci::NfceeId::hci_nfcee(0x86) {
            warn!("[{}] nfcee_mode_set with invalid nfcee_id", self.id);
            self.send_control(nci::NfceeModeSetResponseBuilder { status: nci::Status::Ok }).await?;
            return Ok(());
        }

        self.state.nfcee_state = match cmd.get_nfcee_mode() {
            nci::NfceeMode::Enable => NfceeState::Enabled,
            nci::NfceeMode::Disable => NfceeState::Disabled,
        };

        self.send_control(nci::NfceeModeSetResponseBuilder { status: nci::Status::Ok }).await?;

        self.send_control(nci::NfceeModeSetNotificationBuilder { status: nci::Status::Ok }).await?;

        if self.state.nfcee_state == NfceeState::Enabled {
            // Android host stack expects this notification to know when the
            // NFCEE completes start-up. The list of information entries is
            // filled with defaults observed on real phones.
            self.send_data(nci::DataPacketBuilder {
                mt: nci::MessageType::Data,
                conn_id: nci::ConnId::StaticHci,
                cr: 0,
                payload: Some(bytes::Bytes::copy_from_slice(&[0x81, 0x43, 0xc0, 0x01])),
            })
            .await?;

            self.send_control(nci::RfNfceeDiscoveryReqNotificationBuilder {
                information_entries: vec![
                    nci::InformationEntry {
                        r#type: nci::InformationEntryType::AddDiscoveryRequest,
                        nfcee_id: nci::NfceeId::hci_nfcee(0x86),
                        rf_technology_and_mode: nci::RfTechnologyAndMode::NfcFPassiveListenMode,
                        rf_protocol: nci::RfProtocolType::T3t,
                    },
                    nci::InformationEntry {
                        r#type: nci::InformationEntryType::AddDiscoveryRequest,
                        nfcee_id: nci::NfceeId::hci_nfcee(0x86),
                        rf_technology_and_mode: nci::RfTechnologyAndMode::NfcAPassiveListenMode,
                        rf_protocol: nci::RfProtocolType::IsoDep,
                    },
                    nci::InformationEntry {
                        r#type: nci::InformationEntryType::AddDiscoveryRequest,
                        nfcee_id: nci::NfceeId::hci_nfcee(0x86),
                        rf_technology_and_mode: nci::RfTechnologyAndMode::NfcBPassiveListenMode,
                        rf_protocol: nci::RfProtocolType::IsoDep,
                    },
                ],
            })
            .await?;
        }

        Ok(())
    }

    async fn android_get_caps(&mut self, _cmd: nci::AndroidGetCapsCommand) -> Result<()> {
        info!("[{}] ANDROID_GET_CAPS_CMD", self.id);
        let cap_tlvs = vec![
            nci::CapTlv { t: nci::CapTlvType::PassiveObserverMode, v: vec![1] },
            nci::CapTlv { t: nci::CapTlvType::PollingFrameNotification, v: vec![1] },
        ];
        self.send_control(nci::AndroidGetCapsResponseBuilder {
            status: nci::Status::Ok,
            android_version: 0,
            tlvs: cap_tlvs,
        })
        .await?;
        Ok(())
    }

    async fn android_passive_observe_mode(
        &mut self,
        cmd: nci::AndroidPassiveObserveModeCommand,
    ) -> Result<()> {
        info!("[{}] ANDROID_PASSIVE_OBSERVE_MODE_CMD", self.id);
        info!("     Mode: {:?}", cmd.get_passive_observe_mode());

        self.state.passive_observe_mode = cmd.get_passive_observe_mode();
        self.send_control(nci::AndroidPassiveObserveModeResponseBuilder {
            status: nci::Status::Ok,
        })
        .await?;
        Ok(())
    }

    async fn android_query_passive_observe_mode(
        &mut self,
        _cmd: nci::AndroidQueryPassiveObserveModeCommand,
    ) -> Result<()> {
        info!("[{}] ANDROID_QUERY_PASSIVE_OBSERVE_MODE_CMD", self.id);

        self.send_control(nci::AndroidQueryPassiveObserveModeResponseBuilder {
            status: nci::Status::Ok,
            passive_observe_mode: self.state.passive_observe_mode,
        })
        .await?;
        Ok(())
    }

    async fn receive_command(&mut self, packet: nci::ControlPacket) -> Result<()> {
        use nci::AndroidPacketChild::*;
        use nci::ControlPacketChild::*;
        use nci::CorePacketChild::*;
        use nci::NfceePacketChild::*;
        use nci::ProprietaryPacketChild::*;
        use nci::RfPacketChild::*;

        match packet.specialize() {
            CorePacket(packet) => match packet.specialize() {
                CoreResetCommand(cmd) => self.core_reset(cmd).await,
                CoreInitCommand(cmd) => self.core_init(cmd).await,
                CoreSetConfigCommand(cmd) => self.core_set_config(cmd).await,
                CoreGetConfigCommand(cmd) => self.core_get_config(cmd).await,
                CoreConnCreateCommand(cmd) => self.core_conn_create(cmd).await,
                CoreConnCloseCommand(cmd) => self.core_conn_close(cmd).await,
                CoreSetPowerSubStateCommand(cmd) => self.core_set_power_sub_state(cmd).await,
                _ => unimplemented!("unsupported core oid {:?}", packet.get_oid()),
            },
            RfPacket(packet) => match packet.specialize() {
                RfDiscoverMapCommand(cmd) => self.rf_discover_map(cmd).await,
                RfSetListenModeRoutingCommand(cmd) => self.rf_set_listen_mode_routing(cmd).await,
                RfGetListenModeRoutingCommand(cmd) => self.rf_get_listen_mode_routing(cmd).await,
                RfDiscoverCommand(cmd) => self.rf_discover(cmd).await,
                RfDiscoverSelectCommand(cmd) => self.rf_discover_select(cmd).await,
                RfDeactivateCommand(cmd) => self.rf_deactivate(cmd).await,
                _ => unimplemented!("unsupported rf oid {:?}", packet.get_oid()),
            },
            NfceePacket(packet) => match packet.specialize() {
                NfceeDiscoverCommand(cmd) => self.nfcee_discover(cmd).await,
                NfceeModeSetCommand(cmd) => self.nfcee_mode_set(cmd).await,
                _ => unimplemented!("unsupported nfcee oid {:?}", packet.get_oid()),
            },
            ProprietaryPacket(packet) => match packet.specialize() {
                AndroidPacket(packet) => match packet.specialize() {
                    AndroidGetCapsCommand(cmd) => self.android_get_caps(cmd).await,
                    AndroidPassiveObserveModeCommand(cmd) => {
                        self.android_passive_observe_mode(cmd).await
                    }
                    AndroidQueryPassiveObserveModeCommand(cmd) => {
                        self.android_query_passive_observe_mode(cmd).await
                    }
                    _ => {
                        unimplemented!("unsupported android oid {:?}", packet.get_android_sub_oid())
                    }
                },
                _ => unimplemented!("unsupported proprietary oid {:?}", packet.get_oid()),
            },
            _ => unimplemented!("unsupported gid {:?}", packet.get_gid()),
        }
    }

    async fn rf_conn_data(&mut self, packet: nci::DataPacket) -> Result<()> {
        info!("[{}] received data on RF logical connection", self.id);

        // TODO(henrichataing) implement credit based control flow.
        match self.state.rf_state {
            RfState::PollActive {
                id,
                rf_technology,
                rf_protocol: rf::Protocol::IsoDep,
                rf_interface: nci::RfInterfaceType::IsoDep,
                ..
            }
            | RfState::ListenActive {
                id,
                rf_technology,
                rf_protocol: rf::Protocol::IsoDep,
                rf_interface: nci::RfInterfaceType::IsoDep,
                ..
            } => {
                self.send_rf(rf::DataBuilder {
                    receiver: id,
                    sender: self.id,
                    protocol: rf::Protocol::IsoDep,
                    technology: rf_technology,
                    data: packet.get_payload().into(),
                })
                .await?;
                // Resplenish the credit count for the RF Connection.
                self.send_control(
                    nci::CoreConnCreditsNotificationBuilder {
                        connections: vec![nci::ConnectionCredits {
                            conn_id: nci::ConnId::StaticRf,
                            credits: 1,
                        }],
                    }
                    .build(),
                )
                .await
            }
            RfState::PollActive {
                rf_protocol: rf::Protocol::IsoDep,
                rf_interface: nci::RfInterfaceType::Frame,
                ..
            } => {
                println!("ISO-DEP frame data {:?}", packet.get_payload());
                match packet.get_payload() {
                    // RATS command
                    // TODO(henrichataing) Send back the response received from
                    // the peer in the RF packet.
                    [0xe0, _] => {
                        warn!("[{}] frame RATS command", self.id);
                        self.send_data(nci::DataPacketBuilder {
                            mt: nci::MessageType::Data,
                            conn_id: nci::ConnId::StaticRf,
                            cr: 0,
                            payload: Some(bytes::Bytes::copy_from_slice(
                                &self.state.rf_activation_parameters,
                            )),
                        })
                        .await?
                    }
                    // DESELECT command
                    // TODO(henrichataing) check if the command should be
                    // forwarded to the peer, and if it warrants a response
                    [0xc2] => warn!("[{}] unimplemented frame DESELECT command", self.id),
                    // SLP_REQ command
                    // No response is expected for this command.
                    // TODO(henrichataing) forward a deactivation request to
                    // the peer and deactivate the local interface.
                    [0x50, 0x00] => warn!("[{}] unimplemented frame SLP_REQ command", self.id),
                    _ => unimplemented!(),
                };
                // Resplenish the credit count for the RF Connection.
                self.send_control(
                    nci::CoreConnCreditsNotificationBuilder {
                        connections: vec![nci::ConnectionCredits {
                            conn_id: nci::ConnId::StaticRf,
                            credits: 1,
                        }],
                    }
                    .build(),
                )
                .await
            }
            RfState::PollActive { rf_protocol, rf_interface, .. }
            | RfState::ListenActive { rf_protocol, rf_interface, .. } => unimplemented!(
                "unsupported combination of RF protocol {:?} and interface {:?}",
                rf_protocol,
                rf_interface
            ),
            _ => {
                warn!(
                    "[{}] ignored RF data packet while not in active listen or poll mode",
                    self.id
                );
                Ok(())
            }
        }
    }

    async fn hci_conn_data(&mut self, packet: nci::DataPacket) -> Result<()> {
        info!("[{}] received data on HCI logical connection", self.id);

        // TODO: parse and understand HCI Control Protocol (HCP)
        // to accurately respond to the requests. For now it is sufficient
        // to return hardcoded answers to identified requests.
        let response = match packet.get_payload() {
            // ANY_OPEN_PIPE()
            [0x81, 0x03] => vec![0x81, 0x80],
            // ANY_GET_PARAMETER(index=1)
            [0x81, 0x02, 0x01] => vec![0x81, 0x80, 0xd7, 0xfe, 0x65, 0x66, 0xc7, 0xfe, 0x65, 0x66],
            // ANY_GET_PARAMETER(index=4)
            [0x81, 0x02, 0x04] => vec![0x81, 0x80, 0x00, 0xc0, 0x01],
            // ANY_SET_PARAMETER()
            [0x81, 0x01, 0x03, 0x02, 0xc0]
            | [0x81, 0x01, 0x03, _, _, _]
            | [0x81, 0x01, 0x01, _, 0x00, 0x00, 0x00, _, 0x00, 0x00, 0x00] => vec![0x81, 0x80],
            // ADM_CLEAR_ALL_PIPE()
            [0x81, 0x14, 0x02, 0x01] => vec![0x81, 0x80],
            _ => {
                error!("unimplemented HCI command : {:?}", packet.get_payload());
                unimplemented!()
            }
        };

        self.send_data(nci::DataPacketBuilder {
            mt: nci::MessageType::Data,
            conn_id: nci::ConnId::StaticHci,
            cr: 0,
            payload: Some(bytes::Bytes::copy_from_slice(&response)),
        })
        .await?;

        // Resplenish the credit count for the HCI Connection.
        self.send_control(
            nci::CoreConnCreditsNotificationBuilder {
                connections: vec![nci::ConnectionCredits {
                    conn_id: nci::ConnId::StaticHci,
                    credits: 1,
                }],
            }
            .build(),
        )
        .await
    }

    async fn dynamic_conn_data(&self, _conn_id: u8, _packet: nci::DataPacket) -> Result<()> {
        info!("[{}] received data on dynamic logical connection", self.id);
        todo!()
    }

    async fn receive_data(&mut self, packet: nci::DataPacket) -> Result<()> {
        info!("[{}] receive_data({})", self.id, u8::from(packet.get_conn_id()));

        match packet.get_conn_id() {
            nci::ConnId::StaticRf => self.rf_conn_data(packet).await,
            nci::ConnId::StaticHci => self.hci_conn_data(packet).await,
            nci::ConnId::Dynamic(id) => self.dynamic_conn_data(*id, packet).await,
        }
    }

    async fn poll_command(&mut self, cmd: rf::PollCommand) -> Result<()> {
        trace!("[{}] poll_command()", self.id);

        if self.state.rf_state != RfState::Discovery {
            return Ok(());
        }
        let technology = cmd.get_technology();

        // Android proprietary extension for polling frame notifications.
        // The NFCC should send the NCI_ANDROID_POLLING_FRAME_NTF to the Host
        // after each polling loop frame
        // This notification is independent of whether Passive Observe Mode is
        // active or not. When Passive Observe Mode is active, the NFCC
        // should always send this notification before proceeding with the
        // transaction.
        self.send_control(nci::AndroidPollingLoopNotificationBuilder {
            polling_frames: vec![nci::PollingFrame {
                r#type: match technology {
                    rf::Technology::NfcA => nci::PollingFrameType::Reqa,
                    rf::Technology::NfcB => nci::PollingFrameType::Reqb,
                    rf::Technology::NfcF => nci::PollingFrameType::Reqf,
                    rf::Technology::NfcV => nci::PollingFrameType::Reqv,
                },
                flags: 0,
                timestamp: (self.state.start_time.elapsed().as_millis() as u32).to_be_bytes(),
                gain: 2,
                data: vec![],
            }],
        })
        .await?;

        // When the Passive Observe Mode is active, the NFCC shall not respond
        // to any poll requests during the polling loop in Listen Mode, until
        // explicitly authorized by the Host.
        if self.state.passive_observe_mode == nci::PassiveObserveMode::Enable {
            return Ok(());
        }

        if self.state.discover_configuration.iter().any(|config| {
            matches!(
                (config.technology_and_mode, technology),
                (nci::RfTechnologyAndMode::NfcAPassiveListenMode, rf::Technology::NfcA)
                    | (nci::RfTechnologyAndMode::NfcBPassiveListenMode, rf::Technology::NfcB)
                    | (nci::RfTechnologyAndMode::NfcFPassiveListenMode, rf::Technology::NfcF)
            )
        }) {
            match technology {
                rf::Technology::NfcA => {
                    self.send_rf(rf::NfcAPollResponseBuilder {
                        protocol: rf::Protocol::Undetermined,
                        receiver: cmd.get_sender(),
                        sender: self.id,
                        nfcid1: self.state.nfcid1(),
                        int_protocol: self.state.config_parameters.la_sel_info >> 5,
                        bit_frame_sdd: self.state.config_parameters.la_bit_frame_sdd,
                    })
                    .await?
                }
                // TODO(b/346715736) implement support for NFC-B technology
                rf::Technology::NfcB => (),
                rf::Technology::NfcF => todo!(),
                _ => (),
            }
        }

        Ok(())
    }

    async fn nfca_poll_response(&mut self, cmd: rf::NfcAPollResponse) -> Result<()> {
        info!("[{}] nfca_poll_response()", self.id);

        if self.state.rf_state != RfState::Discovery {
            return Ok(());
        }

        let int_protocol = cmd.get_int_protocol();
        let rf_protocols = match int_protocol {
            0b00 => [rf::Protocol::T2t].iter(),
            0b01 => [rf::Protocol::IsoDep].iter(),
            0b10 => [rf::Protocol::NfcDep].iter(),
            0b11 => [rf::Protocol::NfcDep, rf::Protocol::IsoDep].iter(),
            _ => return Ok(()),
        };
        let sens_res = match cmd.get_nfcid1().len() {
            4 => 0x00,
            7 => 0x40,
            10 => 0x80,
            _ => panic!(),
        } | cmd.get_bit_frame_sdd() as u16;
        let sel_res = int_protocol << 5;

        for rf_protocol in rf_protocols {
            self.state.add_poll_response(RfPollResponse {
                id: cmd.get_sender(),
                rf_protocol: *rf_protocol,
                rf_technology: rf::Technology::NfcA,
                rf_technology_specific_parameters:
                    nci::NfcAPollModeTechnologySpecificParametersBuilder {
                        sens_res,
                        nfcid1: cmd.get_nfcid1().clone(),
                        sel_res,
                    }
                    .build()
                    .encode_to_vec()?,
            })
        }

        Ok(())
    }

    async fn t4at_select_command(&mut self, cmd: rf::T4ATSelectCommand) -> Result<()> {
        info!("[{}] t4at_select_command()", self.id);

        match self.state.rf_state {
            RfState::Discovery => (),
            RfState::ListenSleep { id } if id == cmd.get_sender() => (),
            _ => return Ok(()),
        };

        // TODO(henrichataing): validate that the protocol and technology are
        // valid for the current discovery settings.

        // TODO(henrichataing): use listen mode routing table to decide which
        // interface should be used for the activating device.

        self.state.rf_state = RfState::ListenActive {
            id: cmd.get_sender(),
            rf_technology: rf::Technology::NfcA,
            rf_protocol: rf::Protocol::IsoDep,
            rf_interface: nci::RfInterfaceType::IsoDep,
        };

        // [DIGITAL] 14.6.2 RATS Response (Answer To Select)
        // Construct the response from the values passed in the configuration
        // parameters. The TL byte is excluded from the response.
        let mut rats_response = vec![
            0x78, // TC(1), TB(1), TA(1) transmitted, FSCI=8
            0x80, // TA(1)
            self.state.config_parameters.li_a_rats_tb1,
            self.state.config_parameters.li_a_rats_tc1,
        ];

        rats_response.extend_from_slice(&self.state.config_parameters.li_a_hist_by);

        self.send_rf(rf::T4ATSelectResponseBuilder {
            receiver: cmd.get_sender(),
            sender: self.id,
            rats_response,
        })
        .await?;

        info!("[{}] RF_INTF_ACTIVATED_NTF", self.id);
        info!("         DiscoveryID: {:?}", nci::RfDiscoveryId::from_index(0));
        info!("         Interface: ISO-DEP");
        info!("         Protocol: ISO-DEP");
        info!("         ActivationTechnology: NFC_A_PASSIVE_LISTEN");
        info!("         RATS: {}", cmd.get_param());

        self.send_control(nci::RfIntfActivatedNotificationBuilder {
            rf_discovery_id: nci::RfDiscoveryId::from_index(0),
            rf_interface: nci::RfInterfaceType::IsoDep,
            rf_protocol: nci::RfProtocolType::IsoDep,
            activation_rf_technology_and_mode: nci::RfTechnologyAndMode::NfcAPassiveListenMode,
            max_data_packet_payload_size: MAX_DATA_PACKET_PAYLOAD_SIZE,
            initial_number_of_credits: 1,
            // No parameters are currently defined for NFC-A Listen Mode.
            rf_technology_specific_parameters: vec![],
            data_exchange_rf_technology_and_mode: nci::RfTechnologyAndMode::NfcAPassiveListenMode,
            data_exchange_transmit_bit_rate: nci::BitRate::BitRate106KbitS,
            data_exchange_receive_bit_rate: nci::BitRate::BitRate106KbitS,
            activation_parameters: nci::NfcAIsoDepListenModeActivationParametersBuilder {
                param: cmd.get_param(),
            }
            .build()
            .encode_to_vec()?,
        })
        .await?;

        Ok(())
    }

    async fn t4at_select_response(&mut self, cmd: rf::T4ATSelectResponse) -> Result<()> {
        info!("[{}] t4at_select_response()", self.id);

        let (id, rf_discovery_id, rf_interface, rf_protocol) = match self.state.rf_state {
            RfState::WaitForSelectResponse {
                id,
                rf_discovery_id,
                rf_interface,
                rf_protocol,
                ..
            } => (id, rf_discovery_id, rf_interface, rf_protocol),
            _ => return Ok(()),
        };

        if cmd.get_sender() != id {
            return Ok(());
        }

        self.state.rf_state = RfState::PollActive {
            id,
            rf_protocol: self.state.rf_poll_responses[rf_discovery_id].rf_protocol,
            rf_technology: self.state.rf_poll_responses[rf_discovery_id].rf_technology,
            rf_interface,
        };

        // Save the activation parameters for the RF frame interface
        // implementation. Note: TL is not included in the RATS response
        // and needs to be added manually to the activation parameters.
        self.state.rf_activation_parameters = vec![cmd.get_rats_response().len() as u8];
        self.state.rf_activation_parameters.extend_from_slice(cmd.get_rats_response());

        info!("[{}] RF_INTF_ACTIVATED_NTF", self.id);
        info!("         DiscoveryID: {:?}", nci::RfDiscoveryId::from_index(rf_discovery_id));
        info!("         Interface: {:?}", rf_interface);
        info!("         Protocol: {:?}", rf_protocol);
        info!("         ActivationTechnology: NFC_A_PASSIVE_POLL");
        info!("         RATS: {:?}", cmd.get_rats_response());

        self.send_control(nci::RfIntfActivatedNotificationBuilder {
            rf_discovery_id: nci::RfDiscoveryId::from_index(rf_discovery_id),
            rf_interface,
            rf_protocol: rf_protocol.into(),
            activation_rf_technology_and_mode: nci::RfTechnologyAndMode::NfcAPassivePollMode,
            max_data_packet_payload_size: MAX_DATA_PACKET_PAYLOAD_SIZE,
            initial_number_of_credits: 1,
            rf_technology_specific_parameters: self.state.rf_poll_responses[rf_discovery_id]
                .rf_technology_specific_parameters
                .clone(),
            data_exchange_rf_technology_and_mode: nci::RfTechnologyAndMode::NfcAPassivePollMode,
            data_exchange_transmit_bit_rate: nci::BitRate::BitRate106KbitS,
            data_exchange_receive_bit_rate: nci::BitRate::BitRate106KbitS,
            // TODO(hchataing) the activation parameters should be empty
            // when the RF frame interface is used, since the protocol
            // activation is managed by the DH.
            activation_parameters: nci::NfcAIsoDepPollModeActivationParametersBuilder {
                rats_response: cmd.get_rats_response().clone(),
            }
            .build()
            .encode_to_vec()?,
        })
        .await?;

        Ok(())
    }

    async fn data_packet(&mut self, data: rf::Data) -> Result<()> {
        info!("[{}] data_packet()", self.id);

        match (self.state.rf_state, data.get_protocol()) {
            (
                RfState::PollActive {
                    id, rf_technology, rf_protocol: rf::Protocol::IsoDep, ..
                },
                rf::Protocol::IsoDep,
            )
            | (
                RfState::ListenActive {
                    id, rf_technology, rf_protocol: rf::Protocol::IsoDep, ..
                },
                rf::Protocol::IsoDep,
            ) if data.get_sender() == id && data.get_technology() == rf_technology => {
                self.send_data(nci::DataPacketBuilder {
                    mt: nci::MessageType::Data,
                    conn_id: nci::ConnId::StaticRf,
                    cr: 1, // TODO(henrichataing): credit based control flow
                    payload: Some(bytes::Bytes::copy_from_slice(data.get_data())),
                })
                .await
            }
            (RfState::PollActive { id, .. }, _) | (RfState::ListenActive { id, .. }, _)
                if id != data.get_sender() =>
            {
                warn!("[{}] ignored RF data packet sent from an un-selected device", self.id);
                Ok(())
            }
            (RfState::PollActive { .. }, _) | (RfState::ListenActive { .. }, _) => {
                unimplemented!("unsupported combination of technology and protocol")
            }
            (_, _) => {
                warn!("[{}] ignored RF data packet received in inactive state", self.id);
                Ok(())
            }
        }
    }

    async fn deactivate_notification(&mut self, cmd: rf::DeactivateNotification) -> Result<()> {
        info!("[{}] deactivate_notification()", self.id);

        use rf::DeactivateType::*;

        let mut next_state = match (self.state.rf_state, cmd.get_type_()) {
            (RfState::PollActive { id, .. }, IdleMode) if id == cmd.get_sender() => RfState::Idle,
            (RfState::PollActive { id, .. }, SleepMode | SleepAfMode) if id == cmd.get_sender() => {
                RfState::WaitForHostSelect
            }
            (RfState::PollActive { id, .. }, Discovery) if id == cmd.get_sender() => {
                RfState::Discovery
            }
            (RfState::ListenSleep { id, .. }, IdleMode) if id == cmd.get_sender() => RfState::Idle,
            (RfState::ListenSleep { id, .. }, Discovery) if id == cmd.get_sender() => {
                RfState::Discovery
            }
            (RfState::ListenActive { id, .. }, IdleMode) if id == cmd.get_sender() => RfState::Idle,
            (RfState::ListenActive { id, .. }, SleepMode | SleepAfMode)
                if id == cmd.get_sender() =>
            {
                RfState::ListenSleep { id }
            }
            (RfState::ListenActive { id, .. }, Discovery) if id == cmd.get_sender() => {
                RfState::Discovery
            }
            (_, _) => self.state.rf_state,
        };

        // Update the state now to prevent interface activation from
        // completing if a remote device is being selected.
        (next_state, self.state.rf_state) = (self.state.rf_state, next_state);

        // Deactivate the active RF interface if applicable.
        if next_state != self.state.rf_state {
            self.send_control(nci::RfDeactivateNotificationBuilder {
                deactivation_type: cmd.get_type_().into(),
                deactivation_reason: cmd.get_reason().into(),
            })
            .await?
        }

        Ok(())
    }

    async fn receive_rf(&mut self, packet: rf::RfPacket) -> Result<()> {
        use rf::RfPacketChild::*;

        match packet.specialize() {
            PollCommand(cmd) => self.poll_command(cmd).await,
            NfcAPollResponse(cmd) => self.nfca_poll_response(cmd).await,
            // [NCI] 5.2.2 State RFST_DISCOVERY
            // If discovered by a Remote NFC Endpoint in Listen mode, once the
            // Remote NFC Endpoint has established any underlying protocol(s) needed
            // by the configured RF Interface, the NFCC SHALL send
            // RF_INTF_ACTIVATED_NTF (Listen Mode) to the DH and the state is
            // changed to RFST_LISTEN_ACTIVE.
            T4ATSelectCommand(cmd) => self.t4at_select_command(cmd).await,
            T4ATSelectResponse(cmd) => self.t4at_select_response(cmd).await,
            SelectCommand(_) => unimplemented!(),
            DeactivateNotification(cmd) => self.deactivate_notification(cmd).await,
            Data(cmd) => self.data_packet(cmd).await,
            _ => unimplemented!(),
        }
    }

    /// Activity for activating an RF interface for a discovered device.
    ///
    /// The method send a notification when the interface is successfully
    /// activated, or when the device activation fails.
    ///
    ///  * `rf_discovery_id` - index of the discovered device
    ///  * `rf_interface` - interface to activate
    ///
    /// The RF state is changed to WaitForSelectResponse when
    /// the select command is successfully sent.
    async fn activate_poll_interface(
        &mut self,
        rf_discovery_id: usize,
        rf_protocol: nci::RfProtocolType,
        rf_interface: nci::RfInterfaceType,
    ) -> Result<()> {
        info!("[{}] activate_poll_interface({:?})", self.id, rf_interface);

        let rf_technology = self.state.rf_poll_responses[rf_discovery_id].rf_technology;
        match (rf_protocol, rf_technology) {
            (nci::RfProtocolType::T2t, rf::Technology::NfcA) => {
                self.send_rf(rf::SelectCommandBuilder {
                    sender: self.id,
                    receiver: self.state.rf_poll_responses[rf_discovery_id].id,
                    technology: rf::Technology::NfcA,
                    protocol: rf::Protocol::T2t,
                })
                .await?
            }
            (nci::RfProtocolType::IsoDep, rf::Technology::NfcA) => {
                self.send_rf(rf::T4ATSelectCommandBuilder {
                    sender: self.id,
                    receiver: self.state.rf_poll_responses[rf_discovery_id].id,
                    // [DIGITAL] 14.6.1.6 The FSD supported by the
                    // Reader/Writer SHALL be FSD T4AT,MIN
                    // (set to 256 in Appendix B.6).
                    param: 0x80,
                })
                .await?
            }
            (nci::RfProtocolType::NfcDep, rf::Technology::NfcA) => {
                self.send_rf(rf::NfcDepSelectCommandBuilder {
                    sender: self.id,
                    receiver: self.state.rf_poll_responses[rf_discovery_id].id,
                    technology: rf::Technology::NfcA,
                    lr: 0,
                })
                .await?
            }
            _ => todo!(),
        }

        self.state.rf_state = RfState::WaitForSelectResponse {
            id: self.state.rf_poll_responses[rf_discovery_id].id,
            rf_discovery_id,
            rf_interface,
            rf_protocol: rf_protocol.into(),
            rf_technology,
        };
        Ok(())
    }

    async fn run_until<O>(&mut self, future: impl Future<Output = O>) -> Result<O> {
        let mut future = pin!(future);
        loop {
            tokio::select! {
                packet = self.nci_stream.next() => {
                    let packet = packet.ok_or(anyhow::anyhow!("nci channel closed"))??;
                    let header = nci::PacketHeader::parse(&packet[0..3])?;
                    match header.get_mt() {
                        nci::MessageType::Data => {
                            self.receive_data(nci::DataPacket::parse(&packet)?).await?
                        }
                        nci::MessageType::Command => {
                            self.receive_command(nci::ControlPacket::parse(&packet)?).await?
                        }
                        mt => {
                            return Err(anyhow::anyhow!(
                                "unexpected message type {:?} in received NCI packet",
                                mt
                            ))
                        }
                    }
                },
                rf_packet = self.rf_rx.recv() => {
                    self.receive_rf(
                        rf_packet.ok_or(anyhow::anyhow!("rf_rx channel closed"))?,
                    )
                    .await?
                },
                output = &mut future => break Ok(output)
            }
        }
    }

    /// Timer handler method. This function is invoked at regular interval
    /// on the NFCC instance and is used to drive internal timers.
    async fn tick(&mut self) -> Result<()> {
        if self.state.rf_state != RfState::Discovery {
            return Ok(());
        }

        //info!("[{}] poll", self.id);

        // [NCI] 5.2.2 State RFST_DISCOVERY
        //
        // In this state the NFCC stays in Poll Mode and/or Listen Mode (based
        // on the discovery configuration) until at least one Remote NFC
        // Endpoint is detected or the RF Discovery Process is stopped by
        // the DH.
        //
        // The following implements the Poll Mode Discovery, Listen Mode
        // Discover is implicitly implemented in response to poll and
        // select commands.

        // RF Discovery is ongoing and no peer device has been discovered
        // so far. Send a RF poll command for all enabled technologies.
        self.state.rf_poll_responses.clear();
        for configuration in self.state.discover_configuration.iter() {
            self.send_rf(rf::PollCommandBuilder {
                sender: self.id,
                receiver: u16::MAX,
                protocol: rf::Protocol::Undetermined,
                technology: match configuration.technology_and_mode {
                    nci::RfTechnologyAndMode::NfcAPassivePollMode => rf::Technology::NfcA,
                    nci::RfTechnologyAndMode::NfcBPassivePollMode => rf::Technology::NfcB,
                    nci::RfTechnologyAndMode::NfcFPassivePollMode => rf::Technology::NfcF,
                    nci::RfTechnologyAndMode::NfcVPassivePollMode => rf::Technology::NfcV,
                    _ => continue,
                },
            })
            .await?
        }

        // Wait for poll responses to return.
        self.run_until(time::sleep(Duration::from_millis(POLL_RESPONSE_TIMEOUT))).await?;

        // Check if device was activated in Listen mode during
        // the poll interval, or if the discovery got cancelled.
        if self.state.rf_state != RfState::Discovery || self.state.rf_poll_responses.is_empty() {
            return Ok(());
        }

        // While polling, if the NFCC discovers just one Remote NFC Endpoint
        // that supports just one protocol, the NFCC SHALL try to automatically
        // activate it. The NFCC SHALL first establish any underlying
        // protocol(s) with the Remote NFC Endpoint that are needed by the
        // configured RF Interface. On completion, the NFCC SHALL activate the
        // RF Interface and send RF_INTF_ACTIVATED_NTF (Poll Mode) to the DH.
        // At this point, the state is changed to RFST_POLL_ACTIVE. If the
        // protocol activation is not successful, the NFCC SHALL send
        // CORE_GENERIC_ERROR_NTF to the DH with status
        // DISCOVERY_TARGET_ACTIVATION_FAILED and SHALL stay in the
        // RFST_DISCOVERY state.
        if self.state.rf_poll_responses.len() == 1 {
            let rf_protocol = self.state.rf_poll_responses[0].rf_protocol.into();
            let rf_interface = self.state.select_interface(RfMode::Poll, rf_protocol);
            return self.activate_poll_interface(0, rf_protocol, rf_interface).await;
        }

        debug!("[{}] received {} poll response(s)", self.id, self.state.rf_poll_responses.len());

        // While polling, if the NFCC discovers more than one Remote NFC
        // Endpoint, or a Remote NFC Endpoint that supports more than one RF
        // Protocol, it SHALL start sending RF_DISCOVER_NTF messages to the DH.
        // At this point, the state is changed to RFST_W4_ALL_DISCOVERIES.
        self.state.rf_state = RfState::WaitForHostSelect;
        let last_index = self.state.rf_poll_responses.len() - 1;
        for (index, response) in self.state.rf_poll_responses.clone().iter().enumerate() {
            self.send_control(nci::RfDiscoverNotificationBuilder {
                rf_discovery_id: nci::RfDiscoveryId::from_index(index),
                rf_protocol: response.rf_protocol.into(),
                rf_technology_and_mode: match response.rf_technology {
                    rf::Technology::NfcA => nci::RfTechnologyAndMode::NfcAPassivePollMode,
                    rf::Technology::NfcB => nci::RfTechnologyAndMode::NfcBPassivePollMode,
                    _ => todo!(),
                },
                rf_technology_specific_parameters: response
                    .rf_technology_specific_parameters
                    .clone(),
                notification_type: if index == last_index {
                    nci::DiscoverNotificationType::LastNotification
                } else {
                    nci::DiscoverNotificationType::MoreNotifications
                },
            })
            .await?
        }

        Ok(())
    }

    /// Main NFCC instance routine.
    pub async fn run(
        id: u16,
        nci_stream: nci::StreamRefMut<'a>,
        nci_writer: nci::Writer,
        rf_rx: mpsc::UnboundedReceiver<rf::RfPacket>,
        rf_tx: mpsc::UnboundedSender<rf::RfPacket>,
    ) -> Result<()> {
        // Local controller state.
        let mut nfcc = Controller::new(id, nci_stream, nci_writer, rf_rx, rf_tx);

        // Timer for tick events.
        let mut timer = time::interval(Duration::from_millis(1000));

        loop {
            nfcc.run_until(timer.tick()).await?;
            nfcc.tick().await?;
        }
    }
}
