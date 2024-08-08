#!/usr/bin/env python3

# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import argparse
import inspect
import json
import random
import readline
import socket
import sys
import time
import requests
import struct
import asyncio
from concurrent.futures import ThreadPoolExecutor

import rf_packets as rf


class T4AT:
    def __init__(self, reader, writer):
        self.nfcid1 = bytes([0x08]) + int.to_bytes(random.randint(0, 0xffffff), length=3)
        self.rats_response = bytes([0x2, 0x0])
        self.reader = reader
        self.writer = writer

    async def _read(self) -> rf.RfPacket:
        header_bytes = await self.reader.read(2)
        packet_length = int.from_bytes(header_bytes, byteorder='little')
        packet_bytes = await self.reader.read(packet_length)

        packet = rf.RfPacket.parse_all(packet_bytes)
        packet.show()
        return packet

    def _write(self, packet: rf.RfPacket):
        packet_bytes = packet.serialize()
        header_bytes = int.to_bytes(len(packet_bytes), length=2, byteorder='little')
        self.writer.write(header_bytes + packet_bytes)

    async def listen(self):
        """Emulate device in passive listen mode. Respond to poll requests until
        the device is activated by a select command."""
        while True:
            packet = await self._read()
            match packet:
                case rf.PollCommand(technology=rf.Technology.NFC_A):
                    self._write(rf.NfcAPollResponse(
                        nfcid1=self.nfcid1, int_protocol=0b01))
                case rf.T4ATSelectCommand(_):
                    self._write(rf.T4ATSelectResponse(
                        rats_response=self.rats_response,
                        receiver=packet.sender))
                    print(f"t4at device selected by #{packet.sender}")
                    await self.active(packet.sender)
                case _:
                    pass

    async def poll(self):
        """Emulate device in passive poll mode. Automatically selects the
        first discovered device."""
        while True:
            try:
                self._write(rf.PollCommand(technology=rf.Technology.NFC_A))
                packet = await asyncio.wait_for(self._read(), timeout=1.0)
                match packet:
                    # [DIGITAL] Table 20: SEL_RES Response Format
                    # 01b: Configured for Type 4A Tag Platform
                    case rf.NfcAPollResponse(int_protocol=0b01):
                        nfcid1 = bytes(packet.nfcid1)
                        print(f"discovered t4at device with nfcid1 #{nfcid1.hex()}")
                        self._write(rf.T4ATSelectCommand(receiver=packet.sender, param=0))
                        response = await asyncio.wait_for(
                            self.wait_for_select_response(packet.sender), timeout=1.0)
                        print(f"t4at device activation complete")
                        await self.active(response.sender)
                    case _:
                        pass
                time.sleep(0.150);
            except TimeoutError:
                pass

    async def wait_for_select_response(self, sender_id: int):
        while True:
            packet = await self._read()
            if isinstance(packet, rf.T4ATSelectResponse) and packet.sender == sender_id:
                return packet

    async def active(self, peer: int):
        """Active mode. Respond to data requests until the device
        is deselected."""
        while True:
            packet = await self._read()
            match packet:
                case rf.DeactivateNotification(_):
                    return
                case rf.Data(_):
                    pass
                case _:
                    pass


async def run(address: str, rf_port: int, mode: str):
    """Emulate a T4AT compatible device in Listen mode."""
    try:
        reader, writer = await asyncio.open_connection(address, rf_port)
        device = T4AT(reader, writer)
        if mode == 'poll':
            await device.poll()
        elif mode == 'listen':
            await device.listen()
        else:
            print(f"unsupported device mode {mode}")
    except Exception as exn:
        print(
            f'Failed to connect to Casimir server at address {address}:{rf_port}:\n' +
            f'    {exn}\n' +
            'Make sure the server is running')
        exit(1)


def main():
    """Start a Casimir interactive console."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--address',
                        type=str,
                        default='127.0.0.1',
                        help='Select the casimir server address')
    parser.add_argument('--rf-port',
                        type=int,
                        default=7001,
                        help='Select the casimir TCP RF port')
    parser.add_argument('--mode',
                        type=str,
                        choices=['poll', 'listen'],
                        default='poll',
                        help='Select the tag mode')
    asyncio.run(run(**vars(parser.parse_args())))


if __name__ == '__main__':
    main()
