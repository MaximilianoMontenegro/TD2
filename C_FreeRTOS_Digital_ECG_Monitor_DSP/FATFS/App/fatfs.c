/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file   fatfs.c
  * @brief  Code for fatfs applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
#include "fatfs.h"

uint8_t retUSER;    /* Return value for USER */
char USERPath[4];   /* USER logical drive path */
FATFS USERFatFS;    /* File system object for USER logical drive */
FIL USERFile;       /* File object for USER */

/* USER CODE BEGIN Variables */

/* USER CODE END Variables */

void MX_FATFS_Init(void)
{
  /*## FatFS: Link the USER driver ###########################*/
  retUSER = FATFS_LinkDriver(&USER_Driver, USERPath);

  /* USER CODE BEGIN Init */
  /* additional user code for init */
  /* USER CODE END Init */
}

/**
  * @brief  Gets Time from RTC
  * @param  None
  * @retval Time in DWORD
  */
DWORD get_fattime(void)
{
  /* USER CODE BEGIN get_fattime */
  /* Sin RTC: fecha y hora de COMPILACION (antes 1/1/1980). Los archivos
     quedan al menos ordenados por version de firmware. */
  static const char mon[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  const char *d = __DATE__;   /* "Sep 27 2026" */
  const char *t = __TIME__;   /* "19:20:05"    */
  DWORD m = 1;
  for (DWORD k = 0; k < 12u; k++) {
    if (d[0] == mon[3u * k] && d[1] == mon[3u * k + 1u] && d[2] == mon[3u * k + 2u]) { m = k + 1u; break; }
  }
  DWORD day = ((d[4] == ' ') ? 0u : (DWORD)(d[4] - '0')) * 10u + (DWORD)(d[5] - '0');
  DWORD y   = (DWORD)(d[7] - '0') * 1000u + (DWORD)(d[8] - '0') * 100u
            + (DWORD)(d[9] - '0') * 10u + (DWORD)(d[10] - '0');
  DWORD hh  = (DWORD)(t[0] - '0') * 10u + (DWORD)(t[1] - '0');
  DWORD mm  = (DWORD)(t[3] - '0') * 10u + (DWORD)(t[4] - '0');
  DWORD ss  = (DWORD)(t[6] - '0') * 10u + (DWORD)(t[7] - '0');
  return ((y - 1980u) << 25) | (m << 21) | (day << 16) | (hh << 11) | (mm << 5) | (ss / 2u);
  /* USER CODE END get_fattime */
}

/* USER CODE BEGIN Application */

/* USER CODE END Application */
