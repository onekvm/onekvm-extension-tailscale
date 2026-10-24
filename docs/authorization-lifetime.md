# 浏览器授权的有效期

以下结论核对了当前打包的 Tailscale 1.104.1 源码
`9a522a9786c97eb7910c01ccb7bd66557b04c910`（2026-10-24）。

| 对象 | 有效期与依据 |
| --- | --- |
| Tailscale 服务端登录 URL | 当前客户端源码明确说明 **7 天**；这是上游对 Tailscale 控制服务的说明，不能直接作为自建控制服务的保证。 |
| 客户端复用登录 URL | 仅复用收到后不足 **6 天 23 小时**的 URL；保留一小时余量后再请求登录。 |
| OneKVM 本次等待窗口 | `tailscale up --timeout=10m` 最多等待 **10 分钟**进入 Running；这是 OneKVM 设置的本地上限，不是 URL 的服务器有效期。 |
| 已登录设备的 node key | 官方文档中新建域默认 **180 天**，可调整或按设备关闭；与扫码 URL 无关。 |

LocalAPI 的 Status / IPN 通知以及 `tailscale up --json` 只给出 AuthURL、
后端状态和错误，没有暴露当前 URL 的签发时间或到期时间。
`authURLTime` 是 tailscaled 的内部字段，不是 UI 可以读取的字段；收到链接时也不保证
它刚由服务端签发。因此可以说明七天的上游规则，不能从打开屏幕开始显示七天倒计时。

屏幕提示“5 分钟内扫码，过期重连”。服务端明确报告过期时提前清除；
五分钟二维码窗口到期时先结束等待并清除二维码；CLI 的十分钟只是更外层的等待上限。重试向 tailscaled 发起登录，
由它决定复用仍有效的 URL 或请求 URL；不能承诺每次重试都会换一个字符串。

左上角箭头与右滑返回 App 列表，长按旋钮回主屏，均不终止后台登录。重新打开会显示当前仍在进行的授权；
“取消”才终止本次流程。完成、取消、替换、过期或失败后均清除旧二维码。
`NeedsLogin` 优先于残留的旧 IP，避免设备密钥过期后被误判为已经登录。

状态展示订阅 `watch-ipn-bus`，不定时调用 status 或屏幕 Refresh。


## OneKVM 的统一扫码窗口

OneKVM 对两个提供方统一限制 **300 秒**：从当前 URL 首次可用起，后台用
`CLOCK_MONOTONIC` 的一次性 timerfd 事件计时。显示 `authWindowSeconds: 300`
及对应 URL 的 `authExpiresAt`（Unix 毫秒）。返回、重进和查询 status 不延长期限；
新请求或服务端换码会建立新的窗口。上游提前失败或过期时提前清除。

到期停止本次登录等待、清除 URL / 手动码并通知屏幕，显示“二维码已过期，请重试”；
不自动重新授权。点击连接才重试。此限制属于 OneKVM 的展示和等待策略，
不能改变或保证撤销官方服务端 URL 的有效期；Tailscale 仍可能返回同一个有效链接。
到期时间字段只用于展示；实际到期事件用单调时钟，不受系统时间调整影响。
服务重启保留已经结束的状态，不重新执行旧请求；被重启中断的授权标为失败，
清除旧码，等待用户手动重试。

## 按钮状态机

| 状态 | 连接 | 断开 | 返回 |
| --- | --- | --- | --- |
| 离线、初始状态尚未取得 | 禁用 | 禁用 | 可用 |
| 已就绪、未连接 | 可用 | 禁用 | 可用 |
| 生成链接、待授权、连接中 | 禁用 | 可用，显示“取消” | 可用 |
| 已连接 | 禁用 | 可用 | 可用 |
| 过期、超时、失败 | 可重试 | 禁用 | 可用 |
| 操作请求处理中 | 禁用 | 禁用 | 可用 |

触摸和旋钮走相同的启用条件；旋钮跳过禁用项。后台以文件锁串行发布授权请求，
重复 Connect 返回 `started: false, alreadyPending: true`，保留原请求和到期时间。
取消后才允许重新发起；到期不会自动换码。

## 来源

- [上游七天有效期及 6 天 23 小时复用规则](https://github.com/tailscale/tailscale/blob/9a522a9786c97eb7910c01ccb7bd66557b04c910/ipn/ipnlocal/local.go#L4692)
- [CLI timeout 定义](https://github.com/tailscale/tailscale/blob/9a522a9786c97eb7910c01ccb7bd66557b04c910/cmd/tailscale/cli/up.go#L136)
- [up --json 实际输出字段](https://github.com/tailscale/tailscale/blob/9a522a9786c97eb7910c01ccb7bd66557b04c910/cmd/tailscale/cli/up.go#L263)
- [LocalAPI Status 字段](https://github.com/tailscale/tailscale/blob/9a522a9786c97eb7910c01ccb7bd66557b04c910/ipn/ipnstate/ipnstate.go#L41)
- [官方 CLI 文档](https://tailscale.com/docs/reference/tailscale-cli)
- [官方设备密钥到期说明](https://tailscale.com/docs/features/access-control/key-expiry)
