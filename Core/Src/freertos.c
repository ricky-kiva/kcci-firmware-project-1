/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * File Name          : freertos.c
 * Description        : Code for freertos applications
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "queue.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include "dice.h"
#include "coin.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum {
	DHT_OK = 0, 
  DHT_ERR_TIMEOUT, 
  DHT_ERR_CHECKSUM
} DHT_Status;

typedef enum {
	PAGE_DICE = 0,
  PAGE_COIN,
	PAGE_INFOS,
  PAGE_HISTORY,
  PAGE_HISTORY_ENC
} page_t;

typedef enum {
  MODE_DICE = 0,
  MODE_COIN
} generator_mode_t;

typedef enum {
  ANIM_NONE,
  ANIM_DICE,
  ANIM_COIN
} animation_t;

typedef struct {
	int16_t suhu;
	uint16_t kelembapan;
	uint16_t ldr;
} sensor_data_t;

typedef struct {
    generator_mode_t mode;
    uint32_t seed;
    uint32_t value;
    int16_t suhu;
    uint16_t kelembapan;
    uint16_t ldr;
} display_data_t;

typedef struct {
  generator_mode_t mode;
  uint8_t value;
  uint32_t timestamp;
} eeprom_data_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define DHT_TIMEOUT_US	100

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

extern ADC_HandleTypeDef hadc1;
extern UART_HandleTypeDef huart2;
extern TIM_HandleTypeDef htim3;
extern I2C_HandleTypeDef hi2c1;
extern TIM_HandleTypeDef htim2;

QueueHandle_t SensorQueueHandle;
QueueHandle_t DisplayQueueHandle;
QueueHandle_t EEPROMQueueHandle;

volatile page_t currentPage = PAGE_DICE;
volatile generator_mode_t currentMode = MODE_DICE;
volatile animation_t currentAnimation = ANIM_NONE;

eeprom_data_t history_cache[5];
uint8_t history_count = 0;

// SAMPLE SECRET KEY
const uint8_t SECRET_KEY[8] = {0x5A, 0x3C, 0x7E, 0x18, 0x42, 0x81, 0xBD, 0xE7};

/* USER CODE END Variables */
osThreadId defaultTaskHandle;
osThreadId SensorTaskHandle;
osThreadId RNGTaskHandle;
osThreadId RotaryPageTaskHandle;
osThreadId DisplayTaskHandle;
osThreadId EEPROMTaskHandle;
osThreadId BuzzerTaskHandle;
osMutexId I2CMutexHandle;
osSemaphoreId RNGSemaphoreHandle;
osSemaphoreId BuzzerSemaphoreHandle;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void delay_us(uint16_t us);
void Set_Pin_Output(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);
void Set_Pin_Input(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);
static DHT_Status DHT22_WaitPinState(GPIO_TypeDef *GPIOx, uint16_t pin, GPIO_PinState state);
DHT_Status DHT22_Read(int16_t *suhu, uint16_t *kelembapan);

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void const * argument);
void StartTaskSensor(void const * argument);
void StartTaskRNG(void const * argument);
void StartTaskRotaryPage(void const * argument);
void StartTaskDisplay(void const * argument);
void StartTaskEEPROM(void const * argument);
void StartTaskBuzzer(void const * argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* GetIdleTaskMemory prototype (linked to static allocation support) */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize );

/* USER CODE BEGIN GET_IDLE_TASK_MEMORY */
static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
		StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize) {
	*ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
	*ppxIdleTaskStackBuffer = &xIdleStack[0];
	*pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
	/* place for user code */
}
/* USER CODE END GET_IDLE_TASK_MEMORY */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */
  /* Create the mutex(es) */
  /* definition and creation of I2CMutex */
  osMutexDef(I2CMutex);
  I2CMutexHandle = osMutexCreate(osMutex(I2CMutex));

  /* USER CODE BEGIN RTOS_MUTEX */
	/* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* Create the semaphores(s) */
  /* definition and creation of RNGSemaphore */
  osSemaphoreDef(RNGSemaphore);
  RNGSemaphoreHandle = osSemaphoreCreate(osSemaphore(RNGSemaphore), 1);

  /* definition and creation of BuzzerSemaphore */
  osSemaphoreDef(BuzzerSemaphore);
  BuzzerSemaphoreHandle = osSemaphoreCreate(osSemaphore(BuzzerSemaphore), 1);

  /* USER CODE BEGIN RTOS_SEMAPHORES */
	/* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
	/* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
	/* add queues, ... */
  SensorQueueHandle = xQueueCreate(1, sizeof(sensor_data_t));
  DisplayQueueHandle = xQueueCreate(1, sizeof(display_data_t));
  EEPROMQueueHandle = xQueueCreate(1, sizeof(eeprom_data_t));

  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of defaultTask */
  osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 128);
  defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

  /* definition and creation of SensorTask */
  osThreadDef(SensorTask, StartTaskSensor, osPriorityNormal, 0, 256);
  SensorTaskHandle = osThreadCreate(osThread(SensorTask), NULL);

  /* definition and creation of RNGTask */
  osThreadDef(RNGTask, StartTaskRNG, osPriorityAboveNormal, 0, 256);
  RNGTaskHandle = osThreadCreate(osThread(RNGTask), NULL);

  /* definition and creation of RotaryPageTask */
  osThreadDef(RotaryPageTask, StartTaskRotaryPage, osPriorityNormal, 0, 128);
  RotaryPageTaskHandle = osThreadCreate(osThread(RotaryPageTask), NULL);

  /* definition and creation of DisplayTask */
  osThreadDef(DisplayTask, StartTaskDisplay, osPriorityBelowNormal, 0, 256);
  DisplayTaskHandle = osThreadCreate(osThread(DisplayTask), NULL);

  /* definition and creation of EEPROMTask */
  osThreadDef(EEPROMTask, StartTaskEEPROM, osPriorityBelowNormal, 0, 256);
  EEPROMTaskHandle = osThreadCreate(osThread(EEPROMTask), NULL);

  /* definition and creation of BuzzerTask */
  osThreadDef(BuzzerTask, StartTaskBuzzer, osPriorityBelowNormal, 0, 128);
  BuzzerTaskHandle = osThreadCreate(osThread(BuzzerTask), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
	/* add threads, ... */
  /* USER CODE END RTOS_THREADS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
 * @brief  Function implementing the defaultTask thread.
 * @param  argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void const * argument)
{
  /* USER CODE BEGIN StartDefaultTask */
	/* Infinite loop */
	for (;;) {
		osDelay(1);
	}
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_StartTaskSensor */
/**
 * @brief Function implementing the SensorTask thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartTaskSensor */
void StartTaskSensor(void const * argument)
{
  /* USER CODE BEGIN StartTaskSensor */
	sensor_data_t sensor;

	/* Infinite loop */
	for (;;) {
    sensor = (sensor_data_t){0};

		DHT22_Read(&sensor.suhu, &sensor.kelembapan);

		HAL_ADC_Start(&hadc1);

		if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
			sensor.ldr = HAL_ADC_GetValue(&hadc1);
		}

		HAL_ADC_Stop(&hadc1);
    xQueueOverwrite(SensorQueueHandle, &sensor);
		osDelay(2000);
	}
  /* USER CODE END StartTaskSensor */
}

/* USER CODE BEGIN Header_StartTaskRNG */
/**
* @brief Function implementing the RNGTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTaskRNG */
void StartTaskRNG(void const * argument)
{
  /* USER CODE BEGIN StartTaskRNG */
  sensor_data_t sensor;
  display_data_t display;
  eeprom_data_t eeprom;

  uint32_t seed;

  /* Infinite loop */
  for(;;)
  {
    osSemaphoreWait(RNGSemaphoreHandle, osWaitForever);
    osSemaphoreRelease(BuzzerSemaphoreHandle);

    xQueuePeek(SensorQueueHandle, &sensor, portMAX_DELAY);

    seed = ((uint32_t)sensor.suhu << 16)
      ^ ((uint32_t)sensor.kelembapan << 8)
      ^ sensor.ldr
      ^ HAL_GetTick();

    srand(seed);
    
    switch(currentMode) {
      case MODE_DICE:
        display.value = (rand() % 6) + 1;
        break;
      case MODE_COIN:
        display.value = rand() & 1;
        break;
    }

    display.mode = currentMode;
    display.seed = seed;

    eeprom.mode = currentMode;
    eeprom.value = (uint8_t)display.value;
    eeprom.timestamp = HAL_GetTick();

    xQueueOverwrite(DisplayQueueHandle, &display);
    xQueueOverwrite(EEPROMQueueHandle, &eeprom);

    switch(currentMode) {
      case MODE_DICE:
          currentAnimation = ANIM_DICE;
          break;
      case MODE_COIN:
          currentAnimation = ANIM_COIN;
          break;
      default:
          currentAnimation = ANIM_NONE;
      }
    }
  /* USER CODE END StartTaskRNG */
}

/* USER CODE BEGIN Header_StartTaskRotaryPage */
/**
* @brief Function implementing the RotaryPageTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTaskRotaryPage */
void StartTaskRotaryPage(void const * argument)
{
  /* USER CODE BEGIN StartTaskRotaryPage */
    uint16_t lastCount;
    uint16_t currentCount;

    GPIO_PinState lastButton = GPIO_PIN_SET;

    HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);

    lastCount = __HAL_TIM_GET_COUNTER(&htim3);

  /* Infinite loop */
  for(;;)
  {
    GPIO_PinState button = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);

    if (lastButton == GPIO_PIN_SET && button == GPIO_PIN_RESET) {
      if (currentPage == PAGE_DICE) {
        currentMode = MODE_DICE;
        osSemaphoreRelease(RNGSemaphoreHandle);
      } else if (currentPage == PAGE_COIN) {
        currentMode = MODE_COIN;
        osSemaphoreRelease(RNGSemaphoreHandle);
      }
    }

    lastButton = button;
    currentCount = __HAL_TIM_GET_COUNTER(&htim3);

    if (currentCount != lastCount) {
      if ((int16_t)(currentCount - lastCount) > 0) {
        /* Clockwise */
        currentPage++;

        if (currentPage > PAGE_HISTORY_ENC)
          currentPage = PAGE_DICE;
      } else {
        /* Counter-clockwise */
        if (currentPage == PAGE_DICE)
          currentPage = PAGE_HISTORY_ENC;
        else
          currentPage--;
      }

      lastCount = currentCount;
    }
    
    osDelay(10);
  }
  /* USER CODE END StartTaskRotaryPage */
}

/* USER CODE BEGIN Header_StartTaskDisplay */
/**
* @brief Function implementing the DisplayTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTaskDisplay */
void StartTaskDisplay(void const * argument)
{
  /* USER CODE BEGIN StartTaskDisplay */
  display_data_t display;
  sensor_data_t sensor;

  char buf[32];

  /* Infinite loop */
  for(;;)
  {
    xQueuePeek(DisplayQueueHandle, &display, 0);
    xQueuePeek(SensorQueueHandle, &sensor, 0);

    if (currentAnimation == ANIM_DICE && currentPage == PAGE_DICE) {
      OLED_AnimateDice(display.value);
      currentAnimation = ANIM_NONE;
    } else if (currentAnimation == ANIM_COIN && currentPage == PAGE_COIN) {
      OLED_AnimateCoin(display.value);
      currentAnimation = ANIM_NONE;
    }

    ssd1306_Fill(Black);
	  ssd1306_SetCursor(0, 0);

    switch(currentPage) {
      case PAGE_DICE:
        ssd1306_SetCursor(0, 0);
        ssd1306_WriteString("DICE RNG", Font_6x8, White);

        ssd1306_SetCursor(32, 56);
        ssd1306_WriteString("Press ENC > Roll", Font_6x8, White);

        OLED_DrawDice((display.mode == MODE_DICE && display.value > 0) ? display.value : 1, 0, 0);
        break;
        
      case PAGE_COIN:
        ssd1306_SetCursor(0, 0);
        ssd1306_WriteString("COIN RNG", Font_6x8, White);

        ssd1306_SetCursor(32, 56);
        ssd1306_WriteString("Press ENC > Flip", Font_6x8, White);

        OLED_DrawCoin((display.mode == MODE_COIN) ? display.value : 0, 0);
        break;

      case PAGE_INFOS:
        ssd1306_SetCursor(0, 0);
        ssd1306_WriteString("INFO PAGE", Font_6x8, White);

        sprintf(buf, "Temp: %d.%d C", sensor.suhu / 10, abs(sensor.suhu) % 10);
        ssd1306_SetCursor(0, 15);
        ssd1306_WriteString(buf, Font_6x8, White);

        sprintf(buf, "Humid: %u.%u %%", sensor.kelembapan / 10, sensor.kelembapan % 10);
        ssd1306_SetCursor(0, 24);
        ssd1306_WriteString(buf, Font_6x8, White);

        sprintf(buf, "LDR: %u Raw", sensor.ldr);
        ssd1306_SetCursor(0, 33);
        ssd1306_WriteString(buf, Font_6x8, White);

        sprintf(buf, "Seed: %lu", display.seed);
        ssd1306_SetCursor(0, 42);
        ssd1306_WriteString(buf, Font_6x8, White);

        ssd1306_SetCursor(0, 56);
        ssd1306_WriteString("(C) Rickyslash 2026", Font_6x8, White); 
        break;
      
      case PAGE_HISTORY:
        ssd1306_WriteString("HISTORY (RNG)", Font_6x8, White);
        for(int i = 0; i < history_count; i++) {
            const char *m;

            switch (history_cache[i].mode) {
              case MODE_DICE:
                  m = "Dice";
                  break;
              case MODE_COIN:
                  m = "Coin";
                  break;
              default:
                  m = "????";
                  break;
            }

            uint32_t total_seconds = history_cache[i].timestamp / 1000;
            uint32_t hours   = total_seconds / 3600;
            uint32_t minutes = (total_seconds % 3600) / 60;
            uint32_t seconds = total_seconds % 60;

            if (history_cache[i].mode == MODE_COIN) {
              sprintf(buf,
                "%s: %c | %02lu:%02lu:%02lu",
                m,
                history_cache[i].value ? 'H' : 'T',
                (unsigned long)hours,
                (unsigned long)minutes,
                (unsigned long)seconds);
            } else {
              sprintf(buf,
                "%s: %d | %02lu:%02lu:%02lu",
                m,
                history_cache[i].value,
                (unsigned long)hours,
                (unsigned long)minutes,
                (unsigned long)seconds);
            }
            
            ssd1306_SetCursor(0, 15 + (i * 9));
            ssd1306_WriteString(buf, Font_6x8, White);
        }
        break;

        case PAGE_HISTORY_ENC:
          ssd1306_WriteString("HISTORY (ENCRYPT-RAW)", Font_6x8, White);

          for(int i = 0; i < history_count; i++) {
            uint8_t raw[8] = {0};
            raw[0] = (uint8_t)history_cache[i].mode;
            raw[1] = history_cache[i].value;
            raw[2] = (history_cache[i].timestamp >> 24) & 0xFF;
            raw[3] = (history_cache[i].timestamp >> 16) & 0xFF;
            raw[4] = (history_cache[i].timestamp >> 8) & 0xFF;
            raw[5] = history_cache[i].timestamp & 0xFF;

            for(int k = 0; k < 6; k++) {
              raw[k] ^= SECRET_KEY[k];
            }

            sprintf(buf, "%d: %02X %02X %02X %02X %02X %02X", i+1, raw[0], raw[1], raw[2], raw[3], raw[4], raw[5]);
            
            ssd1306_SetCursor(0, 15 + (i * 9));
            ssd1306_WriteString(buf, Font_6x8, White);
          }
          break;
    }

    osMutexWait(I2CMutexHandle, osWaitForever);
	  ssd1306_UpdateScreen();
    osMutexRelease(I2CMutexHandle);

	  osDelay(50);
  }
  /* USER CODE END StartTaskDisplay */
}

/* USER CODE BEGIN Header_StartTaskEEPROM */
/**
* @brief Function implementing the EEPROMTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTaskEEPROM */
void StartTaskEEPROM(void const * argument)
{
  /* USER CODE BEGIN StartTaskEEPROM */
  eeprom_data_t eeprom;

  uint8_t header[2] = {0, 0}; 
  uint8_t current_index = 0;
  uint8_t total_count = 0;
  
  const uint8_t EEPROM_ADDR = 0xA0;
  const uint8_t MAX_RECORDS = 23;

  osMutexWait(I2CMutexHandle, osWaitForever);
  HAL_I2C_Mem_Read(&hi2c1, EEPROM_ADDR, 0x00, I2C_MEMADD_SIZE_8BIT, header, 2, 100);
  osMutexRelease(I2CMutexHandle);

  current_index = header[0];
  total_count = header[1];

  if (current_index >= MAX_RECORDS || total_count > MAX_RECORDS) {
      current_index = 0;
      total_count = 0;
  }

  history_count = (total_count < 5) ? total_count : 5;

  uint8_t read_idx = current_index;

  for (int i = 0; i < history_count; i++) {
    read_idx = (read_idx == 0) ? (MAX_RECORDS - 1) : (read_idx - 1);
    
    uint8_t read_buf[8];
    osMutexWait(I2CMutexHandle, osWaitForever);
    HAL_I2C_Mem_Read(&hi2c1, EEPROM_ADDR, 8 + (read_idx * 8), I2C_MEMADD_SIZE_8BIT, read_buf, 8, 100);
    osMutexRelease(I2CMutexHandle);

    for(int k = 0; k < 8; k++) {
      read_buf[k] ^= SECRET_KEY[k];
    }

    history_cache[i].mode = (generator_mode_t)read_buf[0];
    history_cache[i].value = read_buf[1];
    history_cache[i].timestamp = ((uint32_t)read_buf[2] << 24) | ((uint32_t)read_buf[3] << 16) | ((uint32_t)read_buf[4] << 8) | read_buf[5];
  }
  /* Infinite loop */
  for(;;)
  {
    if (xQueueReceive(EEPROMQueueHandle, &eeprom, portMAX_DELAY) == pdTRUE) {
      uint8_t write_buf[8] = {0};
      write_buf[0] = (uint8_t)eeprom.mode;
      write_buf[1] = eeprom.value;
      write_buf[2] = (eeprom.timestamp >> 24) & 0xFF;
      write_buf[3] = (eeprom.timestamp >> 16) & 0xFF;
      write_buf[4] = (eeprom.timestamp >> 8) & 0xFF;
      write_buf[5] = eeprom.timestamp & 0xFF;

      for(int k = 0; k < 8; k++) {
        write_buf[k] ^= SECRET_KEY[k];
      }

      uint16_t mem_addr = 8 + (current_index * 8);

      osMutexWait(I2CMutexHandle, osWaitForever);
      HAL_I2C_Mem_Write(&hi2c1, EEPROM_ADDR, mem_addr, I2C_MEMADD_SIZE_8BIT, write_buf, 8, 100);
      osMutexRelease(I2CMutexHandle);
      osDelay(5);

      current_index++;
      if (current_index >= MAX_RECORDS) current_index = 0;
      if (total_count < MAX_RECORDS) total_count++;

      header[0] = current_index;
      header[1] = total_count;
      
      osMutexWait(I2CMutexHandle, osWaitForever);
      HAL_I2C_Mem_Write(&hi2c1, EEPROM_ADDR, 0x00, I2C_MEMADD_SIZE_8BIT, header, 2, 100);
      osMutexRelease(I2CMutexHandle);
      osDelay(5);

      if (history_count < 5) history_count++;
      for(int i = history_count - 1; i > 0; i--) {
          history_cache[i] = history_cache[i-1];
      }

      history_cache[0] = eeprom;
    }
  }
  /* USER CODE END StartTaskEEPROM */
}

/* USER CODE BEGIN Header_StartTaskBuzzer */
/**
* @brief Function implementing the BuzzerTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTaskBuzzer */
void StartTaskBuzzer(void const * argument)
{
  /* USER CODE BEGIN StartTaskBuzzer */
  /* Infinite loop */
  for(;;)
  {
    osSemaphoreWait(BuzzerSemaphoreHandle, osWaitForever);

    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3);

    __HAL_TIM_SET_AUTORELOAD(&htim2, 1000);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, 500); 
    osDelay(40);

    __HAL_TIM_SET_AUTORELOAD(&htim2, 750);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, 375); 
    osDelay(40);

    __HAL_TIM_SET_AUTORELOAD(&htim2, 500);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, 250); 
    osDelay(80);

    HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_3);
  }
  /* USER CODE END StartTaskBuzzer */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
void delay_us(uint16_t us) {
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

	uint32_t start = DWT->CYCCNT;
	uint32_t ticks = us * (HAL_RCC_GetHCLKFreq() / 1000000);

	while ((DWT->CYCCNT - start) < ticks);
}

void Set_Pin_Output(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };

	GPIO_InitStruct.Pin = GPIO_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

	HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
}

void Set_Pin_Input(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };

	GPIO_InitStruct.Pin = GPIO_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_PULLUP;

	HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
}

static DHT_Status DHT22_WaitPinState(GPIO_TypeDef *GPIOx, uint16_t pin,
		GPIO_PinState state) {
	uint32_t t = 0;

	while (HAL_GPIO_ReadPin(GPIOx, pin) != state) {
		delay_us(1);

		if (++t > DHT_TIMEOUT_US)
			return DHT_ERR_TIMEOUT;
	}

	return DHT_OK;
}

DHT_Status DHT22_Read(int16_t *suhu, uint16_t *kelembapan) {
	uint8_t dht_data[5] = { 0 };
	DHT_Status st;

	Set_Pin_Output(GPIOA, GPIO_PIN_5);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
	HAL_Delay(18);

	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
	delay_us(20);
	Set_Pin_Input(GPIOA, GPIO_PIN_5);

	taskENTER_CRITICAL();

	if ((st = DHT22_WaitPinState(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET)) != DHT_OK) { taskEXIT_CRITICAL(); return st; }
	if ((st = DHT22_WaitPinState(GPIOA, GPIO_PIN_5, GPIO_PIN_SET))   != DHT_OK) { taskEXIT_CRITICAL(); return st; }
	if ((st = DHT22_WaitPinState(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET)) != DHT_OK) { taskEXIT_CRITICAL(); return st; }

	for (int i = 0; i < 5; i++) {
		for (int j = 7; j >= 0; j--) {
			if ((st = DHT22_WaitPinState(GPIOA, GPIO_PIN_5, GPIO_PIN_SET)) != DHT_OK)
				return st;

			delay_us(40);

			if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5)) {
				dht_data[i] |= (1 << j);

				if ((st = DHT22_WaitPinState(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET)) != DHT_OK)
					return st;
			}
		}
	}

	taskEXIT_CRITICAL();

	uint8_t sum = dht_data[0] + dht_data[1] + dht_data[2] + dht_data[3];

	if (sum != dht_data[4])
		return DHT_ERR_CHECKSUM;

	*kelembapan = (dht_data[0] << 8) | dht_data[1];

	uint16_t raw_suhu = ((dht_data[2] & 0x7F) << 8) | dht_data[3];

	*suhu = (dht_data[2] & 0x80) ? -(int16_t) raw_suhu : (int16_t) raw_suhu;

	return DHT_OK;
}

/* USER CODE END Application */
