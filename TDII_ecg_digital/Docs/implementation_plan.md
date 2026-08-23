# ECG Digital — Plan de Implementación por Fases

## Confirmaciones del usuario

- **Fiduciales**: Complejo QRS (pico R), onda P, onda T ✅
- **Display**: ILI9341, SPI, 8-bit, 240x320, **con touch** (XPT2046) ✅
- **Electrodos**: 3 electrodos con sopapas (🔴 rojo=RA, 🟡 amarillo=LA, 🟢 verde=RL) ✅
- **Enfoque**: El usuario configura CubeMX con guía paso a paso, no generación automática ✅
- **Estrategia**: Fases incrementales, empezando con UART para debug ✅
- **FreeRTOS**: Se integra como Fase 3, después de validar ADC en bare-metal ✅

---

## Fase 1: Proyecto base + UART (Hello ECG)

**Objetivo**: Crear el proyecto, configurar clocks, y verificar que podemos imprimir por consola.

### Paso 1.1 — Crear `.gitignore` raíz *(lo hago yo)*

Archivo a nivel `Digital-Techniques-II/.gitignore` que cubre todos los proyectos.

### Paso 1.2 — Crear el proyecto en STM32CubeIDE *(lo hacés vos)*

1. **Abrir STM32CubeIDE**
2. **File → New → STM32 Project**
3. En el **Target Selection**:
   - Pestaña **"Board Selector"**
   - Buscar: `NUCLEO-F446RE`
   - Seleccionar y click **Next**
4. **Project Setup**:
   - Project Name: `TDII_ecg_digital`
   - ❌ Desmarcar "Use default location"
   - Location: `C:\Users\facundo\STM32CubeIDE\workspace_1.19.0\Digital-Techniques-II\TDII_ecg_digital`
   - Targeted Language: **C**
   - Targeted Binary Type: **Executable**
   - Targeted Project Type: **STM32Cube** (default)
5. Click **Finish**
6. Te pregunta si querés inicializar periféricos con sus valores de la board → **Yes**
   - Esto pre-configura USART2 (PA2/PA3) que está conectado al ST-Link ✅

### Paso 1.3 — Configurar clocks en CubeMX *(lo hacés vos)*

Una vez abierto el editor `.ioc`:

1. Ir a la pestaña **"Clock Configuration"**
2. Configurar:
   - Input frequency: `8 MHz` (HSE, ya debería estar)
   - PLL Source: **HSE**
   - PLLM: `4`
   - PLLN: `180`
   - PLLP: `/2`
   - System Clock Mux: **PLLCLK**
   - HCLK: debería mostrar **180 MHz**
   - APB1 Prescaler: `/4` → **45 MHz**
   - APB2 Prescaler: `/2` → **90 MHz**
3. Si CubeMX muestra un error de frecuencia máxima, avisame.

### Paso 1.4 — Verificar USART2 *(lo hacés vos)*

1. En la pestaña **"Pinout & Configuration"**, columna izquierda → **Connectivity → USART2**
2. Verificar que Mode = **Asynchronous**
3. Verificar parámetros:
   - Baud Rate: `115200`
   - Word Length: `8 Bits`
   - Parity: `None`
   - Stop Bits: `1`
4. En la vista del chip, verificar que PA2 = USART2_TX y PA3 = USART2_RX

### Paso 1.5 — Generar código *(lo hacés vos)*

1. **Project → Generate Code** (o Ctrl+Shift+G)
2. Se generan todos los archivos en `Core/Src`, `Core/Inc`, `Drivers/`

### Paso 1.6 — Agregar código de prueba *(lo hago yo)*

Yo agrego en `main.c` un "Hello ECG" por UART y un blink del LED LD2.

### Paso 1.7 — Compilar y flashear *(lo hacés vos)*

1. **Project → Build Project** (Ctrl+B)
2. **Run → Debug As → STM32 C/C++ Application**
3. Abrir terminal serie (PuTTY/Tera Term), COM port, 115200 baud
4. Verificar que ves "Hello ECG" en consola

### ✅ Criterio de éxito Fase 1
- [ ] Proyecto compila sin errores
- [ ] LED LD2 parpadea
- [ ] "Hello ECG" aparece en el terminal serie

---

## Fase 2: ADC + Timer + DMA (leer AD8232 en bare-metal)

**Objetivo**: Muestrear la señal del AD8232 a 500 Hz y ver los valores por UART.

> [!NOTE]
> Hacemos esto primero **sin FreeRTOS** para que veas cómo funciona en bare-metal, y después en la Fase 3 lo migramos a tareas. Así se entiende mejor qué problema resuelve el RTOS.

### Paso 2.1 — Configurar ADC1 en CubeMX *(lo hacés vos, con mi guía)*

1. **Pinout & Configuration → Analog → ADC1**
2. Habilitar **IN0** (asigna PA0)
3. En parámetros:
   - Resolution: `12 bits`
   - Scan Conversion Mode: `Disabled`
   - Continuous Conversion Mode: `Disabled` (lo dispara el timer)
   - DMA Continuous Requests: `Enabled`
   - External Trigger Conversion Source: `Timer 3 Trigger Out event`
   - External Trigger Conversion Edge: `Rising Edge`
   - Sampling Time para IN0: `84 Cycles`

### Paso 2.2 — Configurar DMA para ADC1 *(lo hacés vos)*

1. En ADC1, pestaña **"DMA Settings"**
2. Click **Add** → seleccionar `ADC1`
3. Configurar:
   - Stream: `DMA2 Stream 0`
   - Direction: `Peripheral to Memory`
   - Priority: `High`
   - Mode: `Circular`
   - Data Width: Periph = `Half Word`, Memory = `Half Word`

### Paso 2.3 — Configurar TIM3 *(lo hacés vos)*

1. **Timers → TIM3**
2. Clock Source: `Internal Clock`
3. Parámetros:
   - Prescaler: `8999`
   - Counter Period (ARR): `19`
   - Trigger Event Selection (TRGO): `Update Event`

### Paso 2.4 — Configurar GPIO para Leads-Off *(lo hacés vos)*

1. Pin **PB0** → `GPIO_Input` → Label: `LO_PLUS`, Pull: `Pull-Down`
2. Pin **PB1** → `GPIO_Input` → Label: `LO_MINUS`, Pull: `Pull-Down`
3. Pin **PB2** → `GPIO_Output` → Label: `AD8232_SDN`, Default: `High`

### Paso 2.5 — Habilitar interrupciones NVIC *(lo hacés vos)*

1. En **System Core → NVIC**
2. Verificar que **DMA2 Stream0 global interrupt** está habilitada, Prioridad: `1`

### Paso 2.6 — Generar código y agregar lógica *(ambos)*

Yo agrego la lógica de DMA + impresión por UART.

### ✅ Criterio de éxito Fase 2
- [ ] Valores ADC aparecen en la consola serie
- [ ] Valores cambian al tocar/mover los electrodos
- [ ] Frecuencia de muestreo es 500 Hz (verificable con timestamp)

---

## Fase 3: FreeRTOS — Migrar a tareas ⚡ NUEVO

**Objetivo**: Habilitar FreeRTOS y convertir el código bare-metal en tareas independientes.

> [!IMPORTANT]
> Esta fase toma el código que YA funciona de la Fase 2 y lo reorganiza en tareas FreeRTOS. No agregamos funcionalidad nueva — solo cambiamos la **arquitectura**. Si algo se rompe, sabemos que es por la migración, no por código nuevo.

### Paso 3.1 — Habilitar FreeRTOS en CubeMX *(lo hacés vos)*

1. **Middleware and Software Packs → FREERTOS**
2. Interface: **CMSIS_V2**
3. CubeMX mostrará un warning sobre el HAL timebase → aceptar

### Paso 3.2 — Cambiar HAL Timebase a TIM6 *(lo hacés vos)*

1. **System Core → SYS**
2. Timebase Source: cambiar de `SysTick` a **`TIM6`**

> [!WARNING]
> **Esto es obligatorio.** FreeRTOS necesita SysTick para su scheduler. HAL también usa SysTick para `HAL_Delay()`. Si ambos usan SysTick, se pelean. Solución: HAL usa TIM6, FreeRTOS usa SysTick.

### Paso 3.3 — Configurar heap y stack *(lo hacés vos)*

1. En FREERTOS → pestaña **"Config parameters"**:
   - `TOTAL_HEAP_SIZE`: `15360` (15 KB — suficiente para nuestras tareas)
   - `MINIMAL_STACK_SIZE`: `128` (words, para la tarea idle)
   - `MAX_PRIORITIES`: `7`
   - Memory Management scheme: `HEAP_4` (el más flexible y recomendado)

### Paso 3.4 — Crear las tareas iniciales *(lo hacés vos)*

En FREERTOS → pestaña **"Tasks and Queues"**:

**Tareas:**

| Task Name | Priority | Stack Size (words) | Entry Function |
|---|---|---|---|
| `defaultTask` | osPriorityLow | 128 | `StartDefaultTask` |
| `adcTask` | osPriorityAboveNormal | 256 | `StartADCTask` |
| `uartTask` | osPriorityNormal | 256 | `StartUARTTask` |

**Queues:**

| Queue Name | Queue Size | Item Size |
|---|---|---|
| `adcDataQueue` | 128 | `uint16_t` (2 bytes) |

**Semaphores (pestaña "Timers and Semaphores"):**

| Semaphore Name | Type |
|---|---|
| `adcDataReady` | Binary |

> [!NOTE]
> Empezamos con solo 3 tareas. Las de Display y Fiduciales las agregamos cuando lleguemos a esas fases. Así mantenemos la complejidad al mínimo.

### Paso 3.5 — Generar código y migrar lógica *(ambos)*

Yo migro el código bare-metal a las funciones de las tareas:
- `StartADCTask`: Espera el semáforo del DMA → pone muestras en la cola
- `StartUARTTask`: Saca muestras de la cola → imprime por UART

### Paso 3.6 — Ajustar linker script *(lo hago yo)*

Incrementar el heap y stack del linker:
- `_Min_Heap_Size = 0x800` (2 KB mínimo para malloc si fuera necesario)
- `_Min_Stack_Size = 0x800` (2 KB para el stack principal antes de que arranque el scheduler)

### ✅ Criterio de éxito Fase 3
- [ ] El proyecto compila con FreeRTOS habilitado
- [ ] Los valores del ADC siguen apareciendo por UART (misma funcionalidad que Fase 2)
- [ ] Se puede verificar con el debugger que las tareas están corriendo (en la vista de FreeRTOS de STM32CubeIDE: Window → Show View → FreeRTOS Task List)

---

## Fase 4: SPI + Display ILI9341 (como tarea FreeRTOS)

**Objetivo**: Encender el display y dibujar formas/texto, corriendo como tarea FreeRTOS.

### Paso 4.1 — Configurar SPI1 en CubeMX *(lo hacés vos)*

1. **Connectivity → SPI1**
2. Mode: `Full-Duplex Master`
3. Parámetros:
   - Prescaler: `/4` (→ 22.5 MHz)
   - CPOL: `Low`, CPHA: `1 Edge`
   - Data Size: `8 Bits`, First Bit: `MSB First`
   - NSS Signal: `Software`

### Paso 4.2 — Configurar GPIO del display *(lo hacés vos)*

1. **PA5** → se reasigna de GPIO_Output a **SPI1_SCK** (adiós LED LD2)
2. **PA6** → **SPI1_MISO**
3. **PA7** → **SPI1_MOSI**
4. **PB6** → `GPIO_Output` → Label: `TFT_CS`, Default: `High`
5. **PA9** → `GPIO_Output` → Label: `TFT_DC`
6. **PA8** → `GPIO_Output` → Label: `TFT_RST`, Default: `High`

### Paso 4.3 — Agregar tarea del display en CubeMX *(lo hacés vos)*

En FREERTOS → Tasks:

| Task Name | Priority | Stack Size | Entry Function |
|---|---|---|---|
| `displayTask` | osPriorityNormal | 512 | `StartDisplayTask` |

### Paso 4.4 — Agregar driver ILI9341 *(lo hago yo)*

Creo `ili9341.c/.h` basado en Controllers Tech.

### ✅ Criterio de éxito Fase 4
- [ ] Pantalla se enciende y muestra un color sólido
- [ ] Se puede escribir texto ("ECG Digital")
- [ ] El ADC sigue funcionando en paralelo (verificar por UART)

---

## Fase 5: ECG en pantalla (tiempo real)

**Objetivo**: Graficar la señal ECG en el display en tiempo real.

### Lo que hago yo:
- `ecg_display.c/.h` con barrido tipo monitor ECG
- La tarea `displayTask` lee de la cola `adcDataQueue` y grafica
- Mapeo de ADC (0-4095) a coordenadas Y (0-320)

### ✅ Criterio de éxito Fase 5
- [ ] Onda ECG visible y en tiempo real
- [ ] Sin flickering ni pérdida de datos
- [ ] Se puede ver la forma de onda del ECG

---

## Fase 6: Detección de fiduciales (P, QRS/R, T)

**Objetivo**: Detectar los 3 puntos fiduciales y mostrarlos.

### Paso 6.1 — Agregar tarea de fiduciales en CubeMX *(lo hacés vos)*

| Task Name | Priority | Stack Size | Entry Function |
|---|---|---|---|
| `ecgProcessTask` | osPriorityAboveNormal1 | 512 | `StartECGProcessTask` |

**Nueva queue:**

| Queue Name | Queue Size | Item Size |
|---|---|---|
| `fiducialQueue` | 16 | struct con tipo + posición + timestamp |

### Lo que hago yo:
- `ecg_processing.c/.h` con detección de pico R, onda P, onda T
- Marcadores de color en el display
- Cálculo de BPM

### ✅ Criterio de éxito Fase 6
- [ ] Pico R detectado → marcador rojo en pantalla
- [ ] Onda P detectada → marcador azul
- [ ] Onda T detectada → marcador verde
- [ ] BPM mostrado y coherente (~60-100 en reposo)

---

## Fase 7: Polish

- Indicador visual de leads-off
- Tarea de monitoreo de electrodos (`leadsOffTask`)
- UI tipo papel ECG (cuadrícula, leyendas)
- Touch para pausar/reanudar (opcional, con XPT2046)
- Documentación del código

---

## Pinout completo

```
PA0  → ADC1_IN0  (señal OUTPUT del AD8232)
PA2  → USART2_TX (debug consola)
PA3  → USART2_RX (debug consola)
PA5  → SPI1_SCK  (display clock)          [Fase 4: cambia de GPIO a SPI]
PA6  → SPI1_MISO (display MISO)           [Fase 4]
PA7  → SPI1_MOSI (display MOSI)           [Fase 4]
PA8  → TFT_RST   (display reset)          [Fase 4]
PA9  → TFT_DC    (display data/command)   [Fase 4]
PA13 → SWDIO     (debug ST-Link)
PA14 → SWCLK     (debug ST-Link)
PB0  → LO_PLUS   (leads-off +)            [Fase 2]
PB1  → LO_MINUS  (leads-off -)            [Fase 2]
PB2  → AD8232_SDN (shutdown)              [Fase 2]
PB6  → TFT_CS    (display chip select)    [Fase 4]
```

---

## Arquitectura final con FreeRTOS

```
┌─────────────────────────────────────────────────────────────────┐
│                        FreeRTOS Kernel                          │
│                     (SysTick = 1 ms tick)                       │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌──────────────┐  Semáforo  ┌──────────────┐                  │
│  │  DMA ISR     │───────────→│ adcTask      │                  │
│  │ (HW interr.) │           │ (Prio: Alta) │                  │
│  └──────────────┘           └──────┬───────┘                  │
│                                     │                          │
│                              adcDataQueue                      │
│                                     │                          │
│                    ┌────────────────┼─────────────┐            │
│                    ▼                ▼             ▼            │
│           ┌──────────────┐ ┌──────────────┐ ┌──────────┐      │
│           │ecgProcessTask│ │ displayTask  │ │ uartTask │      │
│           │(Prio: Media+)│ │(Prio: Normal)│ │(Prio: N) │      │
│           └──────┬───────┘ └──────────────┘ └──────────┘      │
│                  │                ↑                             │
│           fiducialQueue           │                             │
│                  └────────────────┘                             │
│                                                                 │
│  ┌──────────────┐                                              │
│  │ leadsOffTask │  (monitorea PB0/PB1)                         │
│  │ (Prio: Baja) │                                              │
│  └──────────────┘                                              │
│                                                                 │
│  HAL Timebase: TIM6  │  ADC Trigger: TIM3  │  Display: SPI1   │
└─────────────────────────────────────────────────────────────────┘
```

---

## `.gitignore` — contenido propuesto

### Raíz del repo (`Digital-Techniques-II/.gitignore`)

```gitignore
# ===== STM32CubeIDE / Eclipse Build Artifacts =====
Debug/
Release/
*.o
*.d
*.elf
*.bin
*.hex
*.map
*.list
*.su

# ===== Eclipse/STM32CubeIDE IDE metadata =====
.settings/
.mxproject

# ===== OS artifacts =====
Thumbs.db
.DS_Store
desktop.ini

# ===== Backup & temp =====
*.bak
*.tmp
*~
```

> [!IMPORTANT]
> `.project`, `.cproject` e `.ioc` **SÍ se commitean** (necesarios para importar el proyecto). `.settings/` y `Debug/` **NO** (específicos de cada máquina).

---

## User Review Required

> [!IMPORTANT]
> **Para avanzar necesito que:**
> 1. Apruebes este plan de 7 fases
> 2. Crees el proyecto en STM32CubeIDE siguiendo los pasos 1.2 a 1.5 de la Fase 1
> 3. Me avises cuando esté creado

> [!NOTE]
> **Sobre FreeRTOS como Fase 3**: Lo ubicamos *después* de tener el ADC funcionando en bare-metal. Así podés comparar cómo era antes (todo en `while(1)`) vs. después (tareas separadas con prioridades). Es la mejor forma de aprender qué problema resuelve un RTOS.
