/**
 ******************************************************************************
 * @file    ili9341_touch.h
 * @brief   Driver para pantalla táctil resistiva de 4 hilos del módulo
 *          ILI9341 MAR2406.
 *
 * El touch comparte señales con el bus utilizado por el display ILI9341.
 * Durante una lectura, algunos GPIO se reconfiguran temporalmente para
 * obtener las coordenadas mediante el ADC del STM32.
 *
 * Finalizada la lectura, los GPIO son restaurados para continuar utilizando
 * normalmente el display.
 *
 * @author  Maximiliano Montenegro <maximontenegro@frba.utn.edu.ar>
 ******************************************************************************
 */

#ifndef INC_ILI9341_TOUCH_H_
#define INC_ILI9341_TOUCH_H_


#include "main.h"
#include <stdint.h>


// ============================================================================
// CALIBRACION DEL TOUCH
// ============================================================================

/*
 * Valores obtenidos mediante TouchScreen_Calibr_native
 * utilizando el módulo físico.
 *
 * Wiring:
 *
 * YP = A1
 * XM = A2
 * YM = D7
 * XP = D6
 *
 * Calibration:
 *
 * LEFT  = 907
 * RIGHT = 136
 * TOP   = 942
 * BOTTOM= 139
 */

#define ILI9341_TOUCH_LEFT      907
#define ILI9341_TOUCH_RIGHT     136

#define ILI9341_TOUCH_TOP       942
#define ILI9341_TOUCH_BOTTOM    139


// ============================================================================
// ESTRUCTURA DE DATOS
// ============================================================================

/**
 * @brief Estructura que contiene una lectura del touch.
 *
 * x e y representan las coordenadas calibradas sobre la pantalla.
 *
 * rawX y rawY contienen directamente las lecturas obtenidas mediante ADC.
 *
 * pressure permite estimar si existe una pulsación sobre la pantalla.
 *
 * pressed vale 1 cuando se considera que existe una pulsación válida.
 */
typedef struct
{
    uint16_t x;
    uint16_t y;

    uint16_t rawX;
    uint16_t rawY;

    uint16_t pressure;

    uint8_t pressed;

} ILI9341_TouchPoint;


// ============================================================================
// INICIALIZACION
// ============================================================================

/**
 * @brief Inicializa el sistema de lectura táctil.
 *
 * Prepara los recursos necesarios para utilizar el touch resistivo.
 *
 * El ADC debe estar previamente configurado mediante STM32CubeMX.
 */
void ILI9341_Touch_Init(void);


// ============================================================================
// LECTURAS CRUDAS
// ============================================================================

/**
 * @brief Obtiene la coordenada X cruda del touch.
 *
 * Reconfigura temporalmente los GPIO correspondientes al panel resistivo,
 * realiza la lectura mediante ADC y posteriormente restaura los GPIO.
 *
 * @return Valor ADC correspondiente al eje X.
 */
uint16_t ILI9341_Touch_ReadRawX(void);


/**
 * @brief Obtiene la coordenada Y cruda del touch.
 *
 * Reconfigura temporalmente los GPIO correspondientes al panel resistivo,
 * realiza la lectura mediante ADC y posteriormente restaura los GPIO.
 *
 * @return Valor ADC correspondiente al eje Y.
 */
uint16_t ILI9341_Touch_ReadRawY(void);


// ============================================================================
// PRESION
// ============================================================================

/**
 * @brief Obtiene una estimación de la presión aplicada sobre el touch.
 *
 * Se utiliza principalmente para determinar si la pantalla está siendo
 * presionada.
 *
 * @return Valor proporcional a la presión detectada.
 */
uint16_t ILI9341_Touch_ReadPressure(void);


// ============================================================================
// LECTURA COMPLETA
// ============================================================================

/**
 * @brief Obtiene una lectura completa del panel táctil.
 *
 * Realiza las lecturas X, Y y presión, aplica la calibración y devuelve
 * las coordenadas correspondientes a la pantalla.
 *
 * @param point Puntero a la estructura donde se almacenará el resultado.
 *
 * @return 1 si existe una pulsación válida.
 * @return 0 si no se detectó pulsación.
 */
uint8_t ILI9341_Touch_GetPoint(ILI9341_TouchPoint *point);


#endif /* INC_ILI9341_TOUCH_H_ */
