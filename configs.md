IOC Configs:
- UART -> USART2
  - Mode: Async
  - Baud Rate: 115200 B/s
  - Pins:
    - PA2: TX
    - PA3: RX

- Buzzer -> TIM3
  - Channel 3: PWM Generation CH3
  - Prescaler: 84 - 1
  - ARR: 1000 - 1
  - Pins:
    - PB0: Input

- OLED -> I2C1
  - Mode: I2C
  - Speed: Fast Mode (400 kHz)
  - Pins:
    - PB6: SCL
    - PB7: SDA