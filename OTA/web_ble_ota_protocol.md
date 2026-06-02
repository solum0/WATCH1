# Web BLE OTA Protocol

This document matches `web_ble_ota.html`.

## Browser support

Use Chrome or Edge with Web Bluetooth support. The page must be opened from
HTTPS or from `localhost`. Android Chrome supports Web Bluetooth, so an Android
phone can be used. iPhone Chrome cannot use Web Bluetooth because iOS browsers
use WebKit and do not expose this API.

## Default GATT UUIDs

Change these in the web page if the APP uses different UUIDs.

| Item | UUID |
| --- | --- |
| Service | `0000fff0-0000-1000-8000-00805f9b34fb` |
| Write characteristic | `0000fff3-0000-1000-8000-00805f9b34fb` |
| Notify characteristic | `0000fff1-0000-1000-8000-00805f9b34fb` |

The write characteristic receives frames from the browser. The notify
characteristic sends ACK frames to the browser.

## Web page transfer defaults

The page defaults to `payload_size=64`, `writeChunk=17`, and `chunkDelay=20ms`
in compatibility mode. This keeps each BLE write below the 18/20 byte edge that
can make some KT6328A UART-transparent modules drop bytes near the end of a
transparent write burst. DATA frames may still be larger than one BLE write; the
APP side receives the UART byte stream and reconstructs frames. The browser also
appends `0x55` padding after each OTA frame. Padding is sized dynamically so the
final BLE write contains only padding; if the module truncates the tail of the
burst, it drops padding instead of DATA CRC bytes.

## CRC32

The browser uses the same CRC32 parameters as the current bootloader:

| Parameter | Value |
| --- | --- |
| Polynomial | `0xEDB88320` |
| Init | `0xFFFFFFFF` |
| Final XOR | `0xFFFFFFFF` |
| Input | byte stream, reflected/software style |

## Frame format

All integer fields are little-endian.

### START

Sent once before data transfer.

| Offset | Size | Field |
| --- | ---: | --- |
| 0 | 1 | frame type `0x01` |
| 1 | 4 | magic ASCII `OTA1` |
| 5 | 4 | `image_size` |
| 9 | 4 | `image_crc` |
| 13 | 4 | `version` |
| 17 | 2 | `payload_size` |

APP side should check:

- magic is `OTA1`
- `image_size > 0`
- `image_size <= 0x20000`
- `payload_size <= 480`
- `version >= confirmed_version`; otherwise the device replies with status `8`

After accepting START, the APP should erase download sector 6
`0x08040000` to `0x0805FFFF`, then ACK START.

### DATA

Sent repeatedly. The browser waits for an ACK before sending the next packet.

| Offset | Size | Field |
| --- | ---: | --- |
| 0 | 1 | frame type `0x02` |
| 1 | 2 | `seq` |
| 3 | 4 | `offset` |
| 7 | 2 | `len` |
| 9 | `len` | payload bytes |
| 9 + len | 4 | payload CRC32 |

APP side writes the payload to:

```text
0x08040000 + offset
```

After Flash write succeeds and payload CRC matches, ACK DATA.

### END

Sent after all DATA packets.

| Offset | Size | Field |
| --- | ---: | --- |
| 0 | 1 | frame type `0x03` |
| 1 | 4 | `image_size` |
| 5 | 4 | `image_crc` |

APP side should calculate CRC32 for `image_size` bytes from `0x08040000`.
Only after this matches `image_crc`, write boot metadata at `0x08010000`:

```text
magic         = 0x42545731
version       = 1
state         = 0xA55A0001
image_size    = app.bin size
image_crc     = app.bin CRC32
image_version = firmware version
boot_count    = 0
reserved[0]   = confirmed_version
reserved[1]   = previous_version or 0xFFFFFFFF
reserved[2..8]= 0xFFFFFFFF
```

Then ACK END and call `NVIC_SystemReset()`.

## ACK

Sent by notify characteristic.

| Offset | Size | Field |
| --- | ---: | --- |
| 0 | 1 | frame type `0x79` |
| 1 | 1 | acknowledged frame type: `0x01`, `0x02`, or `0x03` |
| 2 | 1 | status, `0` means success |
| 3 | 2 | `seq`, use `0` for START and END |
| 5 | 4 | next expected offset or current offset |

Suggested non-zero status values:

| Status | Meaning |
| --- | --- |
| `1` | bad magic |
| `2` | bad size |
| `3` | bad sequence |
| `4` | bad offset |
| `5` | bad packet CRC |
| `6` | Flash erase/write failed |
| `7` | full image CRC failed |
| `8` | version rejected |
| `9` | RX frame timeout |
