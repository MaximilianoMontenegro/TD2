## Resumen de funciones

### Inicialización y configuración

| Función | Descripción |
|---|---|
| `ILI9341_Init()` | Inicializa el controlador ILI9341 y deja el display listo para utilizar. |
| `ILI9341_SetRotation(rotation)` | Cambia la orientación de la pantalla y actualiza `ILI9341_WIDTH` e `ILI9341_HEIGHT`. |
| `ILI9341_FillScreen(color)` | Rellena toda la pantalla con un color. |

### Dibujo básico

| Función | Descripción |
|---|---|
| `ILI9341_DrawPixel(x, y, color)` | Dibuja un píxel individual. |
| `ILI9341_DrawLine(x0, y0, x1, y1, color)` | Dibuja una línea entre dos puntos. |
| `ILI9341_DrawHLine(x, y, width, color)` | Dibuja una línea horizontal optimizada. |
| `ILI9341_DrawVLine(x, y, height, color)` | Dibuja una línea vertical optimizada. |

### Rectángulos

| Función | Descripción |
|---|---|
| `ILI9341_DrawRectangle(...)` | Dibuja el contorno de un rectángulo. |
| `ILI9341_FillRectangle(...)` | Dibuja un rectángulo completamente relleno. |
| `ILI9341_DrawRoundRect(...)` | Dibuja un rectángulo con esquinas redondeadas. |
| `ILI9341_FillRoundRect(...)` | Dibuja un rectángulo relleno con esquinas redondeadas. |

### Círculos y triángulos

| Función | Descripción |
|---|---|
| `ILI9341_DrawCircle(...)` | Dibuja el contorno de un círculo. |
| `ILI9341_FillCircle(...)` | Dibuja un círculo relleno. |
| `ILI9341_DrawTriangle(...)` | Dibuja el contorno de un triángulo definido por tres vértices. |
| `ILI9341_FillTriangle(...)` | Dibuja un triángulo relleno. |

### Texto

| Función | Descripción |
|---|---|
| `ILI9341_DrawChar(...)` | Dibuja un carácter utilizando la fuente 5x7. |
| `ILI9341_WriteString(...)` | Escribe una cadena de caracteres. |
| `ILI9341_WriteInt(...)` | Escribe directamente un número entero. |
| `ILI9341_WriteFloat(...)` | Escribe un número decimal con una cantidad configurable de decimales. |
| `ILI9341_WriteValueUnit(...)` | Escribe un valor decimal seguido de su unidad, por ejemplo `2.73 V`. |
| `ILI9341_UpdateValueUnit(...)` | Actualiza un valor con unidad sin borrar previamente el área, reduciendo el parpadeo. |

### Elementos de interfaz

| Función | Descripción |
|---|---|
| `ILI9341_DrawButton(...)` | Dibuja un botón con esquinas redondeadas y texto centrado. |
| `ILI9341_DrawProgressBar(...)` | Dibuja una barra de progreso rectangular. |
| `ILI9341_DrawRoundProgressBar(...)` | Dibuja una barra de progreso con esquinas redondeadas. |

### Gráficos y señales

| Función | Descripción |
|---|---|
| `ILI9341_DrawSignalBuffer(...)` | Representa gráficamente una señal almacenada en un buffer. |
| `ILI9341_DrawGraphColumn(...)` | Actualiza una columna del gráfico, incluyendo señal, fondo y grilla. |
| `ILI9341_SetScrollArea(top, scroll, bottom)` | Configura las regiones fija y desplazable del scroll hardware del ILI9341. |
| `ILI9341_SetScroll(offset)` | Cambia la posición actual del scroll hardware. |

---

## Funciones optimizadas

Para mejorar la velocidad de actualización del display se implementaron versiones optimizadas de varias operaciones:

- `ILI9341_DrawHLine()`
- `ILI9341_DrawVLine()`
- `ILI9341_FillRectangle()`
- `ILI9341_DrawRectangle()`
- `ILI9341_FillTriangle()`

Estas funciones intentan reducir las escrituras individuales de píxeles y aprovechar la escritura consecutiva mediante ventanas de memoria del ILI9341.

Esto es especialmente importante para la representación de señales en tiempo real.

---

## Funciones para datos dinámicos

Para mostrar mediciones pueden utilizarse:

```c
ILI9341_WriteInt(...);
ILI9341_WriteFloat(...);
ILI9341_WriteValueUnit(...);
ILI9341_UpdateValueUnit(...);
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

Para valores que cambian continuamente se recomienda:

```c
ILI9341_UpdateValueUnit(...);
```

ya que sobrescribe directamente el valor anterior y evita el parpadeo producido por borrar previamente el área.

---

## Funciones pendientes / futuras

Funciones consideradas para futuras versiones del driver:

- Campo numérico de ancho fijo.
- `ILI9341_DrawImage()` para imágenes RGB565.
- Carga de imágenes desde microSD.
- Mejoras de la fuente `font5x7`.
- Nuevas funciones específicas para gráficos de señales.
