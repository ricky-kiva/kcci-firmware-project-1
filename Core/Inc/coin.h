#ifndef COIN_H
#define COIN_H

#include "ssd1306.h"
#include <stdint.h>

void OLED_DrawCoin(uint8_t value, int16_t offsetY);
void OLED_AnimateCoin(uint8_t final_value);

#endif
