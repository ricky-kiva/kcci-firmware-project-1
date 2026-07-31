#include "coin.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include "cmsis_os.h"

#define COIN_RADIUS 17
#define CENTER_X 64
#define CENTER_Y 32

extern osMutexId I2CMutexHandle;

void OLED_DrawCoin(uint8_t value, int16_t offsetY)
{
    int y = CENTER_Y + offsetY;

    ssd1306_DrawCircle(CENTER_X, y, COIN_RADIUS, White);
    ssd1306_DrawCircle(CENTER_X, y, COIN_RADIUS - 2, White);

    if (value) {
        ssd1306_SetCursor(CENTER_X - 5, y - 8);
        ssd1306_WriteString("H", Font_11x18, White);
    } else {
        ssd1306_SetCursor(CENTER_X - 5, y - 8);
        ssd1306_WriteString("T", Font_11x18, White);
    }
}

void OLED_AnimateCoin(uint8_t final_value)
{
    int16_t offsets[] = {0, -8, -16, -24, -18, -10, 0, 6, 2, 0};
    
    for (int i = 0; i < 10; i++) {
        ssd1306_Fill(Black);
        
        uint8_t tempFace = i % 2; 
        
        OLED_DrawCoin(tempFace, offsets[i]);
        
        osMutexWait(I2CMutexHandle, osWaitForever);
        ssd1306_UpdateScreen();
        osMutexRelease(I2CMutexHandle);
        
        osDelay(25 + (i * 6)); 
    }
    
    ssd1306_Fill(Black);
    OLED_DrawCoin(final_value, 0);
    
    osMutexWait(I2CMutexHandle, osWaitForever);
    ssd1306_UpdateScreen();
    osMutexRelease(I2CMutexHandle);
}
