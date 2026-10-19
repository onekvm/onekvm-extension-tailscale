# Reuse OE's verified native compiler unpack/install implementation.
require recipes-devtools/go/go-binary-native_1.26.4.bb

SUMMARY = "Recipe-private Go compiler for Tailscale"
# Avoid selecting this compiler as the distribution-wide go-native provider.
PROVIDES = ""
SRC_URI[go_linux_amd64.sha256sum] = "ecbadb99091a3f46e31f5f934b068b1864eafa7995211b39eaddf76996045fe5"
SRC_URI[go_linux_arm64.sha256sum] = "94f3e30b8e374bc285e7dadc11e0865726b9bc6e85b841ccceaabc0214c6b7c8"
SRC_URI[go_linux_ppc64le.sha256sum] = "059d5f01b1607a724ea5ae87c5589041a51c577a6f3f20099e49d00f31c803fa"
