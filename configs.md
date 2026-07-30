IOC Configs:
- UART -> USART2
  - Mode: Async
  - Baud Rate: 115200 B/s
  - Pins:
    - PA2: TX
    - PA3: RX

- Buzzer -> TIM2
  - Channel 3: PWM Generation CH3
  - Prescaler: 84 - 1
  - ARR: 1000 - 1
  - auto-reload preload: enabled
  - Pins:
    - PB10: Input

- OLED & EEPROM -> I2C1
  - Mode: I2C
  - Speed: Fast Mode (400 kHz)
  - Pins:
    - PB6: SCL
    - PB7: SDA

- Rotary Encoder
  - Encoder -> TIM3
    - Combined Channels: Encoder Mode
    - ARR: 65535
    - Encoder Mode: Encoder Mode TI1 and TI2 
    - Pins:
      - PA6: Clock (ENC A)
      - PA7: DT (ENC B)
  - SW -> GPIO
    - Mode: Input
    - Pull: Pull-up
    - Pins:
      - PA0: SW  

- DHT22 -> GPIO 
  - Mode: Output
  - Pull: Pull-up
  - Pins:
    - PA5: out

- LDR -> ADC1
  - Mode: IN4