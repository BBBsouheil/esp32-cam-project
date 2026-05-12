#include <stdint.h>
#include <stdlib.h>
#include "nrf.h"

// ==========================
// Configuration
// ==========================

#define LED_PIN 20

#define I2C_SCL_PIN 0
#define I2C_SDA_PIN 1

#define MPU6050_ADDR          0x68
#define MPU6050_PWR_MGMT_1    0x6B
#define MPU6050_ACCEL_XOUT_H  0x3B

// Seuil de vibration
// Si trop sensible : augmente à 6000, 8000, 10000
// Si pas assez sensible : baisse à 3000, 2000
#define VIBRATION_THRESHOLD   4000

// ==========================
// Delay logiciel
// ==========================

void delay(volatile uint32_t count)
{
    while (count--)
    {
        __asm("nop");
    }
}

// ==========================
// LED
// ==========================

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

// ==========================
// MPU6050
// ==========================

void mpu6050_write_register(uint8_t reg, uint8_t value)
{
    i2c_start();

    i2c_write_byte((MPU6050_ADDR << 1) | 0);
    i2c_write_byte(reg);
    i2c_write_byte(value);

    i2c_stop();
}

void mpu6050_read_registers(uint8_t reg, uint8_t *buffer, uint8_t length)
{
    i2c_start();

    // Adresse MPU6050 en écriture
    i2c_write_byte((MPU6050_ADDR << 1) | 0);

    // Registre de départ
    i2c_write_byte(reg);

    // Restart
    i2c_start();

    // Adresse MPU6050 en lecture
    i2c_write_byte((MPU6050_ADDR << 1) | 1);

    for (uint8_t i = 0; i < length; i++)
    {
        if (i < length - 1)
        {
            buffer[i] = i2c_read_byte(1); // ACK
        }
        else
        {
            buffer[i] = i2c_read_byte(0); // NACK final
        }
    }

    i2c_stop();
}

void mpu6050_init(void)
{
    // Réveiller le MPU6050
    mpu6050_write_register(MPU6050_PWR_MGMT_1, 0x00);

    // Petite attente de stabilisation
    delay(1000000);
}

void mpu6050_read_accel(int16_t *ax, int16_t *ay, int16_t *az)
{
    uint8_t data[6];

    mpu6050_read_registers(MPU6050_ACCEL_XOUT_H, data, 6);

    *ax = (int16_t)((data[0] << 8) | data[1]);
    *ay = (int16_t)((data[2] << 8) | data[3]);
    *az = (int16_t)((data[4] << 8) | data[5]);
}

// ==========================
// Programme principal
// ==========================

int main(void)
{
    int16_t ax, ay, az;
    int16_t old_ax, old_ay, old_az;

    led_off();

    // Bus I2C au repos
    sda_high();
    scl_high();

    delay(1000000);

    // Initialisation MPU6050
    mpu6050_init();

    // Première mesure de référence
    mpu6050_read_accel(&old_ax, &old_ay, &old_az);

    while (1)
    {
        // Lire accélération actuelle
        mpu6050_read_accel(&ax, &ay, &az);

        // Calcul d'une variation globale
        int32_t variation =
            abs(ax - old_ax) +
            abs(ay - old_ay) +
            abs(az - old_az);

        // Si variation importante : vibration détectée
        if (variation > VIBRATION_THRESHOLD)
        {
            // LED allumée brièvement
            led_on();
            delay(3000000);
            led_off();

            // Anti-rebond : ignore les vibrations restantes
            delay(8000000);

            // Nouvelle référence après stabilisation
            mpu6050_read_accel(&old_ax, &old_ay, &old_az);
        }
        else
        {
            // Mise à jour normale de la référence
            old_ax = ax;
            old_ay = ay;
            old_az = az;
        }

        delay(500000);
    }
}