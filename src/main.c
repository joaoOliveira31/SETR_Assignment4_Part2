/*
 * Paulo Pedreiras, 2022/03
 * Simple ADC example - nRF52840DK_nRF52840 board
 * 
 * 
 * Periodically reads ADC input and prints the corresponding raw and voltage value in the console
 * 
 * Adapted from: 
 *            https://devzone.nordicsemi.com/f/nordic-q-a/80685/using-saadc-in-nrf-connect-sdk/334204#334204
 *            https://github.com/simon-iversen/sdk-nrf/blob/light_controller/samples/light_controller/src/main.c
 * 
 *
 *      HW info
 *          https://infocenter.nordicsemi.com/topic/struct_nrf52/struct/nrf52840.html
 *          Section: nRF52840 Product Specification -> Peripherals -> GPIO / GPIOTE and SAADC 
 *          Board specific HW info can be found in the nRF52840_DK_User_Guide_20201203. I/O pins available at pg 27
 *
 *      Peripheral libs
 *          https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/zephyr/reference/peripherals/index.html
 *          ADC     
 *
 *      NOTE 1:     **** NEVER APPLY MORE THAN 3 V (VDD) or negative voltages to ANx ****
 *
 *      NOTE 2:     In this board analog inputs are AIN{1,2,4,5,6,7} (see nRF52840_DK_User_Guide_20201203, page 28)
 *                  Some of the examples found in the internet are set to AIN0 and so do not work. The PCB also has a label "A0", which refers to Arduino and induces in error. 
 *    
 *      NOTE 3:     must add "CONFIG_ADC=y" to prj.conf
 * 
 */

#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>
#include <drivers/adc.h>
#include <string.h>


/*ADC definitions and includes*/
#include <hal/nrf_saadc.h>
#define ADC_NID DT_NODELABEL(adc) 
#define ADC_RESOLUTION 10
#define ADC_GAIN ADC_GAIN_1_4
#define ADC_REFERENCE ADC_REF_VDD_1_4
#define ADC_ACQUISITION_TIME ADC_ACQ_TIME(ADC_ACQ_TIME_MICROSECONDS, 40)
#define ADC_CHANNEL_ID 1  

/* This is the actual nRF ANx input to use. Note that a channel can be assigned to any ANx. In fact a channel can */
/*    be assigned to two ANx, when differential reading is set (one ANx for the positive signal and the other one for the negative signal) */  
/* Note also that the configuration of differnt channels is completely independent (gain, resolution, ref voltage, ...) */
#define ADC_CHANNEL_INPUT NRF_SAADC_INPUT_AIN1 

#define BUFFER_SIZE 1

/* Other defines */
#define TIMER_INTERVAL_MSEC 1000 /* Interval between ADC samples */

/* ADC channel configuration */
static const struct adc_channel_cfg my_channel_cfg = {
	.gain = ADC_GAIN,
	.reference = ADC_REFERENCE,
	.acquisition_time = ADC_ACQUISITION_TIME,
	.channel_id = ADC_CHANNEL_ID,
	.input_positive = ADC_CHANNEL_INPUT
};

/* Global vars */
struct k_timer my_timer;
const struct device *adc_dev = NULL;
static uint16_t adc_sample_buffer[BUFFER_SIZE];

/* Takes one sample */
static int adc_sample(void)
{
	int ret;
	const struct adc_sequence sequence = {
		.channels = BIT(ADC_CHANNEL_ID),
		.buffer = adc_sample_buffer,
		.buffer_size = sizeof(adc_sample_buffer),
		.resolution = ADC_RESOLUTION,
	};

	if (adc_dev == NULL) {
            printk("adc_sample(): error, must bind to adc first \n\r");
            return -1;
	}

	ret = adc_read(adc_dev, &sequence);
	if (ret) {
            printk("adc_read() failed with code %d\n", ret);
	}	

	return ret;
}


void main(void)
{
    int err=0;

    /* Welcome message */
    printk("\n\r Simple adc demo for  \n\r");
    printk(" Reads an analog input connected to AN%d and prints its raw and mV value \n\r", ADC_CHANNEL_ID);
    printk(" *** ASSURE THAT ANx IS BETWEEN [0...3V]\n\r");
         
    /* ADC setup: bind and initialize */
    adc_dev = device_get_binding(DT_LABEL(ADC_NID));
	if (!adc_dev) {
        printk("ADC device_get_binding() failed\n");
    } 
    err = adc_channel_setup(adc_dev, &my_channel_cfg);
    if (err) {
        printk("adc_channel_setup() failed with error code %d\n", err);
    }
    
    /* It is recommended to calibrate the SAADC at least once before use, and whenever the ambient temperature has changed by more than 10 °C */
    NRF_SAADC->TASKS_CALIBRATEOFFSET = 1;
    
    /* Main loop */
    while(true){
        /* Get one sample, checks for errors and prints the values */
        err=adc_sample();
        if(err) {
            printk("adc_sample() failed with error code %d\n\r",err);
        }
        else {
            if(adc_sample_buffer[0] > 1023) {
                printk("adc reading out of range\n\r");
            }
            else {
                /* ADC is set to use gain of 1/4 and reference VDD/4, so input range is 0...VDD (3 V), with 10 bit resolution */
                printk("adc reading: raw:%4u / %4u mV: \n\r",adc_sample_buffer[0],(uint16_t)(1000*adc_sample_buffer[0]*((float)3/1023)));
            }
        }

        /* Sleep a while ... */
        k_msleep(TIMER_INTERVAL_MSEC);
    }
}
