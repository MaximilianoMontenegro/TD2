/*
 * ili9341.c
 *
 *  Created on: Aug 23, 2026
 *      Author: monezemax
 */

#include "ili9341.h"

#if ILI9341_ENABLED
#include "font5x7.h"
#include <math.h>
#include <stdio.h>

uint16_t ILI9341_WIDTH  = ILI9341_TFTWIDTH;
uint16_t ILI9341_HEIGHT = ILI9341_TFTHEIGHT;

static void LCD_Write8(uint8_t data)
{
    HAL_GPIO_WritePin(LCD_D0_GPIO_Port, LCD_D0_Pin,
                      (data & 0x01) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    HAL_GPIO_WritePin(LCD_D1_GPIO_Port, LCD_D1_Pin,
                      (data & 0x02) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    HAL_GPIO_WritePin(LCD_D2_GPIO_Port, LCD_D2_Pin,
                      (data & 0x04) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    HAL_GPIO_WritePin(LCD_D3_GPIO_Port, LCD_D3_Pin,
                      (data & 0x08) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    HAL_GPIO_WritePin(LCD_D4_GPIO_Port, LCD_D4_Pin,
                      (data & 0x10) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    HAL_GPIO_WritePin(LCD_D5_GPIO_Port, LCD_D5_Pin,
                      (data & 0x20) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    HAL_GPIO_WritePin(LCD_D6_GPIO_Port, LCD_D6_Pin,
                      (data & 0x40) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    HAL_GPIO_WritePin(LCD_D7_GPIO_Port, LCD_D7_Pin,
                      (data & 0x80) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    // Pulso de escritura
    HAL_GPIO_WritePin(LCD_WR_GPIO_Port, LCD_WR_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_WR_GPIO_Port, LCD_WR_Pin, GPIO_PIN_SET);
}

static void LCD_WriteCommand(uint8_t cmd)
{
    // RS = 0 -> comando
    HAL_GPIO_WritePin(LCD_RS_GPIO_Port, LCD_RS_Pin, GPIO_PIN_RESET);

    // CS = 0 -> seleccionar LCD
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);

    LCD_Write8(cmd);

    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
}

static void LCD_WriteData(uint8_t data)
{
    // RS = 1 -> dato
    HAL_GPIO_WritePin(LCD_RS_GPIO_Port, LCD_RS_Pin, GPIO_PIN_SET);

    // CS = 0 -> seleccionar LCD
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);

    LCD_Write8(data);

    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
}

static void LCD_Reset(void)
{
    // Dejamos las señales de control inicialmente inactivas
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LCD_WR_GPIO_Port, LCD_WR_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LCD_RD_GPIO_Port, LCD_RD_Pin, GPIO_PIN_SET);

    // Reset físico del ILI9341
    HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(5);

    HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(20);

    HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(150);
}

static void LCD_WriteCommandData(uint8_t cmd,
                                 const uint8_t *data,
                                 uint8_t length)
{
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);

    // RS = 0 -> comando
    HAL_GPIO_WritePin(LCD_RS_GPIO_Port, LCD_RS_Pin, GPIO_PIN_RESET);
    LCD_Write8(cmd);

    // RS = 1 -> datos
    HAL_GPIO_WritePin(LCD_RS_GPIO_Port, LCD_RS_Pin, GPIO_PIN_SET);

    for (uint8_t i = 0; i < length; i++)
    {
        LCD_Write8(data[i]);
    }

    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
}

void ILI9341_Init(void)
{
    LCD_Reset();

    uint8_t data[5];

    // Power Control B
    data[0] = 0x00;
    data[1] = 0x81;
    data[2] = 0x30;
    LCD_WriteCommandData(0xCF, data, 3);

    // Power On Sequence Control
    data[0] = 0x64;
    data[1] = 0x03;
    data[2] = 0x12;
    data[3] = 0x81;
    LCD_WriteCommandData(0xED, data, 4);

    // Driver Timing Control A
    data[0] = 0x85;
    data[1] = 0x10;
    data[2] = 0x78;
    LCD_WriteCommandData(0xE8, data, 3);

    // Power Control A
    data[0] = 0x39;
    data[1] = 0x2C;
    data[2] = 0x00;
    data[3] = 0x34;
    data[4] = 0x02;
    LCD_WriteCommandData(0xCB, data, 5);

    // Pump Ratio Control
    data[0] = 0x20;
    LCD_WriteCommandData(0xF7, data, 1);

    // Driver Timing Control B
    data[0] = 0x00;
    data[1] = 0x00;
    LCD_WriteCommandData(0xEA, data, 2);

    // Power Control 1
    data[0] = 0x21;
    LCD_WriteCommandData(0xC0, data, 1);

    // Power Control 2
    data[0] = 0x11;
    LCD_WriteCommandData(0xC1, data, 1);

    // VCOM Control 1
    data[0] = 0x3F;
    data[1] = 0x3C;
    LCD_WriteCommandData(0xC5, data, 2);

    // VCOM Control 2
    data[0] = 0xB5;
    LCD_WriteCommandData(0xC7, data, 1);

    // Memory Access Control
    data[0] = 0x48;
    LCD_WriteCommandData(0x36, data, 1);

    // Pixel Format: 16 bits/pixel RGB565
    data[0] = 0x55;
    LCD_WriteCommandData(0x3A, data, 1);

    // Frame Rate Control
    data[0] = 0x00;
    data[1] = 0x1B;
    LCD_WriteCommandData(0xB1, data, 2);

    // Display Function Control
    data[0] = 0x0A;
    data[1] = 0xA2;
    data[2] = 0x27;
    data[3] = 0x00;
    LCD_WriteCommandData(0xB6, data, 4);

    // Gamma Function Disable
    data[0] = 0x00;
    LCD_WriteCommandData(0xF2, data, 1);

    // Gamma Curve
    data[0] = 0x01;
    LCD_WriteCommandData(0x26, data, 1);

    // Sleep Out
    LCD_WriteCommand(0x11);
    HAL_Delay(120);

    // Display ON
    LCD_WriteCommand(0x29);
    HAL_Delay(20);
}

static void ILI9341_SetAddressWindow(uint16_t x0,
                                     uint16_t y0,
                                     uint16_t x1,
                                     uint16_t y1)
{
    uint8_t data[4];

    // Column Address Set
    data[0] = (x0 >> 8) & 0xFF;
    data[1] = x0 & 0xFF;
    data[2] = (x1 >> 8) & 0xFF;
    data[3] = x1 & 0xFF;

    LCD_WriteCommandData(0x2A, data, 4);

    // Page Address Set
    data[0] = (y0 >> 8) & 0xFF;
    data[1] = y0 & 0xFF;
    data[2] = (y1 >> 8) & 0xFF;
    data[3] = y1 & 0xFF;

    LCD_WriteCommandData(0x2B, data, 4);
}

void ILI9341_FillScreen(uint16_t color)
{
    ILI9341_SetAddressWindow(0,
                             0,
                             ILI9341_WIDTH - 1,
                             ILI9341_HEIGHT - 1);

    // Seleccionamos el LCD
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port,
                      LCD_CS_Pin,
                      GPIO_PIN_RESET);

    // Memory Write
    HAL_GPIO_WritePin(LCD_RS_GPIO_Port,
                      LCD_RS_Pin,
                      GPIO_PIN_RESET);

    LCD_Write8(0x2C);

    // A partir de ahora enviamos datos de píxeles
    HAL_GPIO_WritePin(LCD_RS_GPIO_Port,
                      LCD_RS_Pin,
                      GPIO_PIN_SET);

    uint8_t highByte = (color >> 8) & 0xFF;
    uint8_t lowByte  = color & 0xFF;

    for (uint32_t i = 0;
         i < (ILI9341_WIDTH * ILI9341_HEIGHT);
         i++)
    {
        LCD_Write8(highByte);
        LCD_Write8(lowByte);
    }

    // Deseleccionamos LCD
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port,
                      LCD_CS_Pin,
                      GPIO_PIN_SET);
}

void ILI9341_DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (x >= ILI9341_WIDTH || y >= ILI9341_HEIGHT)
        return;

    ILI9341_SetAddressWindow(x, y, x, y);

    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);

    // Memory Write
    HAL_GPIO_WritePin(LCD_RS_GPIO_Port, LCD_RS_Pin, GPIO_PIN_RESET);
    LCD_Write8(0x2C);

    // Datos
    HAL_GPIO_WritePin(LCD_RS_GPIO_Port, LCD_RS_Pin, GPIO_PIN_SET);

    LCD_Write8((color >> 8) & 0xFF);
    LCD_Write8(color & 0xFF);

    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
}

void ILI9341_DrawLine(int16_t x0, int16_t y0,
                      int16_t x1, int16_t y1,
                      uint16_t color)
{
    int16_t dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int16_t sx = (x0 < x1) ? 1 : -1;

    int16_t dy = (y1 > y0) ? -(y1 - y0) : -(y0 - y1);
    int16_t sy = (y0 < y1) ? 1 : -1;

    int16_t err = dx + dy;

    while (1)
    {
        ILI9341_DrawPixel(x0, y0, color);

        if (x0 == x1 && y0 == y1)
            break;

        int16_t e2 = 2 * err;

        if (e2 >= dy)
        {
            err += dy;
            x0 += sx;
        }

        if (e2 <= dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}

void ILI9341_DrawRectangle(uint16_t x,
                           uint16_t y,
                           uint16_t width,
                           uint16_t height,
                           uint16_t color)
{
    if (width == 0 || height == 0)
        return;

    ILI9341_DrawHLine(
        x,
        y,
        width,
        color
    );

    ILI9341_DrawHLine(
        x,
        y + height - 1,
        width,
        color
    );

    ILI9341_DrawVLine(
        x,
        y,
        height,
        color
    );

    ILI9341_DrawVLine(
        x + width - 1,
        y,
        height,
        color
    );
}

void ILI9341_FillRectangle(uint16_t x, uint16_t y,
                           uint16_t width, uint16_t height,
                           uint16_t color)
{
    if (x >= ILI9341_WIDTH || y >= ILI9341_HEIGHT)
        return;

    if (width == 0 || height == 0)
        return;

    if ((x + width) > ILI9341_WIDTH)
        width = ILI9341_WIDTH - x;

    if ((y + height) > ILI9341_HEIGHT)
        height = ILI9341_HEIGHT - y;

    ILI9341_SetAddressWindow(
        x,
        y,
        x + width - 1,
        y + height - 1
    );

    HAL_GPIO_WritePin(LCD_CS_GPIO_Port,
                      LCD_CS_Pin,
                      GPIO_PIN_RESET);

    // Memory Write
    HAL_GPIO_WritePin(LCD_RS_GPIO_Port,
                      LCD_RS_Pin,
                      GPIO_PIN_RESET);

    LCD_Write8(0x2C);

    // Datos de píxeles
    HAL_GPIO_WritePin(LCD_RS_GPIO_Port,
                      LCD_RS_Pin,
                      GPIO_PIN_SET);

    uint8_t high = color >> 8;
    uint8_t low  = color & 0xFF;

    uint32_t totalPixels =
        (uint32_t)width * height;

    for (uint32_t i = 0; i < totalPixels; i++)
    {
        LCD_Write8(high);
        LCD_Write8(low);
    }

    HAL_GPIO_WritePin(LCD_CS_GPIO_Port,
                      LCD_CS_Pin,
                      GPIO_PIN_SET);
}

void ILI9341_DrawChar(uint16_t x, uint16_t y,
                      char c,
                      uint16_t color,
                      uint16_t bg,
                      uint8_t size)
{
    if (c < 32 || c > 90)
        c = '?';

    const uint8_t *bitmap = Font5x7[c - 32];

    for (uint8_t col = 0; col < 5; col++)
    {
        uint8_t line = bitmap[col];

        for (uint8_t row = 0; row < 7; row++)
        {
        	uint16_t pixelColor =
        	    (line & (1 << (6 - row))) ? color : bg;

            if (size == 1)
            {
                ILI9341_DrawPixel(x + col,
                                  y + row,
                                  pixelColor);
            }
            else
            {
                ILI9341_FillRectangle(
                    x + col * size,
                    y + row * size,
                    size,
                    size,
                    pixelColor
                );
            }
        }
    }

    // Columna de separación entre caracteres
    for (uint8_t row = 0; row < 7; row++)
    {
        if (size == 1)
        {
            ILI9341_DrawPixel(x + 5,
                              y + row,
                              bg);
        }
        else
        {
            ILI9341_FillRectangle(
                x + 5 * size,
                y + row * size,
                size,
                size,
                bg
            );
        }
    }
}

void ILI9341_WriteString(uint16_t x, uint16_t y,
                         const char *str,
                         uint16_t color,
                         uint16_t bg,
                         uint8_t size)
{
    while (*str)
    {
        ILI9341_DrawChar(x, y,
                         *str,
                         color,
                         bg,
                         size);

        x += 6 * size;

        str++;
    }
}

void ILI9341_SetRotation(uint8_t rotation)
{
    uint8_t madctl;

    switch (rotation % 4)
    {
        case 0:
            madctl = 0x48;
            ILI9341_WIDTH  = 240;
            ILI9341_HEIGHT = 320;
            break;

        case 1:
            madctl = 0x28;
            ILI9341_WIDTH  = 320;
            ILI9341_HEIGHT = 240;
            break;

        case 2:
            madctl = 0x88;
            ILI9341_WIDTH  = 240;
            ILI9341_HEIGHT = 320;
            break;

        case 3:
            madctl = 0xE8;
            ILI9341_WIDTH  = 320;
            ILI9341_HEIGHT = 240;
            break;

        default:
            madctl = 0x48;
            ILI9341_WIDTH  = 240;
            ILI9341_HEIGHT = 320;
            break;
    }

    LCD_WriteCommandData(0x36, &madctl, 1);
}

void ILI9341_DrawCircle(int16_t x0, int16_t y0,
                        int16_t r,
                        uint16_t color)
{
    int16_t x = r;
    int16_t y = 0;
    int16_t err = 0;

    while (x >= y)
    {
        ILI9341_DrawPixel(x0 + x, y0 + y, color);
        ILI9341_DrawPixel(x0 + y, y0 + x, color);
        ILI9341_DrawPixel(x0 - y, y0 + x, color);
        ILI9341_DrawPixel(x0 - x, y0 + y, color);
        ILI9341_DrawPixel(x0 - x, y0 - y, color);
        ILI9341_DrawPixel(x0 - y, y0 - x, color);
        ILI9341_DrawPixel(x0 + y, y0 - x, color);
        ILI9341_DrawPixel(x0 + x, y0 - y, color);

        y++;

        if (err <= 0)
        {
            err += 2 * y + 1;
        }

        if (err > 0)
        {
            x--;
            err -= 2 * x + 1;
        }
    }
}

void ILI9341_FillCircle(int16_t x0, int16_t y0,
                        int16_t r,
                        uint16_t color)
{
    for (int16_t y = -r; y <= r; y++)
    {
        for (int16_t x = -r; x <= r; x++)
        {
            if ((x * x + y * y) <= (r * r))
            {
                ILI9341_DrawPixel(x0 + x,
                                  y0 + y,
                                  color);
            }
        }
    }
}

void ILI9341_DrawSignalBuffer(const int16_t *buffer,
                              uint16_t bufferSize,
                              uint16_t startIndex,
                              uint16_t samplesStored,
                              uint16_t graphX,
                              uint16_t graphY,
                              uint16_t graphW,
                              uint16_t graphH,
                              uint16_t signalColor,
                              uint16_t gridColor)
{
    // Limpiamos interior
    ILI9341_FillRectangle(
        graphX + 1,
        graphY + 1,
        graphW - 2,
        graphH - 2,
        ILI9341_BLACK
    );

    // Grilla vertical
    for (uint16_t xg = graphX + 40;
         xg < graphX + graphW;
         xg += 40)
    {
        ILI9341_DrawLine(
            xg,
            graphY + 1,
            xg,
            graphY + graphH - 2,
            gridColor
        );
    }

    // Grilla horizontal
    for (uint16_t yg = graphY + 30;
         yg < graphY + graphH;
         yg += 30)
    {
        ILI9341_DrawLine(
            graphX + 1,
            yg,
            graphX + graphW - 2,
            yg,
            gridColor
        );
    }

    if (samplesStored < 2)
        return;

    uint16_t samplesToDraw = samplesStored;

    if (samplesToDraw > bufferSize)
        samplesToDraw = bufferSize;

    // Si el buffer todavía no está lleno, arrancamos desde 0.
    // Si está lleno, startIndex apunta a la muestra más antigua.
    uint16_t index;

    if (samplesStored < bufferSize)
        index = 0;
    else
        index = startIndex;

    int16_t previousY = buffer[index];

    for (uint16_t i = 1; i < samplesToDraw; i++)
    {
        index++;

        if (index >= bufferSize)
            index = 0;

        int16_t currentY = buffer[index];

        ILI9341_DrawLine(
            graphX + i,
            previousY,
            graphX + i + 1,
            currentY,
            signalColor
        );

        previousY = currentY;
    }
}

void ILI9341_SetScrollArea(uint16_t top,
                           uint16_t scroll,
                           uint16_t bottom)
{
    uint8_t data[6];

    data[0] = (top >> 8) & 0xFF;
    data[1] = top & 0xFF;

    data[2] = (scroll >> 8) & 0xFF;
    data[3] = scroll & 0xFF;

    data[4] = (bottom >> 8) & 0xFF;
    data[5] = bottom & 0xFF;

    LCD_WriteCommandData(0x33, data, 6);
}

void ILI9341_SetScroll(uint16_t offset)
{
    uint8_t data[2];

    data[0] = (offset >> 8) & 0xFF;
    data[1] = offset & 0xFF;

    LCD_WriteCommandData(0x37, data, 2);
}

void ILI9341_DrawGraphColumn(uint16_t x,
                             int16_t yPrevious,
                             int16_t yNew,
                             uint8_t verticalGrid,
                             uint16_t signalColor,
                             uint16_t gridColor,
                             uint16_t backgroundColor)
{
    if (x >= ILI9341_WIDTH)
        return;

    if (yPrevious < 0)
        yPrevious = 0;

    if (yPrevious >= ILI9341_HEIGHT)
        yPrevious = ILI9341_HEIGHT - 1;

    if (yNew < 0)
        yNew = 0;

    if (yNew >= ILI9341_HEIGHT)
        yNew = ILI9341_HEIGHT - 1;


    int16_t yMin = (yPrevious < yNew) ? yPrevious : yNew;
    int16_t yMax = (yPrevious > yNew) ? yPrevious : yNew;


    // Seleccionamos una sola columna completa
    ILI9341_SetAddressWindow(
        x,
        0,
        x,
        ILI9341_HEIGHT - 1
    );

    HAL_GPIO_WritePin(
        LCD_CS_GPIO_Port,
        LCD_CS_Pin,
        GPIO_PIN_RESET
    );

    // Memory Write
    HAL_GPIO_WritePin(
        LCD_RS_GPIO_Port,
        LCD_RS_Pin,
        GPIO_PIN_RESET
    );

    LCD_Write8(0x2C);

    // Datos
    HAL_GPIO_WritePin(
        LCD_RS_GPIO_Port,
        LCD_RS_Pin,
        GPIO_PIN_SET
    );


    for (uint16_t y = 0; y < ILI9341_HEIGHT; y++)
    {
        uint16_t color = backgroundColor;

        // Si corresponde una línea vertical de grilla,
        // toda la columna empieza siendo gris
        if (verticalGrid)
        {
            color = gridColor;
        }

        // Líneas horizontales
        if ((y == 60) ||
            (y == 120) ||
            (y == 180))
        {
            color = gridColor;
        }

        // La señal siempre queda por encima de la grilla
        if ((y >= yMin) && (y <= yMax))
        {
            color = signalColor;
        }

        LCD_Write8((color >> 8) & 0xFF);
        LCD_Write8(color & 0xFF);
    }


    HAL_GPIO_WritePin(
        LCD_CS_GPIO_Port,
        LCD_CS_Pin,
        GPIO_PIN_SET
    );
}

void ILI9341_DrawHLine(uint16_t x,
                       uint16_t y,
                       uint16_t width,
                       uint16_t color)
{
    if (x >= ILI9341_WIDTH || y >= ILI9341_HEIGHT)
        return;

    if ((x + width) > ILI9341_WIDTH)
    {
        width = ILI9341_WIDTH - x;
    }

    ILI9341_SetAddressWindow(
        x,
        y,
        x + width - 1,
        y
    );

    HAL_GPIO_WritePin(
        LCD_CS_GPIO_Port,
        LCD_CS_Pin,
        GPIO_PIN_RESET
    );

    // Comando Memory Write
    HAL_GPIO_WritePin(
        LCD_RS_GPIO_Port,
        LCD_RS_Pin,
        GPIO_PIN_RESET
    );

    LCD_Write8(0x2C);

    // A partir de ahora enviamos datos
    HAL_GPIO_WritePin(
        LCD_RS_GPIO_Port,
        LCD_RS_Pin,
        GPIO_PIN_SET
    );

    uint8_t high = (color >> 8) & 0xFF;
    uint8_t low  = color & 0xFF;

    for (uint16_t i = 0; i < width; i++)
    {
        LCD_Write8(high);
        LCD_Write8(low);
    }

    HAL_GPIO_WritePin(
        LCD_CS_GPIO_Port,
        LCD_CS_Pin,
        GPIO_PIN_SET
    );
}

void ILI9341_DrawVLine(uint16_t x,
                       uint16_t y,
                       uint16_t height,
                       uint16_t color)
{
    // Verificar límites
    if (x >= ILI9341_WIDTH || y >= ILI9341_HEIGHT)
        return;

    // Recortar si la línea supera la pantalla
    if ((y + height) > ILI9341_HEIGHT)
    {
        height = ILI9341_HEIGHT - y;
    }

    // Configurar una ventana de 1 píxel de ancho
    ILI9341_SetAddressWindow(
        x,
        y,
        x,
        y + height - 1
    );

    HAL_GPIO_WritePin(
        LCD_CS_GPIO_Port,
        LCD_CS_Pin,
        GPIO_PIN_RESET
    );

    // Comando Memory Write
    HAL_GPIO_WritePin(
        LCD_RS_GPIO_Port,
        LCD_RS_Pin,
        GPIO_PIN_RESET
    );

    LCD_Write8(0x2C);

    // Modo datos
    HAL_GPIO_WritePin(
        LCD_RS_GPIO_Port,
        LCD_RS_Pin,
        GPIO_PIN_SET
    );

    uint8_t high = (color >> 8) & 0xFF;
    uint8_t low  = color & 0xFF;

    // Enviar todos los píxeles consecutivamente
    for (uint16_t i = 0; i < height; i++)
    {
        LCD_Write8(high);
        LCD_Write8(low);
    }

    HAL_GPIO_WritePin(
        LCD_CS_GPIO_Port,
        LCD_CS_Pin,
        GPIO_PIN_SET
    );
}

void ILI9341_DrawTriangle(int16_t x0, int16_t y0,
                          int16_t x1, int16_t y1,
                          int16_t x2, int16_t y2,
                          uint16_t color)
{
    // Lado 1
    ILI9341_DrawLine(x0, y0, x1, y1, color);

    // Lado 2
    ILI9341_DrawLine(x1, y1, x2, y2, color);

    // Lado 3
    ILI9341_DrawLine(x2, y2, x0, y0, color);
}

void ILI9341_FillTriangle(int16_t x0, int16_t y0,
                          int16_t x1, int16_t y1,
                          int16_t x2, int16_t y2,
                          uint16_t color)
{
    // Ordenar los vértices de arriba hacia abajo según Y
    if (y0 > y1)
    {
        int16_t temp;

        temp = y0; y0 = y1; y1 = temp;
        temp = x0; x0 = x1; x1 = temp;
    }

    if (y1 > y2)
    {
        int16_t temp;

        temp = y1; y1 = y2; y2 = temp;
        temp = x1; x1 = x2; x2 = temp;
    }

    if (y0 > y1)
    {
        int16_t temp;

        temp = y0; y0 = y1; y1 = temp;
        temp = x0; x0 = x1; x1 = temp;
    }


    // Caso especial: los tres puntos están en la misma línea horizontal
    if (y0 == y2)
    {
        int16_t xmin = x0;
        int16_t xmax = x0;

        if (x1 < xmin) xmin = x1;
        if (x2 < xmin) xmin = x2;

        if (x1 > xmax) xmax = x1;
        if (x2 > xmax) xmax = x2;

        ILI9341_DrawHLine(
            xmin,
            y0,
            xmax - xmin + 1,
            color
        );

        return;
    }


    int32_t dx01 = x1 - x0;
    int32_t dy01 = y1 - y0;

    int32_t dx02 = x2 - x0;
    int32_t dy02 = y2 - y0;

    int32_t dx12 = x2 - x1;
    int32_t dy12 = y2 - y1;

    int32_t sa = 0;
    int32_t sb = 0;

    int16_t y;
    int16_t last;


    // Si y1 == y2, la parte inferior no existe
    if (y1 == y2)
        last = y1;
    else
        last = y1 - 1;


    // Parte superior del triángulo
    for (y = y0; y <= last; y++)
    {
        int16_t a = x0 + sa / dy01;
        int16_t b = x0 + sb / dy02;

        sa += dx01;
        sb += dx02;

        if (a > b)
        {
            int16_t temp = a;
            a = b;
            b = temp;
        }

        ILI9341_DrawHLine(
            a,
            y,
            b - a + 1,
            color
        );
    }


    // Parte inferior del triángulo
    sa = dx12 * (y - y1);
    sb = dx02 * (y - y0);

    for (; y <= y2; y++)
    {
        int16_t a = x1 + sa / dy12;
        int16_t b = x0 + sb / dy02;

        sa += dx12;
        sb += dx02;

        if (a > b)
        {
            int16_t temp = a;
            a = b;
            b = temp;
        }

        ILI9341_DrawHLine(
            a,
            y,
            b - a + 1,
            color
        );
    }
}

void ILI9341_DrawRoundRect(uint16_t x,
                           uint16_t y,
                           uint16_t width,
                           uint16_t height,
                           uint16_t radius,
                           uint16_t color)
{
    if (width == 0 || height == 0)
        return;

    if (radius * 2 > width)
        radius = width / 2;

    if (radius * 2 > height)
        radius = height / 2;

    // Líneas horizontales
    ILI9341_DrawHLine(
        x + radius,
        y,
        width - 2 * radius,
        color
    );

    ILI9341_DrawHLine(
        x + radius,
        y + height - 1,
        width - 2 * radius,
        color
    );

    // Líneas verticales
    ILI9341_DrawVLine(
        x,
        y + radius,
        height - 2 * radius,
        color
    );

    ILI9341_DrawVLine(
        x + width - 1,
        y + radius,
        height - 2 * radius,
        color
    );

    // Centros de las cuatro esquinas
    int16_t cx1 = x + radius;
    int16_t cx2 = x + width - radius - 1;

    int16_t cy1 = y + radius;
    int16_t cy2 = y + height - radius - 1;

    // Dibujamos solo los cuartos de círculo necesarios
    int16_t px = radius;
    int16_t py = 0;
    int16_t err = 0;

    while (px >= py)
    {
        // Superior izquierda
        ILI9341_DrawPixel(cx1 - px, cy1 - py, color);
        ILI9341_DrawPixel(cx1 - py, cy1 - px, color);

        // Superior derecha
        ILI9341_DrawPixel(cx2 + px, cy1 - py, color);
        ILI9341_DrawPixel(cx2 + py, cy1 - px, color);

        // Inferior izquierda
        ILI9341_DrawPixel(cx1 - px, cy2 + py, color);
        ILI9341_DrawPixel(cx1 - py, cy2 + px, color);

        // Inferior derecha
        ILI9341_DrawPixel(cx2 + px, cy2 + py, color);
        ILI9341_DrawPixel(cx2 + py, cy2 + px, color);

        py++;

        if (err <= 0)
        {
            err += 2 * py + 1;
        }

        if (err > 0)
        {
            px--;
            err -= 2 * px + 1;
        }
    }
}

void ILI9341_FillRoundRect(uint16_t x,
                           uint16_t y,
                           uint16_t width,
                           uint16_t height,
                           uint16_t radius,
                           uint16_t color)
{
    if (width == 0 || height == 0)
        return;

    if (radius * 2 > width)
        radius = width / 2;

    if (radius * 2 > height)
        radius = height / 2;

    // Rectángulo central
    ILI9341_FillRectangle(
        x + radius,
        y,
        width - 2 * radius,
        height,
        color
    );

    // Rectángulo lateral izquierdo
    ILI9341_FillRectangle(
        x,
        y + radius,
        radius,
        height - 2 * radius,
        color
    );

    // Rectángulo lateral derecho
    ILI9341_FillRectangle(
        x + width - radius,
        y + radius,
        radius,
        height - 2 * radius,
        color
    );

    // Relleno de las cuatro esquinas redondeadas
    for (int16_t yy = 0; yy < radius; yy++)
    {
        for (int16_t xx = 0; xx < radius; xx++)
        {
            int16_t dx = radius - xx;
            int16_t dy = radius - yy;

            if ((dx * dx + dy * dy) <= (radius * radius))
            {
                // Superior izquierda
                ILI9341_DrawPixel(
                    x + xx,
                    y + yy,
                    color
                );

                // Superior derecha
                ILI9341_DrawPixel(
                    x + width - 1 - xx,
                    y + yy,
                    color
                );

                // Inferior izquierda
                ILI9341_DrawPixel(
                    x + xx,
                    y + height - 1 - yy,
                    color
                );

                // Inferior derecha
                ILI9341_DrawPixel(
                    x + width - 1 - xx,
                    y + height - 1 - yy,
                    color
                );
            }
        }
    }
}

void ILI9341_DrawProgressBar(uint16_t x,
                             uint16_t y,
                             uint16_t width,
                             uint16_t height,
                             uint16_t value,
                             uint16_t maxValue,
                             uint16_t fgColor,
                             uint16_t bgColor,
                             uint16_t borderColor)
{
    if (width < 3 || height < 3)
        return;

    if (maxValue == 0)
        return;

    if (value > maxValue)
        value = maxValue;

    // Dibujar borde
    ILI9341_DrawRectangle(
        x,
        y,
        width,
        height,
        borderColor
    );

    // Área interior
    uint16_t innerWidth  = width - 2;
    uint16_t innerHeight = height - 2;

    // Calcular ancho correspondiente al progreso
    uint32_t filledWidth =
        ((uint32_t)value * innerWidth) / maxValue;

    // Fondo
    ILI9341_FillRectangle(
        x + 1,
        y + 1,
        innerWidth,
        innerHeight,
        bgColor
    );

    // Parte completada
    if (filledWidth > 0)
    {
        ILI9341_FillRectangle(
            x + 1,
            y + 1,
            (uint16_t)filledWidth,
            innerHeight,
            fgColor
        );
    }
}

void ILI9341_DrawRoundProgressBar(uint16_t x,
                                  uint16_t y,
                                  uint16_t width,
                                  uint16_t height,
                                  uint16_t value,
                                  uint16_t maxValue,
                                  uint16_t fgColor,
                                  uint16_t bgColor,
                                  uint16_t borderColor)
{
    if (width < 5 || height < 5)
        return;

    if (maxValue == 0)
        return;

    if (value > maxValue)
        value = maxValue;

    uint16_t radius = height / 2;

    // Fondo completo
    ILI9341_FillRoundRect(
        x,
        y,
        width,
        height,
        radius,
        bgColor
    );

    // Borde
    ILI9341_DrawRoundRect(
        x,
        y,
        width,
        height,
        radius,
        borderColor
    );

    uint16_t innerWidth = width - 4;

    uint32_t filledWidth =
        ((uint32_t)value * innerWidth) / maxValue;

    if (filledWidth > 0)
    {
        uint16_t innerHeight = height - 4;
        uint16_t innerRadius = innerHeight / 2;

        ILI9341_FillRoundRect(
            x + 2,
            y + 2,
            (uint16_t)filledWidth,
            innerHeight,
            innerRadius,
            fgColor
        );
    }
}

void ILI9341_DrawButton(uint16_t x,
                        uint16_t y,
                        uint16_t width,
                        uint16_t height,
                        const char *text,
                        uint16_t textColor,
                        uint16_t fillColor,
                        uint16_t borderColor,
                        uint8_t textSize)
{
    if (width == 0 || height == 0 || text == NULL)
        return;

    // Radio de las esquinas
    uint16_t radius = height / 5;

    // Fondo del botón
    ILI9341_FillRoundRect(
        x,
        y,
        width,
        height,
        radius,
        fillColor
    );

    // Borde
    ILI9341_DrawRoundRect(
        x,
        y,
        width,
        height,
        radius,
        borderColor
    );


    // Calcular longitud del texto
    uint16_t textLength = 0;

    const char *ptr = text;

    while (*ptr != '\0')
    {
        textLength++;
        ptr++;
    }


    /*
     * Nuestra fuente ocupa:
     *
     * 5 píxeles de carácter
     * + 1 píxel de separación
     *
     * por lo tanto:
     *
     * ancho = 6 * textSize
     *
     * alto = 7 * textSize
     */

    uint16_t textWidth =
        textLength * 6 * textSize;

    uint16_t textHeight =
        7 * textSize;


    // Centrado horizontal
    int16_t textX =
        x + ((int32_t)width - textWidth) / 2;

    // Centrado vertical
    int16_t textY =
        y + ((int32_t)height - textHeight) / 2;


    // Evitar coordenadas negativas
    if (textX < x)
        textX = x;

    if (textY < y)
        textY = y;


    // Dibujar texto
    ILI9341_WriteString(
        textX,
        textY,
        text,
        textColor,
        fillColor,
        textSize
    );
}

void ILI9341_WriteInt(uint16_t x,
                      uint16_t y,
                      int32_t value,
                      uint16_t color,
                      uint16_t bg,
                      uint8_t size)
{
    char buffer[12];

    snprintf(
        buffer,
        sizeof(buffer),
        "%ld",
        (long)value
    );

    ILI9341_WriteString(
        x,
        y,
        buffer,
        color,
        bg,
        size
    );
}

void ILI9341_WriteFloat(uint16_t x,
                        uint16_t y,
                        float value,
                        uint8_t decimals,
                        uint16_t color,
                        uint16_t bg,
                        uint8_t size)
{
    char buffer[24];

    if (decimals > 6)
    {
        decimals = 6;
    }

    snprintf(
        buffer,
        sizeof(buffer),
        "%.*f",
        decimals,
        (double)value
    );

    ILI9341_WriteString(
        x,
        y,
        buffer,
        color,
        bg,
        size
    );
}

void ILI9341_WriteValueUnit(uint16_t x,
                            uint16_t y,
                            float value,
                            uint8_t decimals,
                            const char *unit,
                            uint16_t color,
                            uint16_t bg,
                            uint8_t size)
{
    // Primero escribimos el valor
    ILI9341_WriteFloat(
        x,
        y,
        value,
        decimals,
        color,
        bg,
        size
    );

    // Calculamos aproximadamente cuánto ocupa el número
    uint16_t chars = 1;   // al menos un dígito

    float temp = value;

    if (temp < 0.0f)
    {
        chars++;      // signo '-'
        temp = -temp;
    }

    uint32_t integerPart = (uint32_t)temp;

    while (integerPart >= 10)
    {
        integerPart /= 10;
        chars++;
    }

    // Punto decimal + decimales
    if (decimals > 0)
    {
        chars += 1 + decimals;
    }

    // Cada carácter ocupa aproximadamente 6 * size píxeles
    uint16_t unitX = x + chars * 6 * size;

    // Dejamos un pequeño espacio
    unitX += 3 * size;

    ILI9341_WriteString(
        unitX,
        y,
        unit,
        color,
        bg,
        size
    );
}

void ILI9341_UpdateValueUnit(uint16_t x,
                             uint16_t y,
                             uint16_t width,
                             uint16_t height,
                             float value,
                             uint8_t decimals,
                             const char *unit,
                             uint16_t color,
                             uint16_t bg,
                             uint8_t size)
{
    // width y height quedan disponibles para futuras mejoras.
    (void)width;
    (void)height;

    // Sobrescribimos directamente el valor anterior.
    ILI9341_WriteValueUnit(
        x,
        y,
        value,
        decimals,
        unit,
        color,
        bg,
        size
    );
}

void ILI9341_UpdateValueFixed(uint16_t x,
                              uint16_t y,
                              float value,
                              uint8_t decimals,
                              const char *unit,
                              uint8_t fieldWidth,
                              uint16_t color,
                              uint16_t bg,
                              uint8_t size)
{
    char numberBuffer[24];
    char fieldBuffer[40];

    uint8_t index = 0;

    if (decimals > 6)
        decimals = 6;

    /*
     * Convertimos el float manualmente,
     * igual que en ILI9341_WriteFloat().
     */

    if (value < 0.0f)
    {
        numberBuffer[index++] = '-';
        value = -value;
    }

    uint32_t integerPart = (uint32_t)value;

    char intBuffer[12];
    uint8_t intIndex = 0;

    if (integerPart == 0)
    {
        intBuffer[intIndex++] = '0';
    }
    else
    {
        while (integerPart > 0)
        {
            intBuffer[intIndex++] =
                '0' + (integerPart % 10);

            integerPart /= 10;
        }
    }

    while (intIndex > 0)
    {
        numberBuffer[index++] =
            intBuffer[--intIndex];
    }

    if (decimals > 0)
    {
        numberBuffer[index++] = '.';

        float fractionalPart =
            value - (uint32_t)value;

        for (uint8_t i = 0; i < decimals; i++)
        {
            fractionalPart *= 10.0f;

            uint8_t digit =
                (uint8_t)fractionalPart;

            numberBuffer[index++] =
                '0' + digit;

            fractionalPart -= digit;
        }
    }

    // Espacio entre valor y unidad
    numberBuffer[index++] = ' ';

    // Agregar unidad
    if (unit != NULL)
    {
        while (*unit != '\0' &&
               index < sizeof(numberBuffer) - 1)
        {
            numberBuffer[index++] = *unit++;
        }
    }

    numberBuffer[index] = '\0';


    /*
     * Construir campo fijo.
     */

    uint8_t textLength = index;

    if (fieldWidth >= sizeof(fieldBuffer))
        fieldWidth = sizeof(fieldBuffer) - 1;

    uint8_t padding = 0;

    if (fieldWidth > textLength)
        padding = fieldWidth - textLength;

    uint8_t fieldIndex = 0;

    // Espacios iniciales
    for (uint8_t i = 0; i < padding; i++)
    {
        fieldBuffer[fieldIndex++] = ' ';
    }

    // Copiar valor + unidad
    for (uint8_t i = 0;
         i < textLength &&
         fieldIndex < fieldWidth;
         i++)
    {
        fieldBuffer[fieldIndex++] =
            numberBuffer[i];
    }

    // Completar espacios sobrantes
    while (fieldIndex < fieldWidth)
    {
        fieldBuffer[fieldIndex++] = ' ';
    }

    fieldBuffer[fieldIndex] = '\0';


    // Sobrescribir directamente el campo anterior
    ILI9341_WriteString(
        x,
        y,
        fieldBuffer,
        color,
        bg,
        size
    );
}

void ILI9341_DrawDashedLine(int16_t x0,
                            int16_t y0,
                            int16_t x1,
                            int16_t y1,
                            uint16_t color,
                            uint8_t dashLength,
                            uint8_t gapLength)
{
    int16_t dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int16_t sx = (x0 < x1) ? 1 : -1;

    int16_t dy = (y1 > y0) ? -(y1 - y0) : -(y0 - y1);
    int16_t sy = (y0 < y1) ? 1 : -1;

    int16_t err = dx + dy;

    uint16_t counter = 0;
    uint16_t period = dashLength + gapLength;

    while (1)
    {
        if (period == 0 || (counter % period) < dashLength)
        {
            ILI9341_DrawPixel(x0, y0, color);
        }

        if (x0 == x1 && y0 == y1)
            break;

        int16_t e2 = 2 * err;

        if (e2 >= dy)
        {
            err += dy;
            x0 += sx;
        }

        if (e2 <= dx)
        {
            err += dx;
            y0 += sy;
        }

        counter++;
    }
}

void ILI9341_DrawDashedHLine(uint16_t x,
                             uint16_t y,
                             uint16_t width,
                             uint16_t color,
                             uint16_t dashLength,
                             uint16_t gapLength)
{
    if (width == 0 || dashLength == 0)
        return;

    if (x >= ILI9341_WIDTH || y >= ILI9341_HEIGHT)
        return;

    if ((x + width) > ILI9341_WIDTH)
    {
        width = ILI9341_WIDTH - x;
    }

    uint16_t currentX = x;
    uint16_t remaining = width;

    while (remaining > 0)
    {
        uint16_t currentDash = dashLength;

        if (currentDash > remaining)
        {
            currentDash = remaining;
        }

        ILI9341_DrawHLine(
            currentX,
            y,
            currentDash,
            color
        );

        currentX += currentDash;
        remaining -= currentDash;

        if (remaining == 0)
            break;

        uint16_t currentGap = gapLength;

        if (currentGap > remaining)
        {
            currentGap = remaining;
        }

        currentX += currentGap;
        remaining -= currentGap;
    }
}

void ILI9341_DrawDashedVLine(uint16_t x,
                             uint16_t y,
                             uint16_t height,
                             uint16_t color,
                             uint16_t dashLength,
                             uint16_t gapLength)
{
    if (height == 0 || dashLength == 0)
        return;

    if (x >= ILI9341_WIDTH || y >= ILI9341_HEIGHT)
        return;

    if ((y + height) > ILI9341_HEIGHT)
    {
        height = ILI9341_HEIGHT - y;
    }

    uint16_t currentY = y;
    uint16_t remaining = height;

    while (remaining > 0)
    {
        uint16_t currentDash = dashLength;

        if (currentDash > remaining)
        {
            currentDash = remaining;
        }

        ILI9341_DrawVLine(
            x,
            currentY,
            currentDash,
            color
        );

        currentY += currentDash;
        remaining -= currentDash;

        if (remaining == 0)
            break;

        uint16_t currentGap = gapLength;

        if (currentGap > remaining)
        {
            currentGap = remaining;
        }

        currentY += currentGap;
        remaining -= currentGap;
    }
}

void ILI9341_DrawReferenceLine(uint16_t x,
                               uint16_t y,
                               uint16_t width,
                               const char *label,
                               uint16_t lineColor,
                               uint16_t textColor,
                               uint16_t bgColor,
                               uint8_t textSize)
{
    if (width == 0 || label == NULL)
        return;

    if (x >= ILI9341_WIDTH || y >= ILI9341_HEIGHT)
        return;

    // Línea horizontal discontinua
    ILI9341_DrawDashedHLine(
        x,
        y,
        width,
        lineColor,
        6,
        4
    );

    // Etiqueta unos píxeles por encima de la línea
    int16_t labelY = y - (8 * textSize);

    if (labelY < 0)
    {
        labelY = y + 3;
    }

    ILI9341_WriteString(
        x + 4,
        (uint16_t)labelY,
        label,
        textColor,
        bgColor,
        textSize
    );
}

void ILI9341_DrawVerticalScale(uint16_t x,
                               uint16_t y,
                               uint16_t width,
                               uint16_t height,
                               float minValue,
                               float maxValue,
                               uint8_t divisions,
                               const char *unit,
                               uint16_t lineColor,
                               uint16_t textColor,
                               uint16_t bgColor,
                               uint8_t textSize)
{
    if (divisions == 0)
        return;

    float valueStep =
        (maxValue - minValue) / divisions;

    uint16_t pixelStep =
        height / divisions;

    // Espacio reservado para los valores de la escala
    uint16_t labelWidth = 45 * textSize;

    // Inicio real de las líneas de grilla
    uint16_t gridX = x + labelWidth;

    // Ancho disponible para las líneas
    uint16_t gridWidth;

    if (width > labelWidth)
        gridWidth = width - labelWidth;
    else
        return;

    for (uint8_t i = 0; i <= divisions; i++)
    {
        uint16_t lineY =
            y + height - (i * pixelStep);

        float value =
            minValue + (i * valueStep);

        // Dibujar línea de referencia
        ILI9341_DrawDashedHLine(
            gridX,
            lineY,
            gridWidth,
            lineColor,
            6,
            4
        );

        // Posición vertical del texto
        int16_t textY =
            lineY - (3 * textSize);

        if (textY < 0)
            textY = 0;

        // Escribir valor + unidad a la izquierda
        ILI9341_WriteValueUnit(
            x,
            textY,
            value,
            1,
            unit,
            textColor,
            bgColor,
            textSize
        );
    }
}

void ILI9341_DrawTimeScale(uint16_t x,
                           uint16_t y,
                           uint16_t width,
                           float totalTime,
                           uint8_t divisions,
                           uint16_t lineColor,
                           uint16_t textColor,
                           uint16_t bgColor,
                           uint8_t textSize)
{
    if (divisions == 0 || width == 0)
        return;

    if (x >= ILI9341_WIDTH || y >= ILI9341_HEIGHT)
        return;

    float timeStep =
        totalTime / (float)divisions;

    for (uint8_t i = 0; i <= divisions; i++)
    {
        /*
         * Calculamos la posición utilizando primero una operación
         * de mayor precisión para evitar acumular errores.
         */
        uint16_t markX =
            x + (uint16_t)(((uint32_t)width * i) / divisions);

        float timeValue =
            timeStep * i;

        /*
         * Pequeña marca vertical sobre el eje.
         */
        ILI9341_DrawVLine(
            markX,
            y - 5,
            5,
            lineColor
        );

        /*
         * Posición aproximada del texto.
         *
         * Cada carácter de font5x7 ocupa aproximadamente
         * 6 píxeles de ancho multiplicados por textSize.
         */
        int16_t textX =
            (int16_t)markX - (6 * textSize);

        if (i == 0)
        {
            // Primera etiqueta alineada al comienzo.
            textX = x;
        }

        if (textX < 0)
        {
            textX = 0;
        }

        /*
         * Escribir tiempo.
         *
         * Usamos un decimal para que también permita escalas como:
         *
         * 0.0 s
         * 0.5 s
         * 1.0 s
         */
        ILI9341_WriteValueUnit(
            (uint16_t)textX,
            y + 3,
            timeValue,
            1,
            "S",
            textColor,
            bgColor,
            textSize
        );
    }
}

void ILI9341_DrawGraphAxes(uint16_t x,
                           uint16_t y,
                           uint16_t width,
                           uint16_t height,
                           uint8_t xDivisions,
                           uint8_t yDivisions,
                           uint16_t axisColor,
                           uint16_t gridColor)
{
    if (width == 0 || height == 0)
        return;

    if (x >= ILI9341_WIDTH || y >= ILI9341_HEIGHT)
        return;

    // Eje X: abajo físicamente
    ILI9341_DrawHLine(
        x,
        y,
        width,
        axisColor
    );

    // Eje Y: izquierda
    ILI9341_DrawVLine(
        x,
        y,
        height,
        axisColor
    );

    // Divisiones verticales
    if (xDivisions > 0)
    {
        for (uint8_t i = 1; i < xDivisions; i++)
        {
            uint16_t xPos =
                x + ((uint32_t)width * i) / xDivisions;

            ILI9341_DrawDashedVLine(
                xPos,
                y,
                height,
                gridColor,
                4,
                4
            );
        }
    }

    // Divisiones horizontales
    if (yDivisions > 0)
    {
        for (uint8_t i = 1; i < yDivisions; i++)
        {
            uint16_t yPos =
                y + ((uint32_t)height * i) / yDivisions;

            ILI9341_DrawDashedHLine(
                x,
                yPos,
                width,
                gridColor,
                4,
                4
            );
        }
    }
}

void ILI9341_DrawGraphFrame(uint16_t x,
                            uint16_t y,
                            uint16_t width,
                            uint16_t height,
                            uint8_t xDivisions,
                            uint8_t yDivisions,
                            uint16_t borderColor,
                            uint16_t gridColor)
{
    if (width == 0 || height == 0)
        return;

    if (x >= ILI9341_WIDTH || y >= ILI9341_HEIGHT)
        return;

    // Borde inferior
    ILI9341_DrawHLine(
        x,
        y,
        width,
        borderColor
    );

    // Borde superior
    ILI9341_DrawHLine(
        x,
        y + height - 1,
        width,
        borderColor
    );

    // Borde izquierdo
    ILI9341_DrawVLine(
        x,
        y,
        height,
        borderColor
    );

    // Borde derecho
    ILI9341_DrawVLine(
        x + width - 1,
        y,
        height,
        borderColor
    );

    // Divisiones verticales
    if (xDivisions > 0)
    {
        for (uint8_t i = 1; i < xDivisions; i++)
        {
            uint16_t xPos =
                x + ((uint32_t)width * i) / xDivisions;

            ILI9341_DrawDashedVLine(
                xPos,
                y,
                height,
                gridColor,
                4,
                4
            );
        }
    }

    // Divisiones horizontales
    if (yDivisions > 0)
    {
        for (uint8_t i = 1; i < yDivisions; i++)
        {
            uint16_t yPos =
                y + ((uint32_t)height * i) / yDivisions;

            ILI9341_DrawDashedHLine(
                x,
                yPos,
                width,
                gridColor,
                4,
                4
            );
        }
    }
}

void ILI9341_DrawGraphTemplate(uint16_t x,
                               uint16_t y,
                               uint16_t width,
                               uint16_t height,
                               float minValue,
                               float maxValue,
                               uint8_t yDivisions,
                               float totalTime,
                               uint8_t xDivisions,
                               const char *unit,
                               uint16_t borderColor,
                               uint16_t gridColor,
                               uint16_t textColor,
                               uint16_t bgColor,
                               uint8_t textSize)
{
    if (width == 0 || height == 0)
        return;

    if (xDivisions == 0 || yDivisions == 0)
        return;

    /*
     * Espacio reservado para las etiquetas
     * de la escala vertical.
     */
    uint16_t labelWidth = 45 * textSize;

    if (width <= labelWidth)
        return;

    uint16_t graphX = x + labelWidth;
    uint16_t graphWidth = width - labelWidth;

    /*
     * Dibujar marco y grilla.
     */
    ILI9341_DrawGraphFrame(
        graphX,
        y,
        graphWidth,
        height,
        xDivisions,
        yDivisions,
        borderColor,
        gridColor
    );

    /*
     * Dibujar escala vertical.
     *
     * La función ya reserva espacio para
     * los valores a la izquierda.
     */
    ILI9341_DrawVerticalScale(
        x,
        y,
        width,
        height,
        minValue,
        maxValue,
        yDivisions,
        unit,
        gridColor,
        textColor,
        bgColor,
        textSize
    );

    /*
     * Escala temporal.
     *
     * En nuestra orientación física actual,
     * las etiquetas inferiores funcionan bien
     * utilizando una coordenada Y pequeña.
     */
    uint16_t timeY;

    if (y > 15)
        timeY = y - 15;
    else
        timeY = 5;

    ILI9341_DrawTimeScale(
        graphX,
        timeY,
        graphWidth,
        totalTime,
        xDivisions,
        gridColor,
        textColor,
        bgColor,
        textSize
    );
}

int16_t ILI9341_GraphValueToY(float value,
                              float minValue,
                              float maxValue,
                              uint16_t graphY,
                              uint16_t graphHeight)
{
    if (graphHeight == 0)
        return graphY;

    if (maxValue <= minValue)
        return graphY;

    // Limitar el valor al rango de la escala
    if (value < minValue)
        value = minValue;

    if (value > maxValue)
        value = maxValue;

    // Normalizar entre 0.0 y 1.0
    float normalized =
        (value - minValue) /
        (maxValue - minValue);

    // Convertir a posición dentro del gráfico
    int16_t y =
        graphY +
        (int16_t)(normalized * (graphHeight - 1));

    return y;
}

void ILI9341_DrawGraphColumnWithTimeAxis(
    uint16_t x,
    uint16_t graphY,
    uint16_t graphHeight,
    int16_t yPrevious,
    int16_t yNew,
    uint8_t verticalGrid,
    uint16_t signalColor,
    uint16_t gridColor,
    uint16_t backgroundColor)
{
    if (x >= ILI9341_WIDTH)
        return;

    if (graphHeight == 0)
        return;

    uint16_t graphEnd =
        graphY + graphHeight - 1;

    if (graphEnd >= ILI9341_HEIGHT)
        graphEnd = ILI9341_HEIGHT - 1;


    // Limitar la señal al área del gráfico
    if (yPrevious < graphY)
        yPrevious = graphY;

    if (yPrevious > graphEnd)
        yPrevious = graphEnd;

    if (yNew < graphY)
        yNew = graphY;

    if (yNew > graphEnd)
        yNew = graphEnd;


    int16_t yMin =
        (yPrevious < yNew) ? yPrevious : yNew;

    int16_t yMax =
        (yPrevious > yNew) ? yPrevious : yNew;


    // Solo modificamos la zona del gráfico
    ILI9341_SetAddressWindow(
        x,
        graphY,
        x,
        graphEnd
    );


    HAL_GPIO_WritePin(
        LCD_CS_GPIO_Port,
        LCD_CS_Pin,
        GPIO_PIN_RESET
    );


    // Memory Write
    HAL_GPIO_WritePin(
        LCD_RS_GPIO_Port,
        LCD_RS_Pin,
        GPIO_PIN_RESET
    );

    LCD_Write8(0x2C);


    // Modo datos
    HAL_GPIO_WritePin(
        LCD_RS_GPIO_Port,
        LCD_RS_Pin,
        GPIO_PIN_SET
    );


    for (uint16_t y = graphY;
         y <= graphEnd;
         y++)
    {
        uint16_t color = backgroundColor;


        // Línea vertical de grilla
        if (verticalGrid)
        {
            color = gridColor;
        }


        // Líneas horizontales de grilla
        if ((y == 60) ||
            (y == 120) ||
            (y == 180))
        {
            color = gridColor;
        }


        // La señal queda por encima de la grilla
        if ((y >= yMin) &&
            (y <= yMax))
        {
            color = signalColor;
        }


        LCD_Write8(
            (color >> 8) & 0xFF
        );

        LCD_Write8(
            color & 0xFF
        );
    }


    HAL_GPIO_WritePin(
        LCD_CS_GPIO_Port,
        LCD_CS_Pin,
        GPIO_PIN_SET
    );
}

#endif
