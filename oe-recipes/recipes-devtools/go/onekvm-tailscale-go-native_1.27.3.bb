# Reuse OE's verified native compiler unpack/install implementation.
require recipes-devtools/go/go-binary-native_1.26.4.bb

SUMMARY = "Recipe-private Go compiler for Tailscale"
# Avoid selecting this compiler as the distribution-wide go-native provider.
PROVIDES = ""
SRC_URI[go_linux_amd64.sha256sum] = "65efbc80fdc1fa2cd2631cad5b481777a68e8725f34e89f50fc6ac714e379b40"
SRC_URI[go_linux_arm64.sha256sum] = "a06e99309dbcbdca1a8ccd2282c3ee146e55930886e26b4f1193269587165f49"
SRC_URI[go_linux_ppc64le.sha256sum] = "f180f01943865b7942ab3b4acbab6da6f0f669b61da999d004d16c537a49f7b5"
