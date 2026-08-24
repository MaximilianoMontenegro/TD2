# Driver ILI9341 - STM32

Driver para controlar un display TFT ILI9341 mediante interfaz
paralela de 8 bits utilizando STM32 HAL.

Autor: Maximiliano Montenegro
Contacto: maximontenegro@frba.utn.edu.ar

## Archivos

- `ili9341.h`: interfaz pública del driver.
- `ili9341.c`: implementación del controlador.
- `font5x7.h`: definición de la fuente.
- `font5x7.c`: datos de la fuente 5x7.

## Configuración

Para habilitar o deshabilitar el driver se utiliza la siguiente
definición dentro de `ili9341.h`:

```c
#define ILI9341_ENABLED 0
```

Donde:

- `0`: driver deshabilitado.
- `1`: driver habilitado.

Esto permite incorporar los archivos de la librería al proyecto sin
necesidad de configurar inmediatamente los GPIO utilizados por el display.

Los GPIO utilizados para el display deben configurarse como
salidas digitales en STM32CubeMX.

El driver espera las siguientes etiquetas:

- LCD_D0
- LCD_D1
- LCD_D2
- LCD_D3
- LCD_D4
- LCD_D5
- LCD_D6
- LCD_D7
- LCD_WR
- LCD_RS
- LCD_CS
- LCD_RST

## Uso básico

En `main.c`:

```c
#include "ili9341.h"
```

### Inicialización

```c
ILI9341_Init();
ILI9341_SetRotation(1);
ILI9341_FillScreen(ILI9341_BLACK);
```

### Ejemplo de uso

```c
ILI9341_WriteString(
    20,
    20,
    "Hola",
    ILI9341_WHITE,
    ILI9341_BLACK,
    2
);
```
