/*
 * Copyright (C) OpenTX
 *
 * Based on code named
 *   th9x - http://code.google.com/p/th9x
 *   er9x - http://code.google.com/p/er9x
 *   gruvin9x - http://code.google.com/p/gruvin9x
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _PULSES_H_
#define _PULSES_H_

#include "definitions.h"
#include "dataconstants.h"
#include "pulses_common.h"
#include "pxx1.h"
#include "pxx2.h"
#include "multi.h"
#include "modules_helpers.h"
#include "ff.h"

#if defined(PCBSKY9X) && defined(DSM2)
  #define DSM2_BIND_TIMEOUT      255         // 255*11ms
  extern uint8_t dsm2BindTimer;
#endif

#if defined(DSM2)
  #define IS_DSM2_PROTOCOL(protocol)         (protocol>=PROTOCOL_CHANNELS_DSM2_LP45 && protocol<=PROTOCOL_CHANNELS_DSM2_DSMX)
#else
  #define IS_DSM2_PROTOCOL(protocol)         (0)
#endif

#if defined(DSM2_SERIAL)
  #define IS_DSM2_SERIAL_PROTOCOL(protocol)  (IS_DSM2_PROTOCOL(protocol))
#else
  #define IS_DSM2_SERIAL_PROTOCOL(protocol)  (0)
#endif

#if defined(MULTIMODULE)
  #define IS_MULTIMODULE_PROTOCOL(protocol)  (protocol==PROTOCOL_CHANNELS_MULTIMODULE)
  #if !defined(DSM2)
     #error You need to enable DSM2 = PPM for MULTIMODULE support
  #endif
#else
  #define IS_MULTIMODULE_PROTOCOL(protocol)  (0)
#endif

#define IS_SBUS_PROTOCOL(protocol)         (protocol == PROTOCOL_CHANNELS_SBUS)

extern uint8_t s_pulses_paused;

enum ModuleSettingsMode
{
  MODULE_MODE_NORMAL,
  MODULE_MODE_SPECTRUM_ANALYSER,
  MODULE_MODE_POWER_METER,
  MODULE_MODE_GET_HARDWARE_INFO,
  MODULE_MODE_MODULE_SETTINGS,
  MODULE_MODE_RECEIVER_SETTINGS,
  MODULE_MODE_BEEP_FIRST,
  MODULE_MODE_REGISTER = MODULE_MODE_BEEP_FIRST,
  MODULE_MODE_BIND,
  MODULE_MODE_SHARE,
  MODULE_MODE_RANGECHECK,
  MODULE_MODE_RESET,
  MODULE_MODE_AUTHENTICATION,
  MODULE_MODE_OTA_UPDATE,
};


PACK(struct PXX2Version {
  uint8_t major;
  uint8_t revision:4;
  uint8_t minor:4;
});

PACK(struct PXX2HardwareInformation {
  uint8_t modelID;
  PXX2Version hwVersion;
  PXX2Version swVersion;
  uint8_t variant;
  uint32_t capabilities; // variable length
  uint8_t capabilityNotSupported;
});

PACK(struct ModuleInformation {
  int8_t current;
  int8_t maximum;
  uint8_t timeout;
  PXX2HardwareInformation information;
  struct {
    PXX2HardwareInformation information;
    tmr10ms_t timestamp;
  } receivers[PXX2_MAX_RECEIVERS_PER_MODULE];
});

class ModuleSettings {
  public:
    uint8_t state;  // 0x00 = READ 0x40 = WRITE
    tmr10ms_t timeout;
    uint8_t externalAntenna;
    int8_t txPower;
    uint8_t dirty;
};

class ReceiverSettings {
  public:
    uint8_t state;  // 0x00 = READ 0x40 = WRITE
    tmr10ms_t timeout;
    uint8_t receiverId;
    uint8_t dirty;
    uint8_t telemetryDisabled;
    uint8_t telemetry25mw;
    uint8_t pwmRate;
    uint8_t fport;
    uint8_t enablePwmCh5Ch6;
    uint8_t outputsCount;
    uint8_t outputsMapping[24];
};

class BindInformation {
  public:
    int8_t step;
    uint32_t timeout;
    char candidateReceiversNames[PXX2_MAX_RECEIVERS_PER_MODULE][PXX2_LEN_RX_NAME + 1];
    uint8_t candidateReceiversCount;
    uint8_t selectedReceiverIndex;
    uint8_t rxUid;
    uint8_t lbtMode;
    uint8_t flexMode;
    PXX2HardwareInformation receiverInformation;
};

class OtaUpdateInformation: public BindInformation {
  public:
    char filename[_MAX_LFN + 1];
    uint32_t address;
    uint32_t module;
};

typedef void (* ModuleCallback)();

PACK(struct ModuleState {
  uint8_t protocol:4;
  uint8_t mode:4;
  uint8_t paused:1;
  uint8_t spare:7;
  uint16_t counter;
  union
  {
    ModuleInformation * moduleInformation;
    ModuleSettings * moduleSettings;
    ReceiverSettings * receiverSettings;
    BindInformation * bindInformation;
    OtaUpdateInformation * otaUpdateInformation;
  };
  ModuleCallback callback;

  void startBind(BindInformation * destination, ModuleCallback bindCallback = nullptr);

  void readModuleInformation(ModuleInformation * destination, int8_t first, int8_t last)
  {
    moduleInformation = destination;
    moduleInformation->current = first;
    moduleInformation->maximum = last;
    mode = MODULE_MODE_GET_HARDWARE_INFO;
  }

  void readModuleSettings(ModuleSettings * destination)
  {
    moduleSettings = destination;
    moduleSettings->state = PXX2_SETTINGS_READ;
    mode = MODULE_MODE_MODULE_SETTINGS;
  }

  void writeModuleSettings(ModuleSettings * source)
  {
    moduleSettings = source;
    moduleSettings->state = PXX2_SETTINGS_WRITE;
    moduleSettings->timeout = 0;
    mode = MODULE_MODE_MODULE_SETTINGS;
  }

  void readReceiverSettings(ReceiverSettings * destination)
  {
    receiverSettings = destination;
    receiverSettings->state = PXX2_SETTINGS_READ;
    mode = MODULE_MODE_RECEIVER_SETTINGS;
  }

  void writeReceiverSettings(ReceiverSettings * source)
  {
    receiverSettings = source;
    receiverSettings->state = PXX2_SETTINGS_WRITE;
    receiverSettings->timeout = 0;
    mode = MODULE_MODE_RECEIVER_SETTINGS;
  }
});

extern ModuleState moduleState[NUM_MODULES];

template<class T> struct PpmPulsesData {
  T pulses[20];
  T * ptr;
};

#if defined(PPM_PIN_SERIAL)
PACK(struct Dsm2SerialPulsesData {
  uint8_t  pulses[64];
  uint8_t * ptr;
  uint8_t  serialByte ;
  uint8_t  serialBitCount;
  uint16_t _alignment;
});
typedef Dsm2SerialPulsesData Dsm2PulsesData;
#else
#define MAX_PULSES_TRANSITIONS 300
PACK(struct Dsm2TimerPulsesData {
  pulse_duration_t pulses[MAX_PULSES_TRANSITIONS];
  pulse_duration_t * ptr;
  uint16_t rest;
  uint8_t index;
});
typedef Dsm2TimerPulsesData Dsm2PulsesData;
#endif

#define PPM_PERIOD_HALF_US(module)   ((g_model.moduleData[module].ppm.frameLength * 5 + 225) * 200) /*half us*/
#define PPM_PERIOD(module)           (PPM_PERIOD_HALF_US(module) / 2000) /*ms*/
#define DSM2_BAUDRATE                125000
#define DSM2_PERIOD                  22 /*ms*/
#define SBUS_BAUDRATE                100000
#define SBUS_PERIOD_HALF_US          ((g_model.moduleData[EXTERNAL_MODULE].sbus.refreshRate * 5 + 225) * 200) /*half us*/
#define SBUS_PERIOD                  (SBUS_PERIOD_HALF_US / 2000) /*ms*/
#define MULTIMODULE_BAUDRATE         100000
#define MULTIMODULE_PERIOD           7 /*ms*/

#define CROSSFIRE_FRAME_MAXLEN         64
PACK(struct CrossfirePulsesData {
  uint8_t pulses[CROSSFIRE_FRAME_MAXLEN];
  uint8_t length;
});

union InternalModulePulsesData {
#if defined(PXX1)
#if defined(INTMODULE_USART)
  UartPxx1Pulses pxx_uart;
#else
  PwmPxx1Pulses pxx;
#endif
#endif

#if defined(PXX2)
  Pxx2Pulses pxx2;
#endif

#if defined(MULTIMODULE) //&& defined(INTMODULE_USART)
  UartMultiPulses multi;
#endif

#if defined(INTERNAL_MODULE_PPM)
  PpmPulsesData<pulse_duration_t> ppm;
#endif
} __ALIGNED(4);

union ExternalModulePulsesData {
#if defined(PXX1)
#if defined(HARDWARE_EXTERNAL_MODULE_SIZE_SML)
  UartPxx1Pulses pxx_uart;
#endif
#if defined(PPM_PIN_SERIAL)
  SerialPxx1Pulses pxx;
#else
  PwmPxx1Pulses pxx;
#endif
#endif

#if defined(PXX2)
  Pxx2Pulses pxx2;
#endif

#if defined(DSM2) || defined(MULTIMODULE) || defined(SBUS)
  Dsm2PulsesData dsm2;
#endif

  PpmPulsesData<pulse_duration_t> ppm;

  CrossfirePulsesData crossfire;
} __ALIGNED(4);

/* The __ALIGNED keyword is required to align the struct inside the modulePulsesData below,
 * which is also defined to be __DMA  (which includes __ALIGNED) aligned.
 * Arrays in C/C++ are always defined to be *contiguously*. The first byte of the second element is therefore always
 * sizeof(ModulePulsesData). __ALIGNED is required for sizeof(ModulePulsesData) to be a multiple of the alignment.
 */


extern InternalModulePulsesData intmodulePulsesData;
extern ExternalModulePulsesData extmodulePulsesData;

union TrainerPulsesData {
  PpmPulsesData<trainer_pulse_duration_t> ppm;
};

extern TrainerPulsesData trainerPulsesData;

#if defined(HARDWARE_INTERNAL_MODULE)
bool setupPulsesInternalModule();
#endif
bool setupPulsesExternalModule();
void setupPulsesDSM2();
void setupPulsesCrossfire();
void setupPulsesMultiExternalModule();
void setupPulsesMultiInternalModule();
void setupPulsesSbus();
void setupPulsesPPMInternalModule();
void setupPulsesPPMExternalModule();
void setupPulsesPPMTrainer();
void sendByteDsm2(uint8_t b);
void putDsm2Flush();
void putDsm2SerialBit(uint8_t bit);
void sendByteSbus(uint8_t b);
void intmodulePxx1PulsesStart();
void intmodulePxx1SerialStart();
void extmodulePxx1PulsesStart();
void extmodulePxx1SerialStart();
void extmodulePpmStart();
void intmoduleStop();
void extmoduleStop();

inline void startPulses()
{
  s_pulses_paused = false;

#if defined(HARDWARE_INTERNAL_MODULE)
  setupPulsesInternalModule();
#endif

  setupPulsesExternalModule();

#if defined(HARDWARE_EXTRA_MODULE)
  extramodulePpmStart();
#endif
}

enum ChannelsProtocols {
  PROTOCOL_CHANNELS_UNINITIALIZED,
  PROTOCOL_CHANNELS_NONE,
  PROTOCOL_CHANNELS_PPM,
  PROTOCOL_CHANNELS_PXX1_PULSES,
  PROTOCOL_CHANNELS_PXX1_SERIAL,
  PROTOCOL_CHANNELS_DSM2_LP45,
  PROTOCOL_CHANNELS_DSM2_DSM2,
  PROTOCOL_CHANNELS_DSM2_DSMX,
  PROTOCOL_CHANNELS_CROSSFIRE,
  PROTOCOL_CHANNELS_MULTIMODULE,
  PROTOCOL_CHANNELS_SBUS,
  PROTOCOL_CHANNELS_PXX2_LOWSPEED,
  PROTOCOL_CHANNELS_PXX2_HIGHSPEED,
};

inline void stopPulses()
{
  s_pulses_paused = true;
  moduleState[0].protocol = PROTOCOL_CHANNELS_UNINITIALIZED;
}

inline bool pulsesStarted()
{
  return moduleState[0].protocol != PROTOCOL_CHANNELS_UNINITIALIZED;
}

inline void pausePulses()
{
  s_pulses_paused = true;
}

inline void resumePulses()
{
  s_pulses_paused = false;
}

inline void SEND_FAILSAFE_NOW(uint8_t idx)
{
  moduleState[idx].counter = 1;
}

inline void SEND_FAILSAFE_1S()
{
  for (uint8_t i=0; i<NUM_MODULES; i++) {
    moduleState[i].counter = 100;
  }
}

// Assign failsafe values using the current channel outputs
// for channels not set previously to HOLD or NOPULSE
void setCustomFailsafe(uint8_t moduleIndex);

inline bool isModuleInRangeCheckMode()
{
  if (moduleState[0].mode == MODULE_MODE_RANGECHECK)
    return true;

#if NUM_MODULES > 1
  if (moduleState[1].mode == MODULE_MODE_RANGECHECK)
    return true;
#endif

  return false;
}

inline bool isModuleInBeepMode()
{
  if (moduleState[0].mode >= MODULE_MODE_BEEP_FIRST)
    return true;

#if NUM_MODULES > 1
  if (moduleState[1].mode >= MODULE_MODE_BEEP_FIRST)
    return true;
#endif

  return false;
}

#endif // _PULSES_H_