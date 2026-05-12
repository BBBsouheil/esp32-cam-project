#include <stdint.h>
#include "nrf.h"

#define LED_PIN 20

void delay(volatile uint32_t count)
{
    while (count--)
    {
        __asm("nop");
    }
}

int main(void)
{
    // P0.20 en sortie
    NRF_GPIO->DIRSET = (1UL << LED_PIN);

    while (1)
    {
        // LED ON : P0.20 = 1
        NRF_GPIO->OUTSET = (1UL << LED_PIN);
        delay(1000000);

        // LED OFF : P0.20 = 0
        NRF_GPIO->OUTCLR = (1UL << LED_PIN);
        delay(1000000);
    }
}