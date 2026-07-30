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
#include <stdio.h>
#include <string.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum {
	DHT_OK = 0, DHT_ERR_TIMEOUT, DHT_ERR_CHECKSUM
} DHT_Status;

typedef struct {
	int16_t suhu;
	uint16_t kelembapan;
	uint16_t ldr;
} sensor_data_t;

typedef enum {
	PAGE_RANDOM = 0,
	PAGE_SEED,
	PAGE_SENSOR
} page_t;

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

osMessageQId SensorQueueHandle;

volatile page_t currentPage = PAGE_RANDOM;

/* USER CODE END Variables */
osThreadId defaultTaskHandle;
osThreadId SensorTaskHandle;
osThreadId RNGTaskHandle;

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

  /* USER CODE BEGIN RTOS_MUTEX */
	/* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
	/* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
	/* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
	/* add queues, ... */
  osMessageQDef(SensorQueue, 4, sensor_data_t);
  SensorQueueHandle = osMessageCreate(osMessageQ(SensorQueue), NULL);

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
	DHT_Status dht_st;

	char uart_buf[100];

	/* Infinite loop */
	for (;;) {
    sensor = (sensor_data_t){0};

		dht_st = DHT22_Read(&sensor.suhu, &sensor.kelembapan);

		HAL_ADC_Start(&hadc1);

		if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
			sensor.ldr = HAL_ADC_GetValue(&hadc1);
		}

		HAL_ADC_Stop(&hadc1);

    osMessagePut(SensorQueueHandle, (uint32_t)&sensor, osWaitForever);

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
  osEvent event;
  sensor_data_t *sensor;

  uint32_t seed;
  uint32_t random_num;
  char uart_buf[100];

  /* USER CODE BEGIN StartTaskRNG */
  /* Infinite loop */
  for(;;)
  {
    event = osMessageGet(SensorQueueHandle, osWaitForever);

    if (event.status == osEventMessage) {
      sensor = (sensor_data_t *)event.value.p;

      seed = ((uint32_t)sensor->suhu << 16)
        ^ ((uint32_t)sensor->kelembapan << 8)
        ^ sensor->ldr
        ^ HAL_GetTick();

      srand(seed);
      random_num = rand();

      sprintf(uart_buf,
        "Seed:%lu Random:%lu\r\n",
        seed,
        random_num);

      HAL_UART_Transmit(&huart2,
        (uint8_t *)uart_buf,
        strlen(uart_buf),
        HAL_MAX_DELAY);
    }
  }
  /* USER CODE END StartTaskRNG */
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
