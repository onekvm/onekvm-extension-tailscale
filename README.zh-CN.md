# OneKVM Tailscale 扩展

[English](README.md) | 简体中文

在不把 Tailscale 加入基础镜像的情况下，将 OneKVM 连接到 Tailscale 网络。
扩展提供轻量的生命周期控制器和设置页面；上游 `tailscale`、`tailscaled`
二进制文件由 OpenEmbedded 随扩展打包。

> **构建说明：** 本仓库只包含扩展源码。正式软件包由 `onekvm-distro` 中的
> OpenEmbedded（OE）配方统一构建；本仓库不提供独立的正式构建流程。

## 功能

- 支持浏览器交互登录，也支持认证密钥无人值守注册
- 在 OneKVM 中显示连接状态、Tailscale IP 地址和设备主机名
- 可为兼容部署配置自定义控制服务器 URL
- 可配置 UDP 端口、接收广播路由和 shields-up 模式
- 可以直接从扩展页面断开连接或登出

## 安装与启用

软件包名称是 `onekvm-extension-tailscale`。请通过 OneKVM 发行版的软件包
或镜像安装，然后使用插件管理器控制扩展：

```sh
onekvm-plugin-manager enable tailscale
onekvm-plugin-manager disable tailscale
```

扩展需要 `tun` 内核模块。

## 连接与配置

打开 **Tailscale** 扩展页面。认证密钥留空时使用浏览器登录，也可以填写
Auth Key 进行无人值守注册。页面还提供以下选项：

- **控制服务器 URL：** 使用默认 Tailscale 服务时留空。
- **接受广播路由：** 默认关闭，避免意外路由变更影响管理连接。
- **阻止传入连接：** 启用 Tailscale 的 shields-up 模式。
- **UDP 端口：** 默认值为 `41641`；设为 `0` 时自动选择。

## 安全与网络

认证密钥属于敏感设置，会写入扩展私有文件供客户端读取。客户端状态也
保存在扩展私有数据目录中。

扩展会关闭 DNS 接管和 Tailscale netfilter 管理，因此不会替换 KVM 管理
网络的解析器或防火墙策略。启用广播路由仍可能改变流量路径，请按实际网络
规划使用。

## 开发

C++20 控制器可以使用 CMake 和 CTest 构建、测试：

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

上游 Tailscale 二进制文件由 OpenEmbedded 另行提供。

## 许可证

本项目使用 GNU General Public License v3.0，完整条款见
[`LICENSE`](LICENSE)。随包提供的 Tailscale 二进制文件及其依赖继续使用
各自的上游许可证。
