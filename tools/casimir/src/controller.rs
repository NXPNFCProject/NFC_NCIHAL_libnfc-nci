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
use crate::NciReader;
use crate::NciWriter;
use anyhow::Result;
use core::time::Duration;
use pdl_runtime::Packet;
use std::collections::HashMap;
use std::convert::TryFrom;
use tokio::sync::mpsc;
use tokio::sync::Mutex;
use tokio::time;

const NCI_VERSION: nci::NciVersion = nci::NciVersion::Version11;
const MAX_LOGICAL_CONNECTIONS: u8 = 2;
const MAX_ROUTING_TABLE_SIZE: u16 = 512;
const MAX_CONTROL_PACKET_PAYLOAD_SIZE: u8 = 255;
const MAX_DATA_PACKET_PAYLOAD_SIZE: u8 = 255;
const NUMBER_OF_CREDITS: u8 = 0;
const MAX_NFCV_RF_FRAME_SIZE: u16 = 512;

/// Time in milliseconds that Casimir waits for poll responses after
/// sending a poll command.
const POLL_RESPONSE_TIMEOUT: u64 = 200;

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

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
#[allow(missing_docs)]
pub enum RfMode {
    Poll,
    Listen,
}

// Whether Observe Mode is Enabled or Disabled.
#[derive(Clone, Copy, Debug)]
#[allow(missing_docs)]
pub enum ObserveModeState {
    Enable,
    Disable,
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
    pub config_parameters: HashMap<nci::ConfigParameterId, Vec<u8>>,
    pub logical_connections: [Option<LogicalConnection>; MAX_LOGICAL_CONNECTIONS as usize],
    pub discover_configuration: Vec<nci::DiscoverConfiguration>,
    pub discover_map: Vec<nci::MappingConfiguration>,
    pub rf_state: RfState,
    pub rf_poll_responses: Vec<RfPollResponse>,
    pub observe_mode: ObserveModeState,
}

/// State of an NFCC instance.
pub struct Controller {
    id: u16,
    nci_writer: NciWriter,
    rf_tx: mpsc::UnboundedSender<rf::RfPacket>,
    state: Mutex<State>,
}

impl State {
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

impl Controller {
    /// Create a new NFCC instance with default configuration.
    pub fn new(
        id: u16,
        nci_writer: NciWriter,
        rf_tx: mpsc::UnboundedSender<rf::RfPacket>,
    ) -> Controller {
        Controller {
            id,
            nci_writer,
            rf_tx,
            state: Mutex::new(State {
                config_parameters: HashMap::new(),
                logical_connections: [None; MAX_LOGICAL_CONNECTIONS as usize],
                discover_map: vec![],
                discover_configuration: vec![],
                rf_state: RfState::Idle,
                rf_poll_responses: vec![],
                observe_mode: ObserveModeState::Disable,
            }),
        }
    }

    /// Craft the NFCID1 used by this instance in NFC-A poll responses.
    /// Returns a dynamically generated NFCID1 (4 byte long and starts with 08h).
    fn nfcid1(&self) -> Vec<u8> {
        vec![0x08, self.id as u8, (self.id >> 8) as u8, 0]
    }

    async fn send_control(&self, packet: impl Into<nci::ControlPacket>) -> Result<()> {
        self.nci_writer.write(&packet.into().to_vec()).await
    }

    async fn send_data(&self, packet: impl Into<nci::DataPacket>) -> Result<()> {
        self.nci_writer.write(&packet.into().to_vec()).await
    }

    async fn send_rf(&self, packet: impl Into<rf::RfPacket>) -> Result<()> {
        self.rf_tx.send(packet.into())?;
        Ok(())
    }

    async fn core_reset(&self, cmd: nci::CoreResetCommand) -> Result<()> {
        println!("+ core_reset_cmd({:?})", cmd.get_reset_type());

        let mut state = self.state.lock().await;

        match cmd.get_reset_type() {
            nci::ResetType::KeepConfig => (),
            nci::ResetType::ResetConfig => state.config_parameters.clear(),
        }

        for i in 0..MAX_LOGICAL_CONNECTIONS {
            state.logical_connections[i as usize] = None;
        }

        state.discover_map.clear();
        state.discover_configuration.clear();
        state.rf_state = RfState::Idle;
        state.rf_poll_responses.clear();

        self.send_control(nci::CoreResetResponseBuilder { status: nci::Status::Ok }).await?;

        self.send_control(nci::CoreResetNotificationBuilder {
            trigger: nci::ResetTrigger::ResetCommand,
            config_status: match cmd.get_reset_type() {
                nci::ResetType::KeepConfig => nci::ConfigStatus::ConfigKept,
                nci::ResetType::ResetConfig => nci::ConfigStatus::ConfigReset,
            },
            nci_version: NCI_VERSION,
            manufacturer_id: 0,
            manufacturer_specific_information: vec![],
        })
        .await?;

        Ok(())
    }

    async fn core_init(&self, _cmd: nci::CoreInitCommand) -> Result<()> {
        println!("+ core_init_cmd()");

        self.send_control(nci::CoreInitResponseBuilder {
            status: nci::Status::Ok,
            nfcc_features: nci::NfccFeatures {
                discovery_frequency_configuration: nci::FeatureFlag::Disabled,
                discovery_configuration_mode: nci::DiscoveryConfigurationMode::DhOnly,
                hci_network_support: nci::FeatureFlag::Disabled,
                active_communication_mode: nci::FeatureFlag::Disabled,
                technology_based_routing: nci::FeatureFlag::Disabled,
                protocol_based_routing: nci::FeatureFlag::Disabled,
                aid_based_routing: nci::FeatureFlag::Disabled,
                system_code_based_routing: nci::FeatureFlag::Disabled,
                apdu_pattern_based_routing: nci::FeatureFlag::Disabled,
                forced_nfcee_routing: nci::FeatureFlag::Disabled,
                battery_off_state: nci::FeatureFlag::Disabled,
                switched_off_state: nci::FeatureFlag::Disabled,
                switched_on_substates: nci::FeatureFlag::Disabled,
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
                nci::RfInterface {
                    interface: nci::RfInterfaceType::NfceeDirect,
                    extensions: vec![nci::RfInterfaceExtensionType::FrameAggregated],
                },
                nci::RfInterface { interface: nci::RfInterfaceType::NfcDep, extensions: vec![] },
            ],
        })
        .await?;

        Ok(())
    }

    async fn core_set_config(&self, cmd: nci::CoreSetConfigCommand) -> Result<()> {
        println!("+ core_set_config_cmd()");

        let mut state = self.state.lock().await;
        let mut invalid_parameters = vec![];
        for parameter in cmd.get_parameters().iter() {
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
                    state.config_parameters.insert(parameter.id, parameter.value.clone());
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
                println!(
                    " > rejecting unknown configuration parameter ids: {:?}",
                    invalid_parameters
                );
                nci::Status::InvalidParam
            },
            parameters: invalid_parameters,
        })
        .await?;

        Ok(())
    }

    async fn core_get_config(&self, cmd: nci::CoreGetConfigCommand) -> Result<()> {
        println!("+ core_get_config_cmd()");

        let state = self.state.lock().await;
        let mut valid_parameters = vec![];
        let mut invalid_parameters = vec![];
        for id in cmd.get_parameters() {
            match state.config_parameters.get(id) {
                Some(value) => {
                    valid_parameters.push(nci::ConfigParameter { id: *id, value: value.clone() })
                }
                None => invalid_parameters.push(nci::ConfigParameter { id: *id, value: vec![] }),
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

    async fn core_conn_create(&self, cmd: nci::CoreConnCreateCommand) -> Result<()> {
        println!("+ core_conn_create()");

        let mut state = self.state.lock().await;
        let result: std::result::Result<u8, nci::Status> = (|| {
            // Retrieve an unused connection ID for the logical connection.
            let conn_id = {
                (0..MAX_LOGICAL_CONNECTIONS)
                    .find(|conn_id| state.logical_connections[*conn_id as usize].is_none())
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
            if state.logical_connections.iter().any(|c| c.as_ref() == Some(&logical_connection)) {
                return Err(nci::Status::Rejected);
            }

            // Create the connection.
            state.logical_connections[conn_id as usize] = Some(logical_connection);

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

    async fn core_conn_close(&self, cmd: nci::CoreConnCloseCommand) -> Result<()> {
        println!("+ core_conn_close({})", u8::from(cmd.get_conn_id()));

        let mut state = self.state.lock().await;
        let conn_id = match cmd.get_conn_id() {
            nci::ConnId::StaticRf | nci::ConnId::StaticHci => {
                println!(" > core_conn_close with static conn_id");
                self.send_control(nci::CoreConnCloseResponseBuilder {
                    status: nci::Status::Rejected,
                })
                .await?;
                return Ok(());
            }
            nci::ConnId::Dynamic(id) => nci::ConnId::to_dynamic(id),
        };

        let status = if conn_id >= MAX_LOGICAL_CONNECTIONS
            || state.logical_connections[conn_id as usize].is_none()
        {
            // If there is no connection associated to the Conn ID in the CORE_CONN_CLOSE_CMD, the
            // NFCC SHALL reject the connection closure request by sending a CORE_CONN_CLOSE_RSP
            // with a Status of STATUS_REJECTED.
            nci::Status::Rejected
        } else {
            // When it receives a CORE_CONN_CLOSE_CMD for an existing connection, the NFCC SHALL
            // accept the connection closure request by sending a CORE_CONN_CLOSE_RSP with a Status of
            // STATUS_OK, and the Logical Connection is closed.
            state.logical_connections[conn_id as usize] = None;
            nci::Status::Ok
        };

        self.send_control(nci::CoreConnCloseResponseBuilder { status }).await?;

        Ok(())
    }

    async fn core_set_power_sub_state(&self, cmd: nci::CoreSetPowerSubStateCommand) -> Result<()> {
        println!("+ core_set_power_sub_state({:?})", cmd.get_power_state());

        self.send_control(nci::CoreSetPowerSubStateResponseBuilder { status: nci::Status::Ok })
            .await?;

        Ok(())
    }

    async fn rf_discover_map(&self, cmd: nci::RfDiscoverMapCommand) -> Result<()> {
        println!("+ rf_discover_map()");

        let mut state = self.state.lock().await;
        state.discover_map = cmd.get_mapping_configurations().clone();
        self.send_control(nci::RfDiscoverMapResponseBuilder { status: nci::Status::Ok }).await?;

        Ok(())
    }

    async fn rf_set_listen_mode_routing(
        &self,
        _cmd: nci::RfSetListenModeRoutingCommand,
    ) -> Result<()> {
        println!("+ rf_set_listen_mode_routing()");

        self.send_control(nci::RfSetListenModeRoutingResponseBuilder { status: nci::Status::Ok })
            .await?;

        Ok(())
    }

    async fn rf_get_listen_mode_routing(
        &self,
        _cmd: nci::RfGetListenModeRoutingCommand,
    ) -> Result<()> {
        println!("+ rf_get_listen_mode_routing()");

        self.send_control(nci::RfGetListenModeRoutingResponseBuilder {
            status: nci::Status::Ok,
            more_to_follow: 0,
            routing_entries: vec![],
        })
        .await?;

        Ok(())
    }

    async fn rf_discover(&self, cmd: nci::RfDiscoverCommand) -> Result<()> {
        println!("+ rf_discover()");

        let mut state = self.state.lock().await;
        if state.rf_state != RfState::Idle {
            println!("rf_discover_cmd received in {:?} state", state.rf_state);
            self.send_control(nci::RfDiscoverResponseBuilder {
                status: nci::Status::SemanticError,
            })
            .await?;
            return Ok(());
        }

        for config in cmd.get_configurations() {
            println!(" > {:?}", config.technology_and_mode);
        }

        state.discover_configuration = cmd.get_configurations().clone();
        state.rf_state = RfState::Discovery;

        self.send_control(nci::RfDiscoverResponseBuilder { status: nci::Status::Ok }).await?;

        Ok(())
    }

    async fn rf_discover_select(&self, cmd: nci::RfDiscoverSelectCommand) -> Result<()> {
        println!("+ rf_discover_select()");

        let mut state = self.state.lock().await;

        if state.rf_state != RfState::WaitForHostSelect {
            println!("rf_discover_select_cmd received in {:?} state", state.rf_state);
            self.send_control(nci::RfDiscoverSelectResponseBuilder {
                status: nci::Status::SemanticError,
            })
            .await?;
            return Ok(());
        }

        let rf_discovery_id = match cmd.get_rf_discovery_id() {
            nci::RfDiscoveryId::Rfu(_) => {
                println!("rf_discover_select_cmd with reserved rf_discovery_id");
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
        if rf_discovery_id >= state.rf_poll_responses.len() {
            println!("rf_discover_select_cmd with invalid rf_discovery_id");
            self.send_control(nci::RfDiscoverSelectResponseBuilder {
                status: nci::Status::Rejected,
            })
            .await?;
            return Ok(());
        }

        if cmd.get_rf_protocol() != state.rf_poll_responses[rf_discovery_id].rf_protocol.into() {
            println!("rf_discover_select_cmd with invalid rf_protocol");
            self.send_control(nci::RfDiscoverSelectResponseBuilder {
                status: nci::Status::Rejected,
            })
            .await?;
            return Ok(());
        }

        // Send RF select command to the peer to activate the device.
        // The command has varying parameters based on the activated protocol.
        self.activate_poll_interface(
            &mut state,
            rf_discovery_id,
            cmd.get_rf_protocol(),
            cmd.get_rf_interface(),
        )
        .await?;

        Ok(())
    }

    async fn rf_deactivate(&self, cmd: nci::RfDeactivateCommand) -> Result<()> {
        println!("+ rf_deactivate({:?})", cmd.get_deactivation_type());

        use nci::DeactivationType::*;

        let mut state = self.state.lock().await;
        let (status, mut next_state) = match (state.rf_state, cmd.get_deactivation_type()) {
            (RfState::Idle, _) => (nci::Status::SemanticError, RfState::Idle),
            (RfState::Discovery, IdleMode) => (nci::Status::Ok, RfState::Idle),
            (RfState::Discovery, _) => (nci::Status::SemanticError, RfState::Discovery),
            (RfState::PollActive { .. }, IdleMode) => (nci::Status::Ok, RfState::Idle),
            (RfState::PollActive { .. }, SleepMode | SleepAfMode) => {
                (nci::Status::Ok, RfState::WaitForHostSelect)
            }
            (RfState::PollActive { .. }, Discover) => (nci::Status::Ok, RfState::Discovery),
            (RfState::ListenSleep { .. }, IdleMode) => (nci::Status::Ok, RfState::Idle),
            (RfState::ListenSleep { .. }, _) => (nci::Status::SemanticError, state.rf_state),
            (RfState::ListenActive { .. }, IdleMode) => (nci::Status::Ok, RfState::Idle),
            (RfState::ListenActive { id, .. }, SleepMode | SleepAfMode) => {
                (nci::Status::Ok, RfState::ListenSleep { id })
            }
            (RfState::ListenActive { .. }, Discover) => (nci::Status::Ok, RfState::Discovery),
            (RfState::WaitForHostSelect, IdleMode) => (nci::Status::Ok, RfState::Idle),
            (RfState::WaitForHostSelect, _) => {
                (nci::Status::SemanticError, RfState::WaitForHostSelect)
            }
            (RfState::WaitForSelectResponse { .. }, IdleMode) => (nci::Status::Ok, RfState::Idle),
            (RfState::WaitForSelectResponse { .. }, _) => {
                (nci::Status::SemanticError, state.rf_state)
            }
        };

        // Update the state now to prevent interface activation from
        // completing if a remote device is being selected.
        (next_state, state.rf_state) = (state.rf_state, next_state);

        self.send_control(nci::RfDeactivateResponseBuilder { status }).await?;

        // Deactivate the active RF interface if applicable.
        match next_state {
            RfState::PollActive { .. } | RfState::ListenActive { .. } => {
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
                    reason: rf::DeactivateReason::DhRequest,
                })
                .await?
            }
            _ => (),
        }

        Ok(())
    }

    async fn nfcee_discover(&self, _cmd: nci::NfceeDiscoverCommand) -> Result<()> {
        println!("+ nfcee_discover()");

        self.send_control(nci::NfceeDiscoverResponseBuilder {
            status: nci::Status::Ok,
            number_of_nfcees: 0,
        })
        .await?;

        Ok(())
    }

    async fn android_observe_mode(&self, cmd: nci::AndroidObserveModeCmd) -> Result<()> {
        let mut state = self.state.lock().await;
        state.observe_mode = match cmd.get_observe_mode_enable() {
            nci::ObserveModeEnable::Enable => ObserveModeState::Enable,
            nci::ObserveModeEnable::Disable => ObserveModeState::Disable,
        };
        println!("+ observe_mode: {:?}", state.observe_mode);
        self.send_control(nci::AndroidObserveModeRspBuilder { status: nci::Status::Ok }).await?;
        Ok(())
    }

    async fn receive_command(&self, packet: nci::ControlPacket) -> Result<()> {
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
                _ => unimplemented!("unsupported nfcee oid {:?}", packet.get_oid()),
            },
            ProprietaryPacket(packet) => match packet.specialize() {
                AndroidPacket(packet) => match packet.specialize() {
                    AndroidObserveModeCmd(cmd) => self.android_observe_mode(cmd).await,
                    _ => {
                        unimplemented!("unsupported android oid {:?}", packet.get_android_sub_oid())
                    }
                },
                _ => unimplemented!("unsupported proprietary oid {:?}", packet.get_oid()),
            },
            _ => unimplemented!("unsupported gid {:?}", packet.get_gid()),
        }
    }

    async fn rf_conn_data(&self, packet: nci::DataPacket) -> Result<()> {
        println!("  > received data on RF logical connection");

        // TODO(henrichataing) implement credit based control flow.
        let state = self.state.lock().await;
        match state.rf_state {
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
            RfState::PollActive { rf_protocol, rf_interface, .. }
            | RfState::ListenActive { rf_protocol, rf_interface, .. } => unimplemented!(
                "unsupported combination of RF protocol {:?} and interface {:?}",
                rf_protocol,
                rf_interface
            ),
            _ => {
                println!("  > ignored RF data packet while not in active listen or poll mode");
                Ok(())
            }
        }
    }

    async fn hci_conn_data(&self, _packet: nci::DataPacket) -> Result<()> {
        println!("  > received data on HCI logical connection");
        todo!()
    }

    async fn dynamic_conn_data(&self, _conn_id: u8, _packet: nci::DataPacket) -> Result<()> {
        println!("  > received data on dynamic logical connection");
        todo!()
    }

    async fn receive_data(&self, packet: nci::DataPacket) -> Result<()> {
        println!("+ receive_data({})", u8::from(packet.get_conn_id()));

        match packet.get_conn_id() {
            nci::ConnId::StaticRf => self.rf_conn_data(packet).await,
            nci::ConnId::StaticHci => self.hci_conn_data(packet).await,
            nci::ConnId::Dynamic(id) => self.dynamic_conn_data(*id, packet).await,
        }
    }

    async fn poll_command(&self, cmd: rf::PollCommand) -> Result<()> {
        println!("+ poll_command()");

        let state = self.state.lock().await;
        if state.rf_state != RfState::Discovery {
            return Ok(());
        }

        let technology = cmd.get_technology();
        if state.discover_configuration.iter().any(|config| {
            matches!(
                (config.technology_and_mode, technology),
                (nci::RfTechnologyAndMode::NfcAPassiveListenMode, rf::Technology::NfcA)
                    | (nci::RfTechnologyAndMode::NfcBPassiveListenMode, rf::Technology::NfcB)
                    | (nci::RfTechnologyAndMode::NfcFPassiveListenMode, rf::Technology::NfcF)
            )
        }) {
            match (state.observe_mode, technology) {
                (ObserveModeState::Enable, _) => (),
                (ObserveModeState::Disable, rf::Technology::NfcA) => {
                    // Configured for T4AT tag emulation.
                    let int_protocol = 0x01;
                    self.send_rf(rf::NfcAPollResponseBuilder {
                        protocol: rf::Protocol::Undetermined,
                        receiver: cmd.get_sender(),
                        sender: self.id,
                        nfcid1: self.nfcid1(),
                        int_protocol,
                    })
                    .await?
                }
                (ObserveModeState::Disable, rf::Technology::NfcB) => todo!(),
                (ObserveModeState::Disable, rf::Technology::NfcF) => todo!(),
                _ => (),
            }
        }

        Ok(())
    }

    async fn nfca_poll_response(&self, cmd: rf::NfcAPollResponse) -> Result<()> {
        println!("+ nfca_poll_response()");

        let mut state = self.state.lock().await;
        if state.rf_state != RfState::Discovery {
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
        };
        let sel_res = int_protocol << 5;

        for rf_protocol in rf_protocols {
            state.add_poll_response(RfPollResponse {
                id: cmd.get_sender(),
                rf_protocol: *rf_protocol,
                rf_technology: rf::Technology::NfcA,
                rf_technology_specific_parameters: pdl_runtime::Packet::to_vec(
                    nci::NfcAPollModeTechnologySpecificParametersBuilder {
                        sens_res,
                        nfcid1: cmd.get_nfcid1().clone(),
                        sel_res,
                    }
                    .build(),
                ),
            })
        }

        Ok(())
    }

    async fn t4at_select_command(&self, cmd: rf::T4ATSelectCommand) -> Result<()> {
        println!("+ t4at_select_command()");

        let mut state = self.state.lock().await;
        if state.rf_state != RfState::Discovery {
            return Ok(());
        }

        // TODO(henrichataing): validate that the protocol and technology are
        // valid for the current discovery settings.

        // TODO(henrichataing): use listen mode routing table to decide which
        // interface should be used for the activating device.

        state.rf_state = RfState::ListenActive {
            id: cmd.get_sender(),
            rf_technology: rf::Technology::NfcA,
            rf_protocol: rf::Protocol::IsoDep,
            rf_interface: nci::RfInterfaceType::IsoDep,
        };

        self.send_rf(rf::T4ATSelectResponseBuilder {
            receiver: cmd.get_sender(),
            sender: self.id,
            // [DIGITAL] 14.6.2 RATS Response (Answer To Select)
            // TODO(henrichataing): this value is just a valid default;
            // construct the RATS response from global capabilities.
            rats_response: vec![0x2, 0x0],
        })
        .await?;

        self.send_control(nci::RfIntfActivatedNotificationBuilder {
            rf_discovery_id: nci::RfDiscoveryId::reserved(),
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
            .to_vec(),
        })
        .await?;

        Ok(())
    }

    async fn t4at_select_response(&self, cmd: rf::T4ATSelectResponse) -> Result<()> {
        println!("+ t4at_select_response()");

        let mut state = self.state.lock().await;
        let (id, rf_discovery_id, rf_interface, rf_protocol) = match state.rf_state {
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

        state.rf_state = RfState::PollActive {
            id,
            rf_protocol: state.rf_poll_responses[rf_discovery_id].rf_protocol,
            rf_technology: state.rf_poll_responses[rf_discovery_id].rf_technology,
            rf_interface,
        };

        self.send_control(nci::RfIntfActivatedNotificationBuilder {
            rf_discovery_id: nci::RfDiscoveryId::from_index(rf_discovery_id),
            rf_interface,
            rf_protocol: rf_protocol.into(),
            activation_rf_technology_and_mode: nci::RfTechnologyAndMode::NfcAPassivePollMode,
            max_data_packet_payload_size: MAX_DATA_PACKET_PAYLOAD_SIZE,
            initial_number_of_credits: 1,
            rf_technology_specific_parameters: state.rf_poll_responses[rf_discovery_id]
                .rf_technology_specific_parameters
                .clone(),
            data_exchange_rf_technology_and_mode: nci::RfTechnologyAndMode::NfcAPassivePollMode,
            data_exchange_transmit_bit_rate: nci::BitRate::BitRate106KbitS,
            data_exchange_receive_bit_rate: nci::BitRate::BitRate106KbitS,
            activation_parameters: pdl_runtime::Packet::to_vec(
                nci::NfcAIsoDepPollModeActivationParametersBuilder {
                    rats_response: cmd.get_rats_response().clone(),
                }
                .build(),
            ),
        })
        .await?;

        Ok(())
    }

    async fn data_packet(&self, data: rf::Data) -> Result<()> {
        println!("+ data_packet()");

        let state = self.state.lock().await;
        match (state.rf_state, data.get_protocol()) {
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
                println!("  > ignored RF data packet sent from an un-selected device");
                Ok(())
            }
            (RfState::PollActive { .. }, _) | (RfState::ListenActive { .. }, _) => {
                unimplemented!("unsupported combination of technology and protocol")
            }
            (_, _) => {
                println!("  > ignored RF data packet received in inactive state");
                Ok(())
            }
        }
    }

    async fn deactivate_notification(&self, cmd: rf::DeactivateNotification) -> Result<()> {
        println!("+ deactivate_notification()");

        let mut state = self.state.lock().await;
        let mut next_state = match state.rf_state {
            RfState::PollActive { id, .. }
            | RfState::ListenSleep { id }
            | RfState::ListenActive { id, .. }
            | RfState::WaitForSelectResponse { id, .. }
                if id == cmd.get_sender() =>
            {
                RfState::Idle
            }
            _ => state.rf_state,
        };

        // Update the state now to prevent interface activation from
        // completing if a remote device is being selected.
        (next_state, state.rf_state) = (state.rf_state, next_state);

        // Deactivate the active RF interface if applicable.
        if next_state != state.rf_state {
            self.send_control(nci::RfDeactivateNotificationBuilder {
                deactivation_type: nci::DeactivationType::IdleMode,
                deactivation_reason: cmd.get_reason().into(),
            })
            .await?
        }

        Ok(())
    }

    async fn receive_rf(&self, packet: rf::RfPacket) -> Result<()> {
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
            Data(cmd) => self.data_packet(cmd).await,
            DeactivateNotification(cmd) => self.deactivate_notification(cmd).await,
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
        &self,
        state: &mut State,
        rf_discovery_id: usize,
        rf_protocol: nci::RfProtocolType,
        rf_interface: nci::RfInterfaceType,
    ) -> Result<()> {
        println!("+ activate_poll_interface({:?})", rf_interface);

        let rf_technology = state.rf_poll_responses[rf_discovery_id].rf_technology;
        match (rf_interface, rf_technology) {
            (nci::RfInterfaceType::Frame, rf::Technology::NfcA) => {
                self.send_rf(rf::SelectCommandBuilder {
                    sender: self.id,
                    receiver: state.rf_poll_responses[rf_discovery_id].id,
                    technology: rf::Technology::NfcA,
                    protocol: rf::Protocol::T2t,
                })
                .await?
            }
            (nci::RfInterfaceType::IsoDep, rf::Technology::NfcA) => {
                self.send_rf(rf::T4ATSelectCommandBuilder {
                    sender: self.id,
                    receiver: state.rf_poll_responses[rf_discovery_id].id,
                    param: 0,
                })
                .await?
            }
            (nci::RfInterfaceType::NfcDep, rf::Technology::NfcA) => {
                self.send_rf(rf::NfcDepSelectCommandBuilder {
                    sender: self.id,
                    receiver: state.rf_poll_responses[rf_discovery_id].id,
                    technology: rf::Technology::NfcA,
                    lr: 0,
                })
                .await?
            }
            _ => todo!(),
        }

        state.rf_state = RfState::WaitForSelectResponse {
            id: state.rf_poll_responses[rf_discovery_id].id,
            rf_discovery_id,
            rf_interface,
            rf_protocol: rf_protocol.into(),
            rf_technology,
        };
        Ok(())
    }

    /// Timer handler method. This function is invoked at regular interval
    /// on the NFCC instance and is used to drive internal timers.
    async fn tick(&self) -> Result<()> {
        {
            let mut state = self.state.lock().await;
            if state.rf_state != RfState::Discovery {
                return Ok(());
            }

            //println!("+ poll");

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
            state.rf_poll_responses.clear();
            for configuration in state.discover_configuration.iter() {
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
        }

        // Wait for poll responses to return.
        time::sleep(Duration::from_millis(POLL_RESPONSE_TIMEOUT)).await;

        let mut state = self.state.lock().await;

        // Check if device was activated in Listen mode during
        // the poll interval, or if the discovery got cancelled.
        if state.rf_state != RfState::Discovery || state.rf_poll_responses.is_empty() {
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
        if state.rf_poll_responses.len() == 1 {
            let rf_protocol = state.rf_poll_responses[0].rf_protocol.into();
            let rf_interface = state.select_interface(RfMode::Poll, rf_protocol);
            return self.activate_poll_interface(&mut state, 0, rf_protocol, rf_interface).await;
        }

        println!(" > received {} poll response(s)", state.rf_poll_responses.len());

        // While polling, if the NFCC discovers more than one Remote NFC
        // Endpoint, or a Remote NFC Endpoint that supports more than one RF
        // Protocol, it SHALL start sending RF_DISCOVER_NTF messages to the DH.
        // At this point, the state is changed to RFST_W4_ALL_DISCOVERIES.
        state.rf_state = RfState::WaitForHostSelect;
        let last_index = state.rf_poll_responses.len() - 1;
        for (index, response) in state.rf_poll_responses.clone().iter().enumerate() {
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
        nci_reader: NciReader,
        nci_writer: NciWriter,
        mut rf_rx: mpsc::UnboundedReceiver<rf::RfPacket>,
        rf_tx: mpsc::UnboundedSender<rf::RfPacket>,
    ) -> Result<()> {
        // Local controller state.
        let nfcc = Controller::new(id, nci_writer, rf_tx);
        // Send a Reset notification on controller creation corresponding
        // to a power on.
        nfcc.send_control(nci::CoreResetNotificationBuilder {
            trigger: nci::ResetTrigger::PowerOn,
            config_status: nci::ConfigStatus::ConfigReset,
            nci_version: NCI_VERSION,
            manufacturer_id: 0,
            manufacturer_specific_information: vec![],
        })
        .await?;

        // Timer for tick events.
        let mut timer = time::interval(Duration::from_millis(1000));

        let result: Result<((), (), ())> = futures::future::try_join3(
            // NCI event handler.
            async {
                loop {
                    let packet = nci_reader.read().await?;
                    let header = nci::PacketHeader::parse(&packet[0..3])?;
                    match header.get_mt() {
                        nci::MessageType::Data => {
                            nfcc.receive_data(nci::DataPacket::parse(&packet)?).await?
                        }
                        nci::MessageType::Command => {
                            nfcc.receive_command(nci::ControlPacket::parse(&packet)?).await?
                        }
                        mt => {
                            return Err(anyhow::anyhow!(
                                "unexpected message type {:?} in received NCI packet",
                                mt
                            ))
                        }
                    }
                }
            },
            // RF event handler.
            async {
                loop {
                    nfcc.receive_rf(
                        rf_rx.recv().await.ok_or(anyhow::anyhow!("rf_rx channel closed"))?,
                    )
                    .await?
                }
            },
            // Timer event handler.
            async {
                loop {
                    timer.tick().await;
                    nfcc.tick().await?
                }
            },
        )
        .await;
        result?;
        Ok(())
    }
}
