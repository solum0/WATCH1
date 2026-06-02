#!/usr/bin/env python3
import argparse
import binascii
import re
import struct
from pathlib import Path


BOOT_META_ADDR = 0x08010000
BOOT_DOWNLOAD_ADDR = 0x08040000

BOOT_META_MAGIC = 0x42545731
BOOT_META_VERSION = 0x00000001
BOOT_FLAG_UPDATE_PENDING = 0xA55A0001


def read_app_version(header_path):
    text = header_path.read_text(encoding="utf-8")
    values = {}

    for name in ("APP_FW_VERSION_MAJOR", "APP_FW_VERSION_MINOR", "APP_FW_VERSION_PATCH"):
        match = re.search(rf"#define\s+{name}\s+([0-9A-Fa-fx]+)U?", text)
        if not match:
            raise ValueError(f"Cannot find {name} in {header_path}")
        values[name] = int(match.group(1), 0)

    return ((values["APP_FW_VERSION_MAJOR"] << 16) |
            (values["APP_FW_VERSION_MINOR"] << 8) |
            values["APP_FW_VERSION_PATCH"])


def intel_hex_record(addr, record_type, data):
    count = len(data)
    body = bytes([count, (addr >> 8) & 0xFF, addr & 0xFF, record_type]) + data
    checksum = ((~sum(body) + 1) & 0xFF)
    return ":" + body.hex().upper() + f"{checksum:02X}"


def emit_intel_hex(segments):
    lines = []
    active_upper = None

    for base_addr, data in segments:
        offset = 0
        while offset < len(data):
            absolute = base_addr + offset
            upper = absolute >> 16
            if upper != active_upper:
                lines.append(intel_hex_record(0, 0x04, struct.pack(">H", upper)))
                active_upper = upper

            low = absolute & 0xFFFF
            chunk = data[offset:offset + min(16, 0x10000 - low)]
            lines.append(intel_hex_record(low, 0x00, chunk))
            offset += len(chunk)

    lines.append(intel_hex_record(0, 0x01, b""))
    return "\n".join(lines) + "\n"


def make_meta(app_data, image_version):
    image_crc = binascii.crc32(app_data) & 0xFFFFFFFF
    words = [
        BOOT_META_MAGIC,
        BOOT_META_VERSION,
        BOOT_FLAG_UPDATE_PENDING,
        len(app_data),
        image_crc,
        image_version,
        0,
    ] + ([0xFFFFFFFF] * 9)

    return struct.pack("<16I", *words), image_crc


def main():
    parser = argparse.ArgumentParser(description="Build bootloader update metadata and HEX.")
    parser.add_argument("--app", default="Objects/APP/app.bin", help="APP bin path")
    parser.add_argument("--out-dir", default="Objects/Update", help="Output directory")
    parser.add_argument("--version", type=lambda x: int(x, 0), default=None,
                        help="Image version. Defaults to User/app_version.h")
    parser.add_argument("--version-header", default="User/app_version.h",
                        help="APP version header path")
    args = parser.parse_args()

    app_path = Path(args.app)
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    app_data = app_path.read_bytes()
    image_version = args.version
    if image_version is None:
        image_version = read_app_version(Path(args.version_header))
    meta_data, image_crc = make_meta(app_data, image_version)

    meta_path = out_dir / "app_meta.bin"
    download_bin_path = out_dir / "app_download.bin"
    update_hex_path = out_dir / "app_update.hex"

    meta_path.write_bytes(meta_data)
    download_bin_path.write_bytes(app_data)
    update_hex_path.write_text(
        emit_intel_hex([
            (BOOT_DOWNLOAD_ADDR, app_data),
            (BOOT_META_ADDR, meta_data),
        ]),
        encoding="ascii",
    )

    print(f"APP: {app_path}")
    print(f"version: 0x{image_version:08X}")
    print(f"size: {len(app_data)} bytes")
    print(f"crc32: 0x{image_crc:08X}")
    print(f"meta: {meta_path} @ 0x{BOOT_META_ADDR:08X}")
    print(f"download bin: {download_bin_path} @ 0x{BOOT_DOWNLOAD_ADDR:08X}")
    print(f"update hex: {update_hex_path}")


if __name__ == "__main__":
    main()
