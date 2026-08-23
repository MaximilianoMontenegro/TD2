# 🫀 Monitor ECG Digital - Versión Base (FreeRTOS + CMSIS-DSP)

**Autor / Integración Inicial:** Facundo Lautaro Costarelli
**Hardware:** STM32 Nucleo-F446RE + Módulo AD8232

Este proyecto es la **versión base estable** de nuestro monitor de electrocardiograma. Implementa la arquitectura E.P.A. (Escrutar, Procesar, Activar) utilizando un sistema operativo de tiempo real (FreeRTOS) y filtrado digital por hardware (FPU + CMSIS-DSP). 

Actualmente, la salida de datos ("Activar") se realiza a través de UART2 (115200 baudios) para ser visualizada en un Serial Plotter.

## ⚙️ Arquitectura del Sistema
1. **Base de Tiempo:** `TIM4` maneja los ticks de HAL. `SysTick` es exclusivo de FreeRTOS. Reloj HCLK a 84 MHz.
2. **Adquisición (DMA + ADC):** El `TIM3` dispara el `ADC1` a 250 Hz exactos. El `DMA2` guarda los datos de forma circular en un buffer de 100 muestras sin usar la CPU.
3. **Procesamiento (Task_DSP):** Al llenarse medio buffer (50 muestras), el DMA emite una interrupción (`xTaskNotifyFromISR`) que despierta a la tarea de DSP. La tarea aplica tres filtros Biquad en cascada: Notch (50Hz), High-Pass (0.5Hz) y Low-Pass (40Hz).

---

## 🛠️ Instrucciones de uso para el equipo

### 1. Cómo importar el proyecto correctamente
**NO CREEN UN PROYECTO NUEVO.** Para abrir este código en sus PCs:
1. Abran *STM32CubeIDE*.
2. Vayan a `File` -> `Open Projects from File System...`.
3. En *Import source*, hagan clic en `Directory...` y seleccionen esta carpeta (`TDII_ecg_digital`).
4. Clic en *Finish*. El IDE reconocerá automáticamente la configuración de compilador y librerías.

### 2. Reglas de Oro ⚠️
* **NO modifiquen la carpeta `Drivers/DSP`**: Esta carpeta contiene la librería matemática purgada. Si la tocan o intentan re-importarla desde CubeMX, el linker lanzará errores de "*multiple definition*".
* **Regenerar Código desde el `.ioc`**: Si necesitan agregar pines en STM32CubeMX, asegúrense de poner su código SIEMPRE entre los comentarios `/* USER CODE BEGIN ... */` y `/* USER CODE END ... */`. Si lo ponen afuera, CubeMX lo borrará.

### 3. Cómo trabajar con IAs (ChatGPT, Claude, Gemini)
Esta arquitectura es compleja (Bare-Metal + RTOS + DSP). Si le piden a una IA que modifique código sin darle contexto, va a sugerir usar `HAL_Delay` o bloquear el ADC con interrupciones clásicas, **destruyendo el rendimiento de tiempo real**. 

**Usen SIEMPRE este prompt como prefijo antes de hacerle una pregunta a la IA:**

> *"Actúa como un Ingeniero Senior de Firmware (Cortex-M4, FreeRTOS). Estoy trabajando en un proyecto de STM32F446RE con STM32CubeIDE. 
Mi arquitectura usa TIM3 (250Hz) para disparar el ADC1, el cual usa DMA Circular (Double Buffering, 100 muestras). Las interrupciones HalfCplt y Cplt del DMA usan `xTaskNotifyFromISR` para despertar a mi tarea principal `vTask_DSP`. Esta tarea castea las 50 muestras a float y las filtra con `arm_biquad_cascade_df1_f32` de CMSIS-DSP.
Teniendo este flujo estricto y no bloqueante en mente, necesito que me ayudes con lo siguiente: [INSERTA TU PREGUNTA/CÓDIGO AQUÍ]. No sugieras polling ni Delays bloqueantes."*