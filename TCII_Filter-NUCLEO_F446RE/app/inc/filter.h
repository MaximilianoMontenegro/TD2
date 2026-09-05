/*
===============================================================================
 Name        : filter.h
 Author      : Ing. Juan Manuel Cruz, Ing. Cesar Fuoco, Ing. Israel Pavelek
 Version     : 2.0
 Modified    : 9/29/2025
 Copyright   : $(copyright)
 Description : filter definition
===============================================================================
*/

#ifndef INC_FILTER_H_
#define INC_FILTER_H_

#include <stdbool.h>
#include "arm_math.h"

extern TIM_HandleTypeDef htim2;
extern UART_HandleTypeDef huart2;


typedef enum{
	TALKTHROUGH,
	FIR,
	IIR,
	NOTHING,
}filter_type_t;

#define CARGANDO_A false

#define float_filter

#define FIR_TAP_NUM 1

#define IIR_TAP_NUM 1

#define IIR_SOS_NUM 1

#define SAMPLE_RATE_1K	1000ul
#define SAMPLE_RATE_20K	20000ul
#define SAMPLE_RATE_40K	40000ul

#define SAMPLE_RATE	SAMPLE_RATE_1K

#define SAMPLES_PER_BLOCK 1024

extern int32_t fir_taps[];
extern int32_t iir_taps[];
extern float32_t float_fir_taps[];
extern float32_t float_iir_taps[];


typedef enum{
	NO_PROCESAR,
	PROCESAR_A,
	PROCESAR_B,
}estado_t;

#ifdef float_filter
	#define arm_fir_init arm_fir_init_f32
	#define arm_biquad_cascade_df1_init arm_biquad_cascade_df1_init_f32
	#define filter_bicuad_cascade arm_biquad_cascade_df1_f32
	#define arm_fir arm_fir_f32
#else
	#define arm_fir_init arm_fir_init_q31
	#define arm_biquad_cascade_df1_init arm_biquad_cascade_df1_init_q31
	#define filter_bicuad_cascade arm_biquad_cascade_df1_q31
	#define arm_fir arm_fir_q31
#endif

#endif /* INC_FILTER_H_ */



