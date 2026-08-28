# Resumen de funciones

## Inicialización y configuración

| Función | Descripción |
|---|---|
| `ILI9341_Init()` | Inicializa el controlador ILI9341 y deja el display listo para utilizar. |
| `ILI9341_SetRotation(rotation)` | Cambia la orientación de la pantalla y actualiza `ILI9341_WIDTH` e `ILI9341_HEIGHT`. |
| `ILI9341_FillScreen(color)` | Rellena toda la pantalla con un color. |
| `ILI9341_SetScrollArea(top, scroll, bottom)` | Configura las regiones fija superior, desplazable y fija inferior del scroll hardware. |
| `ILI9341_SetScroll(offset)` | Modifica la posición actual del scroll hardware del ILI9341. |

---

## Dibujo básico

| Función | Descripción |
|---|---|
| `ILI9341_DrawPixel(x, y, color)` | Dibuja un píxel individual. |
| `ILI9341_DrawLine(x0, y0, x1, y1, color)` | Dibuja una línea entre dos puntos. |
| `ILI9341_DrawHLine(x, y, width, color)` | Dibuja una línea horizontal optimizada. |
| `ILI9341_DrawVLine(x, y, height, color)` | Dibuja una línea vertical optimizada. |

---

## Rectángulos

| Función | Descripción |
|---|---|
| `ILI9341_DrawRectangle(...)` | Dibuja el contorno de un rectángulo. |
| `ILI9341_FillRectangle(...)` | Dibuja un rectángulo completamente relleno. |
| `ILI9341_DrawRoundRect(...)` | Dibuja un rectángulo con esquinas redondeadas. |
| `ILI9341_FillRoundRect(...)` | Dibuja un rectángulo relleno con esquinas redondeadas. |

Las funciones permiten definir libremente la posición y las dimensiones, por lo que los elementos gráficos pueden colocarse en cualquier región de la pantalla.

---

## Círculos y triángulos

| Función | Descripción |
|---|---|
| `ILI9341_DrawCircle(...)` | Dibuja el contorno de un círculo. |
| `ILI9341_FillCircle(...)` | Dibuja un círculo completamente relleno. |
| `ILI9341_DrawTriangle(...)` | Dibuja el contorno de un triángulo definido mediante tres vértices. |
| `ILI9341_FillTriangle(...)` | Dibuja un triángulo completamente relleno. |

---

## Texto y valores numéricos

| Función | Descripción |
|---|---|
| `ILI9341_DrawChar(...)` | Dibuja un carácter utilizando la fuente 5x7. |
| `ILI9341_WriteString(...)` | Escribe una cadena de caracteres. |
| `ILI9341_WriteInt(...)` | Escribe directamente un número entero. |
| `ILI9341_WriteFloat(...)` | Escribe un número decimal con una cantidad configurable de decimales. |
| `ILI9341_WriteValueUnit(...)` | Escribe un valor decimal seguido de su unidad, por ejemplo `2.73 V`. |
| `ILI9341_UpdateValueUnit(...)` | Actualiza un valor con unidad reduciendo el parpadeo. |
| `ILI9341_UpdateValueFixed(...)` | Actualiza un valor dinámico dentro de un campo de ancho fijo, evitando restos de caracteres cuando cambia la longitud del número. |

### Valores dinámicos de ancho fijo

Durante las pruebas se observó que un valor dinámico puede cambiar su cantidad de caracteres.

Por ejemplo:

```text
9.95 V
10.02 V
```

Para evitar que queden caracteres de la medición anterior se implementó:

```c
ILI9341_UpdateValueFixed(...)
```

Esta función reserva una cantidad fija de caracteres para el campo.

Ejemplo:

```c
ILI9341_UpdateValueFixed(
    5,
    65,
    voltage,
    2,
    "V",
    8,
    ILI9341_YELLOW,
    ILI9341_BLUE,
    1
);
```

Esta función es especialmente útil para mostrar valores provenientes del ADC, sensores o variables que cambian continuamente.

---

## Elementos de interfaz

| Función | Descripción |
|---|---|
| `ILI9341_DrawButton(...)` | Dibuja un botón con esquinas redondeadas y texto centrado. |
| `ILI9341_DrawProgressBar(...)` | Dibuja una barra de progreso rectangular. |
| `ILI9341_DrawRoundProgressBar(...)` | Dibuja una barra de progreso con esquinas redondeadas. |

Los elementos pueden posicionarse libremente indicando sus coordenadas y dimensiones.

---

# Gráficos y representación de señales

| Función | Descripción |
|---|---|
| `ILI9341_DrawSignalBuffer(...)` | Representa gráficamente una señal almacenada en un buffer. |
| `ILI9341_GraphValueToY(...)` | Convierte un valor físico comprendido entre un mínimo y un máximo en una coordenada vertical de pantalla. |
| `ILI9341_DrawGraphColumn(...)` | Actualiza una columna individual del gráfico incluyendo fondo, grilla y señal. |
| `ILI9341_DrawGraphColumnWithTimeAxis(...)` | Variante de actualización por columnas que reserva la región correspondiente al eje temporal. |
| `ILI9341_SetScrollArea(...)` | Define las regiones fijas y desplazables utilizadas por el scroll hardware. |
| `ILI9341_SetScroll(...)` | Modifica la posición del área desplazable. |

---

## Conversión de valores a coordenadas

Para desacoplar la magnitud física de las coordenadas del LCD se implementó:

```c
ILI9341_GraphValueToY(...)
```

Por ejemplo:

```c
int16_t y =
    ILI9341_GraphValueToY(
        voltage,
        0.0f,
        3.3f,
        22,
        198
    );
```

De esta forma una tensión comprendida entre:

```text
0.0 V ... 3.3 V
```

se transforma automáticamente en una coordenada vertical dentro de la región seleccionada del gráfico.

Esto permitirá utilizar posteriormente la misma estructura con señales provenientes del ADC.

---

# Scroll hardware y panel fijo

Se implementó el scroll hardware del controlador ILI9341 utilizando:

```c
ILI9341_SetScrollArea(...)
ILI9341_SetScroll(...)
```

En la configuración utilizada durante las pruebas se dividió la pantalla horizontalmente en:

```text
┌──────────┬────────────────────────────────────┐
│          │                                    │
│  PANEL   │          ÁREA DE GRÁFICO           │
│  FIJO    │             CON SCROLL             │
│          │                                    │
│  70 px   │              250 px                │
│          │                                    │
└──────────┴────────────────────────────────────┘
```

La configuración utilizada es:

```c
ILI9341_SetScrollArea(
    70,
    250,
    0
);
```

El panel lateral permanece inmóvil mientras la señal y la grilla se desplazan mediante el scroll hardware.

Esto permite mostrar permanentemente información como:

```text
ADC1

2.73 V


RUN
```

sin que se desplace junto con la señal.

---

# Grilla dinámica

El gráfico utiliza una actualización por columnas.

Cada nueva columna puede contener:

- Fondo.
- Señal.
- Línea horizontal de grilla.
- Línea vertical de grilla.

La grilla vertical se genera actualmente cada:

```text
50 píxeles
```

La actualización por columnas evita tener que redibujar toda la pantalla en cada iteración.

Para aumentar la velocidad visual del desplazamiento se implementó:

```c
#define SCROLL_STEP 2
```

Cuando `SCROLL_STEP` es mayor que 1 se escriben todas las columnas correspondientes al desplazamiento para evitar espacios vacíos en el gráfico.

---

# Interpolación de la señal

Cuando:

```c
SCROLL_STEP > 1
```

se generan valores intermedios entre dos muestras consecutivas.

Conceptualmente:

```text
Muestra anterior
      ●
       \
        ●  <- interpolación
         \
          ●
       Muestra nueva
```

Esto permite aumentar la velocidad visual del barrido manteniendo una representación continua de la señal.

La interpolación utilizada es:

```c
int16_t yTemp =
    yAnterior +
    ((int32_t)(yNueva - yAnterior) * (k + 1))
    / SCROLL_STEP;
```

---

# Eje temporal

Se agregó una región independiente destinada al eje temporal del gráfico.

La configuración utilizada actualmente es:

```c
#define TIME_LABEL_Y   2
#define TIME_AXIS_Y    20

#define GRAPH_Y        22
#define GRAPH_H        198
```

De esta forma se separan las etiquetas temporales de la región donde se representa la señal.

Conceptualmente:

```text
0.0S   0.5S   1.0S   1.5S   2.0S
───────────────────────────────────

        SEÑAL + GRILLA
```

La función:

```c
ILI9341_DrawGraphColumnWithTimeAxis(...)
```

permite actualizar las columnas del gráfico sin modificar la región reservada para las etiquetas temporales.

---

# Tiempo acumulado

La escala temporal fue modificada para no quedar limitada a una única pantalla.

Inicialmente puede observarse:

```text
0.0S   0.5S   1.0S   1.5S   2.0S
```

posteriormente:

```text
2.5S   3.0S   3.5S   4.0S   4.5S
```

y continúa:

```text
5.0S
5.5S
6.0S
...
10.0S
10.5S
...
```

El buffer gráfico continúa funcionando de forma circular, pero el contador temporal no se reinicia cuando se produce el `wrap`.

Por lo tanto:

```text
Memoria gráfica -> circular

Tiempo -> acumulativo
```

Para llevar la cuenta de las divisiones temporales se utiliza:

```c
uint32_t timeGridCount = 0;
```

y actualmente cada división representa:

```c
float gridTime =
    timeGridCount * 0.5f;
```

---

# Generación de señal de prueba

Durante el desarrollo se utilizó una senoide simulada para verificar el funcionamiento del gráfico:

```c
float voltage =
    1.65f +
    1.65f * sinf(fase);
```

Esto genera una señal comprendida aproximadamente entre:

```text
0 V ... 3.3 V
```

centrada en:

```text
1.65 V
```

La frecuencia de la señal depende del incremento de fase y de la frecuencia con la que se ejecuta la generación de muestras.

Para una frecuencia de actualización ideal de:

```text
fs = 50 Hz
```

una senoide de aproximadamente `1 Hz` puede generarse utilizando:

```c
fase += 0.12566f;

if (fase >= 6.283185f)
{
    fase -= 6.283185f;
}
```

El incremento de fase general puede obtenerse mediante:

```text
Δfase = 2π · f / fs
```

donde:

```text
f  = frecuencia de la señal
fs = frecuencia de muestreo
```

---

# Consideración sobre el tiempo real

Durante las pruebas se comprobó que utilizar:

```c
HAL_Delay(20);
```

no garantiza que cada iteración completa dure exactamente `20 ms`.

El tiempo real de una iteración es aproximadamente:

```text
Tiempo de procesamiento
        +
Tiempo de escritura del LCD
        +
HAL_Delay(20)
```

Por este motivo, una escala temporal calculada únicamente contando iteraciones puede presentar diferencias importantes respecto del tiempo medido con un cronómetro externo.

Esto se observó experimentalmente durante las pruebas del barrido.

Para futuras versiones se plantea desacoplar completamente la adquisición de la actualización gráfica mediante:

```text
             TIMER
               │
               ▼
          ADC + DMA
               │
               ▼
        Buffer de muestras
               │
               ▼
         Procesamiento
               │
               ▼
              LCD
```

De esta forma, la frecuencia de adquisición será independiente del tiempo necesario para actualizar el display.

---

# Funciones optimizadas

Para mejorar la velocidad de actualización del display se implementaron versiones optimizadas de distintas operaciones:

- `ILI9341_DrawHLine()`
- `ILI9341_DrawVLine()`
- `ILI9341_FillRectangle()`
- `ILI9341_DrawRectangle()`
- `ILI9341_FillTriangle()`
- `ILI9341_DrawGraphColumn()`
- `ILI9341_DrawGraphColumnWithTimeAxis()`

Estas funciones intentan reducir las escrituras individuales de píxeles y aprovechar la escritura consecutiva mediante ventanas de memoria del controlador ILI9341.

Esto resulta especialmente importante para la representación de señales en tiempo real.

---

# Funciones para datos dinámicos

Para mostrar mediciones pueden utilizarse:

```c
ILI9341_WriteInt(...);
ILI9341_WriteFloat(...);
ILI9341_WriteValueUnit(...);
ILI9341_UpdateValueUnit(...);
ILI9341_UpdateValueFixed(...);
```

Ejemplo:

```c
ILI9341_WriteValueUnit(
    20,
    50,
    2.73f,
    2,
    "V",
    ILI9341_YELLOW,
    ILI9341_BLACK,
    2
);
```

Resultado:

```text
2.73 V
```

Para valores que cambian continuamente se recomienda utilizar:

```c
ILI9341_UpdateValueFixed(...)
```

ya que permite mantener un campo de ancho constante y evita que queden caracteres correspondientes al valor anterior.

---

# Touch resistivo

Se comenzó el estudio del panel táctil resistivo integrado en el módulo.

Mediante el ejemplo de calibración de Arduino se identificó el siguiente conexionado:

```text
YP = A1
XM = A2
YM = D7
XP = D6
```

También se obtuvieron experimentalmente los siguientes valores de calibración:

```c
#define ILI9341_TOUCH_LEFT      907
#define ILI9341_TOUCH_RIGHT     136

#define ILI9341_TOUCH_TOP       942
#define ILI9341_TOUCH_BOTTOM    139
```

El touch es de tipo resistivo de cuatro hilos y comparte algunas conexiones con el bus paralelo utilizado por el LCD.

La lectura requiere cambiar temporalmente la configuración de determinados GPIO entre:

```text
GPIO Output
GPIO Input
ADC
```

dependiendo del eje que se desea medir.

Se creó inicialmente el archivo:

```text
ili9341_touch.h
```

con una estructura prevista para almacenar:

```c
typedef struct
{
    uint16_t x;
    uint16_t y;

    uint16_t rawX;
    uint16_t rawY;

    uint16_t pressure;

    uint8_t pressed;

} ILI9341_TouchPoint;
```

y se plantearon las siguientes funciones:

```c
void ILI9341_Touch_Init(void);

uint16_t ILI9341_Touch_ReadRawX(void);

uint16_t ILI9341_Touch_ReadRawY(void);

uint16_t ILI9341_Touch_ReadPressure(void);

uint8_t ILI9341_Touch_GetPoint(
    ILI9341_TouchPoint *point
);
```

La implementación del touch en STM32 queda temporalmente suspendida hasta definir la estrategia definitiva para compartir los GPIO entre LCD y ADC.

---

# Estado actual del driver

Actualmente el driver implementado permite:

```text
Inicialización del ILI9341
        │
        ▼
Configuración de rotación
        │
        ▼
Primitivas gráficas
        │
        ▼
Texto y números
        │
        ▼
Valores dinámicos
        │
        ▼
Botones y barras
        │
        ▼
Representación de buffers
        │
        ▼
Conversión valor -> coordenada
        │
        ▼
Grilla
        │
        ▼
Scroll hardware
        │
        ▼
Panel lateral fijo
        │
        ▼
Gráfico dinámico
        │
        ▼
Eje temporal
        │
        ▼
Tiempo acumulativo
```

La capa gráfica queda preparada para que posteriormente la señal simulada:

```c
1.65f + 1.65f * sinf(fase)
```

sea reemplazada por muestras reales provenientes del ADC.

---

# Próximos pasos

Las siguientes etapas previstas para el proyecto son:

1. Configuración del ADC del STM32F446RE.
2. Verificación inicial de las muestras mediante debugger.
3. Configuración de un Timer como base temporal.
4. Adquisición mediante ADC + DMA.
5. Implementación de buffers de muestras.
6. Integración del buffer ADC con el gráfico existente.
7. Corrección de la base temporal del gráfico para representar tiempo real.
8. Integración de múltiples canales ADC.
9. Integración de la tarjeta SD mediante SPI.
10. Retomar el touch resistivo una vez definida la estrategia definitiva de hardware.
11. Implementar `ILI9341_DrawImage()` para imágenes RGB565.

---

# Arquitectura prevista para adquisición y visualización

La arquitectura final propuesta separa la adquisición de datos de la actualización del display:

```text
              ┌─────────────┐
              │    TIMER    │
              └──────┬──────┘
                     │
                     ▼
              ┌─────────────┐
              │     ADC     │
              └──────┬──────┘
                     │
                     ▼
              ┌─────────────┐
              │     DMA     │
              └──────┬──────┘
                     │
                     ▼
              ┌─────────────┐
              │   BUFFER    │
              └──────┬──────┘
                     │
                     ▼
              ┌─────────────┐
              │PROCESAMIENTO│
              └──────┬──────┘
                     │
                     ▼
              ┌─────────────┐
              │    LCD      │
              │   ILI9341   │
              └─────────────┘
```

Esta arquitectura permitirá que la frecuencia de muestreo del ADC sea precisa y no dependa de la velocidad de actualización del LCD.
