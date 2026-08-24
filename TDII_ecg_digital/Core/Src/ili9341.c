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

void ILI9341_DrawRectangle(uint16_t x, uint16_t y,
                           uint16_t width, uint16_t height,
                           uint16_t color)
{
    ILI9341_DrawLine(x, y, x + width - 1, y, color);
    ILI9341_DrawLine(x, y + height - 1,
                     x + width - 1, y + height - 1, color);

    ILI9341_DrawLine(x, y, x, y + height - 1, color);
    ILI9341_DrawLine(x + width - 1, y,
                     x + width - 1, y + height - 1, color);
}

void ILI9341_FillRectangle(uint16_t x, uint16_t y,
                           uint16_t width, uint16_t height,
                           uint16_t color)
{
    if (x >= ILI9341_WIDTH || y >= ILI9341_HEIGHT)
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

#endif
