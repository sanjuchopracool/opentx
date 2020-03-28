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

#include "opentx.h"
#include "frsky_firmware_update.h"

#define PRIM_REQ_POWERUP    0
#define PRIM_REQ_VERSION    1
#define PRIM_CMD_DOWNLOAD   3
#define PRIM_DATA_WORD      4
#define PRIM_DATA_EOF       5

#define PRIM_ACK_POWERUP    0x80
#define PRIM_ACK_VERSION    0x81
#define PRIM_REQ_DATA_ADDR  0x82
#define PRIM_END_DOWNLOAD   0x83
#define PRIM_DATA_CRC_ERR   0x84

const char * readFrSkyFirmwareInformation(const char * filename, FrSkyFirmwareInformation & data)
{
  FIL file;
  UINT count;

  if (f_open(&file, filename, FA_READ) != FR_OK) {
    return "Error opening file";
  }

  if (f_read(&file, &data, sizeof(data), &count) != FR_OK || count != sizeof(data)) {
    f_close(&file);
    return "Error reading file";
  }

  uint32_t size = f_size(&file);
  f_close(&file);

  if (data.headerVersion != 1 && data.fourcc != 0x4B535246) {
    return "Wrong format";
  }

  if (size != sizeof(data) + data.size) {
    return "Wrong size";
  }

  return nullptr;
}

void FrskyDeviceFirmwareUpdate::processFrame(const uint8_t * frame)
{
  if (frame[0] == 0x5E && frame[1] == 0x50) {
    switch (frame[2]) {
      case PRIM_ACK_POWERUP :
        if (state == SPORT_POWERUP_REQ) {
          state = SPORT_POWERUP_ACK;
        }
        break;

      case PRIM_ACK_VERSION:
        if (state == SPORT_VERSION_REQ) {
          state = SPORT_VERSION_ACK;
          // here we could display the version
        }
        break;

      case PRIM_REQ_DATA_ADDR:
        if (state == SPORT_DATA_TRANSFER) {
          address = *((uint32_t *)(&frame[3]));
          state = SPORT_DATA_REQ;
        }
        break;

      case PRIM_END_DOWNLOAD :
        state = SPORT_COMPLETE ;
        break;

      case PRIM_DATA_CRC_ERR :
        state = SPORT_FAIL ;
        break;
    }
  }
}

#if defined(PCBHORUS)
bool FrskyDeviceFirmwareUpdate::readBuffer(uint8_t * buffer, uint8_t count, uint32_t timeout)
{
  watchdogSuspend(timeout);

  switch (module) {
    case INTERNAL_MODULE:
    {
      uint32_t elapsed = 0;
      uint8_t index = 0;
      while (index < count && elapsed < timeout) {
        if (intmoduleFifo.pop(buffer[index])) {
          ++index;
        }
        else {
          RTOS_WAIT_MS(1);
          if (++elapsed == timeout)
            return false;
        }
      }
      break;
    }

    default:
      break;
  }

  return true;
}
#endif

const uint8_t * FrskyDeviceFirmwareUpdate::readFullDuplexFrame(ModuleFifo & fifo, uint32_t timeout)
{
  uint8_t len = 0;
  bool bytestuff = false;
  while (len < 10) {
    uint32_t elapsed = 0;
    uint8_t byte;
    while (!fifo.pop(byte)) {
      RTOS_WAIT_MS(1);
      if (elapsed++ >= timeout) {
        return nullptr;
      }
    }
    if (byte == 0x7D) {
      bytestuff = true;
      continue;
    }
    if (bytestuff) {
      frame[len] = 0x20 ^ byte;
      bytestuff = false;
    }
    else {
      frame[len] = byte;
    }
    if (len > 0 || byte == 0x7E) {
      ++len;
    }
  }
  return &frame[1];
}

const uint8_t * FrskyDeviceFirmwareUpdate::readHalfDuplexFrame(uint32_t timeout)
{
  for (int i=timeout; i>=0; i--) {
    uint8_t byte ;
    while (telemetryGetByte(&byte)) {
      if (pushFrskyTelemetryData(byte)) {
        return telemetryRxBuffer;
      }
    }
    RTOS_WAIT_MS(1);
  }
  return nullptr;
}

const uint8_t * FrskyDeviceFirmwareUpdate::readFrame(uint32_t timeout)
{
  RTOS_WAIT_MS(1);

  switch (module) {
#if defined(INTERNAL_MODULE_PXX2)
    case INTERNAL_MODULE:
      return readFullDuplexFrame(intmoduleFifo, timeout);
#endif

    default:
      return readHalfDuplexFrame(timeout);
  }
}

bool FrskyDeviceFirmwareUpdate::waitState(State newState, uint32_t timeout)
{
#if defined(SIMU)
  UNUSED(state);
  UNUSED(timeout);
  static uint8_t pass = 0;
  if (++pass == 10) {
    pass = 0;
    RTOS_WAIT_MS(1);
  }
  return true;
#else
  watchdogSuspend(timeout / 10);

  const uint8_t * frame = readFrame(timeout);
  if (!frame) {
    return false;
  }

  processFrame(frame);
  return state == newState;
#endif
}

void FrskyDeviceFirmwareUpdate::startFrame(uint8_t command)
{
  frame[0] = 0x50;
  frame[1] = command;
  memset(&frame[2], 0, 6);
}

// TODO merge this function
void FrskyDeviceFirmwareUpdate::sendFrame()
{
  uint8_t * ptr = outputTelemetryBuffer.data;
  *ptr++ = 0x7E;
  *ptr++ = 0xFF;
  frame[7] = crc16(CRC_1021, frame, 7);
  for (int i=0; i<8; i++) {
    if (frame[i] == 0x7E || frame[i] == 0x7D) {
      *ptr++ = 0x7D;
      *ptr++ = 0x20 ^ frame[i];
    }
    else {
      *ptr++ = frame[i];
    }
  }

  switch (module) {
#if defined(INTERNAL_MODULE_PXX2)
    case INTERNAL_MODULE:
      return intmoduleSendBuffer(outputTelemetryBuffer.data, ptr - outputTelemetryBuffer.data);
#endif

    default:
      return sportSendBuffer(outputTelemetryBuffer.data, ptr - outputTelemetryBuffer.data);
  }
}

const char * FrskyDeviceFirmwareUpdate::sendPowerOn()
{
  state = SPORT_POWERUP_REQ;

  RTOS_WAIT_MS(50);
  telemetryClearFifo();

  for (int i=0; i<10; i++) {
    // max 10 attempts
    startFrame(PRIM_REQ_POWERUP);
    sendFrame();
    if (waitState(SPORT_POWERUP_ACK, 100))
      return nullptr;
  }

  if (telemetryProtocol != PROTOCOL_TELEMETRY_FRSKY_SPORT) {
    return TR("Not responding", "Not S.Port 1");
  }

  if (!IS_FRSKY_SPORT_PROTOCOL()) {
    return TR("Not responding", "Not S.Port 2");
  }

  return TR("Not responding", "Device not responding");
}

const char * FrskyDeviceFirmwareUpdate::sendReqVersion()
{
  RTOS_WAIT_MS(20);
  telemetryClearFifo();

  state = SPORT_VERSION_REQ;
  for (int i=0; i<10; i++) {
    // max 10 attempts
    startFrame(PRIM_REQ_VERSION) ;
    sendFrame();
    if (waitState(SPORT_VERSION_ACK, 100))
      return nullptr;
  }
  return "Version request failed";
}

// X12S / X10 IXJT = use TX + RX @ 38400 bauds with BOOTCMD pin inverted
// X10 / X10 ISRM = use TX + RX @ 57600 bauds (no BOOTCMD)
// X9D / X9D+ / X9E / XLite IXJT = use S.PORT @ 57600 bauds
// XLite PRO / X9Lite / X9D+ 2019 ISRM = use TX + RX @ 57600 bauds

const char * FrskyDeviceFirmwareUpdate::doFlashFirmware(const char * filename)
{
  FIL file;
  const char * result;
  FrSkyFirmwareInformation information;
  UINT count;

  if (f_open(&file, filename, FA_READ) != FR_OK) {
    return "Error opening file";
  }

  const char * ext = getFileExtension(filename);
  if (ext && !strcasecmp(ext, FRSKY_FIRMWARE_EXT)) {
    if (f_read(&file, &information, sizeof(FrSkyFirmwareInformation), &count) != FR_OK || count != sizeof(FrSkyFirmwareInformation)) {
      f_close(&file);
      return "Format error";
    }
  }
  else {
#if defined(PCBHORUS)
    information.productId = FIRMWARE_ID_MODULE_XJT;
#endif
  }

#if defined(PCBHORUS)
  if (module == INTERNAL_MODULE && information.productId == FIRMWARE_ID_MODULE_XJT) {
    INTERNAL_MODULE_ON();
    RTOS_WAIT_MS(1);
    intmoduleSerialStart(38400, true, USART_Parity_No, USART_StopBits_1, USART_WordLength_8b);
    GPIO_SetBits(INTMODULE_BOOTCMD_GPIO, INTMODULE_BOOTCMD_GPIO_PIN);
    result = uploadFileToHorusXJT(filename, &file);
    GPIO_ResetBits(INTMODULE_BOOTCMD_GPIO, INTMODULE_BOOTCMD_GPIO_PIN);
    f_close(&file);
    return result;
  }
#endif

  switch (module) {
#if defined(INTERNAL_MODULE_PXX2)
    case INTERNAL_MODULE:
      intmoduleSerialStart(57600, true, USART_Parity_No, USART_StopBits_1, USART_WordLength_8b);
      break;
#endif

    default:
      telemetryInit(PROTOCOL_TELEMETRY_FRSKY_SPORT);
      break;
  }

  if (module == INTERNAL_MODULE)
    INTERNAL_MODULE_ON();
  else if (module == EXTERNAL_MODULE)
    EXTERNAL_MODULE_ON();
  else
    SPORT_UPDATE_POWER_ON();

  result = uploadFileNormal(filename, &file);
  f_close(&file);
  return result;
}

#if defined(PCBHORUS)
const char * FrskyDeviceFirmwareUpdate::uploadFileToHorusXJT(const char * filename, FIL * file)
{
  uint32_t buffer[1024 / sizeof(uint32_t)];
  UINT count;
  uint8_t frame[8];

  if (!readBuffer(frame, 8, 100) || frame[0] != 0x01) {
    return TR("Not responding", "Device not responding");
  }

  intmoduleSendByte(0x81);
  readBuffer(frame, 1, 100);

  if (!readBuffer(frame, 8, 100) || frame[0] != 0x02) {
    return TR("Not responding", "Device not responding");
  }

  intmoduleSendByte(0x82);
  readBuffer(frame, 1, 100);

  uint8_t index = 0;
  while (true) {
    drawProgressScreen(getBasename(filename), STR_WRITING, file->fptr, file->obj.objsize);

    if (f_read(file, buffer, 1024, &count) != FR_OK) {
      return "Error reading file";
    }

    if (!readBuffer(frame, 2, 100))
      return "Data refused";

    if (frame[0] != 0x11 || frame[1] != index)
      return "Wrong request";

    if (count == 0) {
      intmoduleSendByte(0xA1);
      RTOS_WAIT_MS(50);
      return nullptr;
    }

    if (count < 1024)
      memset(((uint8_t *)buffer) + count, 0, 1024 - count);

    intmoduleSendByte(frame[0] + 0x80);
    intmoduleSendByte(frame[1]);

    uint16_t crc_16 = crc16(CRC_1189, (uint8_t *)buffer, 1024, crc16(CRC_1189, &frame[1], 1));
    for (size_t i = 0; i < sizeof(buffer); i++) {
      intmoduleSendByte(((uint8_t *)buffer)[i]);
    }
    intmoduleSendByte(crc_16 >> 8);
    intmoduleSendByte(crc_16);

    index++;
  }
}
#endif

const char * FrskyDeviceFirmwareUpdate::uploadFileNormal(const char * filename, FIL * file)
{
  uint32_t buffer[1024 / sizeof(uint32_t)];
  UINT count;

  const char * result = sendPowerOn();
  if (result)
    return result;

  result = sendReqVersion();
  if (result)
    return result;

  RTOS_WAIT_MS(200);
  telemetryClearFifo();

  state = SPORT_DATA_TRANSFER;
  startFrame(PRIM_CMD_DOWNLOAD);
  sendFrame();

  while (true) {
    if (f_read(file, buffer, 1024, &count) != FR_OK) {
      return "Error reading file";
    }

    count >>= 2;

    for (uint32_t i=0; i<count; i++) {
      if (!waitState(SPORT_DATA_REQ, 2000)) {
        return "Data refused";
      }
      startFrame(PRIM_DATA_WORD);
      uint32_t offset = (address & 1023) >> 2; // 32 bit word offset into buffer
      *((uint32_t *)(frame + 2)) = buffer[offset];
      frame[6] = address & 0x000000FF;
      state = SPORT_DATA_TRANSFER,
      sendFrame();
      if (i == 0) {
        drawProgressScreen(getBasename(filename), STR_WRITING, file->fptr, file->obj.objsize);
      }
    }

    if (count < 256) {
      break;
    }
  }

  return endTransfer();
}

const char * FrskyDeviceFirmwareUpdate::endTransfer()
{
  if (!waitState(SPORT_DATA_REQ, 2000))
    return "Data refused";
  startFrame(PRIM_DATA_EOF);
  sendFrame();
  if (!waitState(SPORT_COMPLETE, 2000)) {
    return "Firmware rejected";
  }
  return nullptr;
}

const char * FrskyDeviceFirmwareUpdate::flashFirmware(const char * filename)
{
  pausePulses();

#if defined(HARDWARE_INTERNAL_MODULE)
  uint8_t intPwr = IS_INTERNAL_MODULE_ON();
  INTERNAL_MODULE_OFF();
#endif

  uint8_t extPwr = IS_EXTERNAL_MODULE_ON();
  EXTERNAL_MODULE_OFF();

  uint8_t spuPwr = IS_SPORT_UPDATE_POWER_ON();
  SPORT_UPDATE_POWER_OFF();

  drawProgressScreen(getBasename(filename), STR_DEVICE_RESET, 0, 0);

  /* wait 2s off */
  watchdogSuspend(1000 /*10s*/);
  RTOS_WAIT_MS(2000);

  const char * result = doFlashFirmware(filename);

  AUDIO_PLAY(AU_SPECIAL_SOUND_BEEP1 );
  BACKLIGHT_ENABLE();

  if (result) {
    POPUP_WARNING(STR_FIRMWARE_UPDATE_ERROR);
    SET_WARNING_INFO(result, strlen(result), 0);
  }
  else {
    POPUP_INFORMATION(STR_FIRMWARE_UPDATE_SUCCESS);
  }

#if defined(HARDWARE_INTERNAL_MODULE)
  INTERNAL_MODULE_OFF();
#endif
  EXTERNAL_MODULE_OFF();
  SPORT_UPDATE_POWER_OFF();

  /* wait 2s off */
  watchdogSuspend(500 /*5s*/);
  RTOS_WAIT_MS(2000);
  telemetryClearFifo();

#if defined(HARDWARE_INTERNAL_MODULE)
  if (intPwr) {
    INTERNAL_MODULE_ON();
    setupPulsesInternalModule();
  }
#endif

  if (extPwr) {
    EXTERNAL_MODULE_ON();
    setupPulsesExternalModule();
  }

  if (spuPwr) {
    SPORT_UPDATE_POWER_ON();
  }

  state = SPORT_IDLE;
  resumePulses();

  return result;
}

#define CHIP_FIRMWARE_UPDATE_TIMEOUT  20000 /* 20s */

const char * FrskyChipFirmwareUpdate::waitAnswer(uint8_t & status)
{
  watchdogSuspend(CHIP_FIRMWARE_UPDATE_TIMEOUT);

  telemetryPortSetDirectionInput();

  uint8_t buffer[12];
  for (uint8_t i = 0; i < sizeof(buffer); i++) {
    uint32_t retry = 0;
    while (true) {
      if (telemetryGetByte(&buffer[i])) {
        if ((i == 0 && buffer[0] != 0x7F) ||
            (i == 1 && buffer[1] != 0xFE) ||
            (i == 10 && buffer[10] != 0x0D) ||
            (i == 11 && buffer[11] != 0x0A)) {
          i = 0;
          continue;
        }
        break;
      }
      if (++retry == CHIP_FIRMWARE_UPDATE_TIMEOUT) {
        return "No answer";
      }
      RTOS_WAIT_MS(1);
    }
  }
  status = buffer[8];
  return nullptr;
}

const char * FrskyChipFirmwareUpdate::startBootloader()
{
  sportSendByte(0x03);
  RTOS_WAIT_MS(20);
  sportSendByte(0x02);
  RTOS_WAIT_MS(20);
  sportSendByte(0x01);

  for (uint8_t i = 0; i < 30; i++)
    sportSendByte(0x7E);

  for (uint32_t i = 0; i < 100; i++) {
    RTOS_WAIT_MS(20);
    sportSendByte(0x7F);
  }

  RTOS_WAIT_MS(20);
  sportSendByte(0xFA);

  /*for (uint8_t i=0; i < 30; i++)
    sportSendByte(0x7E);
  for (uint8_t i=0; i < 50; i++)
    sportSendByte(0x7F);*/

  uint8_t status;
  const char * result = waitAnswer(status);
  if (result)
    return result;

  return status == 0x08 ? nullptr : "Bootloader failed";
}

void FrskyChipFirmwareUpdate::sendByte(uint8_t byte, bool crcFlag)
{
  sportSendByte(byte);
  if (crcFlag) {
    crc ^= byte;
  }
}

const char * FrskyChipFirmwareUpdate::sendUpgradeCommand(char command, uint32_t packetsCount)
{
  crc = 0;

  // Head
  sendByte(0x7F, false);
  sendByte(0xFE, false);

  // Addr
  sendByte(0xFA);

  // Cmd
  sendByte(command);

  // Packets count
  sendByte(packetsCount >> 8);
  sendByte(packetsCount);

  // Len
  sendByte('E' == command ? 0x00 : 0x0C);
  sendByte(0x40);

  // Data
  for (uint8_t i=0; i < 0x40; i++)
    sendByte('E' == command ? 0xF7 : 0x7F);

  // Checksum
  sendByte(crc, false);

  // Tail
  sendByte(0x0D, false);
  sendByte(0x0A, false);

  uint8_t status;
  const char * result = waitAnswer(status);
  if (result)
    return result;

  return status == 0x00 ? nullptr : "Upgrade failed";
}

const char * FrskyChipFirmwareUpdate::sendUpgradeData(uint32_t index, uint8_t * data)
{
  crc = 0;

  // Head
  sendByte(0x7F, false);
  sendByte(0xFE, false);

  // Addr
  sendByte(0xFA);

  // Cmd
  sendByte('W');

  // Packets count
  sendByte(index >> 8);
  sendByte(index);

  // Len
  sendByte(0x00);
  sendByte(0x40);

  // Data
  for (uint8_t i = 0; i < 0x40; i++)
    sendByte(*data++);

  // Checksum
  sendByte(crc, false);

  // Tail
  sendByte(0x0D, false);
  sendByte(0x0A, false);

  uint8_t status;
  const char * result = waitAnswer(status);
  if (result)
    return result;

  return status == 0x00 ? nullptr : "Upgrade failed";
}

const char * FrskyChipFirmwareUpdate::doFlashFirmware(const char * filename)
{
  const char * result;
  FIL file;
  uint8_t buffer[64];
  UINT count;

  result = startBootloader();
  if (result)
    return result;

  if (f_open(&file, filename, FA_READ) != FR_OK) {
    return "Error opening file";
  }

  FrSkyFirmwareInformation * information = (FrSkyFirmwareInformation *)buffer;
  if (f_read(&file, buffer, sizeof(FrSkyFirmwareInformation), &count) != FR_OK || count != sizeof(FrSkyFirmwareInformation)) {
    f_close(&file);
    return "Format error";
  }

  uint32_t packetsCount = (information->size + sizeof(buffer) - 1) / sizeof(buffer);
  drawProgressScreen(getBasename(filename), STR_FLASH_WRITE, 0, packetsCount);

  result = sendUpgradeCommand('A', packetsCount);
  if (result)
    return result;

  uint32_t index = 0;
  while (1) {
    drawProgressScreen(getBasename(filename), STR_FLASH_WRITE, index, packetsCount);
    if (f_read(&file, buffer, sizeof(buffer), &count) != FR_OK) {
      f_close(&file);
      return "Error reading file";
    }
    result = sendUpgradeData(index + 1, buffer);
    if (result)
      return result;
    if (++index == packetsCount)
      break;
  }

  f_close(&file);

  return sendUpgradeCommand('E', packetsCount);
}

const char * FrskyChipFirmwareUpdate::flashFirmware(const char * filename, bool wait)
{
  drawProgressScreen(getBasename(filename), STR_DEVICE_RESET, 0, 0);

  pausePulses();

#if defined(HARDWARE_INTERNAL_MODULE)
  uint8_t intPwr = IS_INTERNAL_MODULE_ON();
  INTERNAL_MODULE_OFF();
#endif

  uint8_t extPwr = IS_EXTERNAL_MODULE_ON();
  EXTERNAL_MODULE_OFF();

  uint8_t spuPwr = IS_SPORT_UPDATE_POWER_ON();
  SPORT_UPDATE_POWER_OFF();

  if (wait) {
    /* wait 2s off */
    watchdogSuspend(1000 /*10s*/);
    RTOS_WAIT_MS(2000);
  }

  telemetryInit(PROTOCOL_TELEMETRY_FRSKY_SPORT);

  const char * result = doFlashFirmware(filename);

  AUDIO_PLAY(AU_SPECIAL_SOUND_BEEP1);
  BACKLIGHT_ENABLE();

  if (result) {
    POPUP_WARNING(STR_FIRMWARE_UPDATE_ERROR);
    SET_WARNING_INFO(result, strlen(result), 0);
  }
  else {
    POPUP_INFORMATION(STR_FIRMWARE_UPDATE_SUCCESS);
  }

  /* wait 2s off */
  watchdogSuspend(1000 /*10s*/);
  RTOS_WAIT_MS(2000);

#if defined(HARDWARE_INTERNAL_MODULE)
  if (intPwr) {
    INTERNAL_MODULE_ON();
    setupPulsesInternalModule();
  }
#endif

  if (extPwr) {
    EXTERNAL_MODULE_ON();
    setupPulsesExternalModule();
  }

  if (spuPwr) {
    SPORT_UPDATE_POWER_ON();
  }

  resumePulses();

  return result;
}
