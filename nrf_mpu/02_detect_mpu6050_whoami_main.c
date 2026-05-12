#include <stdint.h>
#include "nrf.h"

#define LED_PIN 20

#define I2C_SCL_PIN 0
#define I2C_SDA_PIN 1

#define MPU6050_ADDR 0x68
#define MPU6050_WHO_AM_I 0x75

void delay(volatile uint32_t count)
{
    while (count--)
    {
        __asm("nop");
    }
}

void led_on(void)
{
    NRF_GPIO->DIRSET = (1UL << LED_PIN);
    NRF_GPIO->OUTSET = (1UL << LED_PIN);
}

void led_off(void)
{
    NRF_GPIO->DIRSET = (1UL << LED_PIN);
    NRF_GPIO->OUTCLR = (1UL << LED_PIN);
}

void led_blink_error(void)
{
    while (1)
    {
        led_on();
        delay(300000);
        led_off();
        delay(300000);
    }
}

// ==========================
// I2C logiciel
// ==========================

void scl_high(void)
{
    NRF_GPIO->PIN_CNF[I2C_SCL_PIN] =
        (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
        (GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos);
}

void scl_low(void)
{
    NRF_GPIO->OUTCLR = (1UL << I2C_SCL_PIN);
    NRF_GPIO->DIRSET = (1UL << I2C_SCL_PIN);
}

void sda_high(void)
{
    NRF_GPIO->PIN_CNF[I2C_SDA_PIN] =
        (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
        (GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos);
}

void sda_low(void)
{
    NRF_GPIO->OUTCLR = (1UL << I2C_SDA_PIN);
    NRF_GPIO->DIRSET = (1UL << I2C_SDA_PIN);
}

uint32_t sda_read(void)
{
    return (NRF_GPIO->IN >> I2C_SDA_PIN) & 1UL;
}

void i2c_delay(void)
{
    delay(100);
}

void i2c_start(void)
{
    sda_high();
    scl_high();
    i2c_delay();

    sda_low();
    i2c_delay();

    scl_low();
    i2c_delay();
}

void i2c_stop(void)
{
    sda_low();
    i2c_delay();

    scl_high();
    i2c_delay();

    sda_high();
    i2c_delay();
}

uint8_t i2c_write_byte(uint8_t data)
{
    for (int i = 0; i < 8; i++)
    {
        if (data & 0x80)
        {
            sda_high();
        }
        else
        {
            sda_low();
        }

        i2c_delay();
        scl_high();
        i2c_delay();
        scl_low();

        data <<= 1;
    }

    // Lecture ACK
    sda_high();
    i2c_delay();

    scl_high();
    i2c_delay();

    uint8_t ack = sda_read();

    scl_low();
    i2c_delay();

    return ack;
}

uint8_t i2c_read_byte(uint8_t ack)
{
    uint8_t data = 0;

    sda_high();

    for (int i = 0; i < 8; i++)
    {
        data <<= 1;

        scl_high();
        i2c_delay();

        if (sda_read())
        {
            data |= 1;
        }

        scl_low();
        i2c_delay();
    }

    if (ack)
    {
        sda_low();
    }
    else
    {
        sda_high();
    }

    i2c_delay();
    scl_high();
    i2c_delay();
    scl_low();

    sda_high();

    return data;
}

uint8_t mpu6050_read_register(uint8_t reg)
{
    uint8_t value;

    i2c_start();

    // Adresse MPU6050 en écriture
    i2c_write_byte((MPU6050_ADDR << 1) | 0);

    // Registre à lire
    i2c_write_byte(reg);

    // Restart
    i2c_start();

    // Adresse MPU6050 en lecture
    i2c_write_byte((MPU6050_ADDR << 1) | 1);

    // Lecture sans ACK final
    value = i2c_read_byte(0);

    i2c_stop();

    return value;
}

int main(void)
{
    led_off();

    // Bus I2C au repos
    sda_high();
    scl_high();

    delay(1000000);

    uint8_t who = mpu6050_read_register(MPU6050_WHO_AM_I);

    if (who == 0x68)
    {
        // MPU6050 détecté
        led_on();

        while (1)
        {
        }
    }
    else
    {
        // Erreur : MPU6050 non détecté
        led_blink_error();
    }
}