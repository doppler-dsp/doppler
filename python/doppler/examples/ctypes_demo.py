#!/usr/bin/env python3
"""
Direct ctypes demo for the doppler C library.

Shows how to drive libdoppler.so from raw Python ctypes without
the doppler Python extension.  Useful as a reference for embedding
the library in environments where the extension cannot be built.

For production use, prefer the doppler Python package:
    from doppler import Publisher, Subscriber, CF64
"""

from ctypes import (
    CDLL,
    POINTER,
    Structure,
    byref,
    c_char_p,
    c_double,
    c_int,
    c_int32,
    c_size_t,
    c_uint32,
    c_uint64,
    c_void_p,
    cast,
)
import time

import numpy as np

# ---------------------------------------------------------------------------
# Load shared library
# ---------------------------------------------------------------------------

try:
    libdoppler = CDLL("./libdoppler.so")
except OSError:
    libdoppler = CDLL("/usr/local/lib/libdoppler.so")

# ---------------------------------------------------------------------------
# Constants (mirror dp_sample_type_t and error codes from dp/stream.h)
# ---------------------------------------------------------------------------

DP_CI32 = 0
DP_CF64 = 1
DP_CF128 = 2

DP_OK = 0
DP_ERR_INIT = -1
DP_ERR_SEND = -2
DP_ERR_RECV = -3
DP_ERR_INVALID = -4

# ---------------------------------------------------------------------------
# C structures
# ---------------------------------------------------------------------------


class CI32(Structure):
    _fields_ = [("i", c_int32), ("q", c_int32)]


class CF64(Structure):
    _fields_ = [("i", c_double), ("q", c_double)]


class Header(Structure):
    _fields_ = [
        ("magic", c_uint32),
        ("version", c_uint32),
        ("sample_type", c_uint32),
        ("sequence", c_uint64),
        ("timestamp_ns", c_uint64),
        ("sample_rate", c_double),
        ("center_freq", c_double),
        ("num_samples", c_uint64),
        ("reserved", c_uint64 * 4),
    ]


# ---------------------------------------------------------------------------
# Function prototypes
# ---------------------------------------------------------------------------

libdoppler.dp_pub_create.argtypes = [c_char_p, c_int]
libdoppler.dp_pub_create.restype = c_void_p

libdoppler.dp_pub_send_cf64.argtypes = [
    c_void_p,
    POINTER(CF64),
    c_size_t,
    c_double,
    c_double,
]
libdoppler.dp_pub_send_cf64.restype = c_int

libdoppler.dp_pub_send_ci32.argtypes = [
    c_void_p,
    POINTER(CI32),
    c_size_t,
    c_double,
    c_double,
]
libdoppler.dp_pub_send_ci32.restype = c_int

libdoppler.dp_pub_destroy.argtypes = [c_void_p]
libdoppler.dp_pub_destroy.restype = None

libdoppler.dp_sub_create.argtypes = [c_char_p]
libdoppler.dp_sub_create.restype = c_void_p

libdoppler.dp_sub_recv.argtypes = [
    c_void_p,
    POINTER(c_void_p),
    POINTER(c_size_t),
    POINTER(c_int),
    POINTER(Header),
]
libdoppler.dp_sub_recv.restype = c_int

libdoppler.dp_sub_free_samples.argtypes = [c_void_p]
libdoppler.dp_sub_free_samples.restype = None

libdoppler.dp_sub_destroy.argtypes = [c_void_p]
libdoppler.dp_sub_destroy.restype = None


# ---------------------------------------------------------------------------
# Python wrappers
# ---------------------------------------------------------------------------


class DopplerPublisher:
    """ctypes-based PUB socket wrapper."""

    def __init__(self, endpoint, sample_type=DP_CF64):
        self.ctx = libdoppler.dp_pub_create(endpoint.encode(), sample_type)
        if not self.ctx:
            raise RuntimeError("Failed to create publisher")
        self.sample_type = sample_type

    def send_cf64(self, i_data, q_data, sample_rate, center_freq):
        """Send CF64 samples from separate I and Q numpy arrays."""
        if len(i_data) != len(q_data):
            raise ValueError("I and Q arrays must have the same length")
        n = len(i_data)
        buf = (CF64 * n)()
        for idx in range(n):
            buf[idx].i = i_data[idx]
            buf[idx].q = q_data[idx]
        rc = libdoppler.dp_pub_send_cf64(self.ctx, buf, n, sample_rate, center_freq)
        if rc != DP_OK:
            raise RuntimeError(f"Send failed: {rc}")

    def send_ci32(self, i_data, q_data, sample_rate, center_freq):
        """Send CI32 samples from separate I and Q numpy arrays."""
        if len(i_data) != len(q_data):
            raise ValueError("I and Q arrays must have the same length")
        n = len(i_data)
        buf = (CI32 * n)()
        for idx in range(n):
            buf[idx].i = int(i_data[idx])
            buf[idx].q = int(q_data[idx])
        rc = libdoppler.dp_pub_send_ci32(self.ctx, buf, n, sample_rate, center_freq)
        if rc != DP_OK:
            raise RuntimeError(f"Send failed: {rc}")

    def __del__(self):
        if hasattr(self, "ctx") and self.ctx:
            libdoppler.dp_pub_destroy(self.ctx)


class DopplerSubscriber:
    """ctypes-based SUB socket wrapper."""

    def __init__(self, endpoint):
        self.ctx = libdoppler.dp_sub_create(endpoint.encode())
        if not self.ctx:
            raise RuntimeError("Failed to create subscriber")

    def recv(self):
        """Receive samples. Returns (i_array, q_array, header)."""
        samples_ptr = c_void_p()
        num_samples = c_size_t()
        sample_type = c_int()
        header = Header()

        rc = libdoppler.dp_sub_recv(
            self.ctx,
            byref(samples_ptr),
            byref(num_samples),
            byref(sample_type),
            byref(header),
        )
        if rc != DP_OK:
            raise RuntimeError(f"Receive failed: {rc}")

        n = num_samples.value

        if sample_type.value == DP_CF64:
            buf = cast(samples_ptr, POINTER(CF64))
            i_arr = np.array([buf[idx].i for idx in range(n)])
            q_arr = np.array([buf[idx].q for idx in range(n)])
        elif sample_type.value == DP_CI32:
            buf = cast(samples_ptr, POINTER(CI32))
            i_arr = np.array([buf[idx].i for idx in range(n)], dtype=np.int32)
            q_arr = np.array([buf[idx].q for idx in range(n)], dtype=np.int32)
        else:
            libdoppler.dp_sub_free_samples(samples_ptr)
            raise ValueError(f"Unknown sample type: {sample_type.value}")

        libdoppler.dp_sub_free_samples(samples_ptr)
        return i_arr, q_arr, header

    def __del__(self):
        if hasattr(self, "ctx") and self.ctx:
            libdoppler.dp_sub_destroy(self.ctx)


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    import sys

    if len(sys.argv) < 2 or sys.argv[1] not in ("tx", "rx"):
        print("Usage: python3 ctypes_demo.py [tx|rx]")
        sys.exit(1)

    mode = sys.argv[1]

    if mode == "tx":
        print("Python Transmitter (ctypes)")
        print("===========================")

        pub = DopplerPublisher("tcp://*:5555", DP_CF64)

        sample_rate = 1e6
        center_freq = 2.4e9
        signal_freq = 10_000.0
        buffer_size = 8192

        print(f"Sample Rate: {sample_rate / 1e6:.2f} MHz")
        print(f"Center Freq: {center_freq / 1e9:.2f} GHz")
        print(f"Signal Freq: {signal_freq / 1e3:.2f} kHz")
        print("\nTransmitting... Press Ctrl+C to stop\n")

        phase = 0.0
        count = 0
        try:
            while True:
                t = np.arange(buffer_size) / sample_rate
                angle = 2.0 * np.pi * signal_freq * t + phase
                pub.send_cf64(np.cos(angle), np.sin(angle), sample_rate, center_freq)
                phase = (
                    phase + 2.0 * np.pi * signal_freq * buffer_size / sample_rate
                ) % (2.0 * np.pi)
                count += 1
                if count % 100 == 0:
                    print(f"Sent {count * buffer_size} samples\r", end="")
                time.sleep(0.008)
        except KeyboardInterrupt:
            print("\n\nStopping...")

    else:
        print("Python Receiver (ctypes)")
        print("========================")

        sub = DopplerSubscriber("tcp://localhost:5555")
        print("Waiting for data... Press Ctrl+C to stop\n")

        count = 0
        try:
            while True:
                i_samples, q_samples, header = sub.recv()
                power = np.mean(i_samples**2 + q_samples**2)
                power_db = 10.0 * np.log10(power + 1e-12)
                if count == 0:
                    print(f"Sample Rate: {header.sample_rate / 1e6:.2f} MHz")
                    print(f"Center Freq: {header.center_freq / 1e9:.2f} GHz")
                    print(f"Samples:     {len(i_samples)}")
                    print()
                count += 1
                if count % 10 == 0:
                    print(
                        f"Packets: {count:6d} | Power: {power_db:6.2f} dB\r",
                        end="",
                    )
        except KeyboardInterrupt:
            print("\n\nStopping...")
        except Exception as exc:
            print(f"\nError: {exc}")
