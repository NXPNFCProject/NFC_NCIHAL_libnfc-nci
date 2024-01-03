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

//! Packet parsers and serializers.

/// NCI packet parser and serializer.
pub mod nci {
    #![allow(clippy::all)]
    #![allow(unused)]
    #![allow(missing_docs)]

    include!(concat!(env!("OUT_DIR"), "/nci_packets.rs"));

    impl ConnId {
        /// Create a Conn ID with `id` as an offset in the range of dynamic
        /// identifiers.
        pub fn from_dynamic(id: u8) -> Self {
            ConnId::try_from(id as u8 + 2).unwrap()
        }

        /// Return the index for a dynamic Conn ID.
        pub fn to_dynamic(id: Private<u8>) -> u8 {
            *id - 2
        }
    }

    impl RfDiscoveryId {
        /// Create the default reserved RF Discovery ID.
        pub fn reserved() -> Self {
            RfDiscoveryId::try_from(0).unwrap()
        }

        /// Create an RF Discovery ID with `id` as an offset in the range of
        /// non-reserved identifiers.
        pub fn from_index(id: usize) -> Self {
            RfDiscoveryId::try_from(id as u8 + 1).unwrap()
        }

        /// Return the index for a valid RF Discovery ID.
        pub fn to_index(id: Private<u8>) -> usize {
            *id as usize - 1
        }
    }
}

/// RF packet parser and serializer.
pub mod rf {
    #![allow(clippy::all)]
    #![allow(unused)]
    #![allow(missing_docs)]

    include!(concat!(env!("OUT_DIR"), "/rf_packets.rs"));
}

impl From<rf::Protocol> for nci::RfProtocolType {
    fn from(protocol: rf::Protocol) -> Self {
        match protocol {
            rf::Protocol::Undetermined => nci::RfProtocolType::Undetermined,
            rf::Protocol::T1t => nci::RfProtocolType::T1t,
            rf::Protocol::T2t => nci::RfProtocolType::T2t,
            rf::Protocol::T3t => nci::RfProtocolType::T3t,
            rf::Protocol::IsoDep => nci::RfProtocolType::IsoDep,
            rf::Protocol::NfcDep => nci::RfProtocolType::NfcDep,
            rf::Protocol::T5t => nci::RfProtocolType::T5t,
            rf::Protocol::Ndef => nci::RfProtocolType::Ndef,
        }
    }
}

impl From<nci::RfProtocolType> for rf::Protocol {
    fn from(protocol: nci::RfProtocolType) -> Self {
        match protocol {
            nci::RfProtocolType::Undetermined => rf::Protocol::Undetermined,
            nci::RfProtocolType::T1t => rf::Protocol::T1t,
            nci::RfProtocolType::T2t => rf::Protocol::T2t,
            nci::RfProtocolType::T3t => rf::Protocol::T3t,
            nci::RfProtocolType::IsoDep => rf::Protocol::IsoDep,
            nci::RfProtocolType::NfcDep => rf::Protocol::NfcDep,
            nci::RfProtocolType::T5t => rf::Protocol::T5t,
            nci::RfProtocolType::Ndef => rf::Protocol::Ndef,
        }
    }
}

impl TryFrom<nci::RfTechnologyAndMode> for rf::Technology {
    type Error = nci::RfTechnologyAndMode;
    fn try_from(protocol: nci::RfTechnologyAndMode) -> Result<Self, Self::Error> {
        Ok(match protocol {
            nci::RfTechnologyAndMode::NfcAPassivePollMode
            | nci::RfTechnologyAndMode::NfcAPassiveListenMode => rf::Technology::NfcA,
            nci::RfTechnologyAndMode::NfcBPassivePollMode
            | nci::RfTechnologyAndMode::NfcBPassiveListenMode => rf::Technology::NfcB,
            nci::RfTechnologyAndMode::NfcFPassivePollMode
            | nci::RfTechnologyAndMode::NfcFPassiveListenMode => rf::Technology::NfcF,
            nci::RfTechnologyAndMode::NfcVPassivePollMode => rf::Technology::NfcV,
            _ => return Err(protocol),
        })
    }
}

impl From<rf::DeactivateReason> for nci::DeactivationReason {
    fn from(reason: rf::DeactivateReason) -> Self {
        match reason {
            rf::DeactivateReason::DhRequest => nci::DeactivationReason::DhRequest,
            rf::DeactivateReason::EndpointRequest => nci::DeactivationReason::EndpointRequest,
            rf::DeactivateReason::RfLinkLoss => nci::DeactivationReason::RfLinkLoss,
            rf::DeactivateReason::NfcBBadAfi => nci::DeactivationReason::NfcBBadAfi,
            rf::DeactivateReason::DhRequestFailed => nci::DeactivationReason::DhRequestFailed,
        }
    }
}
