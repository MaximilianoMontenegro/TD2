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
#define ILI9341_ENABLED 1

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

/**
 * @brief Dibuja una línea horizontal.
 *
 * @param x Coordenada X inicial.
 * @param y Coordenada Y de la línea.
 * @param width Longitud de la línea en píxeles.
 * @param color Color de la línea en formato RGB565.
 */
void ILI9341_DrawHLine(uint16_t x,
                       uint16_t y,
                       uint16_t width,
                       uint16_t color);

/**
 * @brief Dibuja una línea vertical.
 *
 * Utiliza una ventana de escritura para enviar los píxeles
 * consecutivamente y reducir las operaciones necesarias.
 *
 * @param x Coordenada X de la línea.
 * @param y Coordenada Y inicial.
 * @param height Longitud de la línea en píxeles.
 * @param color Color de la línea en formato RGB565.
 */
void ILI9341_DrawVLine(uint16_t x,
                       uint16_t y,
                       uint16_t height,
                       uint16_t color);

/**
 * @brief Dibuja el contorno de un triángulo.
 *
 * El triángulo se define mediante tres puntos.
 *
 * @param x0 Coordenada X del primer vértice.
 * @param y0 Coordenada Y del primer vértice.
 * @param x1 Coordenada X del segundo vértice.
 * @param y1 Coordenada Y del segundo vértice.
 * @param x2 Coordenada X del tercer vértice.
 * @param y2 Coordenada Y del tercer vértice.
 * @param color Color del triángulo en formato RGB565.
 */
void ILI9341_DrawTriangle(int16_t x0, int16_t y0,
                          int16_t x1, int16_t y1,
                          int16_t x2, int16_t y2,
                          uint16_t color);

/**
 * @brief Dibuja un triángulo completamente relleno.
 *
 * El triángulo se define mediante tres vértices.
 *
 * @param x0 Coordenada X del primer vértice.
 * @param y0 Coordenada Y del primer vértice.
 * @param x1 Coordenada X del segundo vértice.
 * @param y1 Coordenada Y del segundo vértice.
 * @param x2 Coordenada X del tercer vértice.
 * @param y2 Coordenada Y del tercer vértice.
 * @param color Color de relleno en formato RGB565.
 */
void ILI9341_FillTriangle(int16_t x0, int16_t y0,
                          int16_t x1, int16_t y1,
                          int16_t x2, int16_t y2,
                          uint16_t color);

/**
 * @brief Dibuja el contorno de un rectángulo con esquinas redondeadas.
 *
 * @param x Coordenada X de la esquina superior izquierda.
 * @param y Coordenada Y de la esquina superior izquierda.
 * @param width Ancho del rectángulo en píxeles.
 * @param height Alto del rectángulo en píxeles.
 * @param radius Radio de las esquinas.
 * @param color Color del contorno en formato RGB565.
 */
void ILI9341_DrawRoundRect(uint16_t x,
                           uint16_t y,
                           uint16_t width,
                           uint16_t height,
                           uint16_t radius,
                           uint16_t color);

/**
 * @brief Dibuja un rectángulo relleno con esquinas redondeadas.
 *
 * @param x Coordenada X de la esquina superior izquierda.
 * @param y Coordenada Y de la esquina superior izquierda.
 * @param width Ancho del rectángulo en píxeles.
 * @param height Alto del rectángulo en píxeles.
 * @param radius Radio de las esquinas.
 * @param color Color de relleno en formato RGB565.
 */
void ILI9341_FillRoundRect(uint16_t x,
                           uint16_t y,
                           uint16_t width,
                           uint16_t height,
                           uint16_t radius,
                           uint16_t color);

/**
 * @brief Dibuja una barra de progreso.
 *
 * @param x Coordenada X de la esquina superior izquierda.
 * @param y Coordenada Y de la esquina superior izquierda.
 * @param width Ancho total de la barra.
 * @param height Alto total de la barra.
 * @param value Valor actual de progreso.
 * @param maxValue Valor máximo de la escala.
 * @param fgColor Color de la parte completada.
 * @param bgColor Color de la parte no completada.
 * @param borderColor Color del borde.
 */
void ILI9341_DrawProgressBar(uint16_t x,
                             uint16_t y,
                             uint16_t width,
                             uint16_t height,
                             uint16_t value,
                             uint16_t maxValue,
                             uint16_t fgColor,
                             uint16_t bgColor,
                             uint16_t borderColor);

/**
 * @brief Dibuja una barra de progreso con esquinas redondeadas.
 *
 * @param x Coordenada X de la esquina superior izquierda.
 * @param y Coordenada Y de la esquina superior izquierda.
 * @param width Ancho total de la barra.
 * @param height Alto total de la barra.
 * @param value Valor actual de progreso.
 * @param maxValue Valor máximo de la escala.
 * @param fgColor Color de la parte completada.
 * @param bgColor Color de la parte no completada.
 * @param borderColor Color del borde.
 */
void ILI9341_DrawRoundProgressBar(uint16_t x,
                                  uint16_t y,
                                  uint16_t width,
                                  uint16_t height,
                                  uint16_t value,
                                  uint16_t maxValue,
                                  uint16_t fgColor,
                                  uint16_t bgColor,
                                  uint16_t borderColor);

/**
 * @brief Dibuja un botón rectangular con esquinas redondeadas y texto centrado.
 *
 * @param x Coordenada X de la esquina superior izquierda.
 * @param y Coordenada Y de la esquina superior izquierda.
 * @param width Ancho del botón en píxeles.
 * @param height Alto del botón en píxeles.
 * @param text Texto que se mostrará en el botón.
 * @param textColor Color del texto en formato RGB565.
 * @param fillColor Color de relleno del botón.
 * @param borderColor Color del borde del botón.
 * @param textSize Tamaño de la fuente.
 */
void ILI9341_DrawButton(uint16_t x,
                        uint16_t y,
                        uint16_t width,
                        uint16_t height,
                        const char *text,
                        uint16_t textColor,
                        uint16_t fillColor,
                        uint16_t borderColor,
                        uint8_t textSize);

/**
 * @brief Escribe un número entero en la pantalla.
 *
 * Convierte automáticamente el valor entero a texto y lo muestra
 * utilizando la fuente del driver.
 *
 * @param x Coordenada X inicial.
 * @param y Coordenada Y inicial.
 * @param value Valor entero que se desea mostrar.
 * @param color Color del texto en formato RGB565.
 * @param bg Color de fondo en formato RGB565.
 * @param size Factor de escala del texto.
 */
void ILI9341_WriteInt(uint16_t x,
                      uint16_t y,
                      int32_t value,
                      uint16_t color,
                      uint16_t bg,
                      uint8_t size);

/**
 * @brief Escribe un número decimal en la pantalla.
 *
 * Convierte automáticamente un valor de tipo float a texto y lo muestra
 * utilizando la fuente del driver.
 *
 * @param x Coordenada X inicial.
 * @param y Coordenada Y inicial.
 * @param value Valor decimal que se desea mostrar.
 * @param decimals Cantidad de cifras decimales a mostrar.
 * @param color Color del texto en formato RGB565.
 * @param bg Color de fondo en formato RGB565.
 * @param size Factor de escala del texto.
 */
void ILI9341_WriteFloat(uint16_t x,
                        uint16_t y,
                        float value,
                        uint8_t decimals,
                        uint16_t color,
                        uint16_t bg,
                        uint8_t size);

/**
 * @brief Escribe un valor decimal seguido de una unidad.
 *
 * Ejemplo:
 * 2.73 V
 *
 * @param x Coordenada X inicial.
 * @param y Coordenada Y inicial.
 * @param value Valor decimal.
 * @param decimals Cantidad de decimales.
 * @param unit Cadena con la unidad, por ejemplo "V", "Hz" o "%".
 * @param color Color del texto.
 * @param bg Color de fondo.
 * @param size Tamaño del texto.
 */
void ILI9341_WriteValueUnit(uint16_t x,
                            uint16_t y,
                            float value,
                            uint8_t decimals,
                            const char *unit,
                            uint16_t color,
                            uint16_t bg,
                            uint8_t size);

/**
 * @brief Actualiza un valor decimal con unidad dentro de un área fija.
 *
 * Primero limpia el área indicada y luego escribe el nuevo valor,
 * evitando que queden restos del valor anterior.
 *
 * @param x Coordenada X inicial del campo.
 * @param y Coordenada Y inicial del campo.
 * @param width Ancho del área a limpiar.
 * @param height Alto del área a limpiar.
 * @param value Valor decimal a mostrar.
 * @param decimals Cantidad de decimales.
 * @param unit Unidad, por ejemplo "V", "HZ", "%".
 * @param color Color del texto.
 * @param bg Color de fondo.
 * @param size Tamaño del texto.
 */
void ILI9341_UpdateValueUnit(uint16_t x,
                             uint16_t y,
                             uint16_t width,
                             uint16_t height,
                             float value,
                             uint8_t decimals,
                             const char *unit,
                             uint16_t color,
                             uint16_t bg,
                             uint8_t size);

/**
 * @brief Actualiza un valor decimal con unidad utilizando un campo de ancho fijo.
 *
 * El valor se alinea dentro de una cantidad fija de caracteres para evitar
 * restos del contenido anterior cuando cambia la cantidad de dígitos.
 *
 * @param x Coordenada X inicial.
 * @param y Coordenada Y inicial.
 * @param value Valor decimal a mostrar.
 * @param decimals Cantidad de decimales.
 * @param unit Unidad, por ejemplo "V", "HZ" o "%".
 * @param fieldWidth Cantidad total de caracteres reservados para el campo.
 * @param color Color del texto.
 * @param bg Color de fondo.
 * @param size Tamaño del texto.
 */
void ILI9341_UpdateValueFixed(uint16_t x,
                              uint16_t y,
                              float value,
                              uint8_t decimals,
                              const char *unit,
                              uint8_t fieldWidth,
                              uint16_t color,
                              uint16_t bg,
                              uint8_t size);

/**
 * @brief Dibuja una línea discontinua entre dos puntos.
 *
 * @param x0 Coordenada X inicial.
 * @param y0 Coordenada Y inicial.
 * @param x1 Coordenada X final.
 * @param y1 Coordenada Y final.
 * @param color Color de la línea.
 * @param dashLength Longitud de cada tramo visible.
 * @param gapLength Longitud de cada espacio.
 */
void ILI9341_DrawDashedLine(int16_t x0,
                            int16_t y0,
                            int16_t x1,
                            int16_t y1,
                            uint16_t color,
                            uint8_t dashLength,
                            uint8_t gapLength);

/**
 * @brief Dibuja una línea horizontal discontinua.
 *
 * @param x Coordenada X inicial.
 * @param y Coordenada Y de la línea.
 * @param width Longitud total de la línea.
 * @param color Color de la línea.
 * @param dashLength Longitud de cada tramo visible.
 * @param gapLength Longitud de cada espacio.
 */
void ILI9341_DrawDashedHLine(uint16_t x,
                             uint16_t y,
                             uint16_t width,
                             uint16_t color,
                             uint16_t dashLength,
                             uint16_t gapLength);

/**
 * @brief Dibuja una línea vertical discontinua.
 *
 * @param x Coordenada X de la línea.
 * @param y Coordenada Y inicial.
 * @param height Longitud total de la línea.
 * @param color Color de la línea.
 * @param dashLength Longitud de cada tramo visible.
 * @param gapLength Longitud de cada espacio.
 */
void ILI9341_DrawDashedVLine(uint16_t x,
                             uint16_t y,
                             uint16_t height,
                             uint16_t color,
                             uint16_t dashLength,
                             uint16_t gapLength);

/**
 * @brief Dibuja una línea horizontal de referencia con una etiqueta.
 *
 * @param x Coordenada X inicial de la línea.
 * @param y Coordenada Y de la referencia.
 * @param width Longitud de la línea.
 * @param label Texto de la etiqueta, por ejemplo "1.0 V".
 * @param lineColor Color de la línea.
 * @param textColor Color del texto.
 * @param bgColor Color de fondo.
 * @param textSize Tamaño del texto.
 */
void ILI9341_DrawReferenceLine(uint16_t x,
                               uint16_t y,
                               uint16_t width,
                               const char *label,
                               uint16_t lineColor,
                               uint16_t textColor,
                               uint16_t bgColor,
                               uint8_t textSize);

/**
 * @brief Dibuja una escala vertical con líneas de referencia y valores.
 *
 * @param x Coordenada X inicial de las líneas.
 * @param y Coordenada Y superior del gráfico.
 * @param width Ancho de las líneas de referencia.
 * @param height Alto total del gráfico.
 * @param minValue Valor mínimo de la escala.
 * @param maxValue Valor máximo de la escala.
 * @param divisions Cantidad de divisiones verticales.
 * @param unit Unidad a mostrar, por ejemplo "V".
 * @param lineColor Color de las líneas.
 * @param textColor Color de los valores.
 * @param bgColor Color de fondo.
 * @param textSize Tamaño del texto.
 */
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
                               uint8_t textSize);

/**
 * @brief Dibuja una escala horizontal de tiempo debajo de un gráfico.
 *
 * Distribuye automáticamente las marcas de tiempo a lo largo del ancho
 * especificado y muestra el tiempo correspondiente en segundos.
 *
 * @param x Coordenada X inicial del gráfico.
 * @param y Coordenada Y donde se dibujarán las marcas de tiempo.
 * @param width Ancho total del gráfico en píxeles.
 * @param totalTime Tiempo total representado por el gráfico, en segundos.
 * @param divisions Cantidad de divisiones del eje temporal.
 * @param lineColor Color de las marcas verticales.
 * @param textColor Color del texto.
 * @param bgColor Color de fondo del texto.
 * @param textSize Tamaño del texto.
 */
void ILI9341_DrawTimeScale(uint16_t x,
                           uint16_t y,
                           uint16_t width,
                           float totalTime,
                           uint8_t divisions,
                           uint16_t lineColor,
                           uint16_t textColor,
                           uint16_t bgColor,
                           uint8_t textSize);

/**
 * @brief Dibuja los ejes principales de un gráfico con divisiones.
 *
 * @param x Coordenada X inicial del gráfico.
 * @param y Coordenada Y inicial del gráfico.
 * @param width Ancho del gráfico en píxeles.
 * @param height Alto del gráfico en píxeles.
 * @param xDivisions Cantidad de divisiones sobre el eje X.
 * @param yDivisions Cantidad de divisiones sobre el eje Y.
 * @param axisColor Color de los ejes.
 * @param gridColor Color de las divisiones internas.
 */
void ILI9341_DrawGraphAxes(uint16_t x,
                           uint16_t y,
                           uint16_t width,
                           uint16_t height,
                           uint8_t xDivisions,
                           uint8_t yDivisions,
                           uint16_t axisColor,
                           uint16_t gridColor);

/**
 * @brief Dibuja el marco completo de un gráfico con grilla interna.
 *
 * @param x Coordenada X inicial.
 * @param y Coordenada Y inferior del gráfico.
 * @param width Ancho del gráfico.
 * @param height Alto del gráfico.
 * @param xDivisions Cantidad de divisiones verticales.
 * @param yDivisions Cantidad de divisiones horizontales.
 * @param borderColor Color del borde.
 * @param gridColor Color de la grilla.
 */
void ILI9341_DrawGraphFrame(uint16_t x,
                            uint16_t y,
                            uint16_t width,
                            uint16_t height,
                            uint8_t xDivisions,
                            uint8_t yDivisions,
                            uint16_t borderColor,
                            uint16_t gridColor);

/**
 * @brief Dibuja una plantilla completa de gráfico.
 *
 * Combina marco, grilla, escala vertical y escala horizontal de tiempo.
 *
 * @param x Coordenada X inicial del gráfico.
 * @param y Coordenada Y inferior del gráfico.
 * @param width Ancho total del gráfico.
 * @param height Alto total del gráfico.
 * @param minValue Valor mínimo de la escala vertical.
 * @param maxValue Valor máximo de la escala vertical.
 * @param yDivisions Cantidad de divisiones verticales.
 * @param totalTime Tiempo total representado en el eje X.
 * @param xDivisions Cantidad de divisiones temporales.
 * @param unit Unidad de la escala vertical, por ejemplo "V".
 * @param borderColor Color del marco.
 * @param gridColor Color de la grilla.
 * @param textColor Color de las etiquetas.
 * @param bgColor Color de fondo.
 * @param textSize Tamaño del texto.
 */
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
                               uint8_t textSize);

/**
 * @brief Convierte un valor físico en una coordenada Y dentro de un gráfico.
 *
 * @param value Valor que se desea representar.
 * @param minValue Valor mínimo de la escala.
 * @param maxValue Valor máximo de la escala.
 * @param graphY Coordenada Y inferior del gráfico.
 * @param graphHeight Alto del gráfico en píxeles.
 *
 * @return Coordenada Y correspondiente dentro del gráfico.
 */
int16_t ILI9341_GraphValueToY(float value,
                              float minValue,
                              float maxValue,
                              uint16_t graphY,
                              uint16_t graphHeight);

/**
 * @brief Dibuja una columna de gráfico dejando una zona inferior
 *        reservada para la escala temporal.
 *
 * @param x Columna de memoria a actualizar.
 * @param graphY Coordenada Y inferior del área de señal.
 * @param graphHeight Alto del área destinada a la señal.
 * @param yPrevious Coordenada Y de la muestra anterior.
 * @param yNew Coordenada Y de la nueva muestra.
 * @param verticalGrid Indica si corresponde dibujar una línea vertical.
 * @param signalColor Color de la señal.
 * @param gridColor Color de la grilla.
 * @param backgroundColor Color de fondo.
 */
void ILI9341_DrawGraphColumnWithTimeAxis(
    uint16_t x,
    uint16_t graphY,
    uint16_t graphHeight,
    int16_t yPrevious,
    int16_t yNew,
    uint8_t verticalGrid,
    uint16_t signalColor,
    uint16_t gridColor,
    uint16_t backgroundColor
);

#endif /* INC_ILI9341_H_ */
