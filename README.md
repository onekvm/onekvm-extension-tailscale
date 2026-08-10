# OneKVM Tailscale extension

English | [简体中文](README.zh-CN.md)

> **Build note:** This repository contains extension sources only. The
> extension is built and packaged by the OpenEmbedded recipes in
> `onekvm-distro`; this repository does not provide a standalone release build.

This repository contains the small OneKVM lifecycle controller and settings
page for Tailscale. The upstream `tailscale` and `tailscaled` binaries are not
part of the OneKVM base image and are supplied with the extension by
OpenEmbedded.

The controller keeps state and the authentication-key file below the
extension's private data directory. DNS acceptance and Tailscale netfilter
management are deliberately disabled so installing the extension cannot
replace the KVM management network's resolver or firewall policy.
