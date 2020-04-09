option(DISK_CACHE "Enable SD card disk cache" ON)
option(SHUTDOWN_CONFIRMATION "Shutdown confirmation" OFF)
option(LCD_DUAL_BUFFER "Dual LCD Buffer" OFF)
option(PXX1 "PXX1 protocol support" ON)
option(PXX2 "PXX2 protocol support" OFF)
option(INTERNAL_MODULE_PPM "Support for PPM internal module" OFF)
option(AUTOUPDATE "Auto update internal chips from SD" OFF)

#    set(HAPTIC YES)
set(LUA_EXPORT lua_export_x9d)
add_definitions(-DPCB_DEVEBOX)
add_definitions(-DEEPROM_VARIANT=0)
set(GUI_DIR 128x64)
set(BITMAPS_TARGET 9x_bitmaps)
set(FONTS_TARGET 9x_fonts_1bit)
set(LCD_DRIVER lcd_driver_spi.cpp)
set(GVAR_SCREEN model_gvars.cpp)
set(PCBREV 2019 CACHE STRING "PCB Revision")
add_definitions(-DPCBREV=${PCBREV})
if (${PCBREV} STREQUAL 2019)
option(INTERNAL_MODULE_PXX1 "Support for PXX1 internal module" OFF)
option(INTERNAL_MODULE_PXX2 "Support for PXX2 internal module" OFF)
set(FLAVOUR CHOPS_2019)
set(NAVIGATION_TYPE x7)
set(CPU_TYPE STM32F4)
set(CPU_TYPE_FULL STM32F407xE)  # for size report
set(LINKER_SCRIPT targets/bhim/stm32f4_flash.ld)
set(PWR_BUTTON "SWITCH" CACHE STRING "Pwr button type (PRESS/SWITCH)")
set(FIRMWARE_TARGET_SRC
${FIRMWARE_TARGET_SRC}
startup_stm32f40_41xxx.s
)
#      add_definitions(-DRADIO_X9DP2019)
add_definitions(-DSTM32F40_41xxx)
add_definitions(-DSOFTWARE_VOLUME -DPWR_BUTTON_SWITCH -DHARDWARE_POWER_MANAGEMENT_UNIT)
set(STATUS_LEDS NO)
set(PXX2 OFF)
set(PXX1 OFF)
#      set(PXX_FREQUENCY "HIGH")
#      set(BLUETOOTH YES)
set(ROTARY_ENCODER YES)
set(INTERNAL_MODULE_SERIAL YES)
endif()

if(GYRO)
add_definitions(-DGYRO)
set(TARGET_SRC
${TARGET_SRC}
../common/arm/stm32/lsm6ds_driver.cpp
)
set(SRC
${SRC}
gyro.cpp
)
endif()

if(INTERNAL_MODULE_SERIAL)
add_definitions(-DINTERNAL_MODULE_SERIAL)
if(PXX_FREQUENCY STREQUAL HIGH)
add_definitions(-DPXX_FREQUENCY_HIGH)
endif()
set(TARGET_SRC
${TARGET_SRC}
../common/arm/stm32/intmodule_serial_driver.cpp
)
elseif(NOT (PCB STREQUAL X7 AND PCBREV STREQUAL T12))
set(TARGET_SRC
${TARGET_SRC}
intmodule_pulses_driver.cpp
)
endif()

if(ROTARY_ENCODER)
set(TARGET_SRC
${TARGET_SRC}
../common/arm/stm32/rotary_encoder_driver.cpp
)
endif()

if(BLUETOOTH)
add_definitions(-DBLUETOOTH)
set(TARGET_SRC
${TARGET_SRC}
../common/arm/stm32/bluetooth_driver.cpp
)
set(SRC
${SRC}
bluetooth.cpp
)
endif()

if(AUTOUPDATE)
add_definitions(-DAUTOUPDATE)
endif()

set(HSE_VALUE 12000000)
set(SDCARD YES)
set(EEPROM EEPROM_RLC)
set(TARGET_DIR bhim)
set(PPM_LIMITS_SYMETRICAL YES)

if(SPLASH STREQUAL OFF)
message(FATAL_ERROR "Option SPLASH=OFF not supported on Taranis")
endif()

add_definitions(-DPCBTARANIS)
add_definitions(-DAUDIO -DVOICE -DRTCLOCK)

set(SRC
${SRC}
io/frsky_firmware_update.cpp
io/multi_firmware_update.cpp
)

set(GUI_SRC
${GUI_SRC}
model_input_edit.cpp
model_mix_edit.cpp
model_display.cpp
radio_diagkeys.cpp
radio_diaganas.cpp
view_channels.cpp
view_telemetry.cpp
view_about.cpp
bmp.cpp
)

if(STATUS_LEDS)
set(LED_DRIVER led_driver.cpp)
endif()

set(SRC
${SRC}
sbus.cpp
)

set(TARGET_SRC
${TARGET_SRC}
${LED_DRIVER}
backlight_driver.cpp
extmodule_driver.cpp
trainer_driver.cpp
../common/arm/stm32/timers_driver.cpp
../common/arm/stm32/audio_dac_driver.cpp
../common/arm/stm32/adc_driver.cpp
../common/arm/stm32/heartbeat_driver.cpp
)

if(PCB STREQUAL XLITE OR PCB STREQUAL XLITES)
set(TARGET_SRC
${TARGET_SRC}
../common/arm/stm32/sticks_pwm_driver.cpp
)
endif()

set(FIRMWARE_TARGET_SRC
${FIRMWARE_TARGET_SRC}
${LCD_DRIVER}
board.cpp
i2c_driver.cpp
../common/arm/stm32/pwr_driver.cpp
../common/arm/stm32/sdio_sd.c
../common/arm/loadboot.cpp
)

set(STM32LIB_SRC
    STM32F4xx_StdPeriph_Driver/src/stm32f4xx_sdio.c
    )

if(LCD_DUAL_BUFFER)
add_definitions(-DLCD_DUAL_BUFFER)
endif()

if(INTERNAL_MODULE_PXX1)
add_definitions(-DHARDWARE_INTERNAL_MODULE)
add_definitions(-DINTERNAL_MODULE_PXX1)
endif()

if(INTERNAL_MODULE_PXX2)
set(PXX2 ON)
add_definitions(-DHARDWARE_INTERNAL_MODULE)
add_definitions(-DINTERNAL_MODULE_PXX2)
endif()

if(INTERNAL_MODULE_MULTI)
add_definitions(-DHARDWARE_INTERNAL_MODULE)
add_definitions(-DINTERNAL_MODULE_MULTI)
set(TARGET_SRC
${TARGET_SRC}
../common/arm/stm32/intmodule_serial_driver.cpp
)
endif()

if(DISK_CACHE)
  set(SRC ${SRC} disk_cache.cpp)
  add_definitions(-DDISK_CACHE)
endif()

if(INTERNAL_MODULE_PPM)
add_definitions(-DHARDWARE_INTERNAL_MODULE)
add_definitions(-DINTERNAL_MODULE_PPM)
endif()

set(RADIO_DEPENDENCIES ${RADIO_DEPENDENCIES} ${BITMAPS_TARGET})
