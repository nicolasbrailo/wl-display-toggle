.PHONY: all deploytgt run clean format formatall

all: wl-display-toggle

clean:
	rm -rf build
	rm -f ./wl-display-toggle

# System tests
.PHONY: system-deps screenoff screenon
screenoff:
	WAYLAND_DISPLAY="wayland-1" wlr-randr --output HDMI-A-1 --off
screenon:
	WAYLAND_DISPLAY="wayland-1" wlr-randr --output HDMI-A-1 --on

system-deps:
	sudo apt-get install clang-format
	# wayland headers
	sudo apt-get install libwayland-dev
	# wlroots protocol files, needed for wlr-output-power-management-unstable-v1.xml
	sudo apt-get install libwlroots-dev
	# Install protocols.xml into /usr/share/wayland-protocols, not sure if needed
	sudo apt-get install wayland-protocols


XCOMPILE=\
	-target arm-linux-gnueabihf \
	-mcpu=arm1176jzf-s \
	--sysroot ~/src/xcomp-rpiz-env/mnt/  \

CFLAGS=\
	$(XCOMPILE) \
	-fdiagnostics-color=always \
	-ffunction-sections -fdata-sections \
	-ggdb -O3 \
	-std=gnu99 \
	-Wall -Werror -Wextra -Wpedantic \
	-Wendif-labels \
	-Wfloat-equal \
	-Wformat=2 \
	-Wimplicit-fallthrough \
	-Winit-self \
	-Winvalid-pch \
	-Wmissing-field-initializers \
	-Wmissing-include-dirs \
	-Wno-strict-prototypes \
	-Wno-unused-function \
	-Wno-unused-parameter \
	-Woverflow \
	-Wpointer-arith \
	-Wredundant-decls \
	-Wstrict-aliasing=2 \
	-Wundef \
	-Wuninitialized \

# Download wl protocol definitions
wl_protos/wlr-output-management-unstable-v1.xml:
	mkdir -p $(shell dirname "$@")
	curl --output $@ \
		https://gitlab.freedesktop.org/wlroots/wlr-protocols/-/raw/2b8d43325b7012cc3f9b55c08d26e50e42beac7d/unstable/wlr-output-management-unstable-v1.xml?inline=false

# Build glue from wl protocol xml
build/wl_protos/%.h: wl_protos/%.xml
	mkdir -p  build/wl_protos
	wayland-scanner client-header < $< > $@
build/wl_protos/%.c: wl_protos/%.xml
	mkdir -p  build/wl_protos
	wayland-scanner private-code < $< > $@

build/wl_protos/%.o: build/wl_protos/%.c build/wl_protos/%.h
	mkdir -p build
	@if [ ! -d ~/src/xcomp-rpiz-env/mnt/lib/raspberrypi-sys-mods ]; then \
		echo "xcompiler sysroot not detected, try `make xcompile-start`"; \
		@exit 1; \
	fi ;
	clang $(CFLAGS) -isystem ./build $< -c -o $@

build/%.o: %.c %.h
	mkdir -p build
	@if [ ! -d ~/src/xcomp-rpiz-env/mnt/lib/raspberrypi-sys-mods ]; then \
		echo "xcompiler sysroot not detected, try `make xcompile-start`"; \
		@exit 1; \
	fi ;
	clang $(CFLAGS) -isystem ./build $< -c -o $@

wl-display-toggle: build/wl_protos/wlr-output-management-unstable-v1.o \
			build/wl_ctrl.o \
			wl_display_toggle.c
	clang $(CFLAGS) $^ -o $@ -lwayland-client


.PHONY: xcompile-start xcompile-end install_sysroot_deps

xcompile-start:
	./rpiz-xcompile/mount_rpy_root.sh ~/src/xcomp-rpiz-env

xcompile-end:
	./rpiz-xcompile/umount_rpy_root.sh ~/src/xcomp-rpiz-env

install_sysroot_deps:
	./rpiz-xcompile/add_sysroot_pkg.sh ~/src/xcomp-rpiz-env http://archive.raspberrypi.com/debian/pool/main/w/wayland/libwayland-dev_1.22.0-2.1~bpo12+rpt1_armhf.deb

