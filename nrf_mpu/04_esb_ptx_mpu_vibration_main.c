#include "nrf_esb.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "sdk_common.h"
#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_esb_error_codes.h"
#include "nrf_error.h"
#include "app_error.h"

// ==========================
// Configuration
// ==========================

#define LED_DEBUG_PIN 20

#define I2C_SCL_PIN 0
#define I2C_SDA_PIN 1

#define MPU6050_ADDR          0x68
#define MPU6050_PWR_MGMT_1    0x6B
#define MPU6050_ACCEL_XOUT_H  0x3B

#define VIBRATION_THRESHOLD   4000

// Paquet envoyé : "VIB"
static nrf_esb_payload_t tx_payload =
    NRF_ESB_CREATE_PAYLOAD(0, 'V', 'I', 'B', 0x01, 0x00, 0x00, 0x00, 0x00);

// ==========================
// Delay simple
// ==========================

void delay(volatile uint32_t count)
{
    while (count--)
    {
        __asm("nop");
    }
}

// ==========================
// LED debug émetteur
// ==========================

void led_on(void)
{
    nrf_gpio_cfg_output(LED_DEBUG_PIN);
    nrf_gpio_pin_set(LED_DEBUG_PIN);
}

void led_off(void)
{
    nrf_gpio_cfg_output(LED_DEBUG_PIN);
    nrf_gpio_pin_clear(LED_DEBUG_PIN);
}

void led_pulse(void)
{
    led_on();
    delay(1500000);
    led_off();
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
            sda_high();
        else
            sda_low();

        i2c_delay();
        scl_high();
        i2c_delay();
        scl_low();

        data <<= 1;
    }

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
        sda_low();
    else
        sda_high();

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

    i2c_write_byte((MPU6050_ADDR << 1) | 0);
    i2c_write_byte(reg);

    i2c_start();

    i2c_write_byte((MPU6050_ADDR << 1) | 1);

    for (uint8_t i = 0; i < length; i++)
    {
        if (i < length - 1)
            buffer[i] = i2c_read_byte(1);
        else
            buffer[i] = i2c_read_byte(0);
    }

    i2c_stop();
}

void mpu6050_init(void)
{
    mpu6050_write_register(MPU6050_PWR_MGMT_1, 0x00);
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
// Horloge radio
// ==========================

void clocks_start(void)
{
    NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;
    NRF_CLOCK->TASKS_HFCLKSTART = 1;

    while (NRF_CLOCK->EVENTS_HFCLKSTARTED == 0)
    {
    }
}

// ==========================
// ESB callback
// ==========================

void nrf_esb_event_handler(nrf_esb_evt_t const * p_event)
{
    switch (p_event->evt_id)
    {
        case NRF_ESB_EVENT_TX_SUCCESS:
            break;

        case NRF_ESB_EVENT_TX_FAILED:
            nrf_esb_flush_tx();
            break;

        case NRF_ESB_EVENT_RX_RECEIVED:
            break;

        default:
            break;
    }
}

// ==========================
// Initialisation ESB émetteur
// ==========================

uint32_t esb_init(void)
{
    uint32_t err_code;

    uint8_t base_addr_0[4] = {0xE7, 0xE7, 0xE7, 0xE7};
    uint8_t base_addr_1[4] = {0xC2, 0xC2, 0xC2, 0xC2};

    uint8_t addr_prefix[8] =
    {
        0xE7, 0xC2, 0xC3, 0xC4,
        0xC5, 0xC6, 0xC7, 0xC8
    };

    nrf_esb_config_t nrf_esb_config = NRF_ESB_DEFAULT_CONFIG;

    nrf_esb_config.payload_length     = 8;
    nrf_esb_config.protocol           = NRF_ESB_PROTOCOL_ESB_DPL;
    nrf_esb_config.bitrate            = NRF_ESB_BITRATE_2MBPS;
    nrf_esb_config.mode               = NRF_ESB_MODE_PTX;
    nrf_esb_config.event_handler      = nrf_esb_event_handler;
    nrf_esb_config.selective_auto_ack = false;

    err_code = nrf_esb_init(&nrf_esb_config);
    VERIFY_SUCCESS(err_code);

    err_code = nrf_esb_set_base_address_0(base_addr_0);
    VERIFY_SUCCESS(err_code);

    err_code = nrf_esb_set_base_address_1(base_addr_1);
    VERIFY_SUCCESS(err_code);

    err_code = nrf_esb_set_prefixes(addr_prefix, 8);
    VERIFY_SUCCESS(err_code);

    return NRF_SUCCESS;
}

// ==========================
// Envoi paquet vibration
// ==========================

void send_vibration_packet(void)
{
    static uint8_t counter = 0;

    tx_payload.data[3] = counter++;
    tx_payload.noack = false;

    uint32_t err_code = nrf_esb_write_payload(&tx_payload);

    if (err_code == NRF_SUCCESS)
    {
        led_pulse();
    }
    else
    {
        nrf_esb_flush_tx();
    }
}

// ==========================
// Main
// ==========================

int main(void)
{
    int16_t ax, ay, az;
    int16_t old_ax, old_ay, old_az;

    led_off();

    sda_high();
    scl_high();

    delay(1000000);

    clocks_start();

    uint32_t err_code = esb_init();
    APP_ERROR_CHECK(err_code);

    mpu6050_init();

    mpu6050_read_accel(&old_ax, &old_ay, &old_az);

    while (true)
    {
        mpu6050_read_accel(&ax, &ay, &az);

        int32_t variation =
            abs(ax - old_ax) +
            abs(ay - old_ay) +
            abs(az - old_az);

        if (variation > VIBRATION_THRESHOLD)
        {
            send_vibration_packet();

            delay(8000000);

            mpu6050_read_accel(&old_ax, &old_ay, &old_az);
        }
        else
        {
            old_ax = ax;
            old_ay = ay;
            old_az = az;
        }

        delay(500000);
    }
}