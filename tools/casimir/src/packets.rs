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

    impl NfceeId {
        pub fn nfcee(id: u8) -> Self {
            NfceeId::try_from(id).unwrap()
        }

        pub fn hci_nfcee(id: u8) -> Self {
            NfceeId::try_from(id).unwrap()
        }
    }

    use futures::stream::{self, Stream};
    use std::pin::Pin;
    use tokio::io::{AsyncRead, AsyncWrite};
    use tokio::sync::Mutex;

    /// Read NCI Control and Data packets received on the NCI transport.
    /// Performs recombination of the segmented packets.
    pub struct Reader {
        socket: Pin<Box<dyn AsyncRead>>,
    }

    /// Write NCI Control and Data packets received to the NCI transport.
    /// Performs segmentation of the packets.
    pub struct Writer {
        socket: Pin<Box<dyn AsyncWrite>>,
    }

    impl Reader {
        /// Create an NCI reader from an NCI transport.
        pub fn new<T: AsyncRead + 'static>(rx: T) -> Self {
            Reader { socket: Box::pin(rx) }
        }

        /// Read a single NCI packet from the reader. The packet is automatically
        /// re-assembled if segmented on the NCI transport.
        pub async fn read(&mut self) -> anyhow::Result<Vec<u8>> {
            use tokio::io::AsyncReadExt;

            const HEADER_SIZE: usize = 3;
            let mut complete_packet = vec![0; HEADER_SIZE];

            // Note on reassembly:
            // - for each segment of a Control Message, the header of the
            //   Control Packet SHALL contain the same MT, GID and OID values,
            // - for each segment of a Data Message the header of the Data
            //   Packet SHALL contain the same MT and Conn ID.
            // Thus it is correct to keep only the last header of the segmented
            // packet.
            loop {
                // Read the common packet header.
                self.socket.read_exact(&mut complete_packet[0..HEADER_SIZE]).await?;
                let header = PacketHeader::parse(&complete_packet[0..HEADER_SIZE])?;

                // Read the packet payload.
                let payload_length = header.get_payload_length() as usize;
                let mut payload_bytes = vec![0; payload_length];
                self.socket.read_exact(&mut payload_bytes).await?;
                complete_packet.extend(payload_bytes);

                // Check the Packet Boundary Flag.
                match header.get_pbf() {
                    PacketBoundaryFlag::CompleteOrFinal => return Ok(complete_packet),
                    PacketBoundaryFlag::Incomplete => (),
                }
            }
        }

        pub fn into_stream(self) -> impl Stream<Item = anyhow::Result<Vec<u8>>> {
            stream::try_unfold(self, |mut reader| async move {
                Ok(Some((reader.read().await?, reader)))
            })
        }
    }

    /// A mutable reference to the stream returned by into_stream
    pub type StreamRefMut<'a> = Pin<&'a mut dyn Stream<Item = anyhow::Result<Vec<u8>>>>;

    impl Writer {
        /// Create an NCI writer from an NCI transport.
        pub fn new<T: AsyncWrite + 'static>(rx: T) -> Self {
            Writer { socket: Box::pin(rx) }
        }

        /// Write a single NCI packet to the writer. The packet is automatically
        /// segmented if the payload exceeds the maximum size limit.
        pub async fn write(&mut self, mut packet: &[u8]) -> anyhow::Result<()> {
            use tokio::io::AsyncWriteExt;

            let mut header_bytes = [packet[0], packet[1], 0];
            packet = &packet[3..];

            loop {
                // Update header with framing information.
                let chunk_length = std::cmp::min(255, packet.len());
                let pbf = if chunk_length < packet.len() {
                    PacketBoundaryFlag::Incomplete
                } else {
                    PacketBoundaryFlag::CompleteOrFinal
                };
                const PBF_MASK: u8 = 0x10;
                header_bytes[0] &= !PBF_MASK;
                header_bytes[0] |= (pbf as u8) << 4;
                header_bytes[2] = chunk_length as u8;

                // Write the header and payload segment bytes.
                self.socket.write_all(&header_bytes).await?;
                self.socket.write_all(&packet[..chunk_length]).await?;
                packet = &packet[chunk_length..];

                if packet.is_empty() {
                    return Ok(());
                }
            }
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

impl From<rf::DeactivateType> for nci::DeactivationType {
    fn from(type_: rf::DeactivateType) -> Self {
        match type_ {
            rf::DeactivateType::IdleMode => nci::DeactivationType::IdleMode,
            rf::DeactivateType::SleepMode => nci::DeactivationType::SleepMode,
            rf::DeactivateType::SleepAfMode => nci::DeactivationType::SleepAfMode,
            rf::DeactivateType::Discovery => nci::DeactivationType::Discovery,
        }
    }
}

impl From<nci::DeactivationType> for rf::DeactivateType {
    fn from(type_: nci::DeactivationType) -> Self {
        match type_ {
            nci::DeactivationType::IdleMode => rf::DeactivateType::IdleMode,
            nci::DeactivationType::SleepMode => rf::DeactivateType::SleepMode,
            nci::DeactivationType::SleepAfMode => rf::DeactivateType::SleepAfMode,
            nci::DeactivationType::Discovery => rf::DeactivateType::Discovery,
        }
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
