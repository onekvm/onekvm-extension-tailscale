# OneKVM Tailscale Extension

English | [简体中文](README.zh-CN.md)

Connect OneKVM to a Tailscale network without adding Tailscale to the base
image. The extension packages a small lifecycle controller, a settings page,
and the upstream `tailscale` and `tailscaled` binaries through OpenEmbedded.

> **Build note:** This repository contains extension sources only. Release
> packages are built by the OpenEmbedded recipes in `onekvm-distro`; this
> repository does not provide a standalone release build.

## Features

- Interactive browser sign-in or authentication-key enrollment
- Connection status, Tailscale IP addresses, and device hostname in OneKVM
- Optional custom control server URL for compatible deployments
- Configurable UDP port, advertised-route acceptance, and shields-up mode
- Disconnect or log out directly from the extension page

## Install and enable

The package name is `onekvm-extension-tailscale`. Install it through your
OneKVM distribution package or image, then manage it with the plugin manager:

```sh
onekvm-plugin-manager enable tailscale
onekvm-plugin-manager disable tailscale
```

The extension requires the `tun` kernel module.

## Connect and configure

Open the **Tailscale** extension page. Leave the authentication key empty to
use browser sign-in, or enter an auth key for unattended enrollment. The page
also provides these options:

- **Control server URL:** leave empty for the default Tailscale service.
- **Accept advertised routes:** disabled by default to protect the management
  path from unexpected route changes.
- **Block incoming connections:** enables Tailscale's shields-up mode.
- **UDP port:** defaults to `41641`; use `0` for automatic selection.

## Security and networking

The authentication key is stored as a secret setting and written to a private
file for the client. Client state stays below the extension's private data
directory.

DNS acceptance and Tailscale netfilter management are deliberately disabled,
so the extension does not replace the KVM management resolver or firewall
policy. Enabling advertised routes can still change how traffic is routed and
should be used deliberately.

## Development

The C++20 controller can be built and tested with CMake and CTest:

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

The upstream Tailscale binaries are supplied separately by OpenEmbedded.

## License

This project is licensed under the GNU General Public License v3.0. See
[`LICENSE`](LICENSE). The packaged Tailscale binaries and dependencies retain
their respective upstream licenses.
