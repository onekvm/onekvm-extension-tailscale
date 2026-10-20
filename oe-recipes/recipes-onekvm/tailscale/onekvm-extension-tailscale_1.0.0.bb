SUMMARY = "OneKVM Tailscale service extension"
HOMEPAGE = "https://github.com/onekvm/onekvm-extension-tailscale"
LICENSE = "GPL-3.0-only"
LIC_FILES_CHKSUM = "file://LICENSE;md5=1ebbd3e34237af26da5dc08a4e440464"
PR = "r4"

ONEKVM_COMPONENT_GIT_URI ??= "git://github.com/onekvm/onekvm-extension-tailscale.git;protocol=https;branch=main"
ONEKVM_COMPONENT_SRCREV ??= ""
SRC_URI = "${ONEKVM_COMPONENT_GIT_URI}"
SRCREV = "${ONEKVM_COMPONENT_SRCREV}"

inherit cmake pkgconfig onekvm-extension

DEPENDS += "json-c"

ONEKVM_EXTENSION_ID = "tailscale"
ONEKVM_EXTENSION_API_VERSION = "2"
ONEKVM_EXTENSION_MANIFEST_TEMPLATE = "${S}/manifest.json.in"

EXTRA_OECMAKE += " \
    -DCMAKE_INSTALL_PREFIX=${ONEKVM_EXTENSION_ROOT} \
    -DONEKVM_EXTENSION_VERSION=${PV} \
    -DONEKVM_EXTENSION_ARCH=${TARGET_ARCH} \
"

# meta-networking's tailscale package owns both /usr/bin/tailscale and
# /usr/sbin/tailscaled. Deliberately do not depend on its tailscaled service
# package: the extension host owns the daemon lifecycle and state directory.
RDEPENDS:${PN} += "tailscale onekvm-kernel-module-tun"
RREPLACES:${PN} += "onekvm-extension-tailscale-runtime (<= 1.0.0-r1)"
RCONFLICTS:${PN} += "onekvm-extension-tailscale-runtime (<= 1.0.0-r1)"

FILES:${PN} += "${ONEKVM_EXTENSION_ROOT}/screen-app"
