# OneKVM Tailscale 扩展

[English](README.md) | 简体中文

> **构建说明：** 本仓库只包含扩展源码。扩展统一由 `onekvm-distro` 中的
> OpenEmbedded（OE）配方构建和打包；本仓库不提供独立的正式构建流程。

本仓库包含 Tailscale 的 OneKVM 生命周期控制器和设置页面。上游
`tailscale` 与 `tailscaled` 二进制文件不进入 OneKVM 基础镜像，而是作为
扩展的私有运行时由 OpenEmbedded 提供。

控制器把状态和认证密钥文件保存在扩展私有数据目录下。扩展会关闭 DNS
接管和 Tailscale netfilter 管理，避免安装扩展后替换 KVM 管理网络的
解析器或防火墙策略。
