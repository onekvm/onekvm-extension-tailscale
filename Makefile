SCREEN_APP_DIR ?= screen-app/dist
CONTROLLER_BINARY ?= build/onekvm-tailscale-controller
PREFIX ?= /usr/local
DESTDIR ?=
VERSION ?= 0.0.0-dev
ARCH ?= unknown

.PHONY: all install

all:

install:
	test -x $(CONTROLLER_BINARY)
	install -Dm0755 $(CONTROLLER_BINARY) $(DESTDIR)$(PREFIX)/bin/onekvm-tailscale-controller
	install -Dm0644 web/page.js $(DESTDIR)$(PREFIX)/web/page.js
	install -Dm0644 web/logo.svg $(DESTDIR)$(PREFIX)/web/logo.svg
	install -d $(DESTDIR)$(PREFIX)
	sed -e 's/@ARCH@/$(ARCH)/g' -e 's/@VERSION@/$(VERSION)/g' \
		manifest.json.in >$(DESTDIR)$(PREFIX)/manifest.json
	chmod 0644 $(DESTDIR)$(PREFIX)/manifest.json
	install -d $(DESTDIR)$(PREFIX)/screen-app
	install -Dm0644 $(SCREEN_APP_DIR)/main.vue-vapor.js $(DESTDIR)$(PREFIX)/screen-app/main.vue-vapor.js
	install -Dm0644 $(SCREEN_APP_DIR)/main.vue-vapor.pak $(DESTDIR)$(PREFIX)/screen-app/main.vue-vapor.pak
