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
#include <math.h>

#if defined(MULTIMODULE)
void lcdDrawMultiProtocolString(coord_t x, coord_t y, uint8_t moduleIdx, uint8_t protocol, LcdFlags flags)
{
  if (protocol <= MODULE_SUBTYPE_MULTI_LAST) {
    lcdDrawTextAtIndex(x, y, STR_MULTI_PROTOCOLS, protocol, flags);
    return;
  }
  else {
    MultiModuleStatus &status = getMultiModuleStatus(moduleIdx);
    if (status.protocolName[0] && status.isValid())
      lcdDrawText(x, y, status.protocolName, flags);
    else
      lcdDrawNumber(x, y, protocol + 3, flags); // Convert because of OpenTX FrSky fidling (OpenTX protocol tables and Multiprotocol tables don't match)
  }
}

void lcdDrawMultiSubProtocolString(coord_t x, coord_t y, uint8_t moduleIdx, uint8_t subType, LcdFlags flags)
{
  const mm_protocol_definition *pdef = getMultiProtocolDefinition(g_model.moduleData[moduleIdx].getMultiProtocol());
  if (subType <= pdef->maxSubtype && pdef->subTypeString != nullptr) {
    lcdDrawTextAtIndex(x, y, pdef->subTypeString, subType, flags);
    return;
  }
  else {
    MultiModuleStatus &status = getMultiModuleStatus(moduleIdx);
    if (status.protocolName[0] && status.isValid())
      lcdDrawText(x, y, status.protocolSubName, flags);
    else
      lcdDrawNumber(x, y, subType, flags);
  }
}
#endif

void drawStringWithIndex(coord_t x, coord_t y, const char * str, uint8_t idx, LcdFlags flags)
{
  if (flags & RIGHT) {
    lcdDrawNumber(x, y, idx, flags);
    lcdDrawText(lcdNextPos, y, str, flags & ~LEADING0);
  }
  else {
    lcdDrawText(x, y, str, flags & ~LEADING0);
    lcdDrawNumber(lcdNextPos, y, idx, flags|LEFT, 2);
  }
}

void drawTrimMode(coord_t x, coord_t y, uint8_t flightMode, uint8_t idx, LcdFlags att)
{
  trim_t v = getRawTrimValue(flightMode, idx);
  unsigned int mode = v.mode;
  unsigned int p = mode >> 1;

  if (mode == TRIM_MODE_NONE) {
    lcdDrawText(x, y, "--", att);
  }
  else {
    if (mode % 2 == 0)
      lcdDrawChar(x, y, ':', att|FIXEDWIDTH);
    else
      lcdDrawChar(x, y, '+', att|FIXEDWIDTH);
    lcdDrawChar(lcdNextPos, y, '0'+p, att);
  }
}

void drawValueWithUnit(coord_t x, coord_t y, int32_t val, uint8_t unit, LcdFlags att)
{
  // convertUnit(val, unit);
  lcdDrawNumber(x, y, val, att & (~NO_UNIT));
  if (!(att & NO_UNIT) && unit != UNIT_RAW) {
    lcdDrawTextAtIndex(lcdLastRightPos/*+1*/, y, STR_VTELEMUNIT, unit, 0);
  }
}

FlightModesType editFlightModes(coord_t x, coord_t y, event_t event, FlightModesType value, uint8_t attr)
{
  int posHorz = menuHorizontalPosition;

  for (uint8_t p=0; p<MAX_FLIGHT_MODES; p++) {
    LcdFlags flags = 0;
    if (attr) {
      flags |= INVERS;
      if (posHorz==p) flags |= BLINK;
    }
    if (value & (1<<p))
      lcdDrawChar(x, y, ' ', flags|FIXEDWIDTH);
    else
      lcdDrawChar(x, y, '0'+p, flags);
    x += FW;
  }

  if (attr) {
    if (s_editMode && event==EVT_KEY_BREAK(KEY_ENTER)) {
      s_editMode = 0;
      value ^= (1<<posHorz);
      storageDirty(EE_MODEL);
    }
  }

  return value;
}

void editName(coord_t x, coord_t y, char * name, uint8_t size, event_t event, uint8_t active, LcdFlags attr)
{
  uint8_t mode = 0;
  if (active) {
    if (s_editMode <= 0)
      mode = INVERS + FIXEDWIDTH;
    else
      mode = FIXEDWIDTH;
  }

  lcdDrawSizedText(x, y, name, size, attr | mode);
  coord_t backupNextPos = lcdNextPos;

  if (active) {
    uint8_t cur = editNameCursorPos;
    if (s_editMode > 0) {
      int8_t c = name[cur];
      int8_t v = c;

      if (IS_NEXT_EVENT(event) || IS_PREVIOUS_EVENT(event)) {
        if (attr == ZCHAR) {
          v = checkIncDec(event, abs(v), 0, ZCHAR_MAX, 0);
          if (c <= 0) v = -v;
        }
        else {
          v = checkIncDec(event, abs(v), '0', 'z', 0);
        }
      }

      switch (event) {
        case EVT_KEY_BREAK(KEY_ENTER):
          if (s_editMode == EDIT_MODIFY_FIELD) {
            s_editMode = EDIT_MODIFY_STRING;
            cur = 0;
          }
          else if (cur < size - 1)
            cur++;
          else
            s_editMode = 0;
          break;

#if defined(NAVIGATION_XLITE) || defined(NAVIGATION_9X)
        case EVT_KEY_BREAK(KEY_LEFT):
          if (cur>0) cur--;
          break;

        case EVT_KEY_BREAK(KEY_RIGHT):
          if (cur<size-1) cur++;
          break;
#endif

#if defined(NAVIGATION_XLITE)
        case EVT_KEY_BREAK(KEY_SHIFT):
#elif defined(NAVIGATION_9X)
        case EVT_KEY_LONG(KEY_LEFT):
        case EVT_KEY_LONG(KEY_RIGHT):
#else
        case EVT_KEY_LONG(KEY_ENTER):
#endif

          if (attr & ZCHAR) {
#if defined(PCBTARANIS) && !defined(PCBXLITE)
            if (v == 0) {
              s_editMode = 0;
              killEvents(event);
            }
#endif
            if (v >= -26 && v <= 26) {
              v = -v; // toggle case
            }
          }
          else {
#if !defined(NAVIGATION_XLITE)
            if (v == ' ') {
              s_editMode = 0;
              killEvents(event);
              break;
            }
            else
#endif
            if (v >= 'A' && v <= 'Z') {
              v = 'a' + v - 'A'; // toggle case
            }
            else if (v >= 'a' && v <= 'z') {
              v = 'A' + v - 'a'; // toggle case
            }
          }
#if defined(NAVIGATION_9X)
          if (event==EVT_KEY_LONG(KEY_LEFT))
            killEvents(KEY_LEFT);
#endif
          break;
      }

      if (c != v) {
        name[cur] = v;
        storageDirty(isModelMenuDisplayed() ? EE_MODEL : EE_GENERAL);
      }

      if (attr == ZCHAR) {
        lcdDrawChar(x+editNameCursorPos*FW, y, zchar2char(v), ERASEBG|INVERS|FIXEDWIDTH);
      }
      else {
        lcdDrawChar(x+editNameCursorPos*FW, y, v, ERASEBG|INVERS|FIXEDWIDTH);
      }
    }
    else {
      cur = 0;
    }
    editNameCursorPos = cur;
    lcdNextPos = backupNextPos;
  }
}

void gvarWeightItem(coord_t x, coord_t y, MixData * md, LcdFlags attr, event_t event)
{
  u_int8int16_t weight;
  MD_WEIGHT_TO_UNION(md, weight);
  weight.word = GVAR_MENU_ITEM(x, y, weight.word, GV_RANGELARGE_WEIGHT_NEG, GV_RANGELARGE_WEIGHT, attr, 0, event);
  MD_UNION_TO_WEIGHT(weight, md);
}

void drawGVarName(coord_t x, coord_t y, int8_t idx, LcdFlags flags)
{
  char s[8];
  getGVarString(s, idx);
  lcdDrawText(x, y, s, flags);
}

void drawPower(coord_t x, coord_t y, int8_t dBm, LcdFlags att)
{
  float power_W_PREC1 = pow(10.0, (dBm - 30.0) / 10.0) * 10;
  if (dBm >= 30) {
    lcdDrawNumber(x, y, power_W_PREC1, PREC1 | att);
    lcdDrawText(lcdNextPos, y, "W", att);
  }
  else if (dBm < 10) {
    uint16_t power_MW_PREC1 = round(power_W_PREC1 * 1000);
    lcdDrawNumber(x, y, power_MW_PREC1, PREC1 | att);
    lcdDrawText(lcdNextPos, y, "mW", att);
  }
  else {
    uint16_t power_MW = round(power_W_PREC1 * 100);
    if (power_MW >= 50) {
      power_MW = (power_MW / 5) * 5;
      lcdDrawNumber(x, y, power_MW, att);
      lcdDrawText(lcdNextPos, y, "mW", att);
    }
    else {
      lcdDrawNumber(x, y, power_MW, att);
      lcdDrawText(lcdNextPos, y, "mW", att);
    }
  }
}

void drawReceiverName(coord_t x, coord_t y, uint8_t moduleIdx, uint8_t receiverIdx, LcdFlags flags)
{
  if (isModulePXX2(moduleIdx)) {
    if (g_model.moduleData[moduleIdx].pxx2.receiverName[receiverIdx][0] != '\0')
      lcdDrawSizedText(x, y, g_model.moduleData[moduleIdx].pxx2.receiverName[receiverIdx], effectiveLen(g_model.moduleData[moduleIdx].pxx2.receiverName[receiverIdx], PXX2_LEN_RX_NAME), flags);
    else
      lcdDrawText(x, y, "---", flags);
  }
#if defined(HARDWARE_INTERNAL_MODULE)
  else if (moduleIdx == INTERNAL_MODULE) {
    lcdDrawText(x, y, "Internal", flags);
  }
#endif
  else {
    lcdDrawText(x, y, "External", flags);
  }
}

void lcdDrawMMM(coord_t x, coord_t y, LcdFlags flags)
{
  lcdDrawTextAtIndex(x, y, STR_MMMINV, 0, flags);
}

#if defined(FLIGHT_MODES)
void drawFlightMode(coord_t x, coord_t y, int8_t idx, LcdFlags att)
{
  if (idx==0) {
    lcdDrawMMM(x, y, att);
    return;
  }
  // TODO this code was not included in Taranis! and used with abs(...) on Horus
  if (idx < 0) {
    lcdDrawChar(x-2, y, '!', att);
    idx = -idx;
  }
#if defined(CONDENSED)
  if (att & CONDENSED) {
    lcdDrawNumber(x+FW*1, y, idx-1, (att & ~CONDENSED), 1);
    return;
  }
#endif
  drawStringWithIndex(x, y, STR_FM, idx-1, att);
}
#endif

void drawCurveRef(coord_t x, coord_t y, CurveRef & curve, LcdFlags att)
{
  if (curve.value != 0) {
    switch (curve.type) {
      case CURVE_REF_DIFF:
        lcdDrawText(x, y, "D", att);
        GVAR_MENU_ITEM(lcdNextPos, y, curve.value, -100, 100, LEFT|att, 0, 0);
        break;

      case CURVE_REF_EXPO:
        lcdDrawText(x, y, "E", att);
        GVAR_MENU_ITEM(lcdNextPos, y, curve.value, -100, 100, LEFT|att, 0, 0);
        break;

      case CURVE_REF_FUNC:
        lcdDrawTextAtIndex(x, y, STR_VCURVEFUNC, curve.value, att);
        break;

      case CURVE_REF_CUSTOM:
        drawCurveName(x, y, curve.value, att);
        break;
    }
  }
}

void drawSensorCustomValue(coord_t x, coord_t y, uint8_t sensor, int32_t value, LcdFlags flags)
{
  if (sensor >= MAX_TELEMETRY_SENSORS) {
    // Lua luaLcdDrawChannel() can call us with a bad value
    return;
  }

  TelemetryItem & telemetryItem = telemetryItems[sensor];
  TelemetrySensor & telemetrySensor = g_model.telemetrySensors[sensor];

  if (telemetrySensor.unit == UNIT_DATETIME) {
    drawDate(x, y, telemetryItem, flags);
  }
  else if (telemetrySensor.unit == UNIT_GPS) {
    drawGPSSensorValue(x, y, telemetryItem, flags);
  }
  else if (telemetrySensor.unit == UNIT_BITFIELD) {
    if (IS_FRSKY_SPORT_PROTOCOL()) {
      if (telemetrySensor.id >= RBOX_STATE_FIRST_ID && telemetrySensor.id <= RBOX_STATE_LAST_ID) {
        if (telemetrySensor.subId == 0) {
          if (value == 0) {
            lcdDrawText(x, y, "OK", flags);
          }
          else {
            for (uint8_t i = 0; i < 16; i++) {
              if (value & (1u << i)) {
                char s[] = "CH__ KO";
                strAppendUnsigned(&s[2], i + 1, 2);
                lcdDrawText(x, flags & DBLSIZE ? y + 1 : y, s, flags & ~DBLSIZE);
                break;
              }
            }
          }
        }
        else {
          if (value == 0) {
            lcdDrawText(x, flags & DBLSIZE ? y + 1 : y, "Rx OK", flags & ~DBLSIZE);
          }
          else {
            static const char * const RXS_STATUS[] = {
              "Rx1 Ovl",
              "Rx2 Ovl",
              "SBUS Ovl",
              "Rx1 FS",
              "Rx1 LF",
              "Rx2 FS",
              "Rx2 LF",
              "Rx1 Lost",
              "Rx2 Lost",
              "Rx1 NS",
              "Rx2 NS",
            };
            for (uint8_t i = 0; i < DIM(RXS_STATUS); i++) {
              if (value & (1u << i)) {
                lcdDrawText(x, flags & DBLSIZE ? y + 1 : y, RXS_STATUS[i], flags & ~DBLSIZE);
                break;
              }
            }
          }
        }
      }
      else if (telemetrySensor.id >= RB3040_OUTPUT_FIRST_ID && telemetrySensor.id <= RB3040_OUTPUT_LAST_ID) {
        if (telemetrySensor.subId == 0) {
          if (value == 0) {
            lcdDrawText(x, y, "OK", flags);     
          }
          else {
            for (uint8_t i = 0; i < 9; i++) {
              if (value & (1u << i)) {
                if (i < 8) {
                  char s[] = "CH__ KO";
                  strAppendUnsigned(&s[2], i + 17, 2);
                  lcdDrawText(x, flags & DBLSIZE ? y + 1 : y, s, flags & ~DBLSIZE);
                  break;
                }
                else {
                  char s[] = "S.P Ovl";
                  lcdDrawText(x, flags & DBLSIZE ? y + 1 : y, s, flags & ~DBLSIZE);
                  break;
                }
              }
            }
          }
        }
      }
    }
  }
  else if (telemetrySensor.unit == UNIT_TEXT) {
    lcdDrawSizedText(x, flags & DBLSIZE ? y + 1 : y, telemetryItem.text, sizeof(telemetryItem.text), flags & ~DBLSIZE);
  }
  else {
    if (telemetrySensor.prec > 0) {
      flags |= (telemetrySensor.prec == 1 ? PREC1 : PREC2);
    }
    drawValueWithUnit(x, y, value, telemetrySensor.unit == UNIT_CELLS ? UNIT_VOLTS : telemetrySensor.unit, flags);
  }
}

void drawSourceCustomValue(coord_t x, coord_t y, source_t source, int32_t value, LcdFlags flags)
{
  if (source >= MIXSRC_FIRST_TELEM) {
    source = (source-MIXSRC_FIRST_TELEM) / 3;
    drawSensorCustomValue(x, y, source, value, flags);
  }
  else if (source >= MIXSRC_FIRST_TIMER || source == MIXSRC_TX_TIME) {
    if (value < 0) flags |= BLINK|INVERS;
    drawTimer(x, y, value, flags);
  }
  else if (source == MIXSRC_TX_VOLTAGE) {
    lcdDrawNumber(x, y, value, flags|PREC1);
  }
#if defined(INTERNAL_GPS)
    else if (source == MIXSRC_TX_GPS) {
    if (gpsData.fix) {
      drawGPSPosition(x, y, gpsData.longitude, gpsData.latitude, flags);
    }
    else {
      lcdDrawText(x, y, "sats: ", flags);
      lcdDrawNumber(lcdNextPos, y, gpsData.numSat, flags);
    }
  }
#endif
#if defined(GVARS)
  else if (source >= MIXSRC_FIRST_GVAR && source <= MIXSRC_LAST_GVAR) {
    drawGVarValue(x, y, source - MIXSRC_FIRST_GVAR, value, flags);
  }
#endif
  else if (source < MIXSRC_FIRST_CH) {
    lcdDrawNumber(x, y, calcRESXto100(value), flags);
  }
  else if (source <= MIXSRC_LAST_CH) {
#if defined(PPM_UNIT_PERCENT_PREC1)
    lcdDrawNumber(x, y, calcRESXto1000(value), flags|PREC1);
#else
    lcdDrawNumber(x, y, calcRESXto100(value), flags);
#endif
  }
  else {
    lcdDrawNumber(x, y, value, flags);
  }
}

void drawSourceValue(coord_t x, coord_t y, source_t source, LcdFlags flags)
{
  getvalue_t value = getValue(source);
  drawSourceCustomValue(x, y, source, value, flags);
}
