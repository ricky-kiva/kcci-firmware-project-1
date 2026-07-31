#ifndef DICE_H
#define DICE_H

#include "ssd1306.h"
#include <stdint.h>

void OLED_DrawDice(uint8_t value, int16_t offsetX, int16_t offsetY);
void OLED_AnimateDice(uint8_t final_value); // <-- New function

#endif