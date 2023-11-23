#include "mcc_generated_files/mcc.h"

volatile bool s1_pressed, s2_pressed = false;
typedef enum 
{ 
    NORMAL, 
    CONFIG 
} mode;

void S1_ISR(void);
void S2_ISR(void);

void main(void)
{
    // initialize the device
    SYSTEM_Initialize();

    // Enable the Global Interrupts
    INTERRUPT_GlobalInterruptEnable();
    // Enable the Peripheral Interrupts
    INTERRUPT_PeripheralInterruptEnable();
    // Disable the Global Interrupts
    //INTERRUPT_GlobalInterruptDisable();
    // Disable the Peripheral Interrupts
    //INTERRUPT_PeripheralInterruptDisable();

    // Interrupt on Change (IOC) for switches
    IOCBF4_SetInterruptHandler(S1_ISR);
    IOCCF5_SetInterruptHandler(S2_ISR);
    
    while (1)
    {
        switch (mode)
        {
            case NORMAL:
                if (s1_pressed) 
                { 
                    if (alarm) 
                    {
                        clear_alarm();
                        mode = NORMAL;
                    }
                    else { mode = CONFIG; }
                }
                else if (s2_pressed) 
                { 
                    show_records();
                    mode = NORMAL;
                }
                break;
            
            case CONFIG:
                if (s1_pressed) 
                { 
                    move_cursor();
                    if (cursor_end) { mode = NORMAL; }
                    else { mode = CONFIG; }
                }
                else if (s2_pressed) 
                { 
                    modify_field();
                    mode = CONFIG;
                }
                break;
            
            //TODO: consider SLEEP()
        }
    }
}


//TODO: add deboucing

void S1_ISR(void)
{
    s1_pressed = true;
}

void S2_ISR(void)
{
    s2_pressed = true;
}