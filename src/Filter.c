#include "Filter.h"

void thread_B_code(void *argA , void *argB, void *argC)
{
    struct data_item_t2 *data_ab;
    struct data_item_t2 data_bc;
    /* Local variables */
    uint16_t array[10],soma=0,media=0,media2=0;
    int cnt=1;
    int cnt2=0;
    printk("Thread B init (sporadic, waits on a semaphore by task A)\n");
    while(1) {
        data_ab = k_fifo_get(&fifo_ab, K_FOREVER);
        //filtro
       
       if(cnt<=10)//encher
       {
         array[cnt]=data_ab->data;;
         cnt++;
       }

       else
       {
           for(int i = 9 ; i > 0 ; i--) //fifo em array
           {
              array[i] = array[i-1];
           }
           array[0] = data_ab->data;

           for(int i=0;i<10;i++){//soma
              soma+=array[i];
           }
           printk("Soma = %4u\n",soma);         
           media=soma/10;//media 
           printk("Media = %4u\n",media); 
               
       
           uint16_t lim = media/10;
       
           for(int i=0;i<10;i++)
           {
              if(array[i] < (media-lim) || array[i] > (media+lim))
              {
                soma = soma - array[i];
                cnt2++;
              }
           }
          
     }
     if(cnt2==10)media2=0;
     else media2 = soma /(10-cnt2);
     data_bc.data = media2;     
     soma=0;
     cnt2=0;
     k_fifo_put(&fifo_bc, &data_bc);
       
  }
}