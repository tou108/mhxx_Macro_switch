#--------------------------------------------------------------
# MHXX Macro Controller - Switch Sysmodule
#--------------------------------------------------------------
TARGET  := mhxx-macro
SOURCES := .

DEVKITPRO ?= /opt/devkitpro
DEVKITA64 ?= $(DEVKITPRO)/devkitA64

export PATH := $(DEVKITA64)/bin:$(DEVKITPRO)/tools/bin:$(PATH)

CC  := aarch64-none-elf-gcc
CXX := aarch64-none-elf-g++
LD  := aarch64-none-elf-g++

ARCH    := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
CFLAGS  := -g -Wall -O2 -ffunction-sections $(ARCH) -D__SWITCH__
CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++17
LDFLAGS := -specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH)

INCLUDE  := -I$(DEVKITPRO)/libnx/include
LIBPATHS := -L$(DEVKITPRO)/libnx/lib
LIBS     := -lnx

CPPFILES := $(wildcard *.cpp)
OFILES   := $(CPPFILES:.cpp=.o)

.PHONY: all clean

all: $(TARGET).nso

$(TARGET).nso: $(TARGET).elf
	elf2nso $< $@
	@echo "Built: $@"

$(TARGET).elf: $(OFILES)
	$(LD) $(LDFLAGS) $^ $(LIBPATHS) $(LIBS) -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

clean:
	rm -f *.o *.elf *.nso *.map
