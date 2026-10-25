# Visor HIK

Live viewer for **Hikvision** DVRs, NVRs and cameras on Linux, with support for the **Hik-Connect stream encryption key** (verification code).

[Versión en español](README.md)

## Why

When stream encryption is enabled on a Hikvision device, there is almost no way to watch the cameras on Linux:

- The device **web interface** has no field for the encryption key.
- **RTSP** with ffmpeg/VLC shows a gray image because key frames arrive encrypted.
- **iVMS-4200** no longer has a Linux version.
- RTSP-based Docker solutions have the same problem.

This viewer uses the official Hikvision SDK (HCNetSDK + PlayCtrl), which decrypts the video with the key.

## Features

- First-run setup form: IP, port, user, password and optional encryption key, with a connection test button.
- Automatic camera detection (analog channels and online IP channels).
- Grid view of all cameras using the substream.
- **Double click** a camera to view it full size with the main stream; double click or `Esc` to go back.
- **Digital zoom** with the mouse wheel, centered on the cursor; the zoom level briefly shows in the camera corner.
- Left-drag to pan while zoomed; middle click resets zoom.
- **Controls that appear when moving the mouse** and hide by themselves, with a tooltip on each button:
  - Top bar: connection status, grid quality (SD/HD), previous/next in full view and automatic tour.
  - Per-camera bar: zoom, SD/HD quality for that camera, snapshot and full view.
- **Automatic tour**: shows each camera full size every 5, 10, 30 or 60 seconds.
- **Snapshots** as JPG of the current frame, saved in `~/Pictures/visor-hik/`.
- **Automatic reconnection** when the network drops or the device reboots; cameras without video show "Sin señal" (no signal) or "Reconectando..." (reconnecting).
- Password and encryption key are stored in the **system keyring** (GNOME Keyring, KWallet).

![Screenshot](docs/captura.png)

## Requirements

- 64-bit Linux (x86_64)
- X11 session, or Wayland with XWayland (the program forces X11)
- `g++`, `make`, `pkg-config`, GTK 3 and Xlib

```bash
sudo apt install build-essential pkg-config libgtk-3-dev libx11-dev
```

## Installation

1. Download **Device Network SDK (for Linux 64-bit)** from the official Hikvision website. The SDK is **not included** in this repository because it has its own license.

**Hikvision → Support → Tools → Integration SDKs → Device Network SDK_Linux64**

https://www.hikvision.com/en/support/tools/hitools/?type=IP

2. Clone this repository and copy the SDK `incEn` and `lib` folders into `sdk/`:

```bash
git clone https://github.com/YOUR_USER/visor-hikvision-linux.git
cd visor-hikvision-linux
unzip ~/Downloads/EN-HCNetSDK*_linux64.zip -d /tmp/hiksdk
mkdir sdk
cp -r /tmp/hiksdk/*/incEn /tmp/hiksdk/*/lib sdk/
```

3. Build and run:

```bash
make
./visor-hik
```

Use `make SDK=/path/to/sdk` if the SDK is elsewhere.

4. Optionally add it to your applications menu:

```bash
make instalar      # launcher and icon in ~/.local/share
make desinstalar   # removes them
```

The icon is `icono/visor-hik.svg`; replace it with your own SVG using the same name and run `make instalar` again. The launcher points to this folder, so don't move it after installing.

## Usage

```bash
./visor-hik            # open with saved settings
./visor-hik --config   # show the setup form again
./visor-hik --debug    # print device and channel information
```

Move the mouse to show the controls; hover a button to see what it does. Snapshots are taken from the decoded video, so in the grid they have the substream resolution; take them in full view or with the camera in HD for more detail.

Settings are stored in `~/.config/visor-hik/config.ini` with `600` permissions. The password and encryption key go to the system keyring (Secret Service: GNOME Keyring, KWallet) through `libsecret`, which is loaded at runtime and not needed to build. If no keyring is available they are stored in that file in plain text and the program prints a warning. Older settings with the password in the file are moved to the keyring automatically when the viewer starts.

The viewer connects through the **SDK port** (8000 by default), not HTTP or RTSP. The encryption key is the verification code set under *Network → Advanced → Platform Access (Hik-Connect)*. Leave it empty if encryption is off.

## Tested devices

See the table in the [Spanish README](README.md#modelos-probados). Please open an issue with your model, firmware and the output of `./visor-hik --debug`.

## License

Source code is under the [MIT](LICENSE) license. The Hikvision SDK is not included and is subject to its own terms.

This project is not affiliated with or endorsed by Hikvision. Hikvision and Hik-Connect are registered trademarks of Hangzhou Hikvision Digital Technology Co., Ltd.

The photos in the screenshot are public domain (CC0); see [docs/CREDITOS.md](docs/CREDITOS.md).
