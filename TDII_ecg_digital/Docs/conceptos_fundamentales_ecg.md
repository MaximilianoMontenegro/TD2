# Conceptos Fundamentales — Proyecto ECG Digital

Guía de referencia para entender cada decisión técnica del proyecto.

---

## 1. ¿Qué es DMA y por qué usarlo?

### El problema: ¿cómo mueve datos el microcontrolador?

Imaginá que el ADC convierte un valor analógico a digital 500 veces por segundo. Ese número tiene que llegar desde el registro del ADC hasta un array en la memoria RAM donde vos lo podés usar. Hay **3 formas** de hacer esto:

### Opción A: Polling (encuesta)

```c
while (1) {
    HAL_ADC_Start(&hadc1);                          // Iniciar conversión
    HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY); // ESPERAR a que termine
    uint16_t valor = HAL_ADC_GetValue(&hadc1);       // Leer el valor
    // ... hacer algo con valor ...
}
```

**Problema**: El CPU se queda **bloqueado esperando** en `PollForConversion`. No puede hacer nada más mientras espera. Si tenés que dibujar en la pantalla, detectar fiduciales, y leer el ADC, no podés hacer todo a la vez. Es como estar parado mirando el horno hasta que se cocina la pizza — no podés hacer nada más.

| Pros | Contras |
|---|---|
| Código simple | CPU bloqueado, no puede hacer otra cosa |
| Fácil de debuggear | No escala a múltiples tareas |

### Opción B: Interrupciones (sin DMA)

```c
// Se configura el ADC para que genere una interrupción cuando termina
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
    buffer[index] = HAL_ADC_GetValue(hadc);  // El CPU lee y guarda
    index++;
}
```

**Mejor**: El CPU puede hacer otras cosas. Cuando el ADC termina, el CPU es **interrumpido**, copia el dato, y vuelve a lo que estaba haciendo. Es como poner una alarma del horno — podés ir a hacer otra cosa y volvés cuando suena.

**Pero**: A 500 Hz, el CPU es interrumpido **500 veces por segundo** solo para copiar un número de un registro a otro. Es un trabajo mecánico y repetitivo que desperdicia capacidad del CPU.

| Pros | Contras |
|---|---|
| CPU libre entre interrupciones | 500 interrupciones/seg para copiar datos |
| Respuesta rápida | Overhead de entrar/salir de interrupción |

### Opción C: DMA (Direct Memory Access) ✅ Lo que usamos

```
ADC convierte → DMA copia automáticamente al buffer → CPU ni se entera
```

**DMA es un coprocesador de hardware** que transfiere datos entre periféricos y memoria **sin usar el CPU para nada**. Es como tener un asistente que saca la pizza del horno por vos — vos ni te enterás, y la pizza aparece en la mesa.

```c
// Configuración inicial (una sola vez):
HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_buffer, BUFFER_SIZE);
// Listo. El DMA llena adc_buffer automáticamente. Para siempre.

// El CPU solo se entera cuando el buffer está medio lleno o lleno:
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef* hadc) {
    // Procesar la primera mitad del buffer
}
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
    // Procesar la segunda mitad del buffer
}
```

**Cómo funciona internamente:**

```
┌──────────┐    ┌─────────┐    ┌─────────────────────────────────┐
│   ADC    │───→│   DMA   │───→│  RAM: adc_buffer[0..255]        │
│ (PA0)    │    │ Channel │    │  [  dato0 | dato1 | dato2 | ... │
└──────────┘    └─────────┘    └─────────────────────────────────┘
                     │
                     │  Interrumpe al CPU solo cuando:
                     │  - Buffer mitad lleno (HalfComplete)
                     │  - Buffer completo (Complete)
                     ▼
                 ┌───────┐
                 │  CPU  │  → Procesa 128 muestras de una vez
                 └───────┘    (mucho más eficiente)
```

| Pros | Contras |
|---|---|
| CPU 100% libre | Configuración un poco más compleja |
| Transferencia automática | Usa un canal DMA (hay limitados, pero suficientes) |
| Solo 2 interrupciones por buffer completo | — |
| Ideal para muestreo continuo | — |

### Modo Circular del DMA

Configuramos el DMA en **modo circular**: cuando llega al final del buffer, vuelve al principio y sigue escribiendo. Es un buffer circular infinito. Las dos interrupciones (HalfComplete y Complete) nos permiten procesar una mitad mientras el DMA llena la otra, sin perder muestras.

```
Buffer:  [ ---- MITAD A ---- | ---- MITAD B ---- ]
          ↑ CPU procesa A      ↑ DMA llena B
          
Luego:   [ ---- MITAD A ---- | ---- MITAD B ---- ]
          ↑ DMA llena A        ↑ CPU procesa B
```

> [!TIP]
> **En resumen**: DMA = el ADC llena un buffer automáticamente sin molestar al CPU. El CPU solo se activa para procesar lotes de datos. Es la forma estándar y profesional de muestrear señales continuas como el ECG.

---

## 2. ¿Por qué TIM3 y no TIM2? ¿Cuál es la diferencia?

### ¿Para qué se usa un Timer con el ADC?

El ADC necesita que "alguien" le diga **cuándo convertir**. Si el ADC convierte "cuando quiere" (modo continuo libre), la frecuencia de muestreo no es precisa ni predecible. En un ECG, necesitamos muestrear a **exactamente 500 Hz** (cada 2 ms), ni más ni menos.

Para eso usamos un Timer:

```
TIM3 cuenta → overflow cada 2 ms → genera evento TRGO → ADC convierte
```

El Timer cuenta hasta un valor (Period), genera un evento, y ese evento **dispara** el ADC automáticamente (sin intervención del CPU).

### Diferencias entre TIM2 y TIM3

| Característica | TIM2 | TIM3 |
|---|---|---|
| **Resolución del contador** | **32 bits** (0 a 4,294,967,295) | **16 bits** (0 a 65,535) |
| **Bus** | APB1 | APB1 |
| **Frecuencia del timer** | 90 MHz* | 90 MHz* |
| **Puede disparar ADC** | ✅ Sí | ✅ Sí |
| **Canales de captura/comparación** | 4 | 4 |

*Con SYSCLK=180 MHz, APB1 prescaler /4 = 45 MHz, pero timers APB1 se multiplican x2 = 90 MHz.

### Entonces, ¿por qué elegí TIM3?

**No hay una razón técnica fuerte.** Ambos pueden hacer exactamente lo mismo para nuestro caso. La elección fue por organización:

- **TIM2 es de 32 bits** → Es más "valioso" porque puede contar intervalos muchísimo más largos. Lo dejamos libre por si después queremos medir tiempos largos (por ejemplo, el intervalo R-R para calcular BPM con alta precisión).
- **TIM3 es de 16 bits** → Para generar un disparo cada 2 ms (500 Hz), con 90 MHz de base, el Period máximo que necesitamos es pequeño. 16 bits es **más que suficiente**.

> [!NOTE]
> **Podés usar TIM2 sin ningún problema.** Si te resulta más cómodo porque ya lo conocés del TP anterior, adelante. La configuración es idéntica. Solo cambia el nombre del timer en CubeMX.

### Cálculo del Timer para 500 Hz

```
f_timer = 90 MHz (frecuencia del timer en APB1)
f_muestreo = 500 Hz

Prescaler + 1 = 9000  →  f_tick = 90 MHz / 9000 = 10 kHz (tick cada 100 μs)
Period + 1   = 20     →  f_overflow = 10 kHz / 20 = 500 Hz ✓

Entonces: Prescaler = 8999, Period (ARR) = 19
```

---

## 3. ¿Qué es SPI y cómo funciona?

### Concepto

**SPI (Serial Peripheral Interface)** es un protocolo de comunicación **serial, síncrono, full-duplex** que se usa para transferir datos entre un **Master** (nuestro STM32) y uno o más **Slaves** (el display ILI9341).

- **Serial**: Los bits se envían uno atrás de otro por un solo cable.
- **Síncrono**: Hay un cable de reloj (clock) que marca el ritmo. El master dice "ahora leé un bit".
- **Full-duplex**: Puede enviar y recibir datos al mismo tiempo (por cables separados).

### Los 4 cables fundamentales del SPI

```
    STM32 (Master)              ILI9341 (Slave)
    ┌──────────────┐            ┌──────────────┐
    │         SCK  ├───────────→│ SCK          │   🕐 Reloj
    │        MOSI  ├───────────→│ SDI (MOSI)   │   📤 Datos Master→Slave
    │        MISO  ├←───────────│ SDO (MISO)   │   📥 Datos Slave→Master
    │          CS  ├───────────→│ CS           │   🎯 "¡Te hablo a vos!"
    └──────────────┘            └──────────────┘
```

| Pin | Nombre completo | Dirección | ¿Para qué sirve? |
|---|---|---|---|
| **SCK** | Serial Clock | Master → Slave | El reloj. Cada pulso = un bit transferido. El master lo genera, el slave lo sigue. |
| **MOSI** | Master Out, Slave In | Master → Slave | Por acá el STM32 **envía** datos al display (comandos, pixeles, colores). |
| **MISO** | Master In, Slave Out | Slave → Master | Por acá el display **devuelve** datos al STM32 (por ejemplo, leer el ID del chip). En la práctica, para displays casi no se usa. |
| **CS** | Chip Select (activo LOW) | Master → Slave | Cuando el STM32 pone CS en **LOW**, el display sabe que le están hablando. Cuando CS está en **HIGH**, el display ignora todo. Esto permite tener **múltiples slaves** en el mismo bus SPI. |

### Cables adicionales del display (NO son parte de SPI, son del ILI9341)

| Pin | ¿Para qué? |
|---|---|
| **DC (Data/Command)** | Si está en **LOW**, el byte que envío es un **comando** (ej: "inicializate", "ponete en modo sleep"). Si está en **HIGH**, el byte es **data** (ej: el color de un píxel). |
| **RESET** | Pulso LOW para hacer un **hardware reset** del display. Se usa solo al inicio. |
| **LED/BL** | Enciende/apaga el **backlight** (la luz de fondo). Sin esto, la pantalla está oscura. |

### ¿Cómo se transmite un byte por SPI?

Ejemplo: enviar el byte `0xA5` (10100101 en binario):

```
CS   ───┐                                              ┌───
        └──────────────────────────────────────────────┘
SCK      ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐
     ────┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─────
MOSI ──┐   ┌───┐       ┌───┐       ┌───┐ 
       └───┘   └───────┘   └───────┘   └──────
         1   0   1   0   0   1   0   1
         ↑                               ↑
       MSB first                        LSB
```

1. Master baja CS → "¡Display, escuchá!"
2. Master genera 8 pulsos de clock en SCK
3. En cada flanco del clock, el display lee un bit de MOSI
4. Después de 8 pulsos, el byte completo (`0xA5`) llegó al display
5. Master sube CS → "Listo, ya no te hablo"

### ¿Por qué SPI y no I2C para el display?

| | SPI | I2C |
|---|---|---|
| **Velocidad** | Hasta **45 MHz** en STM32F4 | Hasta **400 kHz** (o 1 MHz en Fast-mode+) |
| **Cables** | 4+ (SCK, MOSI, MISO, CS) | 2 (SDA, SCL) |
| **Para un display** | ✅ Ideal | ❌ Demasiado lento |

Un display de 240x320 con 16 bits por píxel = 153,600 bytes por frame. A 400 kHz (I2C) tardaría ~3 segundos por frame. A 22.5 MHz (SPI) tarda ~55 ms. **SPI es 50x más rápido.**

> [!TIP]
> **Regla práctica**: I2C para sensores lentos (temperatura, presión), SPI para cosas rápidas (displays, memorias flash, SD cards).

---

## 4. ¿Puedo imprimir los valores del ADC en consola?

### ¡SÍ! Y es la mejor forma de empezar. 🎉

La NUCLEO-F446RE tiene el **ST-Link V2-1** integrado, que incluye un **puente USB-UART**. Esto significa que el puerto USB que usás para programar la placa **también funciona como puerto serie (COM)**.

```
STM32 (USART2: PA2/PA3) ──→ ST-Link ──→ USB ──→ Tu PC (COM port)
```

### ¿Cómo se configura?

En CubeMX, habilitar **USART2** (que es el que está conectado al ST-Link):
- PA2 = USART2_TX (el STM32 **envía** datos a la PC)
- PA3 = USART2_RX (el STM32 **recibe** datos de la PC)
- Baud Rate: 115200
- Word Length: 8 bits
- Parity: None
- Stop Bits: 1

### ¿Cómo se usa en código?

```c
#include <stdio.h>
#include <string.h>

// Función simple para enviar un string por UART
void UART_Print(char *msg) {
    HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
}

// En tu loop principal:
char buffer[64];
sprintf(buffer, "ADC: %d\r\n", adc_value);
UART_Print(buffer);
```

### ¿Cómo lo ves en la PC?

1. Conectás la Nucleo por USB
2. Abrís un **terminal serie** (PuTTY, Tera Term, o el terminal integrado de STM32CubeIDE)
3. Seleccionás el COM port correcto (lo ves en Administrador de Dispositivos)
4. Configurás 115200 baud, 8N1
5. Ves los valores en tiempo real:
```
ADC: 2048
ADC: 2051
ADC: 2045
ADC: 2053
...
```

> [!IMPORTANT]
> **Mi recomendación fuerte: empezá con UART primero, display después.** Así podés verificar que el ADC funciona, que el AD8232 da señal, que el muestreo es correcto, todo **sin agregar la complejidad del display**. Una vez que el ADC funcione perfecto, agregamos el display sobre una base sólida.

---

## 5. ¿Existe librería oficial de Adafruit para ILI9341 en STM32?

### Respuesta corta: No directamente, pero hay opciones excelentes.

### La librería de Adafruit

La [Adafruit_ILI9341](https://github.com/adafruit/Adafruit_ILI9341) está escrita en **C++ para el framework Arduino**. Usa clases, herencia (`Adafruit_GFX`), y las funciones de Arduino (`digitalWrite`, `SPI.transfer`). **No se puede usar directamente** con STM32 HAL, que es C puro.

### ¿Qué usamos entonces?

Hay varias opciones bien probadas para STM32 + HAL + ILI9341:

| Opción | Fuente | Descripción |
|---|---|---|
| **Controllers Tech ILI9341** | [controllerstech.com](https://controllerstech.com/stm32-with-ili9341-tft-display/) | Tutorial paso a paso con driver completo para HAL. Muy bien documentado. **Recomendada por tu profe.** |
| **martnak/STM32-ILI9341** | [GitHub](https://github.com/martnak/STM32-ILI9341) | Driver liviano, solo SPI, compatible con HAL. Fácil de integrar. |
| **afiskon/stm32-ili9341** | [GitHub](https://github.com/afiskon/stm32-ili9341) | Otro driver popular, incluye fuentes y formas geométricas. |

Todas estas librerías hacen lo mismo internamente:

```c
// Enviar un comando al display:
void ILI9341_SendCommand(uint8_t cmd) {
    HAL_GPIO_WritePin(DC_GPIO_Port, DC_Pin, GPIO_PIN_RESET);  // DC = LOW → comando
    HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET);  // CS = LOW → seleccionar
    HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);         // Enviar por SPI
    HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);    // CS = HIGH → deseleccionar
}

// Enviar datos (ej: color de un píxel):
void ILI9341_SendData(uint8_t *data, uint16_t size) {
    HAL_GPIO_WritePin(DC_GPIO_Port, DC_Pin, GPIO_PIN_SET);    // DC = HIGH → datos
    HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi1, data, size, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);
}
```

> [!TIP]
> **Plan**: Vamos a tomar como base el driver de Controllers Tech (que tu profe recomienda como fuente), adaptarlo a nuestro pinout, y agregar las funciones específicas que necesitemos para el ECG (scroll, gráfico en tiempo real).

### ¿Y el touch?

El touch de tu pantalla probablemente usa el chip **XPT2046**, que también es SPI. Se puede compartir el mismo bus SPI1 con un CS diferente, o usar otro SPI. Pero esto lo dejamos para una fase posterior — primero ECG funcionando.

---

## 6. Los electrodos: verde, amarillo y rojo

Los colores siguen el estándar internacional IEC:

| Color | Nombre | Ubicación | Pin AD8232 |
|---|---|---|---|
| 🔴 **Rojo** | RA (Right Arm) | **Brazo derecho** / debajo de la clavícula derecha | INPUT - |
| 🟡 **Amarillo** | LA (Left Arm) | **Brazo izquierdo** / debajo de la clavícula izquierda | INPUT + |
| 🟢 **Verde** | RL (Right Leg) | **Pierna derecha** / cadera derecha / costilla inferior derecha | Right Leg Drive (referencia) |

```
        🔴 RA                    🟡 LA
    (Right Arm)              (Left Arm)
         ×─────────────────────×
         │                     │
         │     ♥ Corazón       │
         │                     │
         │                     │
         └──────────×──────────┘
                 🟢 RL
             (Right Leg)
```

> [!NOTE]
> El electrodo verde (RL) **no mide señal**. Su función es ser la referencia para el circuito Right Leg Drive del AD8232, que cancela activamente el ruido de modo común. Sin él, la señal sería muy ruidosa.

---

## 7. ¿Qué es FreeRTOS y por qué lo necesitamos?

### El problema: hacer muchas cosas "al mismo tiempo"

Hasta ahora pensamos en un programa que corre en un `while(1)` infinito. Dentro de ese loop, el CPU hace todo secuencialmente:

```c
// Sin RTOS: todo en un solo loop
while (1) {
    leer_adc();              // ¿Y si tarda mucho?
    procesar_ecg();          // ¿Y si el ADC necesita atención ya?
    actualizar_display();    // Esto tarda ~50ms ¡y es bloqueante!
    verificar_leads_off();
    imprimir_uart();
}
```

**Problemas:**
- Si `actualizar_display()` tarda 50 ms, durante ese tiempo **no se procesan muestras del ECG** → perdés datos
- Si `procesar_ecg()` tarda mucho, **el display no se actualiza** → se ve lento
- No podés definir qué es más urgente y qué puede esperar
- Todo depende de todo: un cambio en una función afecta el timing de las demás

### La solución: un Sistema Operativo en Tiempo Real (RTOS)

Un RTOS (Real-Time Operating System) es un mini sistema operativo que corre **dentro del microcontrolador** y permite ejecutar múltiples **tareas (tasks)** de forma "simultánea", con **prioridades** y **garantías de tiempo**.

> [!NOTE]
> **No es como Windows o Linux.** FreeRTOS es extremadamente liviano (~5-10 KB de código). No tiene interfaz gráfica, no maneja archivos, no tiene drivers USB. Solo hace UNA cosa: **repartir tiempo del CPU entre tareas según prioridades.**

### ¿Qué es FreeRTOS específicamente?

- **Free**: Es open-source y gratuito (licencia MIT)
- **RTOS**: Real-Time Operating System
- Es el RTOS **más usado del mundo** en microcontroladores (usado por Amazon/AWS IoT)
- **STM32CubeMX lo integra nativamente** → lo habilitás con un click y se genera todo
- Está mantenido activamente y tiene excelente documentación

### Conceptos fundamentales

#### 7.1 Tasks (Tareas)

Una **task** es como un programa independiente que corre dentro del microcontrolador. Cada task tiene su propio código, su propio stack (memoria), y **cree que tiene el CPU para ella sola**.

```c
// Ejemplo: tarea que lee el ADC cada 2ms
void Task_ADC(void *argument) {
    for (;;) {  // Loop infinito (cada task tiene el suyo)
        // Procesar nuevas muestras del ADC
        procesar_muestras();
        
        // "Dormirse" por 2ms y dejar que otras tareas corran
        osDelay(2);
    }
}

// Ejemplo: tarea que actualiza el display cada 33ms (~30 FPS)
void Task_Display(void *argument) {
    for (;;) {
        actualizar_pantalla_ecg();
        
        osDelay(33);  // ~30 frames por segundo
    }
}
```

**El RTOS "salta" entre tareas** tan rápido (cada 1 ms por defecto) que parece que todas corren al mismo tiempo. Esto se llama **scheduling** (planificación).

#### 7.2 Prioridades

Cada tarea tiene una **prioridad** (un número). Cuando dos tareas quieren correr al mismo tiempo, gana la de **mayor prioridad**. La de menor prioridad queda "pausada" hasta que la de mayor prioridad se duerma o termine.

Para nuestro ECG:

```
Prioridad ALTA    → Task_ADC_Processing   (no podemos perder muestras)
Prioridad MEDIA   → Task_ECG_Fiducials    (procesar los puntos fiduciales)
Prioridad BAJA    → Task_Display          (actualizar pantalla puede esperar unos ms)
Prioridad BAJA    → Task_UART_Debug       (imprimir por consola es lo menos urgente)
```

Así, si el display está dibujando y llega una nueva muestra del ADC, el RTOS **pausa el display**, procesa la muestra, y **retoma el display** donde lo dejó. **Nunca se pierde una muestra.**

#### 7.3 El Scheduler (Planificador)

Es el "cerebro" de FreeRTOS. Decide qué tarea corre en cada momento:

```
Tiempo →→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→

Task_ADC      ██░░░░░░░░██░░░░░░░░██░░░░░░░░██░░░░░░░░░░░██
Task_ECG      ░░██░░░░░░░░██░░░░░░░░░░░░░░░░░░██░░░░░░░░░░░░
Task_Display  ░░░░████░░░░░░████░░░░████████░░░░████░░░░░░░░
Task_UART     ░░░░░░░░██░░░░░░░░░░░░░░░░░░░░░░░░░░░░████░░░░

              █ = corriendo    ░ = esperando/dormida
```

El scheduler usa un **tick** (generalmente cada 1 ms) para decidir. En cada tick, mira:
1. ¿Hay alguna tarea de alta prioridad lista para correr? → Le da el CPU
2. ¿La tarea actual llamó a `osDelay()`? → La pone a dormir y busca otra
3. ¿Dos tareas tienen la misma prioridad? → Las alterna (round-robin)

#### 7.4 Queues (Colas de mensajes)

Las tareas necesitan **comunicarse entre sí**. Por ejemplo, la tarea del ADC obtiene muestras y la tarea del display necesita esos datos para graficarlos.

Una **Queue** es como un buzón:

```
Task_ADC  ──→  pone muestra en la cola  ──→  [ 2048 | 2051 | 2045 | ... ]
                                                                    │
Task_Display  ←── saca muestras de la cola ←────────────────────────┘
```

```c
// La tarea del ADC pone datos en la cola:
void Task_ADC(void *argument) {
    for (;;) {
        uint16_t muestra = obtener_muestra_adc();
        osMessageQueuePut(ecg_queue, &muestra, 0, 0);  // Poner en la cola
        osDelay(2);
    }
}

// La tarea del display los saca:
void Task_Display(void *argument) {
    for (;;) {
        uint16_t muestra;
        if (osMessageQueueGet(ecg_queue, &muestra, NULL, 10) == osOK) {
            dibujar_punto_ecg(muestra);  // Graficar
        }
        osDelay(33);
    }
}
```

> [!TIP]
> Las Queues son **thread-safe** (seguras para múltiples tareas). FreeRTOS se encarga de que dos tareas no escriban al mismo tiempo y se corrompan los datos.

#### 7.5 Semaphores (Semáforos)

Un **semáforo binario** es como una bandera que dice "pasó algo". Se usa para que una interrupción (ISR) le avise a una tarea que tiene trabajo nuevo.

```
DMA Interrupt (ISR)                        Task_ECG_Processing
      │                                           │
      │  "¡Llegaron 128 muestras nuevas!"         │
      │──── osSemaphoreRelease(sem) ──────────→    │
      │                                    "¡Ah, ya me fijo!"
      │                                    osSemaphoreAcquire(sem)
      │                                    procesar_128_muestras();
```

```c
// En la interrupción del DMA (se ejecuta cuando el buffer está lleno):
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
    osSemaphoreRelease(ecg_data_ready);  // "¡Hay datos nuevos!"
}

// En la tarea de procesamiento:
void Task_ECG_Processing(void *argument) {
    for (;;) {
        osSemaphoreAcquire(ecg_data_ready, osWaitForever);  // Esperar señal
        // Procesar las muestras del buffer DMA
        detectar_fiduciales(adc_buffer);
    }
}
```

> [!NOTE]
> La ventaja vs. hacer todo en la interrupción: las interrupciones deben ser **cortísimas** (microsegundos). El procesamiento pesado (detectar fiduciales, calcular BPM) se hace en la tarea, no en la ISR.

#### 7.6 Mutexes (Exclusión Mutua)

Un **mutex** es como la llave del baño: solo una persona puede entrar a la vez. Se usa cuando dos tareas necesitan acceder al mismo recurso (por ejemplo, el bus SPI del display).

```c
// Si el display y el touch usan el mismo SPI:
osMutexAcquire(spi_mutex, osWaitForever);  // "Pido el SPI"
ILI9341_DrawPixel(x, y, color);            // Uso el SPI
osMutexRelease(spi_mutex);                  // "Libero el SPI"
```

### 7.7 ¿Cómo se activa FreeRTOS en CubeMX?

Es **un click**:

1. En CubeMX → **Middleware and Software Packs → FREERTOS**
2. Interface: **CMSIS_V2** (es un wrapper estándar sobre FreeRTOS)
3. Se crean las tareas desde la pestaña **Tasks and Queues**

> [!IMPORTANT]
> **Cuando habilitás FreeRTOS, CubeMX automáticamente cambia el HAL timebase de SysTick a TIM6.** Esto es porque FreeRTOS necesita SysTick para su propio scheduler, y HAL también lo usa para `HAL_Delay()`. Para evitar conflictos, HAL pasa a usar otro timer (TIM6) y FreeRTOS se queda con SysTick.

### 7.8 Nuestras tareas para el proyecto ECG

| Tarea | Prioridad | Función | Stack |
|---|---|---|---|
| `Task_ADC_Processing` | **Alta** (osPriorityAboveNormal) | Recibe semáforo del DMA, copia datos | 256 words |
| `Task_ECG_Fiducials` | **Media-Alta** (osPriorityNormal1) | Detecta P, QRS(R), T, calcula BPM | 512 words |
| `Task_Display` | **Normal** (osPriorityNormal) | Dibuja ECG y fiduciales en ILI9341 | 512 words |
| `Task_LeadsOff` | **Baja** (osPriorityBelowNormal) | Monitorea LO+ y LO- | 128 words |
| `defaultTask` | **Idle** (osPriorityLow) | No hace nada (requerida por FreeRTOS) | 128 words |

### 7.9 Diagrama de comunicación entre tareas

```
                    ┌───────────────────┐
                    │   DMA Interrupt   │
                    │  (buffer lleno)   │
                    └────────┬──────────┘
                             │ osSemaphoreRelease
                             ▼
                    ┌───────────────────┐
                    │ Task_ADC_Process  │
                    │   (prioridad ↑)   │──── copia datos a buffer circular
                    └────────┬──────────┘
                             │ osMessageQueuePut
                             ▼
              ┌──────────────────────────────┐
              │        ecg_data_queue        │  ← Cola de muestras procesadas
              └──────┬───────────────┬───────┘
                     │               │
                     ▼               ▼
           ┌──────────────┐  ┌──────────────┐
           │ Task_ECG_    │  │ Task_Display │
           │ Fiducials    │  │ (prioridad ↓)│
           │ (prioridad →)│  │              │
           └──────┬───────┘  └──────────────┘
                  │                  ↑
                  │ osMessageQueuePut│
                  ▼                  │
           ┌──────────────┐          │
           │ fiducial_q   │──────────┘
           │ (P, R, T,    │  ← El display lee de ambas colas
           │  BPM datos)  │
           └──────────────┘
```

### 7.10 Analogía final: el restaurante

Pensá en FreeRTOS como el **gerente de un restaurante**:

| Concepto RTOS | Analogía restaurante |
|---|---|
| **Tasks** | Los empleados: cocinero, mozo, cajero |
| **Scheduler** | El gerente que decide quién trabaja ahora |
| **Prioridades** | El cocinero tiene prioridad sobre el que limpia |
| **Queue** | La ventanilla de pedidos entre el mozo y la cocina |
| **Semaphore** | La campana "¡pedido listo!" |
| **Mutex** | La llave del baño (solo uno a la vez) |
| **Stack** | El espacio de trabajo de cada empleado |

---

## Resumen: Enfoque por fases (actualizado con FreeRTOS)

| Fase | Qué hacemos | Qué verificamos |
|---|---|---|
| **1. Base** | Crear proyecto, configurar clocks, USART2, LED blink | Compilar, flashear, ver "Hello ECG" en consola |
| **2. ADC** | Configurar ADC + TIM3 + DMA, leer AD8232 | Ver valores ADC crudos en consola serie |
| **3. FreeRTOS** | Habilitar FreeRTOS, migrar a tareas | ADC + UART funcionando como tareas separadas |
| **4. Display** | Agregar SPI + ILI9341 como tarea | Pantalla enciende, muestra colores/texto |
| **5. ECG en pantalla** | Graficar señal ECG en tiempo real | Onda ECG visible en display |
| **6. Fiduciales** | Algoritmo de detección P, QRS(R), T | Marcar puntos en pantalla, calcular BPM |
| **7. Polish** | Leads-off, UI bonita, touch (opcional) | Todo integrado y funcionando |

Cada fase se verifica antes de pasar a la siguiente. **Nunca sumamos dos problemas al mismo tiempo.**
