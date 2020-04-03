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

constexpr coord_t LEFT_NAME_COLUMN = MENUS_MARGIN_LEFT;
constexpr coord_t RIGHT_NAME_COLUMN = LCD_W / 2;
constexpr coord_t ANA_OFFSET = 150;

bool menuRadioDiagAnalogs(event_t event)
{
  SIMPLE_SUBMENU(STR_MENU_RADIO_ANALOGS, ICON_MODEL_SETUP, 0);

  for (uint8_t i = 0; i < NUM_ANALOGS; i++) {
    coord_t y = MENU_HEADER_HEIGHT + 1 + (i / 2) * FH;
    uint8_t x = i & 1 ? LCD_W / 2 + 10 : LEFT_NAME_COLUMN;
    lcdDrawNumber(x, y, i + 1, LEADING0 | LEFT, 2);
    lcdDrawChar(x + 2 * 15 - 2, y, ':');
    lcdDrawHexNumber(x + 3 * 15 - 1, y, anaIn(i));
#if defined(JITTER_MEASURE)
    lcdDrawNumber(x + ANA_OFFSET - 1, y, rawJitter[i].get(), RIGHT);
    lcdDrawNumber(x + ANA_OFFSET + 30 - 1, y, avgJitter[i].get(), RIGHT);
    lcdDrawNumber(x + ANA_OFFSET + 70 - 1, y, (int16_t) calibratedAnalogs[CONVERT_MODE(i)] * 25 / 256, RIGHT);
#else
    if (i < NUM_STICKS + NUM_POTS + NUM_SLIDERS)
      lcdDrawNumber(x + ANA_OFFSET - 1, y, (int16_t) calibratedAnalogs[CONVERT_MODE(i)] * 25 / 256, RIGHT);
#if NUM_MOUSE_ANALOGS > 0
    else if (i >= MOUSE1)
      lcdDrawNumber(x + ANA_OFFSET - 1, y, (int16_t) calibratedAnalogs[CALIBRATED_MOUSE1 + i - MOUSE1] * 25 / 256, RIGHT);
#endif
#endif
  }

#if (NUM_PWMSTICKS > 0) && !defined(SIMU)
  lcdDrawText(MENUS_MARGIN_LEFT, MENU_CONTENT_TOP + 8 * FH, STICKS_PWM_ENABLED() ? "Sticks: PWM" : "Sticks: ANA");
#endif

  return true;
}
