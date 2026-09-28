/**
 ******************************************************************************
 * @file    sd_spi.c
 * @brief   Driver minimo de tarjeta SD en modo SPI (SDSC y SDHC/SDXC).
 * @details Ver sd_spi.h para el conexionado y la secuencia de comandos.
 *          Lo usa user_diskio.c, que es la capa que llama FatFs.
 * @author  Grupo TD II - UTN FRBA
 ******************************************************************************
 */
#include "sd_spi.h"
#include <string.h>

extern SPI_HandleTypeDef hspi1;              /* SPI1, generado por CubeMX */
SD_CardType g_sd_type = SD_TYPE_UNKNOWN;

/** Seleccion de la tarjeta por BSRR (escritura atomica, sin HAL). */
#define SD_CS_LOW()   (SD_CS_GPIO_Port->BSRR = (uint32_t)SD_CS_Pin << 16u)
#define SD_CS_HIGH()  (SD_CS_GPIO_Port->BSRR = (uint32_t)SD_CS_Pin)

/**
 * @brief  Envia y recibe un byte por SPI (full-duplex).
 * @param  out  Byte a enviar (0xFF para solo leer).
 * @return Byte recibido.
 */
static uint8_t SD_XferByte(uint8_t out)
{
  uint8_t in = 0xFF;
  HAL_SPI_TransmitReceive(&hspi1, &out, &in, 1, 50);
  return in;
}

/**
 * @brief  Cambia la velocidad del SPI.
 * @param  fast  0 = 328 kHz (la inicializacion exige <= 400 kHz), 1 = 10,5 MHz.
 * @note   El manual de referencia pide apagar SPE antes de tocar los bits BR.
 */
static void SD_SetSpeed(uint8_t fast)
{
  uint32_t br = fast ? SPI_BAUDRATEPRESCALER_8      /* 84 MHz / 8   = 10.5 MHz */
                     : SPI_BAUDRATEPRESCALER_256;   /* 84 MHz / 256 = 328 kHz  */
  CLEAR_BIT(hspi1.Instance->CR1, SPI_CR1_SPE);
  MODIFY_REG(hspi1.Instance->CR1, SPI_CR1_BR, br);
  SET_BIT(hspi1.Instance->CR1, SPI_CR1_SPE);
}

/**
 * @brief  Espera a que la tarjeta libere la linea (responde 0xFF).
 * @param  timeout_ms  Tiempo maximo de espera.
 * @return 1 = lista, 0 = sigue ocupada.
 */
static uint8_t SD_WaitReady(uint32_t timeout_ms)
{
  uint32_t t0 = HAL_GetTick();
  uint8_t r;
  do { r = SD_XferByte(0xFF); } while (r != 0xFF && (HAL_GetTick() - t0) < timeout_ms);
  return (r == 0xFF);
}

/**
 * @brief  Envia un comando de 6 bytes y espera la respuesta R1.
 * @param  cmd  Indice del comando (0..63).
 * @param  arg  Argumento de 32 bits.
 * @param  crc  CRC7 + bit de fin (solo se verifica en CMD0 y CMD8).
 * @return R1 (bit 7 = 0 si hubo respuesta; 0x00 = OK, 0x01 = en reposo).
 * @note   Deja CS en bajo: el llamador lee lo que siga y libera la tarjeta.
 */
static uint8_t SD_SendCmd(uint8_t cmd, uint32_t arg, uint8_t crc)
{
  SD_CS_HIGH(); SD_XferByte(0xFF);
  SD_CS_LOW();
  if (!SD_WaitReady(500)) { SD_CS_HIGH(); return 0xFF; }

  SD_XferByte(0x40 | cmd);
  SD_XferByte((uint8_t)(arg >> 24));
  SD_XferByte((uint8_t)(arg >> 16));
  SD_XferByte((uint8_t)(arg >> 8));
  SD_XferByte((uint8_t)arg);
  SD_XferByte(crc);

  uint8_t r1, n = 10;
  do { r1 = SD_XferByte(0xFF); } while ((r1 & 0x80) && --n);
  return r1;
}

uint8_t SD_Init(void)
{
  g_sd_type = SD_TYPE_UNKNOWN;
  SD_SetSpeed(0);
  SD_CS_HIGH();
  for (uint8_t i = 0; i < 10; i++) { SD_XferByte(0xFF); }   /* 80 clocks con CS alto */

  if (SD_SendCmd(0, 0, 0x95) != 0x01) { SD_CS_HIGH(); return 1; }

  uint8_t ocr[4];
  if (SD_SendCmd(8, 0x1AAu, 0x87) == 0x01) {                /* SD v2 */
    for (uint8_t i = 0; i < 4; i++) { ocr[i] = SD_XferByte(0xFF); }
    if (ocr[2] != 0x01 || ocr[3] != 0xAA) { SD_CS_HIGH(); return 2; }

    uint32_t t0 = HAL_GetTick(); uint8_t r1;
    do {
      SD_SendCmd(55, 0, 0x01);
      r1 = SD_SendCmd(41, 0x40000000u, 0x01);                /* ACMD41 con HCS */
    } while (r1 != 0x00 && (HAL_GetTick() - t0) < 1500);
    if (r1 != 0x00) { SD_CS_HIGH(); return 3; }

    if (SD_SendCmd(58, 0, 0x01) != 0x00) { SD_CS_HIGH(); return 4; }
    for (uint8_t i = 0; i < 4; i++) { ocr[i] = SD_XferByte(0xFF); }
    g_sd_type = (ocr[0] & 0x40) ? SD_TYPE_SDHC : SD_TYPE_SDSC;
  } else {                                                   /* SD v1 */
    uint32_t t0 = HAL_GetTick(); uint8_t r1;
    do {
      SD_SendCmd(55, 0, 0x01);
      r1 = SD_SendCmd(41, 0, 0x01);
    } while (r1 != 0x00 && (HAL_GetTick() - t0) < 1500);
    if (r1 != 0x00) { SD_CS_HIGH(); return 5; }
    g_sd_type = SD_TYPE_SDSC;
  }

  if (g_sd_type == SD_TYPE_SDSC) {
    if (SD_SendCmd(16, 512, 0x01) != 0x00) { SD_CS_HIGH(); return 6; }
  }

  SD_CS_HIGH();
  SD_XferByte(0xFF);
  SD_SetSpeed(1);
  return 0;
}

uint8_t SD_ReadBlock(uint32_t sector, uint8_t *buff)
{
  uint32_t addr = (g_sd_type == SD_TYPE_SDHC) ? sector : (sector * 512u);
  if (SD_SendCmd(17, addr, 0xFF) != 0x00) { SD_CS_HIGH(); return 1; }

  uint32_t t0 = HAL_GetTick(); uint8_t token;
  do { token = SD_XferByte(0xFF); } while (token == 0xFF && (HAL_GetTick() - t0) < 200);
  if (token != 0xFE) { SD_CS_HIGH(); return 2; }

  /* HAL_SPI_Receive en maestro full-duplex transmite el propio buffer:
     se llena con 0xFF para mantener MOSI alto como pide la norma. */
  memset(buff, 0xFF, 512);
  HAL_SPI_Receive(&hspi1, buff, 512, 100);
  SD_XferByte(0xFF); SD_XferByte(0xFF);                      /* CRC descartado */

  SD_CS_HIGH();
  SD_XferByte(0xFF);
  return 0;
}

uint8_t SD_WriteBlock(uint32_t sector, const uint8_t *buff)
{
  uint32_t addr = (g_sd_type == SD_TYPE_SDHC) ? sector : (sector * 512u);
  if (SD_SendCmd(24, addr, 0xFF) != 0x00) { SD_CS_HIGH(); return 1; }

  SD_XferByte(0xFE);                                         /* token de inicio */
  HAL_SPI_Transmit(&hspi1, (uint8_t *)buff, 512, 200);
  SD_XferByte(0xFF); SD_XferByte(0xFF);                      /* CRC dummy */

  uint8_t resp = SD_XferByte(0xFF);
  if ((resp & 0x1F) != 0x05) { SD_CS_HIGH(); return 2; }     /* dato rechazado */
  if (!SD_WaitReady(500))    { SD_CS_HIGH(); return 3; }     /* sigue ocupada */

  SD_CS_HIGH();
  SD_XferByte(0xFF);
  return 0;
}
