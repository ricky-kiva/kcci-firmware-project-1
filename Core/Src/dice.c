#include "dice.h"
#include "ssd1306.h"
#include <stdlib.h>
#include "cmsis_os.h"

#define DICE_SIZE 35

void OLED_DrawDice(uint8_t value, int16_t offsetX, int16_t offsetY)
{
    int x = 46 + offsetX; 
    int y = 14 + offsetY;

    int xL = x + 9;
    int xC = x + 17;
    int xR = x + 25;

    int yT = y + 9;
    int yM = y + 17;
    int yB = y + 25;

    ssd1306_DrawRectangle(x, y, x + DICE_SIZE, y + DICE_SIZE, White);

    switch(value)
    {
        case 1:
            ssd1306_FillCircle(xC, yM, 3, White);
            break;

        case 2:
            ssd1306_FillCircle(xL, yT, 3, White);
            ssd1306_FillCircle(xR, yB, 3, White);
            break;

        case 3:
            ssd1306_FillCircle(xL, yT, 3, White);
            ssd1306_FillCircle(xC, yM, 3, White);
            ssd1306_FillCircle(xR, yB, 3, White);
            break;

        case 4:
            ssd1306_FillCircle(xL, yT, 3, White);
            ssd1306_FillCircle(xR, yT, 3, White);
            ssd1306_FillCircle(xL, yB, 3, White);
            ssd1306_FillCircle(xR, yB, 3, White);
            break;

        case 5:
            ssd1306_FillCircle(xL, yT, 3, White);
            ssd1306_FillCircle(xR, yT, 3, White);
            ssd1306_FillCircle(xC, yM, 3, White);
            ssd1306_FillCircle(xL, yB, 3, White);
            ssd1306_FillCircle(xR, yB, 3, White);
            break;

        case 6:
            ssd1306_FillCircle(xL, yT, 3, White);
            ssd1306_FillCircle(xR, yT, 3, White);
            ssd1306_FillCircle(xL, yM, 3, White);
            ssd1306_FillCircle(xR, yM, 3, White);
            ssd1306_FillCircle(xL, yB, 3, White);
            ssd1306_FillCircle(xR, yB, 3, White);
            break;
    }
}

void OLED_AnimateDice(uint8_t final_value)
{
    for (int i = 0; i < 10; i++) {
        ssd1306_Fill(Black);
        
        uint8_t tempFace = (rand() % 6) + 1;
        
        int shakeIntensity = 10 - i; 
        int16_t animOffX = (rand() % (shakeIntensity + 1)) - (shakeIntensity / 2);
        int16_t animOffY = (rand() % (shakeIntensity + 1)) - (shakeIntensity / 2);
        
        OLED_DrawDice(tempFace, animOffX, animOffY);
        
        extern osMutexId I2CMutexHandle;
        osMutexWait(I2CMutexHandle, osWaitForever);
        ssd1306_UpdateScreen();
        osMutexRelease(I2CMutexHandle);
        
        osDelay(20 + (i * 6)); 
    }
    
    ssd1306_Fill(Black);
    OLED_DrawDice(final_value, 0, 0);
    
    extern osMutexId I2CMutexHandle;
    osMutexWait(I2CMutexHandle, osWaitForever);
    ssd1306_UpdateScreen();
    osMutexRelease(I2CMutexHandle);
}
