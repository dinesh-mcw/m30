"""
file: device.py

Copyright (C) 2023 Lumotive, Inc. All rights reserved.

This file provides a driver for the I2C bus, an I2C Device
that will utilized the bus with various byte packing and
endianness and a Device class to connect
an FPGA YAML memory map to FPGA peripherals.
All FPGA peripherals inherit from the Device class.
"""
import contextlib
import io
import os
import time
import struct
from threading import Lock
from typing import ContextManager
import usb.core
import usb.util
import sys
import ctypes

try:
    import fcntl
except (ModuleNotFoundError, OSError):
    pass

import Pyro5.api

from cobra_system_control.cobra_log import log

current_dir = os.path.dirname(os.path.abspath(__file__))
lib = ctypes.CDLL(os.path.join(current_dir, 'libfx3_transfer.so'))
ctx = ctypes.POINTER(ctypes.c_void_p)()
handle = ctypes.POINTER(ctypes.c_void_p)()

STRUCT_MAP = {
    1: 'B',
    2: 'H',
    4: 'I',
    8: 'Q',
}

# class I2CBus:
#     """A class to interface with the CPU I2C bus
#     for serial communication.
#     """
#     def __init__(self, bus: int):
#         self.resource = f'{bus}'
#         self.lock = Lock()
#         self.fw = None
#         self.fr = None
#         self._is_connected = False
#         self.ioctl_i2c_slave = 0x0703

#     def connect(self):
#         if not self._is_connected:
#             # pass
#             # self.fw = io.open(self.resource, "wb", buffering=0)
#             # self.fr = io.open(self.resource, "rb", buffering=0)
#             self._is_connected = True

#     def write(self, device_addr: int, ba: bytearray):
#         return
#         with self.lock:
#             return
#             fcntl.ioctl(self.fw, self.ioctl_i2c_slave, device_addr)
#             try:
#                 self.fw.write(ba)
#             except OSError:
#                 return

#     def read(self, device_addr: int, ba: bytearray, nbytes: int) -> bytearray:
#         return
#         with self.lock:
#             fcntl.ioctl(self.fw, self.ioctl_i2c_slave, device_addr)
#             fcntl.ioctl(self.fr, self.ioctl_i2c_slave, device_addr)
#             try:
#                 self.fw.write(ba)
#                 ret = self.fr.read(nbytes)
#             except OSError as e:
#                 print('i2c read error', e)
#                 return
#         return ret

#     def disconnect(self):
#         if self.fw is not None:
#             self.fw.close()
#         if self.fr is not None:
#             self.fr.close()
#         self._is_connected = False


# class I2CDevice:
#     """A class to define a device that uses the I2C Bus with
#     a given data format.
#     """
#     def __init__(self, bus: 'I2CBus',
#                  device_addr: int, addr_nbytes: int, data_nbytes: int,
#                  addr_bigendian: bool, data_bigendian: bool):
#         self.bus = bus
#         self.device_addr = device_addr
#         self.addr_nbytes = addr_nbytes
#         self.data_nbytes = data_nbytes
#         addr_endian = ">" if addr_bigendian else "<" 
#         data_endian = ">" if data_bigendian else "<"
#         self.addr_pack = f'{addr_endian}{STRUCT_MAP[addr_nbytes]}'
#         self.data_pack = f'{data_endian}{STRUCT_MAP[data_nbytes]}'

#     def write(self, addr: int, data: int):
        # ba = bytearray([
        #     *struct.pack(self.addr_pack, addr),
        #     *struct.pack(self.data_pack, data)
        # ])
#         self.bus.write(self.device_addr, ba)

#     def read(self, addr: int) -> int:
#         # ba = bytearray([*struct.pack(self.addr_pack, addr)])
#         ba = bytearray(self.data_nbytes)
#         # ba = self.bus.read(self.device_addr, ba, self.data_nbytes)
#         return struct.unpack(self.data_pack, ba)[0]

#     def write_bytes(self, addr: int, data: bytearray):
#         ba = bytearray([
#             *struct.pack(self.addr_pack, addr),
#             *data
#         ])
#         self.bus.write(self.device_addr, ba)

#     def read_bytes(self, addr: int, length: int, inc_addr: bool = True) -> int:
#         MAX_READ = 256
#         read_bytes = bytearray()
#         while length > 0:
#             ba = bytearray([*struct.pack(self.addr_pack, addr)])
#             bytes_to_read = MAX_READ if length > MAX_READ else length
#             rb = self.bus.read(self.device_addr, ba, bytes_to_read)
#             read_bytes = rb
#             if inc_addr:
#                 addr += bytes_to_read
#             length -= bytes_to_read
#         return read_bytes


# @Pyro5.api.behavior(instance_mode='single')
# @Pyro5.api.expose
# class Device:
#     """A class to define a Device that communicates over the
#     I2C Bus using a defined memory map.
#     """
#     def __init__(self, bus: 'I2CBus', device_addr: int,
#                  addr_bytes: int, data_bytes: int,
#                  mmap_periph: 'MemoryMapPeriph',
#                  addr_bigendian: bool, data_bigendian: bool):
#         self.i2c = I2CDevice(bus, device_addr, addr_bytes, data_bytes,
#                              addr_bigendian, data_bigendian)
#         self.periph = mmap_periph

#     def read_all_periph_fields(self, with_print=False):
#         return self.periph.read_all_periph_fields(with_print=with_print)

#     def connect(self):
#         self.i2c.bus.connect()
#         self.periph.register_write_callback(self.i2c.write)
#         self.periph.register_read_callback(self.i2c.read)
#         self.periph.register_readdata_callback(lambda x: x)

#     def write_fields(self, **kwargs):
#         self.periph.write_fields(**kwargs)

#     def read_fields(self, *args, use_mnemonic: bool = False):
#         return self.periph.read_fields(*args, use_mnemonic=use_mnemonic)

#     def get_pos(self, field_name: str) -> int:
#         """Gets the bit position in a word"""
#         return self.periph.fields[field_name].pos

#     def get_offset(self, field_name: str) -> int:
#         """Gets the addr offset of a word"""
#         return self.periph.fields[field_name].offset

#     def get_abs_addr(self, field_name: str) -> int:
#         """Returns the absolute byte address of a word.
#         """
#         return self.addr_base + self.periph.fields[field_name].offset

#     @property
#     def addr_base(self):
#         return self.periph.addr_base

#     def get_size(self, field_name: str) -> int:
#         """Returns the size of the field, in bits"""
#         return self.periph.fields[field_name].size

#     def setup(self):
#         """Hook for subsystem setup

#         Implementation should perform any one-time setup procedures, followed
#         by a call to ``update`` with default settings."""

#     def apply_settings(self, settings=None):
#         """Hook for subsystem hardware update

#         Implementation should update hardware according to settings object."""

#     def enable(self):
#         """Hook for actions taken right before starting scan controller"""

#     def disable(self):
#         """Hook for actions to take right after stopping scan controller
#         E.g., to put the LCM in standby mode."""

#     def disconnect(self):
#         """Hook to disconnect the hardware

#         Implementation should disconnect any hardware that was connected in
#         ``connect``."""

#     def cleanup(self):
#         self.disable()
#         self.disconnect()

#     @classmethod
#     @contextlib.contextmanager
#     def open(cls, *args, **kwargs) -> ContextManager:
#         """Instantiates resource and initializes
#         resource communications within a context block.

#         Appropriately handles exceptions raised while
#         manipulating returned ContextManager object.

#         Args:
#             *args: passed directly to the underlying system constructor
#             *kwargs: passed directly to the underlying system constructor

#         Returns:
#             (ContextManager[LidarResource]) context manager for system

#         Typical use:

#         .. code:: python
#             from cobra_system_control.cobra import Cobra

#             # calls cob.disable() at block end during unhandled exception
#             with Cobra.open() as cob:

#         """
#         system = cls(*args, **kwargs)
#         system.connect()  # connects to hardware
#         system.setup()  # puts system in a good initial state

#         try:
#             yield system
#         except KeyboardInterrupt:
#             log.info('Aborting script early due to keyboard interrupt')
#         except Exception as e:
#             log.error('An unexpected exception was raised: %s', e)
#             raise e
#         finally:
#             system.cleanup()

#     @classmethod
#     def __init_subclass__(cls, **kwargs):
#         # automatically expose all subclasses of Device to Pyro for development
#         cls_ = Pyro5.api.expose(cls)
#         cls_ = Pyro5.api.behavior(instance_mode='single')(cls_)
#         return cls_


# # These are Dummy Classes that should act like
# # a device class but shouldn't do anything
# # Do not need to call super().__init__()
# # Need to accept any number of args for any method call
# # pylint: disable=super-init-not-called
# # pylint: disable=unused-argument
# class DummyObject(Device):
#     """A dummy class that should act like
#     a device class but shouldn't do anything
#     Do not need to call super().__init__()
#     Need to accept any number of args for any method call
#     """
#     def __init__(self, *args, **kwargs):
#         pass

#     def __getattr__(self, attr):
#         def func(*args, **kwargs):
#             pass
#         return func


# class DummyDevice(Device):
#     """A dummy class that should act like
#     a device class but shouldn't do anything
#     Do not need to call super().__init__()
#     Need to accept any number of args for any method call
#     """
#     def __init__(self, *args, **kwargs):
#         # add some nested objects that might get called
#         self.dac = DummyObject()
#         self.bus = DummyObject()
#         self.i2c = DummyObject()

#     def __getattr__(self, attr):
#         def func(*args, **kwargs):
#             pass
#         return func
# pylint: enable=super-init-not-called
# pylint: enable=unused-argument

######################################################################################
####################################################################################
##################################################################################

class USB:
    """A class to interface with the USB device."""
    def __init__(self, vendor_id: int, product_id: int):
        self.vendor_id = vendor_id
        self.product_id = product_id
        self.device = None
        self.lock = Lock()
        self._is_connected = False

    def connect(self):
        if not self._is_connected:
            lib.connectFX3.argtypes = [ctypes.POINTER(ctypes.POINTER(ctypes.c_void_p)), ctypes.POINTER(ctypes.POINTER(ctypes.c_void_p))]
            lib.connectFX3.restype = ctypes.c_int

            if lib.connectFX3(ctypes.byref(ctx), ctypes.byref(handle), self.vendor_id, self.product_id) != 0:
                print("Failed to connect to FX3 device")
                exit(1)

            self._is_connected = True
            
    def write(self, data: bytearray):
        try:
            with self.lock:
                data_buffer = (ctypes.c_ubyte * len(data))(*data)
                bytes_sent = lib.writeData(handle, data_buffer, len(data))

                print(f"Sent {bytes_sent} bytes to FX3")
                time.sleep(1)

        except KeyboardInterrupt:
            log.info("Keyboard interrupt detected. Disconnecting device...")
            self.disconnect()
            sys.exit(0)

    def read(self, length: int = None, ba: bytearray = None) -> bytearray:
        if length is None:
            length = 1
        try:
            with self.lock:
                if ba is None:
                    response_buffer = (ctypes.c_ubyte * length)()
                    bytes_received = lib.readData(handle, response_buffer, length)
                    response = bytes(response_buffer[:bytes_received])
                else:
                    data_buffer = (ctypes.c_ubyte * len(ba))(*ba)
                    bytes_sent = lib.writeData(handle, data_buffer, len(ba))
                    print("buffer_sent")
                    time.sleep(1)

                    response_buffer = (ctypes.c_ubyte * bytes_sent)()
                    bytes_received = lib.readData(handle, response_buffer, bytes_sent)
                    response = bytes(response_buffer[:length])

                print(f"Received data from FX3: {response}")
                time.sleep(1)
        
        except KeyboardInterrupt:
            log.info("Keyboard interrupt detected. Disconnecting device...")
            self.disconnect()
            sys.exit(0)

        return response #bytearray(response)
    
    def calculate_crc16(self, data: bytearray):
        crc = 0x0000
        crc16tab = (0x0000,0x1021,0x2042,0x3063,0x4084,0x50a5,0x60c6,0x70e7,
                    0x8108,0x9129,0xa14a,0xb16b,0xc18c,0xd1ad,0xe1ce,0xf1ef,
                    0x1231,0x0210,0x3273,0x2252,0x52b5,0x4294,0x72f7,0x62d6,
                    0x9339,0x8318,0xb37b,0xa35a,0xd3bd,0xc39c,0xf3ff,0xe3de,
                    0x2462,0x3443,0x0420,0x1401,0x64e6,0x74c7,0x44a4,0x5485,
                    0xa56a,0xb54b,0x8528,0x9509,0xe5ee,0xf5cf,0xc5ac,0xd58d,
                    0x3653,0x2672,0x1611,0x0630,0x76d7,0x66f6,0x5695,0x46b4,
                    0xb75b,0xa77a,0x9719,0x8738,0xf7df,0xe7fe,0xd79d,0xc7bc,
                    0x48c4,0x58e5,0x6886,0x78a7,0x0840,0x1861,0x2802,0x3823,
                    0xc9cc,0xd9ed,0xe98e,0xf9af,0x8948,0x9969,0xa90a,0xb92b,
                    0x5af5,0x4ad4,0x7ab7,0x6a96,0x1a71,0x0a50,0x3a33,0x2a12,
                    0xdbfd,0xcbdc,0xfbbf,0xeb9e,0x9b79,0x8b58,0xbb3b,0xab1a,
                    0x6ca6,0x7c87,0x4ce4,0x5cc5,0x2c22,0x3c03,0x0c60,0x1c41,
                    0xedae,0xfd8f,0xcdec,0xddcd,0xad2a,0xbd0b,0x8d68,0x9d49,
                    0x7e97,0x6eb6,0x5ed5,0x4ef4,0x3e13,0x2e32,0x1e51,0x0e70,
                    0xff9f,0xefbe,0xdfdd,0xcffc,0xbf1b,0xaf3a,0x9f59,0x8f78,
                    0x9188,0x81a9,0xb1ca,0xa1eb,0xd10c,0xc12d,0xf14e,0xe16f,
                    0x1080,0x00a1,0x30c2,0x20e3,0x5004,0x4025,0x7046,0x6067,
                    0x83b9,0x9398,0xa3fb,0xb3da,0xc33d,0xd31c,0xe37f,0xf35e,
                    0x02b1,0x1290,0x22f3,0x32d2,0x4235,0x5214,0x6277,0x7256,
                    0xb5ea,0xa5cb,0x95a8,0x8589,0xf56e,0xe54f,0xd52c,0xc50d,
                    0x34e2,0x24c3,0x14a0,0x0481,0x7466,0x6447,0x5424,0x4405,
                    0xa7db,0xb7fa,0x8799,0x97b8,0xe75f,0xf77e,0xc71d,0xd73c,
                    0x26d3,0x36f2,0x0691,0x16b0,0x6657,0x7676,0x4615,0x5634,
                    0xd94c,0xc96d,0xf90e,0xe92f,0x99c8,0x89e9,0xb98a,0xa9ab,
                    0x5844,0x4865,0x7806,0x6827,0x18c0,0x08e1,0x3882,0x28a3,
                    0xcb7d,0xdb5c,0xeb3f,0xfb1e,0x8bf9,0x9bd8,0xabbb,0xbb9a,
                    0x4a75,0x5a54,0x6a37,0x7a16,0x0af1,0x1ad0,0x2ab3,0x3a92,
                    0xfd2e,0xed0f,0xdd6c,0xcd4d,0xbdaa,0xad8b,0x9de8,0x8dc9,
                    0x7c26,0x6c07,0x5c64,0x4c45,0x3ca2,0x2c83,0x1ce0,0x0cc1,
                    0xef1f,0xff3e,0xcf5d,0xdf7c,0xaf9b,0xbfba,0x8fd9,0x9ff8,
                    0x6e17,0x7e36,0x4e55,0x5e74,0x2e93,0x3eb2,0x0ed1,0x1ef0
                    )
        
        for byte in data:
             crc = (crc<<8) ^ crc16tab[((crc>>8) ^ byte)&0x00FF]

        return (crc & 0xFFFF)

    def disconnect(self):
        if self._is_connected:
            lib.disconnectFX3(ctx, handle)
            self._is_connected = False
        else:
            print("No device connected.")

class USBDevice:
    def __init__(self, usb_device: 'USB', 
                 device_addr: int, addr_nbytes: int, data_nbytes: int,
                 addr_bigendian: bool, data_bigendian: bool):
        self.device = usb_device
        self.device_addr = device_addr
        self.addr_nbytes = addr_nbytes
        self.data_nbytes = data_nbytes
        addr_endian = ">" if addr_bigendian else "<" 
        data_endian = ">" if data_bigendian else "<"
        self.addr_pack = f'{addr_endian}{STRUCT_MAP[addr_nbytes]}'
        self.data_pack = f'{data_endian}{STRUCT_MAP[data_nbytes]}'

    def write(self, addr: int, data: int):
        ba = bytearray([
            *struct.pack(self.addr_pack, addr),
            *struct.pack(self.data_pack, data)
        ])
        # self.device.write(ba)

    def read(self, addr: int) -> int:
        # ba = bytearray([*struct.pack(self.addr_pack, addr)])
        ba = bytearray(self.data_nbytes)
        # ba = self.device.read(self.data_nbytes, ba)
        return struct.unpack(self.data_pack, ba)[0]

    def write_bytes(self, addr: int, data: bytearray):
        ba = bytearray([
            *struct.pack(self.addr_pack, addr),
            *data
        ])
        self.device.write(ba)

    def read_bytes(self, addr: int, length: int, inc_addr: bool = True) -> bytearray:
        MAX_READ = 512
        read_bytes = bytearray()
        while length > 0:
            bytes_to_read = MAX_READ if length > MAX_READ else length
            rb = self.device.read(bytes_to_read)
            read_bytes.extend(rb)
            if inc_addr:
                addr += bytes_to_read
            length -= bytes_to_read
        return read_bytes
    
@Pyro5.api.behavior(instance_mode='single')
@Pyro5.api.expose
class Device:
    """A class to define a Device that communicates over USB using a memory map."""
    def __init__(self, usb: 'USB', device_addr: int,
                 addr_bytes: int, data_bytes: int,
                 mmap_periph: 'MemoryMapPeriph',
                 addr_bigendian: bool, data_bigendian: bool):
        self.usb = USBDevice(usb, device_addr, addr_bytes, 
                             data_bytes, addr_bigendian, data_bigendian)
        self.periph = mmap_periph

    def read_all_periph_fields(self, with_print=False):
        return self.periph.read_all_periph_fields(with_print=with_print)

    def connect(self):
        """Connect to the USB device and set up the memory map."""
        self.usb.device.connect()
        self.periph.register_write_callback(self.usb.write)
        self.periph.register_read_callback(self.usb.read)
        self.periph.register_readdata_callback(lambda x: x)

    def write_fields(self, **kwargs):
        self.periph.write_fields(**kwargs)

    def read_fields(self, *args, use_mnemonic: bool = False):
        return self.periph.read_fields(*args, use_mnemonic=use_mnemonic)

    def get_pos(self, field_name: str) -> int:
        """Gets the bit position in a word."""
        return self.periph.fields[field_name].pos

    def get_offset(self, field_name: str) -> int:
        """Gets the addr offset of a word."""
        return self.periph.fields[field_name].offset

    def get_abs_addr(self, field_name: str) -> int:
        """Returns the absolute byte address of a word."""
        return self.addr_base + self.periph.fields[field_name].offset

    @property
    def addr_base(self):
        return self.periph.addr_base

    def get_size(self, field_name: str) -> int:
        """Returns the size of the field, in bits."""
        return self.periph.fields[field_name].size

    def setup(self):
        """Hook for subsystem setup."""
        pass

    def apply_settings(self, settings=None):
        """Hook for subsystem hardware update."""
        pass

    def enable(self):
        """Hook for actions taken before starting the scan controller."""
        pass

    def disable(self):
        """Hook for actions taken after stopping the scan controller."""
        pass

    def disconnect(self):
        """Hook to disconnect the hardware."""
        pass

    def cleanup(self):
        self.disable()
        self.disconnect()

    @classmethod
    @contextlib.contextmanager
    def open(cls, *args, **kwargs) -> ContextManager:
        """Instantiates resource and initializes resource communications."""
        system = cls(*args, **kwargs)
        system.connect()
        system.setup()
        try:
            yield system
        except KeyboardInterrupt:
            log.info('Aborting script early due to keyboard interrupt')
        except Exception as e:
            log.error('An unexpected exception was raised: %s', e)
            raise e
        finally:
            system.cleanup()

    @classmethod
    def __init_subclass__(cls, **kwargs):
        cls_ = Pyro5.api.expose(cls)
        cls_ = Pyro5.api.behavior(instance_mode='single')(cls_)
        return cls_