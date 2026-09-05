/*
===============================================================================
 Name        : app.c
 Author      : Ing. Juan Manuel Cruz, Ing. Cesar Fuoco, Ing. Israel Pavelek
 Version     : 2.0
 Modified    : 9/29/2025
 Copyright   : $(copyright)
 Description : application definition
===============================================================================
*/
#include "main.h"
#include "filter.h"

/* Variables  ------------------------------------------------------------------*/
//Función que ejecuta > TALKTHROUGH / FIR / IIR
filter_type_t filter = TALKTHROUGH;

estado_t estado=NO_PROCESAR;
extern ADC_HandleTypeDef hadc1;
extern DAC_HandleTypeDef hdac;

//Filter Struct
arm_fir_instance_f32 SFIR;
arm_biquad_casd_df1_inst_f32 SIIR;

//Variables de estado
float32_t fir_state[FIR_TAP_NUM+SAMPLES_PER_BLOCK-1];
float32_t iir_state[IIR_TAP_NUM+SAMPLES_PER_BLOCK-1];

//Ping Pong Buffers
float32_t InputA[SAMPLES_PER_BLOCK]={0};
float32_t InputB[SAMPLES_PER_BLOCK]={0};
float32_t OutputA[SAMPLES_PER_BLOCK]={0};
float32_t OutputB[SAMPLES_PER_BLOCK]={0};

/* Filter Taps  ------------------------------------------------------------------*/
float32_t float_fir_taps[FIR_TAP_NUM] = {0};
float32_t float_iir_taps[IIR_TAP_NUM] = {0};

/* main loop ------------------------------------------------------------------*/

void app_init(void){
	/* Start adc & timer */
	HAL_ADC_Start_IT(&hadc1);
	HAL_TIM_Base_Start_IT(&htim2);
	HAL_DAC_Start(&hdac,DAC_CHANNEL_1);

  // Inicializa los filtros
	arm_fir_init_f32(&SFIR,FIR_TAP_NUM,float_fir_taps,fir_state,SAMPLES_PER_BLOCK);
	arm_biquad_cascade_df1_init_f32(&SIIR,IIR_SOS_NUM,float_iir_taps,iir_state);
}

void app_update(void){
	if(estado!=NO_PROCESAR){
		switch (filter){

			case TALKTHROUGH:
					for(uint16_t i=0;i<SAMPLES_PER_BLOCK;i++){
						if(estado==PROCESAR_A)OutputA[i]=InputA[i];
						else OutputB[i]=InputB[i];
					}
					break;

			case FIR:
					if(estado==PROCESAR_A){
						arm_fir_f32(&SFIR,InputA, OutputA, SAMPLES_PER_BLOCK);
					}else {
						arm_fir_f32(&SFIR,InputB, OutputB , SAMPLES_PER_BLOCK);
					}
					break;

			case IIR:
					if(estado==PROCESAR_A){
						arm_biquad_cascade_df1_f32(&SIIR, InputA, OutputA, SAMPLES_PER_BLOCK);
					}else {
						arm_biquad_cascade_df1_f32(&SIIR, InputB, OutputB, SAMPLES_PER_BLOCK);
						}
					break;

			default:break;

		}
		estado=NO_PROCESAR;
	}
}

//ADC Callback
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc){
	static bool estadoADC = CARGANDO_A;
	static uint16_t index = 0;
	uint32_t val=HAL_ADC_GetValue(&hadc1);

	if (estadoADC==CARGANDO_A){
		InputA[index] =(float32_t)val;
		InputA[index] = InputA[index] - 2048;
		HAL_DAC_SetValue(&hdac,DAC_CHANNEL_1,DAC_ALIGN_12B_R,OutputA[index]+2048);

	}
	else {
		InputB[index] =(float32_t) val ;
		InputB[index] = InputB[index] - 2048;
		HAL_DAC_SetValue(&hdac,DAC_CHANNEL_1,DAC_ALIGN_12B_R,OutputB[index]+2048);
	}
	index++;
	if (index == SAMPLES_PER_BLOCK) {
		index = 0;
		if(estadoADC==CARGANDO_A)
			estado=PROCESAR_A;
		else
			estado=PROCESAR_B;
		estadoADC ^= 1;
	}
    //HAL_GPIO_TogglePin(Test_GPIO_Port, Test_Pin);
}




