/** \file Filter.h
* \brief Digital Filter
* 
* Process the data from the tread A using a filter 
* Digital filter: moving average filter, with a window size of 10 samples. Removes the
* outliers (10% or high deviation from average) and computes the average of the remaining
* samples.
*
* \author Goncalo Moniz, Joao Oliveira, Diogo Leao
* \date 31-5-2022
* \bug There are no bugs
*/

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
//------------------FIFOS CONFIG
extern struct k_fifo fifo_ab;
extern struct k_fifo fifo_bc;
/* Create fifo data structure and variables */
struct data_item_t2 {
    void *fifo_reserved;    /* 1st word reserved for use by FIFO */
    uint16_t data;          /* Actual data */
};
#ifndef _FILTER_H
#define _FILTER_H


/**Thread B 
* 
* Thread for the Filter
* \author Goncalo Moniz, Joao Oliveira, Diogo Leao
* \param[in,out] 
* void
* void
* \return void
* \date 31-5-2022
*/
void thread_B_code(void *argA , void *argB, void *argC);

#endif