# Makefile for libusb based examples
BTSTACK_ROOT ?= ../..

# Role: master (APP_ROLE=0 -> /dev/ttyACM0) or slave (APP_ROLE=1 -> /dev/ttyACM1)
# Usage: make [ROLE=master|slave]   default: master
ROLE ?= master

# Convenience mapping for EATT demos:
# - make eatt_test_client  -> ROLE=master
# - make eatt_test_server  -> ROLE=slave
ifneq (,$(filter eatt_test_client,$(MAKECMDGOALS)))
ROLE := master
endif
ifneq (,$(filter eatt_test_server,$(MAKECMDGOALS)))
ROLE := slave
endif

ifeq ($(ROLE),master)
ROLE_CFLAGS := -DAPP_ROLE=0
else ifeq ($(ROLE),slave)
ROLE_CFLAGS := -DAPP_ROLE=1
else
$(error ROLE must be 'master' or 'slave', got: $(ROLE))
endif

CORE += \
	btstack_chipset_bcm.c \
	btstack_ltv_builder.c \
	btstack_chipset_cc256x.c \
	btstack_chipset_csr.c \
	btstack_chipset_em9301.c \
	btstack_chipset_realtek.c \
	btstack_chipset_stlc2500d.c \
	btstack_chipset_tc3566x.c \
	btstack_chipset_zephyr.c \
	btstack_link_key_db_tlv.c \
	btstack_run_loop_posix.c \
	btstack_audio.c \
    btstack_audio_portaudio.c \
    btstack_main_config.c \
	btstack_tlv_posix.c \
	btstack_uart_posix.c \
	hci_dump_posix_fs.c \
	hci_transport_h4.c \
	le_device_db_tlv.c \
	rijndael.c \
	main.c \
	btstack_stdin_posix.c \
	btstack_signal.c \
	wav_util.c 					\
	0000000_META_hci_patches_v7.c \
	bluetooth_init_cc2564C_1.5.c \
	# bluetooth_init_cc2564B_1.8_BT_Spec_4.1.c \
	# bluetooth_init_cc2564_2.14.c \

# TI-WL183x requires TIInit_11.8.32.c

# examples
include ${BTSTACK_ROOT}/example/Makefile.inc

# fetch and convert TI init scripts
include ${BTSTACK_ROOT}/chipset/cc256x/Makefile.inc

# fetch Broadcom init scripts
include ${BTSTACK_ROOT}/chipset/bcm/Makefile.inc

CFLAGS  += -g -Wall -Werror \
	-I$(BTSTACK_ROOT)/platform/embedded \
	-I$(BTSTACK_ROOT)/platform/posix \
	-I$(BTSTACK_ROOT)/chipset/bcm \
	-I$(BTSTACK_ROOT)/chipset/cc256x \
	-I$(BTSTACK_ROOT)/chipset/csr \
	-I$(BTSTACK_ROOT)/chipset/em9301 \
	-I$(BTSTACK_ROOT)/chipset/realtek \
	-I$(BTSTACK_ROOT)/chipset/stlc2500d \
	-I$(BTSTACK_ROOT)/chipset/tc3566x \
	-I$(BTSTACK_ROOT)/chipset/zephyr \
    -I${BTSTACK_ROOT}/3rd-party/rijndael \
    -I${BTSTACK_ROOT}/3rd-party/tinydir

CFLAGS  += $(ROLE_CFLAGS)

VPATH += ${BTSTACK_ROOT}/3rd-party/rijndael

VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/platform/embedded

VPATH += ${BTSTACK_ROOT}/chipset/bcm	
VPATH += ${BTSTACK_ROOT}/chipset/cc256x
VPATH += ${BTSTACK_ROOT}/chipset/csr
VPATH += ${BTSTACK_ROOT}/chipset/em9301
VPATH += ${BTSTACK_ROOT}/chipset/realtek
VPATH += ${BTSTACK_ROOT}/chipset/stlc2500d
VPATH += ${BTSTACK_ROOT}/chipset/tc3566x
VPATH += ${BTSTACK_ROOT}/chipset/zephyr

# add pthread for ctrl-c signal handler
LDFLAGS += -lpthread

EXAMPLES = ${EXAMPLES_GENERAL} ${EXAMPLES_CLASSIC_ONLY} ${EXAMPLES_LE_ONLY} ${EXAMPLES_DUAL_MODE}
EXAMPLES += pan_lwip_http_server

# use pkg-config for portaudio
# CFLAGS  += $(shell pkg-config portaudio-2.0 --cflags) -DHAVE_PORTAUDIO
# LDFLAGS += $(shell pkg-config portaudio-2.0 --libs)
# hard coded flags for portaudio in /usr/local/lib
# CFLAGS  += -I/usr/local/include -DHAVE_PORTAUDIO
# LDFLAGS += -L/sw/lib -lportaudio -Wl,-framework,CoreAudio -Wl,-framework,AudioToolbox -Wl,-framework,AudioUnit -Wl,-framework,Carbon

# -----------------------------------------------------------------------
# Role-aware build targets
# Build both master and slave binaries in separate directories:
#   build-master/  -> APP_ROLE=0  /dev/ttyACM0
#   build-slave/   -> APP_ROLE=1  /dev/ttyACM1
# -----------------------------------------------------------------------
.PHONY: all master slave _build_examples prep_eatt_test_client prep_eatt_test_server

all: master slave

master: BCM43430A1.hcd
	@echo "=== Building MASTER  (APP_ROLE=0, /dev/ttyACM0) ==="
	@mkdir -p build-master
	$(MAKE) ROLE=master _build_examples
	@cp $(EXAMPLES) build-master/
	@echo "=== Master binaries -> build-master/ ==="

slave: BCM43430A1.hcd
	@echo "=== Building SLAVE   (APP_ROLE=1, /dev/ttyACM1) ==="
	@mkdir -p build-slave
	@rm -f *.o
	$(MAKE) ROLE=slave _build_examples
	@cp $(EXAMPLES) build-slave/
	@echo "=== Slave binaries  -> build-slave/ ==="

_build_examples: ${EXAMPLES}

# Ensure main.c is rebuilt with the role selected for each EATT target.
eatt_test_client: prep_eatt_test_client
eatt_test_server: prep_eatt_test_server

prep_eatt_test_client:
	@rm -f main.o
	@echo "Building eatt_test_client with ROLE=master (APP_ROLE=0)"

prep_eatt_test_server:
	@rm -f main.o
	@echo "Building eatt_test_server with ROLE=slave (APP_ROLE=1)"

