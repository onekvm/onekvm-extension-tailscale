# OpenEmbedded layer

This layer packages the Tailscale extension for OneKVM. It supports the
`wrynose` release and depends on OE-Core, `meta-networking`, and the
distro-owned `onekvm` layer, which provides `onekvm-extension.bbclass`.

The extension package directly depends on the `tailscale` package provided by
`meta-networking`. The integrating distro must set `ONEKVM_COMPONENT_SRCREV`
to the same fixed component commit that supplies this layer. OneKVM release
builds derive that value from the distro component lock instead of storing a
self-referential commit in this recipe.

The append to the pinned `tailscale_1.94.2.bb` provider updates the client to
Tailscale 1.104.4, with its own checked module and license includes. It retains
the upstream package split: the extension depends on `tailscale`, while the
separate `tailscaled` init-service package remains optional.
The extension requires `tailscale >= 1.104.4`, so marketplace upgrades also
upgrade an older client when the matching package is available in the feed.

Tailscale 1.104.4 requires Go 1.27.3. The recipe uses the private
`onekvm-tailscale-go-native` 1.27.3 compiler and builds without cgo; the distro's
Go 1.26 provider remains available to other recipes. The compiler reuses
OE-Core's native Go installation recipe and is not a target runtime dependency.

Run `bitbake -c update_modules tailscale` after changing the upstream revision.
The append directs generated metadata into this component's recipe directory,
so the pinned `meta-networking` checkout is not modified.
