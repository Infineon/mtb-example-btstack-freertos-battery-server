################################################################################
# \file Makefile
# \version 1.0
#
# \brief
# Top-level application make file.
#
################################################################################
# \copyright
# Copyright 2018-2022, Cypress Semiconductor Corporation (an Infineon company)
# SPDX-License-Identifier: Apache-2.0
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
#     http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
################################################################################


################################################################################
# Basic Configuration
################################################################################

-include ./libs/mtb.mk

# Target board/hardware (BSP).
# To change the target, it is recommended to use the Library manager
# ('make modlibs' from command line), which will also update Eclipse IDE launch
# configurations. If TARGET is manually edited, ensure TARGET_<BSP>.mtb with a
# valid URL exists in the application, run 'make getlibs' to fetch BSP contents
# and update or regenerate launch configurations for your IDE.
TARGET=CY8CKIT-062S2-43012

# Underscore needed for $(TARGET) directory
TARGET_UNDERSCORE=$(subst -,_,$(TARGET))

# Core processor
CORE?=CM4

# Name of application (used to derive name of final linked file).
#
# If APPNAME is edited, ensure to update or regenerate launch
# configurations for your IDE.
APPNAME=mtb-example-anycloud-ble-battery-server

# Name of toolchain to use. Options include:
#
# GCC_ARM -- GCC 7.2.1, provided with ModusToolbox IDE
# ARM     -- ARM Compiler (must be installed separately)
# IAR     -- IAR Compiler (must be installed separately)
#
# See also: CY_COMPILER_PATH below
TOOLCHAIN=GCC_ARM

# Default build configuration. Options include:
#
# Debug   -- build with minimal optimizations, focus on debugging.
# Release -- build with full optimizations
# Custom -- build with custom configuration, set the optimization flag in CFLAGS
#
# If CONFIG is manually edited, ensure to update or regenerate launch configurations
# for your IDE.
CONFIG=Debug

# If set to "true" or "1", display full command-lines when building.
VERBOSE=

# Set to 1 to add OTA defines, sources, and libraries (must be used with MCUBoot)
# NOTE: Extra code must be called from your app to initialize AnyCloud OTA middleware.
OTA_SUPPORT=1

# Set to 1 to add OTA external Flash support.
# Set to 0 for internal flash
# Make sure MCUboot has the same configuration
OTA_USE_EXTERNAL_FLASH?=1

################################################################################
# Advanced Configuration
################################################################################

# Enable optional code that is ordinarily disabled by default.
#
# Available components depend on the specific targeted hardware and firmware
# in use. In general, if you have
#
#    COMPONENTS=foo bar
#
# ... then code in directories named COMPONENT_foo and COMPONENT_bar will be
# added to the build
#
COMPONENTS= FREERTOS PSOC6HAL WICED_BLE OTA_BLUETOOTH

# Like COMPONENTS, but disable optional code that was enabled by default.
DISABLE_COMPONENTS=

# By default the build system automatically looks in the Makefile's directory
# tree for source code and builds it. The SOURCES variable can be used to
# manually add source code to the build process from a location not searched
# by default, or otherwise not found by the build system.
SOURCES=

# Like SOURCES, but for include directories. Value should be paths to
# directories (without a leading -I).
INCLUDES=./configs

# Add additional defines to the build process (without a leading -D).
DEFINES+=CY_RETARGET_IO_CONVERT_LF_TO_CRLF CY_RTOS_AWARE

# Select softfp or hardfp floating point. Default is softfp.
VFP_SELECT=

# Additional / custom C compiler flags.
#
# NOTE: Includes and defines should use the INCLUDES and DEFINES variable
# above.
CFLAGS=

# Additional / custom C++ compiler flags.
#
# NOTE: Includes and defines should use the INCLUDES and DEFINES variable
# above.
CXXFLAGS=

# Additional / custom assembler flags.
#
# NOTE: Includes and defines should use the INCLUDES and DEFINES variable
# above.
ASFLAGS=

# Additional / custom linker flags.
LDFLAGS=

# Additional / custom libraries to link in to the application.
LDLIBS=

# Custom pre-build commands to run.
PREBUILD=

# Custom post-build commands to run.
POSTBUILD=

# Check for default Version values
# Change the version here or over-ride by setting an environment variable
# before building the application.
#
# export APP_VERSION_MAJOR=3
#
APP_VERSION_MAJOR?=3
APP_VERSION_MINOR?=0
APP_VERSION_BUILD?=0

###########################################################################
#
# OTA Support
#
ifeq ($(OTA_SUPPORT),1)
    # OTA / MCUBoot defines

    # These libs are only needed for WiFi transports
    CY_IGNORE+=$(SEARCH_aws-iot-device-sdk-embedded-C)
    CY_IGNORE+=$(SEARCH_aws-iot-device-sdk-port)
    CY_IGNORE+=$(SEARCH_cy-mbedtls-acceleration)
    CY_IGNORE+=$(SEARCH_http-client)
    CY_IGNORE+=$(SEARCH_mqtt)
    CY_IGNORE+=$(SEARCH_secure-sockets)
    CY_IGNORE+=$(SEARCH_wifi-connection-manager)
    CY_IGNORE+=$(SEARCH_lwip)
    CY_IGNORE+=$(SEARCH_mbedtls)
    CY_IGNORE+=$(SEARCH_wifi-host-driver)
    CY_IGNORE+=$(SEARCH_wifi-mw-core)
    CY_IGNORE+=$(SEARCH_whd-bsp-integration)
    CY_IGNORE+=$(SEARCH_wpa3-external-supplicant)
    # IMPORTANT NOTE: These defines are also used in the building of MCUBOOT
    #                 they must EXACTLY match the values added to
    #                 mcuboot/boot/cypress/MCUBootApp/MCUBootApp.mk
    #
    # Must be a multiple of 1024 (must leave __vectors on a 1k boundary)
    MCUBOOT_HEADER_SIZE=0x400
    ifeq ($(OTA_USE_EXTERNAL_FLASH),1)
        MCUBOOT_MAX_IMG_SECTORS=32
        # SCRATCH SIZE is the size of FLASH set aside for the SWAP of Primary and Secondary Slots
        # Please see mcuboot documentation for more information
        CY_BOOT_SCRATCH_SIZE=0x0004000
        # Boot loader size defines for mcuboot & app are different, but value is the same
        MCUBOOT_BOOTLOADER_SIZE=0x00018000
        CY_BOOT_BOOTLOADER_SIZE=$(MCUBOOT_BOOTLOADER_SIZE)
        # Primary Slot Currently follows Bootloader sequentially
        CY_BOOT_PRIMARY_1_START=0x00018000
        CY_BOOT_PRIMARY_1_SIZE=0x001C0000
        # offset from start of external FLASH 18000000
        CY_BOOT_SECONDARY_1_START=0x00000000
        CY_BOOT_SECONDARY_1_SIZE=0x001C0000
        CY_FLASH_ERASE_VALUE=0xFF
    else
        MCUBOOT_MAX_IMG_SECTORS=32
        CY_BOOT_SCRATCH_SIZE=0x0004000
        # Boot loader size defines for mcuboot & app are different, but value is the same
        MCUBOOT_BOOTLOADER_SIZE=0x00018000
        CY_BOOT_BOOTLOADER_SIZE=$(MCUBOOT_BOOTLOADER_SIZE)
        # Primary Slot Currently follows Bootloader sequentially
        CY_BOOT_PRIMARY_1_START=0x00018000
        CY_BOOT_PRIMARY_1_SIZE=0x000EE000
        CY_BOOT_SECONDARY_1_SIZE=0x000EE000
        CY_BOOT_PRIMARY_2_SIZE=0x01000
        CY_BOOT_SECONDARY_2_START=0x001E0000
        CY_FLASH_ERASE_VALUE=0x00
    endif

    # Additional / custom linker flags.
    # This needs to be before finding LINKER_SCRIPT_WILDCARD as we need the extension defined
    ifeq ($(TOOLCHAIN),GCC_ARM)
    CY_ELF_TO_HEX=$(CY_CROSSPATH)/bin/arm-none-eabi-objcopy
    CY_ELF_TO_HEX_OPTIONS="-O ihex"
    CY_ELF_TO_HEX_FILE_ORDER="elf_first"
    CY_TOOLCHAIN=GCC
    CY_TOOLCHAIN_LS_EXT=ld
    LDFLAGS+="-Wl,--defsym,MCUBOOT_HEADER_SIZE=$(MCUBOOT_HEADER_SIZE),--defsym,MCUBOOT_BOOTLOADER_SIZE=$(MCUBOOT_BOOTLOADER_SIZE),--defsym,CY_BOOT_PRIMARY_1_SIZE=$(CY_BOOT_PRIMARY_1_SIZE)"
    else
    ifeq ($(TOOLCHAIN),IAR)
    CY_ELF_TO_HEX="$(CY_CROSSPATH)/bin/ielftool"
    CY_ELF_TO_HEX_OPTIONS="--ihex"
    CY_ELF_TO_HEX_FILE_ORDER="elf_first"
    CY_TOOLCHAIN=$(TOOLCHAIN)
    CY_TOOLCHAIN_LS_EXT=icf
    LDFLAGS+=--config_def MCUBOOT_HEADER_SIZE=$(MCUBOOT_HEADER_SIZE) --config_def MCUBOOT_BOOTLOADER_SIZE=$(MCUBOOT_BOOTLOADER_SIZE) --config_def CY_BOOT_PRIMARY_1_SIZE=$(CY_BOOT_PRIMARY_1_SIZE)
    else
    ifeq ($(TOOLCHAIN),ARM)
    CY_ELF_TO_HEX=$(CY_CROSSPATH)/bin/fromelf
    CY_ELF_TO_HEX_OPTIONS="--i32 --output"
    CY_ELF_TO_HEX_FILE_ORDER="hex_first"
    CY_TOOLCHAIN=GCC
    CY_TOOLCHAIN_LS_EXT=sct
    LDFLAGS+=--pd=-DMCUBOOT_HEADER_SIZE=$(MCUBOOT_HEADER_SIZE) --pd=-DMCUBOOT_BOOTLOADER_SIZE=$(MCUBOOT_BOOTLOADER_SIZE) --pd=-DCY_BOOT_PRIMARY_1_SIZE=$(CY_BOOT_PRIMARY_1_SIZE)
    else
    LDFLAGS+=
    endif #ARM
    endif #IAR
    endif #GCC_ARM

    # Linker Script
    LINKER_SCRIPT_WILDCARD:= $(SEARCH_anycloud-ota)/$(TARGET_UNDERSCORE)/COMPONENT_$(CORE)/TOOLCHAIN_$(TOOLCHAIN)/ota/*_ota_int.$(CY_TOOLCHAIN_LS_EXT)
    LINKER_SCRIPT:=$(wildcard $(LINKER_SCRIPT_WILDCARD))

    # MCUBoot flash support location
    MCUBOOT_DIR= $(SEARCH_anycloud-ota)/source/mcuboot

    # build location
    BUILD_LOCATION=./build

    # output directory for use in the sign_script.bash
    OUTPUT_FILE_PATH=$(BUILD_LOCATION)/$(TARGET)/$(CONFIG)

    DEFINES+=OTA_SUPPORT=1 \
        MCUBOOT_HEADER_SIZE=$(MCUBOOT_HEADER_SIZE) \
        MCUBOOT_MAX_IMG_SECTORS=$(MCUBOOT_MAX_IMG_SECTORS) \
        CY_BOOT_SCRATCH_SIZE=$(CY_BOOT_SCRATCH_SIZE) \
        MCUBOOT_IMAGE_NUMBER=1\
        MCUBOOT_BOOTLOADER_SIZE=$(MCUBOOT_BOOTLOADER_SIZE) \
        CY_BOOT_BOOTLOADER_SIZE=$(CY_BOOT_BOOTLOADER_SIZE) \
        CY_BOOT_PRIMARY_1_START=$(CY_BOOT_PRIMARY_1_START) \
        CY_BOOT_PRIMARY_1_SIZE=$(CY_BOOT_PRIMARY_1_SIZE) \
        CY_BOOT_SECONDARY_1_START=$(CY_BOOT_SECONDARY_1_START) \
        CY_BOOT_SECONDARY_1_SIZE=$(CY_BOOT_SECONDARY_1_SIZE) \
        CY_BOOT_PRIMARY_2_SIZE=$(CY_BOOT_PRIMARY_2_SIZE) \
        CY_BOOT_SECONDARY_2_START=$(CY_BOOT_SECONDARY_2_START) \
        CY_FLASH_ERASE_VALUE=$(CY_FLASH_ERASE_VALUE)\
        APP_VERSION_MAJOR=$(APP_VERSION_MAJOR)\
        APP_VERSION_MINOR=$(APP_VERSION_MINOR)\
        APP_VERSION_BUILD=$(APP_VERSION_BUILD)

    ifeq ($(OTA_USE_EXTERNAL_FLASH),1)
        DEFINES+=CY_BOOT_USE_EXTERNAL_FLASH=1
    endif

    CY_HEX_TO_BIN="$(CY_COMPILER_GCC_ARM_DIR)/bin/arm-none-eabi-objcopy"
    CY_BUILD_VERSION=$(APP_VERSION_MAJOR).$(APP_VERSION_MINOR).$(APP_VERSION_BUILD)

    # Use "Create" for PSoC 062 instead of "sign", and no key path (use a space " " for keypath to keep batch happy)
    # MCUBoot must also be modified to skip checking the signature
    #   Comment out and re-build MCUBootApp
    #   <mcuboot>/boot/cypress/MCUBootApp/config/mcuboot_config/mcuboot_config.h
    #   line 37, 38, 77
    # 37: //#define MCUBOOT_SIGN_EC256
    # 38: //#define NUM_ECC_BYTES (256 / 8)   // P-256 curve size in bytes, rnok: to make compilable
    # 77: //#define MCUBOOT_VALIDATE_PRIMARY_SLOT

    ifneq ($(TARGET),MULTI_CM0_CM4)
        # signing scripts and keys from MCUBoot
        # Defaults for 062 non-secure boards
        SIGN_SCRIPT_FILE_PATH=$(SEARCH_anycloud-ota)/scripts/sign_script.bash
        IMGTOOL_SCRIPT_NAME=imgtool_v1.7.0/imgtool.py
        MCUBOOT_SCRIPT_FILE_DIR=$(MCUBOOT_DIR)/scripts
        MCUBOOT_KEY_DIR=$(MCUBOOT_DIR)/keys
        MCUBOOT_KEY_FILE=$(MCUBOOT_KEY_DIR)/cypress-test-ec-p256.pem
        IMGTOOL_COMMAND_ARG=create
        CY_SIGNING_KEY_ARG=""

        POSTBUILD=$(SIGN_SCRIPT_FILE_PATH) $(OUTPUT_FILE_PATH) $(APPNAME) $(CY_PYTHON_PATH)\
                  $(CY_ELF_TO_HEX) $(CY_ELF_TO_HEX_OPTIONS) $(CY_ELF_TO_HEX_FILE_ORDER)\
                  $(MCUBOOT_SCRIPT_FILE_DIR) $(IMGTOOL_SCRIPT_NAME) $(IMGTOOL_COMMAND_ARG) $(CY_FLASH_ERASE_VALUE) $(MCUBOOT_HEADER_SIZE)\
                  $(MCUBOOT_MAX_IMG_SECTORS) $(CY_BUILD_VERSION) $(CY_BOOT_PRIMARY_1_START) $(CY_BOOT_PRIMARY_1_SIZE)\
                  $(CY_HEX_TO_BIN) $(CY_SIGNING_KEY_ARG)
    else
        # preparing for possible multi-image use in future
        SIGN_SCRIPT_FILE_PATH=$(SEARCH_anycloud-ota)/scripts/sign_tar.bash
        MCUBOOT_KEY_DIR=$(CY_MCUBOOT_SCRIPT_FILE_DIR)/keys
        CY_SIGN_SCRIPT_FILE_PATH=$(SEARCH_anycloud-ota)/scripts/sign_tar.bash
        MCUBOOT_KEY_FILE=$(CY_MCUBOOT_KEY_DIR)/cypress-test-ec-p256.pem
        IMGTOOL_COMMAND_ARG=sign
        CY_SIGNING_KEY_ARG="-k $(MCUBOOT_KEY_FILE)"

        POSTBUILD=$(CY_SIGN_SCRIPT_FILE_PATH) $(CY_OUTPUT_FILE_PATH) $(CY_BUILD) $(CY_OBJ_COPY)\
                $(CY_MCUBOOT_SCRIPT_FILE_DIR) $(IMGTOOL_SCRIPT_NAME) $(IMGTOOL_COMMAND_ARG) $(CY_FLASH_ERASE_VALUE) $(MCUBOOT_HEADER_SIZE)\
                $(CY_BUILD_VERSION) $(CY_BOOT_PRIMARY_1_START) $(CY_BOOT_PRIMARY_1_SIZE)\
                $(CY_BOOT_PRIMARY_2_SIZE) $(CY_BOOT_SECONDARY_1_START)\
                $(MCUBOOT_KEY_DIR) $(CY_SIGNING_KEY_ARG)
    endif

endif # OTA Support

################################################################################
# Paths
################################################################################

# Relative path to the project directory (default is the Makefile's directory).
#
# This controls where automatic source code discovery looks for code.
CY_APP_PATH=

# Relative path to the shared repo location.
#
# All .mtb files have the format, <URI>#<COMMIT>#<LOCATION>. If the <LOCATION> field
# begins with $$ASSET_REPO$$, then the repo is deposited in the path specified by
# the CY_GETLIBS_SHARED_PATH variable. The default location is one directory level
# above the current app directory.
# This is used with CY_GETLIBS_SHARED_NAME variable, which specifies the directory name.
CY_GETLIBS_SHARED_PATH=../

# Directory name of the shared repo location.
#
CY_GETLIBS_SHARED_NAME=mtb_shared

# Absolute path to the compiler's "bin" directory.
#
# The default depends on the selected TOOLCHAIN (GCC_ARM uses the ModusToolbox
# IDE provided compiler by default).
CY_COMPILER_PATH=


# Locate ModusToolbox IDE helper tools folders in default installation
# locations for Windows, Linux, and macOS.
CY_WIN_HOME=$(subst \,/,$(USERPROFILE))
CY_TOOLS_PATHS ?= $(wildcard \
    $(CY_WIN_HOME)/ModusToolbox/tools_* \
    $(HOME)/ModusToolbox/tools_* \
    /Applications/ModusToolbox/tools_*)

# If you install ModusToolbox IDE in a custom location, add the path to its
# "tools_X.Y" folder (where X and Y are the version number of the tools
# folder).
CY_TOOLS_PATHS+=

# Default to the newest installed tools folder, or the users override (if it's
# found).
CY_TOOLS_DIR=$(lastword $(sort $(wildcard $(CY_TOOLS_PATHS))))

ifeq ($(CY_TOOLS_DIR),)
$(error Unable to find any of the available CY_TOOLS_PATHS -- $(CY_TOOLS_PATHS). On Windows, use forward slashes.)
endif

$(info Tools Directory: $(CY_TOOLS_DIR))

include $(CY_TOOLS_DIR)/make/start.mk
