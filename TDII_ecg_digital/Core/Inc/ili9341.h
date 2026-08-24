/*/**
 ******************************************************************************
 * @file    ili9341.h
 * @brief   Driver para display TFT ILI9341 con interfaz paralela de 8 bits.
 *
 * Este archivo contiene las definiciones, colores y prototipos de las
 * funciones utilizadas para controlar un display TFT basado en el
 * controlador ILI9341.
 *
 * El driver incluye:
 * - Inicialización del display.
 * - Control de orientación.
 * - Manejo de colores RGB565.
 * - Dibujo de píxeles y líneas.
 * - Rectángulos y círculos.
 * - Escritura de caracteres y cadenas de texto.
 * - Representación gráfica de señales.
 * - Scroll por hardware.
 * - Actualización de gráficos en tiempo real.
 *
 * @author  Maximiliano Montenegro <maximontenegro@frba.utn.edu.ar>
 ******************************************************************************
 */

#ifndef INC_ILI9341_H_
#define INC_ILI9341_H_


#include "main.h"
#include <stdint.h>

// ============================================================================
// habilito el uso de la libreria:
// con '0' deshabilitas, con '1' lo habilitas
// ============================================================================
#define ILI9341_ENABLED 0

// ============================================================================
// DIMENSIONES FISICAS DEL DISPLAY
// ============================================================================

/**
 * @brief Ancho nativo del display ILI9341 en píxeles.
 */
#define ILI9341_TFTWIDTH   240

/**
 * @brief Alto nativo del display ILI9341 en píxeles.
 */
#define ILI9341_TFTHEIGHT  320


// ============================================================================
// COLORES RGB565
// ============================================================================

/**
 * @brief Colores predefinidos en formato RGB565.
 */
#define ILI9341_BLACK   0x0000
#define ILI9341_BLUE    0x001F
#define ILI9341_RED     0xF800
#define ILI9341_GREEN   0x07E0
#define ILI9341_WHITE   0xFFFF
#define ILI9341_YELLOW  0xFFE0
#define ILI9341_CYAN    0x07FF
#define ILI9341_GRAY    0x8410


// ============================================================================
// DIMENSIONES ACTUALES DEL DISPLAY
// ============================================================================

/**
 * @brief Ancho actual de la pantalla en píxeles.
 *
 * Su valor depende de la orientación configurada mediante
 * ILI9341_SetRotation().
 *
 * Puede tomar los valores 240 o 320.
 */
extern uint16_t ILI9341_WIDTH;


/**
 * @brief Alto actual de la pantalla en píxeles.
 *
 * Su valor depende de la orientación configurada mediante
 * ILI9341_SetRotation().
 *
 * Puede tomar los valores 320 o 240.
 */
extern uint16_t ILI9341_HEIGHT;


// ============================================================================
// INICIALIZACION
// ============================================================================

/**
 * @brief Inicializa el display TFT ILI9341.
 *
 * Realiza la secuencia de inicialización necesaria para configurar
 * el controlador ILI9341 y dejar la pantalla lista para ser utilizada.
 *
 * Debe llamarse antes de utilizar cualquier otra función gráfica.
 */
void ILI9341_Init(void);


// ============================================================================
// FUNCIONES BASICAS DE DIBUJO
// ============================================================================

/**
 * @brief Rellena toda la pantalla con un color.
 *
 * @param color Color de relleno en formato RGB565.
 */
void ILI9341_FillScreen(uint16_t color);


/**
 * @brief Dibuja un píxel en la pantalla.
 *
 * @param x Coordenada X del píxel.
 * @param y Coordenada Y del píxel.
 * @param color Color del píxel en formato RGB565.
 */
void ILI9341_DrawPixel(uint16_t x,
                       uint16_t y,
                       uint16_t color);


/**
 * @brief Dibuja una línea entre dos puntos.
 *
 * @param x0 Coordenada X del punto inicial.
 * @param y0 Coordenada Y del punto inicial.
 * @param x1 Coordenada X del punto final.
 * @param y1 Coordenada Y del punto final.
 * @param color Color de la línea en formato RGB565.
 */
void ILI9341_DrawLine(int16_t x0,
                      int16_t y0,
                      int16_t x1,
                      int16_t y1,
                      uint16_t color);


/**
 * @brief Dibuja el contorno de un rectángulo.
 *
 * El interior del rectángulo no es modificado.
 *
 * @param x Coordenada X de la esquina superior izquierda.
 * @param y Coordenada Y de la esquina superior izquierda.
 * @param width Ancho del rectángulo en píxeles.
 * @param height Alto del rectángulo en píxeles.
 * @param color Color del contorno en formato RGB565.
 */
void ILI9341_DrawRectangle(uint16_t x,
                           uint16_t y,
                           uint16_t width,
                           uint16_t height,
                           uint16_t color);


/**
 * @brief Dibuja un rectángulo completamente relleno.
 *
 * @param x Coordenada X de la esquina superior izquierda.
 * @param y Coordenada Y de la esquina superior izquierda.
 * @param width Ancho del rectángulo en píxeles.
 * @param height Alto del rectángulo en píxeles.
 * @param color Color de relleno en formato RGB565.
 */
void ILI9341_FillRectangle(uint16_t x,
                           uint16_t y,
                           uint16_t width,
                           uint16_t height,
                           uint16_t color);


// ============================================================================
// TEXTO
// ============================================================================

/**
 * @brief Dibuja un carácter en la pantalla.
 *
 * Utiliza la fuente incluida en el driver para representar un único
 * carácter en las coordenadas especificadas.
 *
 * @param x Coordenada X inicial.
 * @param y Coordenada Y inicial.
 * @param c Carácter que se desea mostrar.
 * @param color Color del carácter en formato RGB565.
 * @param bg Color de fondo en formato RGB565.
 * @param size Factor de escala del carácter:
 *             1 = tamaño normal,
 *             2 = doble,
 *             3 = triple, etc.
 */
void ILI9341_DrawChar(uint16_t x,
                      uint16_t y,
                      char c,
                      uint16_t color,
                      uint16_t bg,
                      uint8_t size);


/**
 * @brief Escribe una cadena de texto en la pantalla.
 *
 * @param x Coordenada X inicial.
 * @param y Coordenada Y inicial.
 * @param str Cadena de texto.
 * @param color Color del texto.
 * @param bg Color de fondo.
 * @param size Tamaño del texto.
 */
void ILI9341_WriteString(uint16_t x,
                         uint16_t y,
                         const char *str,
                         uint16_t color,
                         uint16_t bg,
                         uint8_t size);


// ============================================================================
// ROTACION DEL DISPLAY
// ============================================================================

/**
 * @brief Configura la orientación de la pantalla.
 *
 * Modifica la orientación del display y actualiza automáticamente
 * las variables ILI9341_WIDTH e ILI9341_HEIGHT.
 *
 * @param rotation Orientación deseada:
 *        - 0: Vertical.
 *        - 1: Horizontal.
 *        - 2: Vertical invertida.
 *        - 3: Horizontal invertida.
 */
void ILI9341_SetRotation(uint8_t rotation);


// ============================================================================
// CIRCULOS
// ============================================================================

/**
 * @brief Dibuja el contorno de un círculo.
 *
 * @param x0 Coordenada X del centro del círculo.
 * @param y0 Coordenada Y del centro del círculo.
 * @param r Radio del círculo en píxeles.
 * @param color Color del contorno en formato RGB565.
 */
void ILI9341_DrawCircle(int16_t x0,
                        int16_t y0,
                        int16_t r,
                        uint16_t color);


/**
 * @brief Dibuja un círculo completamente relleno.
 *
 * @param x0 Coordenada X del centro del círculo.
 * @param y0 Coordenada Y del centro del círculo.
 * @param r Radio del círculo en píxeles.
 * @param color Color de relleno en formato RGB565.
 */
void ILI9341_FillCircle(int16_t x0,
                        int16_t y0,
                        int16_t r,
                        uint16_t color);


// ============================================================================
// GRAFICOS MEDIANTE BUFFER
// ============================================================================

/**
 * @brief Dibuja una señal almacenada en un buffer.
 *
 * Representa gráficamente las muestras almacenadas dentro de una región
 * determinada de la pantalla.
 *
 * Permite trabajar con un buffer circular indicando cuál es la primera
 * muestra y cuántas muestras válidas se encuentran almacenadas.
 *
 * Esta función corresponde al método de representación mediante
 * redibujado del buffer por software.
 *
 * @param buffer Puntero al buffer que contiene las muestras.
 * @param bufferSize Tamaño total del buffer.
 * @param startIndex Índice de la primera muestra a representar.
 * @param samplesStored Cantidad de muestras válidas almacenadas.
 * @param graphX Coordenada X inicial del gráfico.
 * @param graphY Coordenada Y inicial del gráfico.
 * @param graphW Ancho del gráfico en píxeles.
 * @param graphH Alto del gráfico en píxeles.
 * @param signalColor Color de la señal en formato RGB565.
 * @param gridColor Color de la grilla en formato RGB565.
 */
void ILI9341_DrawSignalBuffer(const int16_t *buffer,
                              uint16_t bufferSize,
                              uint16_t startIndex,
                              uint16_t samplesStored,
                              uint16_t graphX,
                              uint16_t graphY,
                              uint16_t graphW,
                              uint16_t graphH,
                              uint16_t signalColor,
                              uint16_t gridColor);


// ============================================================================
// SCROLL POR HARDWARE
// ============================================================================

/**
 * @brief Configura las regiones utilizadas por el scroll por hardware.
 *
 * Divide la memoria visible del display en tres regiones:
 *
 * - Región fija inicial.
 * - Región desplazable.
 * - Región fija final.
 *
 * En la orientación horizontal utilizada para el gráfico, este mecanismo
 * permite realizar el desplazamiento horizontal de la señal sin tener que
 * redibujar toda la pantalla.
 *
 * @param top Tamaño de la primera región fija en píxeles.
 * @param scroll Tamaño de la región desplazable en píxeles.
 * @param bottom Tamaño de la última región fija en píxeles.
 */
void ILI9341_SetScrollArea(uint16_t top,
                           uint16_t scroll,
                           uint16_t bottom);


/**
 * @brief Establece la posición actual del scroll por hardware.
 *
 * Modifica el punto desde el cual el controlador ILI9341 comienza
 * a mostrar la región desplazable de la memoria gráfica.
 *
 * Permite desplazar el contenido sin necesidad de redibujar todos
 * los píxeles de la pantalla.
 *
 * @param offset Posición de desplazamiento dentro del área de scroll.
 */
void ILI9341_SetScroll(uint16_t offset);


// ============================================================================
// GRAFICO EN TIEMPO REAL
// ============================================================================

/**
 * @brief Dibuja una nueva columna del gráfico en tiempo real.
 *
 * Construye una columna completa del gráfico incluyendo:
 *
 * - Color de fondo.
 * - Líneas horizontales de la grilla.
 * - Línea vertical de la grilla, cuando corresponda.
 * - Segmento correspondiente a la señal.
 *
 * Está diseñada para trabajar junto con el scroll por hardware del
 * controlador ILI9341.
 *
 * De esta manera solamente se actualiza la nueva columna que entra
 * en pantalla, evitando redibujar el gráfico completo en cada muestra.
 *
 * @param x Columna de memoria que se desea actualizar.
 * @param yPrevious Coordenada Y correspondiente a la muestra anterior.
 * @param yNew Coordenada Y correspondiente a la nueva muestra.
 * @param verticalGrid Indica si debe dibujarse una línea vertical
 *                     de grilla en esta columna:
 *                     0 = no dibujar,
 *                     1 = dibujar.
 * @param signalColor Color de la señal en formato RGB565.
 * @param gridColor Color de la grilla en formato RGB565.
 * @param backgroundColor Color de fondo en formato RGB565.
 */
void ILI9341_DrawGraphColumn(uint16_t x,
                             int16_t yPrevious,
                             int16_t yNew,
                             uint8_t verticalGrid,
                             uint16_t signalColor,
                             uint16_t gridColor,
                             uint16_t backgroundColor);


#endif /* INC_ILI9341_H_ */
