# Keep the pinned meta-networking provider and its service/package split.
# Only the upstream source, dependency metadata and compiler are updated here.
PV = "1.104.1"
SRCREV = "9a522a9786c97eb7910c01ccb7bd66557b04c910"

SRC_URI = "git://github.com/tailscale/tailscale.git;protocol=https;branch=release-branch/${MAJOR_MINOR};destsuffix=${GO_SRCURI_DESTSUFFIX} \
           file://default \
           file://tailscaled.init \
           "
LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://src/${GO_IMPORT}/LICENSE;md5=cadeae10a8856ddfdb129866b75b33e3"
require tailscale-go-mods.inc
require tailscale-licenses.inc

# Tailscale 1.104 requires Go 1.27. Use a recipe-private native compiler,
# leaving the distribution's Go 1.26 cross toolchain and other clients intact.
DEPENDS_GOLANG:class-target = "onekvm-tailscale-go-native"
GO = "${STAGING_BINDIR_NATIVE}/go"
GOROOT = "${STAGING_LIBDIR_NATIVE}/go"
GOTOOLDIR = "${GOROOT}/pkg/tool/${BUILD_GOTUPLE}"
GOTOOLCHAIN = "local"
export GOTOOLCHAIN
CGO_ENABLED = "0"
GOBUILDFLAGS:remove = "-buildmode=pie"

# Keep dependency regeneration in this component layer.
ONEKVM_TAILSCALE_METADATA_DIR := "${THISDIR}"
python do_update_modules:prepend() {
    d.setVar("THISDIR", d.getVar("ONEKVM_TAILSCALE_METADATA_DIR"))
}
