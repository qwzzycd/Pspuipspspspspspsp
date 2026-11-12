# NOVA launcher for PSP (ARK-4 / ARK-5)
#   make                -> EBOOT.PBP with ISO/CSO/ZSO launching (needs psp-cfw-sdk, same as ARK-5)
#   make NO_CFW=1       -> plain build, runs in PPSSPP; launches only homebrew
#   make HEN_SPEED=1    -> also call sctrlHENSetSpeed (only if your ARK exports it)
TARGET = NOVA
OBJS = src/platform_psp.o src/ui.o src/gfx.o

CFLAGS = -O2 -G0 -Wall -std=gnu99 -ffunction-sections -fdata-sections
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)
LIBDIR =
LIBS = -lm

ifeq ($(NO_CFW),1)
CFLAGS += -DNO_CFW
else
LIBS += -lpspsystemctrl_user
endif
ifeq ($(HEN_SPEED),1)
CFLAGS += -DUSE_HEN_SET_SPEED
endif

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = NOVA
PSP_EBOOT_ICON = assets/ICON0.PNG
PSP_FW_VERSION = 661

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
