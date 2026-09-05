/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "arm_math.h"
#include "FreeRTOS.h"
#include "stream_buffer.h"
#include <string.h>
#include "ili9341.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define ADC_OSR             8u
#define DSP_BLOCK_SIZE      25u                          /* 25 @250Hz = 100 ms */
#define ADC_HALF_SIZE       (DSP_BLOCK_SIZE * ADC_OSR)   /* 200 crudas */
#define ADC_BUFFER_SIZE_N   (ADC_HALF_SIZE * 2u)         /* 400 = doble buffer */

/* Referencia de salida del AD8232 = Vs/2 -> 1.65 V -> ~2048 counts.
   Ajustar si medis otro valor en reposo. */
#define ECG_DC_OFFSET   2048.0f

/* --- Detector de QRS --- */
#define QRS_FS          250u   /* fs del camino de deteccion        */
#define QRS_WIN_LEN      37u   /* ventana de integracion = 150 ms   */
#define QRS_REFRACT      50u   /* periodo refractario  = 200 ms     */
#define QRS_RR_HIST       8u   /* promedio de los ultimos 8 R-R     */
#define QRS_LEARN      (2u * QRS_FS)   /* 2 s de fase de aprendizaje */

/* Leads-off del AD8232: salidas activas en alto. */
#define ECG_LO_PORT     GPIOC
#define ECG_LO_P_PIN    GPIO_PIN_10
#define ECG_LO_N_PIN    GPIO_PIN_11

/* Ganancia total del modulo AD8232 ~1100, ADC 12 bits sobre 3.3 V:
   1 mV en la piel -> 1.1 V -> ~1365 counts. Calibrar si tu modulo difiere. */
#define ECG_COUNTS_PER_MV   1365.0f
#define ECG_FULLSCALE_MV       2.0f   /* +-2 mV a pantalla completa */
#define ECG_GRID_BOLD        0x7800   /* rojo medio: cuadro grande */

#define NOTIF_HALF1     0x01u
#define NOTIF_HALF2     0x02u

/* Geometria del grafico en rotacion 1 (320 x 240).
   El driver NO define estas constantes: venian del main de ejemplo. */
#define TIME_LABEL_Y      2
#define TIME_AXIS_Y      20
#define GRAPH_Y          22
#define GRAPH_H         198
#define PANEL_W          70
#define GRAPH_W         250

#define AUTO_HIST       40u   /* 40 bloques x 100 ms = 4 s de autoescala */
#define SWEEP_REVERSED   0    /* poner en 1 si el trazo barre al reves   */
#define QRS_HIST_LEN   128u   /* 512 ms de historia para medir el QRS    */

#define DISPLAY_DECIM    2u   /* 1 columna cada 2 muestras -> 2 s/pantalla */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* Muestra de la historia: back = 0 es la mas reciente. */
#define QRS_H(back)  qrs_hist[(qrs_hist_idx + QRS_HIST_LEN - 1u - (back)) % QRS_HIST_LEN]

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart2;

/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* USER CODE BEGIN PV */
uint16_t adc_buffer[ADC_BUFFER_SIZE_N];

/* Notch 50 Hz con Q = 8 (antes Q = 22.7).
   Un Q alto suena mejor en el papel, pero deja una cola de ringing de
   Q/(pi*f0) = 145 ms despues de cada QRS. Con Q = 8 baja a ~50 ms y
   ademas cubre 47-53 Hz de deriva de la red. */
arm_biquad_casd_df1_inst_f32 notch_filter;
float32_t notch_state[4];
float32_t notch_coeffs[5] = { 0.9438954f, -0.5833578f, 0.9438954f,
                              0.5833578f, -0.8877909f };

/* Pasa-altos Butterworth 2do orden, fc = 0.5 Hz (estandar de monitoreo AHA). */
arm_biquad_casd_df1_inst_f32 hp_filter;
float32_t hp_state[4];
float32_t hp_coeffs[5] = { 0.991154f, -1.982307f, 0.991154f,
                           1.982229f, -0.982385f };

/* Pasa-bajos Butterworth 4to orden, fc = 40 Hz = 2 biquads en cascada.
   El de 2do orden atenuaba solo 4.5 dB en 50 Hz; este llega a ~10 dB
   y corta mucho mejor el EMG. */
arm_biquad_casd_df1_inst_f32 lp_filter;
float32_t lp_state[8];
float32_t lp_coeffs[10] = {
  0.130380f, 0.260760f, 0.130380f, 0.602063f, -0.123563f,  /* Q = 0.5412 */
  0.175407f, 0.350814f, 0.175407f, 0.809946f, -0.511573f   /* Q = 1.3066 */
};

float32_t dsp_a[DSP_BLOCK_SIZE];
float32_t dsp_b[DSP_BLOCK_SIZE];

TaskHandle_t         dspTaskHandle = NULL;
StreamBufferHandle_t ecgStream     = NULL;

volatile uint32_t dsp_overrun_count = 0;  /* bloques donde la tarea se atraso */
volatile uint8_t  ecg_lead_status   = 0;  /* 1 = electrodo desconectado */

/* Pasa-banda 5-15 Hz (Q = 0.866, f0 = 8.66 Hz) solo para el camino de
   deteccion. Es la banda donde el QRS domina sobre la onda T y el EMG. */
arm_biquad_casd_df1_inst_f32 qrs_bp_filter;
float32_t qrs_bp_state[4];
float32_t qrs_bp_coeffs[5] = { 0.110874f, 0.0f, -0.110874f,
                               1.736345f, -0.778251f };
float32_t qrs_bp[DSP_BLOCK_SIZE];

/* Estado del detector: persiste entre bloques. */
static float32_t qrs_x[5]             = {0};
static float32_t qrs_win[QRS_WIN_LEN] = {0};
static uint16_t  qrs_win_idx = 0;
static float32_t qrs_win_sum = 0.0f;
static float32_t qrs_spk = 0.0f;   /* estimador de pico de senal */
static float32_t qrs_npk = 0.0f;   /* estimador de pico de ruido */
static uint32_t  qrs_n         = 0;
static uint32_t  qrs_last_peak = 0;
static uint16_t  qrs_rr[QRS_RR_HIST] = {0};
static uint8_t   qrs_rr_idx = 0;
static uint8_t   qrs_rr_cnt = 0;

/* Historia de la senal filtrada para delinear el QRS hacia atras. */
static float32_t qrs_hist[QRS_HIST_LEN];
static uint16_t  qrs_hist_idx = 0;

/* Lectura publica para la pantalla. */
volatile uint16_t ecg_bpm   = 0;
volatile uint16_t ecg_rr_ms = 0;
volatile uint16_t ecg_qrs_ms = 0;   /* duracion del QRS en ms */
volatile uint8_t  ecg_regular = 1;
volatile float    ecg_r_mv    = 0.0f;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM3_Init(void);
void StartDefaultTask(void *argument);

/* USER CODE BEGIN PFP */
void vTask_DSP(void *pvParameters);
static void ECG_ProcessBlock(const uint16_t *pRaw, int16_t *pOut);
static void ECG_ResetFilters(void);
static void ECG_PlotBlock(const int16_t *p, uint16_t n);

static void ECG_DetectQRS(const float32_t *pDet, const float32_t *pRaw, uint16_t n);
static void ECG_ResetQRS(void);

static void LCD_Config(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART2_UART_Init();
  MX_ADC1_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
  /*Configo la pantalla*/
  LCD_Config();

  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */

  /* El StreamBuffer se crea antes que la tarea que lo escribe. */
  ecgStream = xStreamBufferCreate(DSP_BLOCK_SIZE * sizeof(int16_t) * 4,
                                  DSP_BLOCK_SIZE * sizeof(int16_t));


  configASSERT(ecgStream != NULL);
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* Prioridad 40 = osPriorityHigh. Antes era 55, la banda reservada a
     osPriorityISR, que dejaba al DSP por encima del daemon de timers. */
  configASSERT(xTaskCreate(vTask_DSP, "DSP", 512, NULL, 40, &dspTaskHandle) == pdPASS);
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
  hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T3_TRGO;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = ENABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_10;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 8399;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 4;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 921600;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA2_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, LCD_RST_Pin|LCD_D1_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, LCD_RD_Pin|LCD_WR_Pin|LCD_RS_Pin|LCD_D7_Pin
                          |LCD_D0_Pin|LCD_D2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LCD_CS_Pin|LCD_D6_Pin|LCD_D3_Pin|LCD_D5_Pin
                          |LCD_D4_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(SDN_GPIO_Port, SDN_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LCD_RST_Pin */
  GPIO_InitStruct.Pin = LCD_RST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(LCD_RST_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LCD_RD_Pin LCD_WR_Pin LCD_RS_Pin LCD_D7_Pin
                           LCD_D0_Pin LCD_D2_Pin */
  GPIO_InitStruct.Pin = LCD_RD_Pin|LCD_WR_Pin|LCD_RS_Pin|LCD_D7_Pin
                          |LCD_D0_Pin|LCD_D2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : LCD_CS_Pin LCD_D6_Pin */
  GPIO_InitStruct.Pin = LCD_CS_Pin|LCD_D6_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : LCD_D1_Pin */
  GPIO_InitStruct.Pin = LCD_D1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(LCD_D1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LO_P_Pin LO_N_Pin */
  GPIO_InitStruct.Pin = LO_P_Pin|LO_N_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : SDN_Pin */
  GPIO_InitStruct.Pin = SDN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(SDN_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LCD_D3_Pin LCD_D5_Pin LCD_D4_Pin */
  GPIO_InitStruct.Pin = LCD_D3_Pin|LCD_D5_Pin|LCD_D4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* Bus 8080 en reposo. CubeMX deja RD y CS en bajo, y RD bajo pone al
     panel a manejar el bus contra las salidas del micro. */
  HAL_GPIO_WritePin(GPIOA, LCD_RD_Pin | LCD_WR_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/* Configuracion inicial de la pantalla y del panel lateral. */
static void LCD_Config(void)
{
    ILI9341_Init();
    ILI9341_SetRotation(1);
    ILI9341_FillScreen(ILI9341_BLACK);

    ILI9341_FillRectangle(0, 0, PANEL_W, 240, ILI9341_BLUE);
    ILI9341_DrawVLine(PANEL_W - 1, 0, 240, ILI9341_WHITE);

    ILI9341_WriteString(8, 8, "ECG",
                        ILI9341_WHITE, ILI9341_BLUE, 2);

    ILI9341_WriteString(6, 42,  "FC",    ILI9341_CYAN,  ILI9341_BLUE, 1);
    ILI9341_WriteString(6, 78,  "BPM",   ILI9341_WHITE, ILI9341_BLUE, 1);
    ILI9341_WriteString(6, 96,  "R-R",   ILI9341_CYAN,  ILI9341_BLUE, 1);
    ILI9341_WriteString(6, 126, "QRS",   ILI9341_CYAN,  ILI9341_BLUE, 1);
    ILI9341_WriteString(6, 156, "R AMP", ILI9341_CYAN,  ILI9341_BLUE, 1);
    ILI9341_WriteString(6, 186, "RITMO", ILI9341_CYAN,  ILI9341_BLUE, 1);

    HAL_Delay(20);
}

/* ---- ETAPA 1: ESCRUTAR (callbacks del DMA) ---- */
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc) {
  BaseType_t xWoken = pdFALSE;
  xTaskNotifyFromISR(dspTaskHandle, NOTIF_HALF1, eSetBits, &xWoken);
  portYIELD_FROM_ISR(xWoken);
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
  BaseType_t xWoken = pdFALSE;
  xTaskNotifyFromISR(dspTaskHandle, NOTIF_HALF2, eSetBits, &xWoken);
  portYIELD_FROM_ISR(xWoken);
}

/* Limpia el estado de los biquads. Sin esto, al reconectar un electrodo
   el transitorio guardado en el pasa-altos de 0.5 Hz tarda segundos en irse. */
static void ECG_ResetFilters(void) {
  memset(notch_state, 0, sizeof(notch_state));
  memset(hp_state,    0, sizeof(hp_state));
  memset(lp_state,    0, sizeof(lp_state));

  memset(qrs_bp_state, 0, sizeof(qrs_bp_state));
  ECG_ResetQRS();
}

/* int16 -> ASCII a mano y una sola transmision por bloque.
   El printf por muestra hacia 50 llamadas bloqueantes; esto hace una. */
static void ECG_PlotBlock(const int16_t *p, uint16_t n) {
  static char txbuf[DSP_BLOCK_SIZE * 8];
  char *w = txbuf;

  for (uint16_t i = 0; i < n; i++) {
    int32_t v = p[i];
    char tmp[6];
    uint8_t k = 0;
    if (v < 0) { *w++ = '-'; v = -v; }
    do { tmp[k++] = (char)('0' + (v % 10)); v /= 10; } while (v);
    while (k) { *w++ = tmp[--k]; }
    *w++ = '\r'; *w++ = '\n';
  }
  /* Timeout acotado: un UART trabado no puede colgar la cadena entera. */
  HAL_UART_Transmit(&huart2, (uint8_t *)txbuf, (uint16_t)(w - txbuf), 50);
}

/* ---- ETAPA 2: PROCESAR ---- */
static void ECG_ResetQRS(void)
{
  memset(qrs_x,        0, sizeof(qrs_x));
  memset(qrs_win,      0, sizeof(qrs_win));
  memset(qrs_hist,     0, sizeof(qrs_hist));
  memset(qrs_bp_state, 0, sizeof(qrs_bp_state));
  qrs_win_idx  = 0;  qrs_win_sum = 0.0f;
  qrs_hist_idx = 0;
  qrs_spk = 0.0f;    qrs_npk = 0.0f;
  qrs_n   = 0;       qrs_last_peak = 0;
  qrs_rr_idx = 0;    qrs_rr_cnt = 0;
  ecg_bpm = 0;       ecg_rr_ms = 0;   ecg_qrs_ms = 0;
  ecg_regular = 1;   ecg_r_mv = 0.0f;
}

static void ECG_DetectQRS(const float32_t *pDet, const float32_t *pRaw, uint16_t n)
{
  for (uint16_t i = 0; i < n; i++)
  {
    /* La historia guarda la senal de display, no la de deteccion: el
       pasa-banda de 5-15 Hz ensancha el QRS y falsearia la duracion. */
    qrs_hist[qrs_hist_idx] = pRaw[i];
    if (++qrs_hist_idx >= QRS_HIST_LEN) { qrs_hist_idx = 0; }

    /* 1. Derivada de 5 puntos: realza la pendiente rapida del QRS. */
    qrs_x[4] = qrs_x[3];  qrs_x[3] = qrs_x[2];
    qrs_x[2] = qrs_x[1];  qrs_x[1] = qrs_x[0];
    qrs_x[0] = pDet[i];

    float32_t d = (2.0f * qrs_x[0] + qrs_x[1]
                 - qrs_x[3] - 2.0f * qrs_x[4]) * 0.125f;

    /* 2. Cuadrado. 3. Ventana movil de 150 ms con suma corrida: O(1). */
    float32_t sq = d * d;
    qrs_win_sum -= qrs_win[qrs_win_idx];
    qrs_win[qrs_win_idx] = sq;
    qrs_win_sum += sq;
    if (++qrs_win_idx >= QRS_WIN_LEN) { qrs_win_idx = 0; }

    float32_t integ = qrs_win_sum * (1.0f / (float32_t)QRS_WIN_LEN);
    qrs_n++;

    /* Fase de aprendizaje de 2 s: sin esto el umbral arranca en cero y
       dispara con cualquier cosa. */
    if (qrs_n < QRS_LEARN) {
      if (integ > qrs_spk) { qrs_spk = integ; }
      qrs_npk = 0.125f * integ + 0.875f * qrs_npk;
      continue;
    }

    /* 4. Umbral adaptativo entre el nivel de ruido y el de senal. */
    float32_t thr = qrs_npk + 0.25f * (qrs_spk - qrs_npk);

    if (integ <= thr) {
      qrs_npk = 0.125f * integ + 0.875f * qrs_npk;
      continue;
    }

    /* Refractario de 200 ms: la ventana se mantiene alta varias muestras
       y contaria el mismo QRS dos veces. */
    if ((qrs_n - qrs_last_peak) <= QRS_REFRACT) { continue; }

    uint32_t rr = qrs_n - qrs_last_peak;
    qrs_last_peak = qrs_n;
    qrs_spk = 0.125f * integ + 0.875f * qrs_spk;

    /* Frecuencia cardiaca. Descarta R-R fuera de 20-300 bpm. */
    if (rr >= (QRS_FS * 60u / 300u) && rr <= (QRS_FS * 60u / 20u)) {
      qrs_rr[qrs_rr_idx] = (uint16_t)rr;
      if (++qrs_rr_idx >= QRS_RR_HIST) { qrs_rr_idx = 0; }
      if (qrs_rr_cnt < QRS_RR_HIST)    { qrs_rr_cnt++; }

      uint32_t acc = 0;
      for (uint8_t k = 0; k < qrs_rr_cnt; k++) { acc += qrs_rr[k]; }
      float32_t rr_avg = (float32_t)acc / (float32_t)qrs_rr_cnt;

      ecg_rr_ms = (uint16_t)(rr_avg * (1000.0f / (float32_t)QRS_FS));
      ecg_bpm   = (uint16_t)((60.0f * (float32_t)QRS_FS) / rr_avg);

      /* Regularidad del ritmo: dispersion de los ultimos R-R.
         Mas del 12% de variacion se considera irregular. */
      uint16_t rmn = qrs_rr[0], rmx = qrs_rr[0];
      for (uint8_t k = 1; k < qrs_rr_cnt; k++) {
        if (qrs_rr[k] < rmn) { rmn = qrs_rr[k]; }
        if (qrs_rr[k] > rmx) { rmx = qrs_rr[k]; }
      }
      ecg_regular = (((float32_t)(rmx - rmn)) < (0.12f * rr_avg)) ? 1u : 0u;

    }

    /* Duracion del QRS. El detector dispara con retardo, asi que primero
       se localiza el pico R real: maximo absoluto 20-180 ms hacia atras. */
    int16_t   rb   = 5;
    float32_t rmax = 0.0f;
    for (int16_t b = 5; b < 45; b++) {
      float32_t a = fabsf(QRS_H(b));
      if (a > rmax) { rmax = a; rb = b; }
    }

    /* Amplitud del pico R en mV. */
    ecg_r_mv = rmax / ECG_COUNTS_PER_MV;

    float32_t dmax = 0.0f;
    for (int16_t b = rb - 12; b <= rb + 12; b++) {
      if (b < 0 || b > 100) { continue; }
      float32_t dd = fabsf(QRS_H(b) - QRS_H(b + 1));
      if (dd > dmax) { dmax = dd; }
    }

    if (dmax > 0.0f) {
      /* Inicio y fin del QRS = donde la pendiente cae al 15% de la maxima. */
      float32_t thr_d = 0.15f * dmax;
      int16_t on = rb, off = rb;

      for (int16_t b = rb; b < rb + 30 && b < 100; b++) {
        on = b;
        if (fabsf(QRS_H(b) - QRS_H(b + 1)) < thr_d) { break; }
      }
      for (int16_t b = rb; b > 1; b--) {
        off = b;
        if (fabsf(QRS_H(b) - QRS_H(b - 1)) < thr_d) { break; }
      }

      int16_t width = on - off;
      if (width >= 5 && width <= 50) {      /* 20-200 ms plausibles */
        ecg_qrs_ms = (uint16_t)((uint32_t)width * 1000u / QRS_FS);
      }
    }
  }
}

static void ECG_ProcessBlock(const uint16_t *pRaw, int16_t *pOut) {
  /* Lectura directa del IDR: leads-off no justifica una llamada a HAL. */
  if (ECG_LO_PORT->IDR & (ECG_LO_P_PIN | ECG_LO_N_PIN)) {
    ECG_ResetFilters();
    memset(pOut, 0, DSP_BLOCK_SIZE * sizeof(int16_t));
    ecg_lead_status = 1u;
  } else {
    ecg_lead_status = 0u;

    /* Decimacion por promedio: 8 muestras crudas -> 1 util.
           Se resta la continua ANTES de filtrar para que el pasa-altos no
           arranque con un escalon de 2048 counts. */
	for (uint16_t i = 0; i < DSP_BLOCK_SIZE; i++) {
	  const uint16_t *s = &pRaw[i * ADC_OSR];
	  uint32_t acc = 0;
	  for (uint8_t k = 0; k < ADC_OSR; k++) { acc += s[k]; }
	  dsp_a[i] = ((float32_t)acc * (1.0f / (float32_t)ADC_OSR)) - ECG_DC_OFFSET;
	}

	 /* HP primero: saca el residuo de continua antes de que el notch y el
	   pasa-bajos lo vean, asi ninguno de los tres arranca con un
	   transitorio grande. Al ser filtros LTI el orden no cambia la
	   respuesta en regimen, solo el arranque. */
	arm_biquad_cascade_df1_f32(&hp_filter,    dsp_a, dsp_b, DSP_BLOCK_SIZE);
	arm_biquad_cascade_df1_f32(&notch_filter, dsp_b, dsp_a, DSP_BLOCK_SIZE);
	arm_biquad_cascade_df1_f32(&lp_filter,    dsp_a, dsp_b, DSP_BLOCK_SIZE);

    /* Camino de deteccion, independiente del de display. */
    arm_biquad_cascade_df1_f32(&qrs_bp_filter, dsp_b, qrs_bp, DSP_BLOCK_SIZE);
    ECG_DetectQRS(qrs_bp, dsp_b, DSP_BLOCK_SIZE);

    for (uint16_t i = 0; i < DSP_BLOCK_SIZE; i++) {
      pOut[i] = (int16_t)dsp_b[i];
    }
  }

  /* Envio no bloqueante: si Display se atrasa, el DSP nunca se frena. */
  if (ecgStream != NULL) {
    xStreamBufferSend(ecgStream, pOut, DSP_BLOCK_SIZE * sizeof(int16_t), 0);
  }
  ECG_PlotBlock(pOut, DSP_BLOCK_SIZE);
}

void vTask_DSP(void *pvParameters) {
  uint32_t ulNotified;
  int16_t  out_block[DSP_BLOCK_SIZE];

  arm_biquad_cascade_df1_init_f32(&notch_filter, 1, notch_coeffs, notch_state);
  arm_biquad_cascade_df1_init_f32(&hp_filter,    1, hp_coeffs,    hp_state);
  arm_biquad_cascade_df1_init_f32(&lp_filter,    2, lp_coeffs,    lp_state);
  arm_biquad_cascade_df1_init_f32(&qrs_bp_filter, 1, qrs_bp_coeffs, qrs_bp_state);

  /* El hardware arranca aca: el scheduler ya corre y la tarea ya existe,
     asi que ninguna ISR puede notificar a un handle invalido. */
  HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_buffer, ADC_BUFFER_SIZE_N);
  HAL_TIM_Base_Start(&htim3);

  for (;;) {
    xTaskNotifyWait(0, 0xFFFFFFFFu, &ulNotified, portMAX_DELAY);

    /* Los dos bits juntos = la tarea perdio un bloque de tiempo real.
       Se procesan AMBAS mitades en orden cronologico (antes se descartaba
       la segunda) y se registra el evento. */
    if ((ulNotified & (NOTIF_HALF1 | NOTIF_HALF2))
                   == (NOTIF_HALF1 | NOTIF_HALF2)) {
      dsp_overrun_count++;
    }
    if (ulNotified & NOTIF_HALF1) {
      ECG_ProcessBlock(&adc_buffer[0], out_block);
    }
    if (ulNotified & NOTIF_HALF2) {
      ECG_ProcessBlock(&adc_buffer[ADC_HALF_SIZE], out_block);
    }
  }
}



/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
	 int16_t display_block[DSP_BLOCK_SIZE];

	uint16_t col   = 0;
	int16_t  yLast = GRAPH_Y + (GRAPH_H / 2);

	uint8_t  dec_cnt = 0;
	int16_t  gLo = 0, gHi = 0;

	for (;;)
	{
		size_t received = xStreamBufferReceive(
			ecgStream, display_block, sizeof(display_block), portMAX_DELAY);

		if (received != sizeof(display_block)) { continue; }

		/* Centrado lento sin tocar la ganancia: la grilla sigue
		   significando mV y ms. */
		int16_t bmin = display_block[0];
		int16_t bmax = display_block[0];
		for (uint16_t i = 1; i < DSP_BLOCK_SIZE; i++) {
			if (display_block[i] < bmin) { bmin = display_block[i]; }
			if (display_block[i] > bmax) { bmax = display_block[i]; }
		}

		static float32_t centro = 0.0f;
		centro += 0.02f * (0.5f * ((float32_t)bmax + (float32_t)bmin) - centro);

		const float32_t semi = ECG_FULLSCALE_MV * ECG_COUNTS_PER_MV;

		for (uint16_t i = 0; i < DSP_BLOCK_SIZE; i++)
		{
			/* SIN negar: el panel espeja el eje Y, asi que pasando el valor
			   directo la onda R queda hacia arriba. */
			int16_t y = ILI9341_GraphValueToY(
				(float32_t)display_block[i],
				centro - semi,
				centro + semi,
				GRAPH_Y,
				GRAPH_H
			);

			/* Decimacion por min/max: conserva el pico R en vez de
			   descartarlo como haria un salteo simple. */
			if (dec_cnt == 0u) { gLo = y; gHi = y; }
			else {
				if (y < gLo) { gLo = y; }
				if (y > gHi) { gHi = y; }
			}

			if (++dec_cnt < DISPLAY_DECIM) { continue; }
			dec_cnt = 0u;

			/* Se extiende el tramo hasta el ultimo y dibujado para que el
			   trazo quede continuo entre columnas. */
			int16_t lo = (gLo < yLast) ? gLo : yLast;
			int16_t hi = (gHi > yLast) ? gHi : yLast;
			yLast = y;

			uint8_t vgrid = ((col % 25u) == 0u) ? 2u
						  : (((col % 5u) == 0u) ? 1u : 0u);

#if SWEEP_REVERSED
			uint16_t xs = PANEL_W + (GRAPH_W - 1u) - col;
#else
			uint16_t xs = PANEL_W + col;
#endif

			ILI9341_DrawGraphColumn(xs, lo, hi, vgrid,
									ILI9341_GREEN, ECG_GRID_BOLD, ILI9341_BLACK);

			if (++col >= GRAPH_W) {
				col   = 0;
				yLast = y;
			}
		}

		/* Refresco de parametros ~3 veces por segundo. */
		static uint8_t ui_div = 0;
		if (++ui_div >= 3u) {
			ui_div = 0;

			ILI9341_UpdateValueFixed(6, 54, (float)ecg_bpm, 0, "", 3,
									 ILI9341_YELLOW, ILI9341_BLUE, 2);
			ILI9341_UpdateValueFixed(6, 108, (float)ecg_rr_ms, 0, "MS", 7,
									 ILI9341_YELLOW, ILI9341_BLUE, 1);
			ILI9341_UpdateValueFixed(6, 138, (float)ecg_qrs_ms, 0, "MS", 7,
									 ILI9341_YELLOW, ILI9341_BLUE, 1);
			ILI9341_UpdateValueFixed(6, 168, ecg_r_mv, 2, "MV", 7,
									 ILI9341_YELLOW, ILI9341_BLUE, 1);

			ILI9341_WriteString(6, 198,
								ecg_regular ? "REGULAR  " : "IRREGULAR",
								ecg_regular ? ILI9341_GREEN : ILI9341_YELLOW,
								ILI9341_BLUE, 1);

			ILI9341_WriteString(6, 220,
								ecg_lead_status ? "LEAD OFF" : "OK      ",
								ecg_lead_status ? ILI9341_RED : ILI9341_GREEN,
								ILI9341_BLUE, 1);
		}
	}
  /* USER CODE END 5 */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM4 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM4)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
