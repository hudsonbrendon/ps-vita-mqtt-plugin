TARGET := ps-vita-mqtt
OBJS_VITA := \
    src/main.o \
    src/vita_newlib_stubs.o \
    src/log_vita.o \
    src/config.o \
    src/config_vita.o \
    src/publisher.o \
    src/mqtt/mqtt_client.o \
    src/mqtt/mqtt_packet.o \
    src/mqtt/mqtt_socket_vita.o \
    src/ha/ha_discovery.o \
    src/collectors/battery_vita.o \
    src/collectors/system_vita.o \
    src/collectors/app_vita.o \
    src/collectors/network_vita.o \
    third_party/cJSON/cJSON.o

PREFIX  := arm-vita-eabi
CC      := $(PREFIX)-gcc
CFLAGS  := -Wl,-q -Wall -O2 -nostartfiles -DPSVITA_BUILD -I. -Isrc
LIBS    := -lScePower_stub_weak \
           -lSceNet_stub_weak -lSceNetCtl_stub_weak \
           -lSceKernelThreadMgr_stub_weak \
           -lSceKernelModulemgr_stub_weak \
           -lSceLibKernel_stub_weak \
           -lSceIofilemgr_stub_weak \
           -lSceSysmem_stub_weak

.PHONY: suprx clean host-tests

suprx: build/$(TARGET).suprx

build/$(TARGET).velf: $(OBJS_VITA)
	@mkdir -p build
	$(CC) $(CFLAGS) $^ $(LIBS) -o build/$(TARGET).elf
	vita-elf-create -e exports.yml build/$(TARGET).elf build/$(TARGET).velf

build/$(TARGET).suprx: build/$(TARGET).velf
	vita-make-fself -c build/$(TARGET).velf build/$(TARGET).suprx

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build $(OBJS_VITA) host-tests

# ---- host-side unit tests --------------------------------------------------
HOST_CC := cc
HOST_CFLAGS := -std=c99 -Wall -Wextra -D_GNU_SOURCE -I.

HOST_SRCS_COMMON := \
    src/log_host.c src/config.c \
    src/mqtt/mqtt_client.c src/mqtt/mqtt_packet.c src/mqtt/mqtt_socket_host.c \
    src/ha/ha_discovery.c src/publisher.c \
    src/collectors/battery_host.c src/collectors/system_host.c \
    src/collectors/app_host.c src/collectors/network_host.c \
    third_party/cJSON/cJSON.c

host-tests:
	@mkdir -p host-tests
	$(HOST_CC) $(HOST_CFLAGS) tests/test_log.c \
	   src/log_host.c third_party/cJSON/cJSON.c -o host-tests/test_log
	$(HOST_CC) $(HOST_CFLAGS) tests/test_mqtt_packet.c \
	   src/mqtt/mqtt_packet.c -o host-tests/test_mqtt_packet
	$(HOST_CC) $(HOST_CFLAGS) tests/test_config.c \
	   src/config.c third_party/cJSON/cJSON.c -o host-tests/test_config
	$(HOST_CC) $(HOST_CFLAGS) tests/test_ha_discovery.c \
	   src/ha/ha_discovery.c -o host-tests/test_ha_discovery
	$(HOST_CC) $(HOST_CFLAGS) tests/test_collectors.c \
	   src/collectors/battery_host.c src/collectors/system_host.c \
	   src/collectors/app_host.c src/collectors/network_host.c \
	   -o host-tests/test_collectors
	$(HOST_CC) $(HOST_CFLAGS) tests/test_publisher.c \
	   $(HOST_SRCS_COMMON) -o host-tests/test_publisher
	@for t in host-tests/test_*; do echo "==> $$t" && $$t || exit 1; done
