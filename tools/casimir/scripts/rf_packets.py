# File generated from <stdin>, with the command:
#  /home/henrichataing/Projects/github/google/pdl/pdl-compiler/scripts/generate_python_backend.py
# /!\ Do not edit by hand.
from dataclasses import dataclass, field, fields
from typing import Optional, List, Tuple, Union
import enum
import inspect
import math

@dataclass
class Packet:
    payload: Optional[bytes] = field(repr=False, default_factory=bytes, compare=False)

    @classmethod
    def parse_all(cls, span: bytes) -> 'Packet':
        packet, remain = getattr(cls, 'parse')(span)
        if len(remain) > 0:
            raise Exception('Unexpected parsing remainder')
        return packet

    @property
    def size(self) -> int:
        pass

    def show(self, prefix: str = ''):
        print(f'{self.__class__.__name__}')

        def print_val(p: str, pp: str, name: str, align: int, typ, val):
            if name == 'payload':
                pass

            # Scalar fields.
            elif typ is int:
                print(f'{p}{name:{align}} = {val} (0x{val:x})')

            # Byte fields.
            elif typ is bytes:
                print(f'{p}{name:{align}} = [', end='')
                line = ''
                n_pp = ''
                for (idx, b) in enumerate(val):
                    if idx > 0 and idx % 8 == 0:
                        print(f'{n_pp}{line}')
                        line = ''
                        n_pp = pp + (' ' * (align + 4))
                    line += f' {b:02x}'
                print(f'{n_pp}{line} ]')

            # Enum fields.
            elif inspect.isclass(typ) and issubclass(typ, enum.IntEnum):
                print(f'{p}{name:{align}} = {typ.__name__}::{val.name} (0x{val:x})')

            # Struct fields.
            elif inspect.isclass(typ) and issubclass(typ, globals().get('Packet')):
                print(f'{p}{name:{align}} = ', end='')
                val.show(prefix=pp)

            # Array fields.
            elif getattr(typ, '__origin__', None) == list:
                print(f'{p}{name:{align}}')
                last = len(val) - 1
                align = 5
                for (idx, elt) in enumerate(val):
                    n_p  = pp + ('├── ' if idx != last else '└── ')
                    n_pp = pp + ('│   ' if idx != last else '    ')
                    print_val(n_p, n_pp, f'[{idx}]', align, typ.__args__[0], val[idx])

            # Custom fields.
            elif inspect.isclass(typ):
                print(f'{p}{name:{align}} = {repr(val)}')

            else:
                print(f'{p}{name:{align}} = ##{typ}##')

        last = len(fields(self)) - 1
        align = max(len(f.name) for f in fields(self) if f.name != 'payload')

        for (idx, f) in enumerate(fields(self)):
            p  = prefix + ('├── ' if idx != last else '└── ')
            pp = prefix + ('│   ' if idx != last else '    ')
            val = getattr(self, f.name)

            print_val(p, pp, f.name, align, f.type, val)

class Technology(enum.IntEnum):
    NFC_A = 0x0
    NFC_B = 0x1
    NFC_F = 0x2
    NFC_V = 0x3

    @staticmethod
    def from_int(v: int) -> Union[int, 'Technology']:
        try:
            return Technology(v)
        except ValueError as exn:
            raise exn


class Protocol(enum.IntEnum):
    UNDETERMINED = 0x0
    T1T = 0x1
    T2T = 0x2
    T3T = 0x3
    ISO_DEP = 0x4
    NFC_DEP = 0x5
    T5T = 0x6
    NDEF = 0x7

    @staticmethod
    def from_int(v: int) -> Union[int, 'Protocol']:
        try:
            return Protocol(v)
        except ValueError as exn:
            raise exn


class RfPacketType(enum.IntEnum):
    DATA = 0x0
    POLL_COMMAND = 0x1
    POLL_RESPONSE = 0x2
    SELECT_COMMAND = 0x3
    SELECT_RESPONSE = 0x4
    DEACTIVATE_NOTIFICATION = 0x5

    @staticmethod
    def from_int(v: int) -> Union[int, 'RfPacketType']:
        try:
            return RfPacketType(v)
        except ValueError as exn:
            raise exn


@dataclass
class RfPacket(Packet):
    sender: int = field(kw_only=True, default=0)
    receiver: int = field(kw_only=True, default=0)
    technology: Technology = field(kw_only=True, default=Technology.NFC_A)
    protocol: Protocol = field(kw_only=True, default=Protocol.UNDETERMINED)
    packet_type: RfPacketType = field(kw_only=True, default=RfPacketType.DATA)

    def __post_init__(self):
        pass

    @staticmethod
    def parse(span: bytes) -> Tuple['RfPacket', bytes]:
        fields = {'payload': None}
        if len(span) < 7:
            raise Exception('Invalid packet size')
        value_ = int.from_bytes(span[0:2], byteorder='little')
        fields['sender'] = value_
        value_ = int.from_bytes(span[2:4], byteorder='little')
        fields['receiver'] = value_
        fields['technology'] = Technology.from_int(span[4])
        fields['protocol'] = Protocol.from_int(span[5])
        fields['packet_type'] = RfPacketType.from_int(span[6])
        span = span[7:]
        payload = span
        span = bytes([])
        fields['payload'] = payload
        try:
            return PollCommand.parse(fields.copy(), payload)
        except Exception as exn:
            pass
        try:
            return NfcAPollResponse.parse(fields.copy(), payload)
        except Exception as exn:
            pass
        try:
            return T4ATSelectCommand.parse(fields.copy(), payload)
        except Exception as exn:
            pass
        try:
            return T4ATSelectResponse.parse(fields.copy(), payload)
        except Exception as exn:
            pass
        try:
            return NfcDepSelectCommand.parse(fields.copy(), payload)
        except Exception as exn:
            pass
        try:
            return NfcDepSelectResponse.parse(fields.copy(), payload)
        except Exception as exn:
            pass
        try:
            return SelectCommand.parse(fields.copy(), payload)
        except Exception as exn:
            pass
        try:
            return DeactivateNotification.parse(fields.copy(), payload)
        except Exception as exn:
            pass
        try:
            return Data.parse(fields.copy(), payload)
        except Exception as exn:
            pass
        return RfPacket(**fields), span

    def serialize(self, payload: bytes = None) -> bytes:
        _span = bytearray()
        if self.sender > 65535:
            print(f"Invalid value for field RfPacket::sender: {self.sender} > 65535; the value will be truncated")
            self.sender &= 65535
        _span.extend(int.to_bytes((self.sender << 0), length=2, byteorder='little'))
        if self.receiver > 65535:
            print(f"Invalid value for field RfPacket::receiver: {self.receiver} > 65535; the value will be truncated")
            self.receiver &= 65535
        _span.extend(int.to_bytes((self.receiver << 0), length=2, byteorder='little'))
        _span.append((self.technology << 0))
        _span.append((self.protocol << 0))
        _span.append((self.packet_type << 0))
        _span.extend(payload or self.payload or [])
        return bytes(_span)

    @property
    def size(self) -> int:
        return len(self.payload) + 7

@dataclass
class PollCommand(RfPacket):


    def __post_init__(self):
        self.packet_type = RfPacketType.POLL_COMMAND

    @staticmethod
    def parse(fields: dict, span: bytes) -> Tuple['PollCommand', bytes]:
        if fields['packet_type'] != RfPacketType.POLL_COMMAND:
            raise Exception("Invalid constraint field values")
        return PollCommand(**fields), span

    def serialize(self, payload: bytes = None) -> bytes:
        _span = bytearray()
        return RfPacket.serialize(self, payload = bytes(_span))

    @property
    def size(self) -> int:
        return 0

@dataclass
class NfcAPollResponse(RfPacket):
    nfcid1: bytearray = field(kw_only=True, default_factory=bytearray)
    int_protocol: int = field(kw_only=True, default=0)
    bit_frame_sdd: int = field(kw_only=True, default=0)

    def __post_init__(self):
        self.technology = Technology.NFC_A
        self.packet_type = RfPacketType.POLL_RESPONSE

    @staticmethod
    def parse(fields: dict, span: bytes) -> Tuple['NfcAPollResponse', bytes]:
        if fields['technology'] != Technology.NFC_A or fields['packet_type'] != RfPacketType.POLL_RESPONSE:
            raise Exception("Invalid constraint field values")
        if len(span) < 1:
            raise Exception('Invalid packet size')
        nfcid1_size = span[0]
        span = span[1:]
        if len(span) < nfcid1_size:
            raise Exception('Invalid packet size')
        fields['nfcid1'] = list(span[:nfcid1_size])
        span = span[nfcid1_size:]
        if len(span) < 2:
            raise Exception('Invalid packet size')
        fields['int_protocol'] = (span[0] >> 0) & 0x3
        fields['bit_frame_sdd'] = span[1]
        span = span[2:]
        return NfcAPollResponse(**fields), span

    def serialize(self, payload: bytes = None) -> bytes:
        _span = bytearray()
        _span.append(((len(self.nfcid1) * 1) << 0))
        _span.extend(self.nfcid1)
        if self.int_protocol > 3:
            print(f"Invalid value for field NfcAPollResponse::int_protocol: {self.int_protocol} > 3; the value will be truncated")
            self.int_protocol &= 3
        _span.append((self.int_protocol << 0))
        if self.bit_frame_sdd > 255:
            print(f"Invalid value for field NfcAPollResponse::bit_frame_sdd: {self.bit_frame_sdd} > 255; the value will be truncated")
            self.bit_frame_sdd &= 255
        _span.append((self.bit_frame_sdd << 0))
        return RfPacket.serialize(self, payload = bytes(_span))

    @property
    def size(self) -> int:
        return len(self.nfcid1) * 1 + 3

@dataclass
class T4ATSelectCommand(RfPacket):
    param: int = field(kw_only=True, default=0)

    def __post_init__(self):
        self.technology = Technology.NFC_A
        self.protocol = Protocol.ISO_DEP
        self.packet_type = RfPacketType.SELECT_COMMAND

    @staticmethod
    def parse(fields: dict, span: bytes) -> Tuple['T4ATSelectCommand', bytes]:
        if fields['technology'] != Technology.NFC_A or fields['protocol'] != Protocol.ISO_DEP or fields['packet_type'] != RfPacketType.SELECT_COMMAND:
            raise Exception("Invalid constraint field values")
        if len(span) < 1:
            raise Exception('Invalid packet size')
        fields['param'] = span[0]
        span = span[1:]
        return T4ATSelectCommand(**fields), span

    def serialize(self, payload: bytes = None) -> bytes:
        _span = bytearray()
        if self.param > 255:
            print(f"Invalid value for field T4ATSelectCommand::param: {self.param} > 255; the value will be truncated")
            self.param &= 255
        _span.append((self.param << 0))
        return RfPacket.serialize(self, payload = bytes(_span))

    @property
    def size(self) -> int:
        return 1

@dataclass
class T4ATSelectResponse(RfPacket):
    rats_response: bytearray = field(kw_only=True, default_factory=bytearray)

    def __post_init__(self):
        self.technology = Technology.NFC_A
        self.protocol = Protocol.ISO_DEP
        self.packet_type = RfPacketType.SELECT_RESPONSE

    @staticmethod
    def parse(fields: dict, span: bytes) -> Tuple['T4ATSelectResponse', bytes]:
        if fields['technology'] != Technology.NFC_A or fields['protocol'] != Protocol.ISO_DEP or fields['packet_type'] != RfPacketType.SELECT_RESPONSE:
            raise Exception("Invalid constraint field values")
        if len(span) < 1:
            raise Exception('Invalid packet size')
        rats_response_size = span[0]
        span = span[1:]
        if len(span) < rats_response_size:
            raise Exception('Invalid packet size')
        fields['rats_response'] = list(span[:rats_response_size])
        span = span[rats_response_size:]
        return T4ATSelectResponse(**fields), span

    def serialize(self, payload: bytes = None) -> bytes:
        _span = bytearray()
        _span.append(((len(self.rats_response) * 1) << 0))
        _span.extend(self.rats_response)
        return RfPacket.serialize(self, payload = bytes(_span))

    @property
    def size(self) -> int:
        return len(self.rats_response) * 1 + 1

@dataclass
class NfcDepSelectCommand(RfPacket):
    lr: int = field(kw_only=True, default=0)

    def __post_init__(self):
        self.protocol = Protocol.NFC_DEP
        self.packet_type = RfPacketType.SELECT_COMMAND

    @staticmethod
    def parse(fields: dict, span: bytes) -> Tuple['NfcDepSelectCommand', bytes]:
        if fields['protocol'] != Protocol.NFC_DEP or fields['packet_type'] != RfPacketType.SELECT_COMMAND:
            raise Exception("Invalid constraint field values")
        if len(span) < 1:
            raise Exception('Invalid packet size')
        fields['lr'] = (span[0] >> 0) & 0x3
        span = span[1:]
        return NfcDepSelectCommand(**fields), span

    def serialize(self, payload: bytes = None) -> bytes:
        _span = bytearray()
        if self.lr > 3:
            print(f"Invalid value for field NfcDepSelectCommand::lr: {self.lr} > 3; the value will be truncated")
            self.lr &= 3
        _span.append((self.lr << 0))
        return RfPacket.serialize(self, payload = bytes(_span))

    @property
    def size(self) -> int:
        return 1

@dataclass
class NfcDepSelectResponse(RfPacket):
    atr_response: bytearray = field(kw_only=True, default_factory=bytearray)

    def __post_init__(self):
        self.protocol = Protocol.NFC_DEP
        self.packet_type = RfPacketType.SELECT_RESPONSE

    @staticmethod
    def parse(fields: dict, span: bytes) -> Tuple['NfcDepSelectResponse', bytes]:
        if fields['protocol'] != Protocol.NFC_DEP or fields['packet_type'] != RfPacketType.SELECT_RESPONSE:
            raise Exception("Invalid constraint field values")
        if len(span) < 1:
            raise Exception('Invalid packet size')
        atr_response_size = span[0]
        span = span[1:]
        if len(span) < atr_response_size:
            raise Exception('Invalid packet size')
        fields['atr_response'] = list(span[:atr_response_size])
        span = span[atr_response_size:]
        return NfcDepSelectResponse(**fields), span

    def serialize(self, payload: bytes = None) -> bytes:
        _span = bytearray()
        _span.append(((len(self.atr_response) * 1) << 0))
        _span.extend(self.atr_response)
        return RfPacket.serialize(self, payload = bytes(_span))

    @property
    def size(self) -> int:
        return len(self.atr_response) * 1 + 1

@dataclass
class SelectCommand(RfPacket):


    def __post_init__(self):
        self.packet_type = RfPacketType.SELECT_COMMAND

    @staticmethod
    def parse(fields: dict, span: bytes) -> Tuple['SelectCommand', bytes]:
        if fields['packet_type'] != RfPacketType.SELECT_COMMAND:
            raise Exception("Invalid constraint field values")
        return SelectCommand(**fields), span

    def serialize(self, payload: bytes = None) -> bytes:
        _span = bytearray()
        return RfPacket.serialize(self, payload = bytes(_span))

    @property
    def size(self) -> int:
        return 0

class DeactivateType(enum.IntEnum):
    IDLE_MODE = 0x0
    SLEEP_MODE = 0x1
    SLEEP_AF_MODE = 0x2
    DISCOVERY = 0x3

    @staticmethod
    def from_int(v: int) -> Union[int, 'DeactivateType']:
        try:
            return DeactivateType(v)
        except ValueError as exn:
            raise exn


class DeactivateReason(enum.IntEnum):
    DH_REQUEST = 0x0
    ENDPOINT_REQUEST = 0x1
    RF_LINK_LOSS = 0x2
    NFC_B_BAD_AFI = 0x3
    DH_REQUEST_FAILED = 0x4

    @staticmethod
    def from_int(v: int) -> Union[int, 'DeactivateReason']:
        try:
            return DeactivateReason(v)
        except ValueError as exn:
            raise exn


@dataclass
class DeactivateNotification(RfPacket):
    type_: DeactivateType = field(kw_only=True, default=DeactivateType.IDLE_MODE)
    reason: DeactivateReason = field(kw_only=True, default=DeactivateReason.DH_REQUEST)

    def __post_init__(self):
        self.packet_type = RfPacketType.DEACTIVATE_NOTIFICATION

    @staticmethod
    def parse(fields: dict, span: bytes) -> Tuple['DeactivateNotification', bytes]:
        if fields['packet_type'] != RfPacketType.DEACTIVATE_NOTIFICATION:
            raise Exception("Invalid constraint field values")
        if len(span) < 2:
            raise Exception('Invalid packet size')
        fields['type_'] = DeactivateType.from_int(span[0])
        fields['reason'] = DeactivateReason.from_int(span[1])
        span = span[2:]
        return DeactivateNotification(**fields), span

    def serialize(self, payload: bytes = None) -> bytes:
        _span = bytearray()
        _span.append((self.type_ << 0))
        _span.append((self.reason << 0))
        return RfPacket.serialize(self, payload = bytes(_span))

    @property
    def size(self) -> int:
        return 2

@dataclass
class Data(RfPacket):
    data: bytearray = field(kw_only=True, default_factory=bytearray)

    def __post_init__(self):
        self.packet_type = RfPacketType.DATA

    @staticmethod
    def parse(fields: dict, span: bytes) -> Tuple['Data', bytes]:
        if fields['packet_type'] != RfPacketType.DATA:
            raise Exception("Invalid constraint field values")
        fields['data'] = list(span)
        span = bytes()
        return Data(**fields), span

    def serialize(self, payload: bytes = None) -> bytes:
        _span = bytearray()
        _span.extend(self.data)
        return RfPacket.serialize(self, payload = bytes(_span))

    @property
    def size(self) -> int:
        return len(self.data) * 1
