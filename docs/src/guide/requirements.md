# Requirements

The fork patches the **host** GTK and serves the app over Broadway, so requirements split between the host (where GTK runs) and the client (the browser).

## Host

| What | Requirement |
| ---- | ----------- |
| OS | Linux |
| Distro | Debian (`.deb`), Arch (PKGBUILD), or Docker base image - see [Installation](installation.md) |
| GTK | 4.24.x series, matching the system GTK |
| GLib | >= 2.89.3 |
| Pango | >= 1.58 |
| Meson (source builds) | >= 1.8 |
| Architecture | amd64, arm64 |

Migration target (unreleased):

| GTK    | Ubuntu base    | Library file           | Fork branch | Patch |
| ------ | -------------- | ---------------------- | ----------- | ----- |
| 4.24.1 | `ubuntu:26.10` | `libgtk-4.so.1.2400.1` | [`4.24.1-brotway`](https://github.com/droserasprout/gtk-brotway/tree/4.24.1-brotway) | [diff](https://github.com/droserasprout/gtk-brotway/compare/4.24.1...4.24.1-brotway) |

The ELF SONAME remains `libgtk-4.so.1`; the full filename identifies the build.
Upgrade the library and `gtk4-broadwayd` together and restart all applications:
4.24 changes their private grab request layout. The browser GRAB opcode is unchanged.
Published v3.2.0 packages still use GTK 4.22.4 and Ubuntu 26.04.

## Client

Any device with a modern browser, no install - desktop or mobile, touch included. Client OS and architecture don't matter.

### Tested browsers

- Linux desktop:
  - Firefox 151.0.2
  - Chromium 149.0.7827.53-1.1
  - Google Chrome 149.0.7827.114-1
- Android:
  - Firefox Beta 152.0b4
  - Google Chrome 146.0.7680.119
- Headless
  - Playwright w/ Chrome driver

### Not tested

- Windows browsers
- macOS browsers
- iOS (Safari, Orion)
- Mixed devices (e.g. laptops with a touchscreen)
- Mobile linux (postmarketOS)
