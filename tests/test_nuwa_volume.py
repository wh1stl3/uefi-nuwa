"""Execute the exact firmware envelope parser as host C (not device validation)."""
import ctypes
import pathlib
import random
import struct
import subprocess
import tempfile
import unittest
import zlib

ROOT = pathlib.Path(__file__).resolve().parents[1]
MAGIC = bytes.fromhex('4e5557412d564f4ca3719c4e82d650b9')


def envelope(data):
    return MAGIC + struct.pack('<IIQI28x', 1, 64, len(data), zlib.crc32(data)) + data


class ParserTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        path = pathlib.Path(cls.tmp.name)
        header = ROOT / 'Platforms/Xiaomi/nuwaPkg/Drivers/NuwaBootMemoryDxe/NuwaVolume.h'
        source = '''#include <stdint.h>
#include <stddef.h>
typedef uint8_t UINT8;
typedef uint32_t UINT32;
typedef uint64_t UINT64;
typedef size_t UINTN;
typedef int BOOLEAN;
#define STATIC static
#define CONST const
#define TRUE 1
#define FALSE 0
#include "NuwaVolume.h"
static BOOLEAN crc(const UINT8 *p, UINTN n, UINT32 *out) {
  UINT32 c = ~0U;
  for (UINTN i=0;i<n;i++) {
    c ^= p[i];
    for (int bit=0;bit<8;bit++) c=(c>>1)^((0U-(c&1))&0xedb88320U);
  }
  *out=~c; return TRUE;
}
int parse(const UINT8 *p, UINTN n, UINTN *off, UINTN *len) {
  return NuwaFindVolume(p,n,crc,off,len);
}
'''
        (path / 'test.c').write_text(source)
        subprocess.run(['cc', '-shared', '-fPIC', '-O2', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=undefined', '-I', str(header.parent),
                        str(path / 'test.c'), '-o', str(path / 'test.so')], check=True)
        cls.lib = ctypes.CDLL(str(path / 'test.so'))
        cls.lib.parse.argtypes = [ctypes.c_void_p, ctypes.c_size_t,
                                 ctypes.POINTER(ctypes.c_size_t), ctypes.POINTER(ctypes.c_size_t)]
        cls.lib.parse.restype = ctypes.c_int

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def parse(self, data):
        off, size = ctypes.c_size_t(), ctypes.c_size_t()
        result = self.lib.parse(ctypes.create_string_buffer(data), len(data),
                                ctypes.byref(off), ctypes.byref(size))
        return result, off.value, size.value

    def test_unaligned_vendor_prefix_and_bootconfig_suffix(self):
        data = bytes(range(256)) * 4
        prefix = b'vendor' * 37 + b'x'
        self.assertEqual(self.parse(prefix + envelope(data) + b'bootconfig#BOOTCONFIG\n'),
                         (1, len(prefix) + 64, len(data)))

    def test_absent_empty_and_short(self):
        for data in (b'', b'x' * 1000, MAGIC[:15], b'FAT32   ' * 100):
            self.assertEqual(self.parse(data)[0], 0)

    def test_all_truncations(self):
        data = envelope(b'x' * 512)
        for length in range(len(data)):
            self.assertNotEqual(self.parse(data[:length])[0], 1)

    def test_overflow_zero_nonsector_and_bad_version(self):
        data = bytearray(envelope(b'x' * 512))
        for length in (0, 1, 511, 513, 1024, 2**64 - 1):
            struct.pack_into('<Q', data, 24, length)
            self.assertEqual(self.parse(bytes(data))[0], 2)
        for offset, value in ((16, 2), (20, 63), (36, 1)):
            data = bytearray(envelope(b'x' * 512)); data[offset] = value
            self.assertEqual(self.parse(bytes(data))[0], 2)

    def test_corruption_and_duplicate(self):
        data = bytearray(envelope(b'x' * 512)); data[-1] ^= 1
        self.assertEqual(self.parse(bytes(data))[0], 2)
        data = envelope(b'x' * 512)
        self.assertEqual(self.parse(data + b'prefix' + data)[0], 3)

    def test_magic_inside_file_data(self):
        data = (MAGIC + b'file data').ljust(512, b'\0')
        self.assertEqual(self.parse(envelope(data)), (1, 64, 512))

    def test_random_placement(self):
        rng = random.Random(8550)
        for _ in range(200):
            prefix = rng.randbytes(rng.randrange(2048))
            data = rng.randbytes(512 * rng.randrange(1, 9))
            suffix = rng.randbytes(rng.randrange(128))
            self.assertEqual(self.parse(prefix + envelope(data) + suffix),
                             (1, len(prefix) + 64, len(data)))


if __name__ == '__main__':
    unittest.main()
