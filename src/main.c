/*
 * Goncalo Moniz, 2022/05
 * Prof. Paulo Pedreiras
 * Assigmnent 4 - nRF52840DK_nRF52840 board
 * 
 * 
 * Periodically reads ADC input and prints the corresponding raw and voltage value in the console and updates de duty cycle
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

 /* Pin at which LED is connected. Addressing is direct (i.e., pin number)                  */
                /* Note 1: The PMW channel must be associated with the SAME pin in the DTS file            */
                /*         See the overlay file in this project to see how to change the assignment        */
                /*         *** Note: RUN CMAKE (Project -> Run Cmake) after editing the overlay file***    */
                /* Note 2: the pin can (and should) be obtained automatically from the DTS file.           */
                /*         I'm doing it manually to avoid entering in (cryptic) DT macros and to force     */ 
                /*         you to read the dts file.                                                       */
                /*         This line would do the trick: #define BOARDLED_PIN DT_PROP(PWM0_NID, ch0_pin)   */       


#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>
#include <drivers/adc.h>
#include <string.h>
#include <devicetree.h>
#include <sys/printk.h>
#include <sys/__assert.h>
#include <timing/timing.h>
#include <stdio.h>
#include <sys/printk.h>
#include <hal/nrf_saadc.h>

#define ADC_NID DT_NODELABEL(adc) 
#define ADC_RESOLUTION 10
#define ADC_GAIN ADC_GAIN_1_4
#define ADC_REFERENCE ADC_REF_VDD_1_4
#define ADC_ACQUISITION_TIME ADC_ACQ_TIME(ADC_ACQ_TIME_MICROSECONDS, 40)
#define ADC_CHANNEL_ID 1  



//-----------------ADC CONFIG


#define ADC_CHANNEL_INPUT NRF_SAADC_INPUT_AIN1 
#define BUFFER_SIZE 1
#define TIMER_INTERVAL_MSEC 50 /* Interval between ADC samples */

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


//----------------THREAD CONFIG


/* Size of stack area used by each thread (can be thread specific, if necessary)*/
#define STACK_SIZE 1024
/* Thread scheduling priority */
#define thread_A_prio 1
#define thread_B_prio 1
#define thread_C_prio 1
/* Therad periodicity (in ms)*/
#define thread_A_period 100
/* Create thread stack space */
K_THREAD_STACK_DEFINE(thread_A_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(thread_B_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(thread_C_stack, STACK_SIZE);
/* Create variables for thread data */
struct k_thread thread_A_data;
struct k_thread thread_B_data;
struct k_thread thread_C_data;
/* Create task IDs */
k_tid_t thread_A_tid;
k_tid_t thread_B_tid;
k_tid_t thread_C_tid;
/* Thread code prototypes */
void thread_A_code(void *, void *, void *);
void thread_B_code(void *, void *, void *);
void thread_C_code(void *, void *, void *);


//------------------FIFOS CONFIG


struct k_fifo fifo_ab;
struct k_fifo fifo_bc;
/* Create fifo data structure and variables */
struct data_item_t {
    void *fifo_reserved;    /* 1st word reserved for use by FIFO */
    uint16_t data;          /* Actual data */
};


//---------------PWM CONFIG



#define BOARDLED_PIN DT_PROP(PWM0_NID, ch0_pin)
#include <drivers/pwm.h>
/* Refer to dts file */
#define GPIO0_NID DT_NODELABEL(gpio0) 
#define PWM0_NID DT_NODELABEL(pwm0) 

/* Int related declarations */
static struct gpio_callback but1_cb_data; /* Callback structure */

/* Callback function and variables*/
volatile int dcToggleFlag = 0; /* Flag to signal a BUT1 press */



//----------------MAIN


void main(void)
{
 
    /* Create/Init fifos */
    k_fifo_init(&fifo_ab);
    k_fifo_init(&fifo_bc);
          
    /* Then create the task */
    thread_A_tid = k_thread_create(&thread_A_data, thread_A_stack,
    K_THREAD_STACK_SIZEOF(thread_A_stack), thread_A_code,
    NULL, NULL, NULL, thread_A_prio, 0, K_NO_WAIT);

    thread_B_tid = k_thread_create(&thread_B_data, thread_B_stack,
    K_THREAD_STACK_SIZEOF(thread_B_stack), thread_B_code,
    NULL, NULL, NULL, thread_B_prio, 0, K_NO_WAIT);

    thread_C_tid = k_thread_create(&thread_C_data, thread_C_stack,
    K_THREAD_STACK_SIZEOF(thread_C_stack), thread_C_code,
    NULL, NULL, NULL, thread_C_prio, 0, K_NO_WAIT);
}



void thread_A_code(void *argA , void *argB, void *argC)
 {
    /* Local vars */
    struct data_item_t data_ab;  
 
    /* Local vars */
    int64_t fin_time=0, release_time=0;     /* Timing variables to control task periodicity */    
                        
    int err=0;
    /* Task init code */
    printk("Thread A init (periodic)\n");

    /* Compute next release instant */
    release_time = k_uptime_get() + thread_A_period;
    
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


    /* Thread loop */
    /* Main loop */
    while(true)
    {
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
                data_ab.data = adc_sample_buffer[0] ;
                printk("adc reading: raw:%4u / %4u mV: \n\r",adc_sample_buffer[0],(uint16_t)(1000*adc_sample_buffer[0]*((float)3/1023)));
                k_fifo_put(&fifo_ab, &data_ab);
            }
        }
  
        /* Wait for next release instant */ 
        fin_time = k_uptime_get();
        if( fin_time < release_time) 
        {
            k_msleep(release_time - fin_time);
            release_time += thread_A_period;
        }

        /* Stop timing functions */
        timing_stop();
        }    
  }


void thread_B_code(void *argA , void *argB, void *argC)
{
    /* Local variables */
     
    long int nact = 0;
    struct data_item_t *data_ab;
    struct data_item_t data_bc;

    printk("Thread B init (sporadic, waits on a semaphore by task A)\n");
    while(1) {
        
        data_ab = k_fifo_get(&fifo_ab, K_FOREVER);

        data_bc.data = data_ab->data ;

        k_fifo_put(&fifo_bc, &data_bc);
                       
  }
}


void thread_C_code(void *argA , void *argB, void *argC)
{
    /* Local variables */
    long int nact = 0;
    int ret=0;      
    int duty=0;
    struct data_item_t *data_bc;
    const struct device *gpio0_dev;         /* Pointer to GPIO device structure */
    const struct device *pwm0_dev;          /* Pointer to PWM device structure */
    unsigned int pwmPeriod_us = 1000;       /* PWM priod in us */

/* Bind to GPIO 0 and PWM0 */
    gpio0_dev = device_get_binding(DT_LABEL(GPIO0_NID));
    if (gpio0_dev == NULL) {
        printk("Error: Failed to bind to GPIO0\n\r");        
	return;
    }
    else {
        printk("Bind to GPIO0 successfull \n\r");        
    }
    
    pwm0_dev = device_get_binding(DT_LABEL(PWM0_NID));
    if (pwm0_dev == NULL) {
	printk("Error: Failed to bind to PWM0\n r");
	return;
    }
    else  {
        printk("Bind to PWM0 successful\n\r");            
    }

    printk("Thread C init (sporadic, waits on a semaphore by task A)\n");
    while(1) {
        data_bc = k_fifo_get(&fifo_bc, K_FOREVER);
        duty=(uint16_t)(1000*(data_bc->data)*((float)3/1023))/30;
        printk("PWM DC value set to %u %%\n\n\r",duty);
        ret = pwm_pin_set_usec(pwm0_dev, BOARDLED_PIN,pwmPeriod_us,(unsigned int)((pwmPeriod_us*duty)/100), PWM_POLARITY_NORMAL);           
  }
}