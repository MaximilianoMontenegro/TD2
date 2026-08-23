# ECG Digital — Task Tracker

## Fase 1: Proyecto base + UART (Hello ECG)

- [x] 1.1 Crear `.gitignore` raíz
- [ ] 1.2 Crear proyecto en STM32CubeIDE (lo hace el usuario)
- [ ] 1.3 Configurar clocks en CubeMX (lo hace el usuario)
- [ ] 1.4 Verificar USART2 (lo hace el usuario)
- [ ] 1.5 Generar código (lo hace el usuario)
- [ ] 1.6 Agregar código de prueba ("Hello ECG" + blink)
- [ ] 1.7 Compilar y flashear

## Fase 2: ADC + Timer + DMA (leer AD8232)

- [ ] 2.1 Configurar ADC1 en CubeMX
- [ ] 2.2 Configurar DMA para ADC1
- [ ] 2.3 Configurar TIM3
- [ ] 2.4 Configurar GPIO para Leads-Off
- [ ] 2.5 Habilitar interrupciones NVIC
- [ ] 2.6 Generar código y agregar lógica ADC+UART
- [ ] 2.7 Verificar lectura del AD8232 por consola

## Fase 3: FreeRTOS — Migrar a tareas

- [ ] 3.1 Habilitar FreeRTOS en CubeMX
- [ ] 3.2 Cambiar HAL Timebase a TIM6
- [ ] 3.3 Configurar heap y stack
- [ ] 3.4 Crear tareas iniciales (adcTask, uartTask)
- [ ] 3.5 Generar código y migrar lógica
- [ ] 3.6 Ajustar linker script
- [ ] 3.7 Verificar que ADC+UART siguen funcionando como tareas

## Fase 4: SPI + Display ILI9341

- [ ] 4.1 Configurar SPI1 en CubeMX
- [ ] 4.2 Configurar GPIO del display
- [ ] 4.3 Agregar tarea displayTask
- [ ] 4.4 Agregar driver ILI9341
- [ ] 4.5 Verificar display enciende y muestra color/texto

## Fase 5: ECG en pantalla

- [ ] 5.1 Crear ecg_display.c/.h
- [ ] 5.2 Graficar señal ECG en tiempo real
- [ ] 5.3 Verificar onda visible sin flickering

## Fase 6: Detección de fiduciales (P, QRS/R, T)

- [ ] 6.1 Agregar tarea ecgProcessTask
- [ ] 6.2 Crear ecg_processing.c/.h
- [ ] 6.3 Detección de pico R
- [ ] 6.4 Detección de onda P y onda T
- [ ] 6.5 Cálculo de BPM
- [ ] 6.6 Marcadores visuales en pantalla

## Fase 7: Polish

- [ ] 7.1 Indicador de leads-off
- [ ] 7.2 UI tipo papel ECG
- [ ] 7.3 Touch (opcional)
- [ ] 7.4 Documentación
