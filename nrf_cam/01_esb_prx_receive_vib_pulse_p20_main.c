/**
 * Récepteur ESB nRF51822
 * Objectif :
 * - Le nRF51 récepteur écoute les paquets ESB.
 * - Quand un paquet est reçu, il allume la LED branchée sur P0.20.
 */

#include "nrf_esb.h"

#include <stdbool.h>
#include <stdint.h>

#include "sdk_common.h"
#include "nrf.h"
#include "nrf_esb_error_codes.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "nrf_error.h"

// Logs Nordic
#define NRF_LOG_MODULE_NAME "APP"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"

// ==========================
// Configuration LED
// ==========================

#define LED_RECEIVE_PIN 20

// Payload reçu
nrf_esb_payload_t rx_payload;

// ==========================
// Callback ESB
// Cette fonction est appelée automatiquement
// quand un événement radio se produit.
// ==========================

void nrf_esb_event_handler(nrf_esb_evt_t const * p_event)
{
    switch (p_event->evt_id)
    {
        case NRF_ESB_EVENT_TX_SUCCESS:
            NRF_LOG_DEBUG("TX SUCCESS EVENT\r\n");
            break;

        case NRF_ESB_EVENT_TX_FAILED:
            NRF_LOG_DEBUG("TX FAILED EVENT\r\n");
            break;

        case NRF_ESB_EVENT_RX_RECEIVED:
            NRF_LOG_DEBUG("RX RECEIVED EVENT\r\n");

            if (nrf_esb_read_rx_payload(&rx_payload) == NRF_SUCCESS)
            {
                if (rx_payload.data[0] == 'V' &&
                    rx_payload.data[1] == 'I' &&
                    rx_payload.data[2] == 'B')
                {
                    // Message vibration reçu : impulsion sur P0.20
                    nrf_gpio_pin_set(LED_RECEIVE_PIN);
                    nrf_delay_ms(500);
                    nrf_gpio_pin_clear(LED_RECEIVE_PIN);

                    NRF_LOG_DEBUG("VIB packet received\r\n");
                }
            }
            break;
        default:
            break;
    }
}

// ==========================
// Démarrage de l'horloge HF
// Obligatoire pour la radio
// ==========================

void clocks_start(void)
{
    NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;
    NRF_CLOCK->TASKS_HFCLKSTART = 1;

    while (NRF_CLOCK->EVENTS_HFCLKSTARTED == 0)
    {
        // Attente horloge HF
    }
}

// ==========================
// Initialisation GPIO
// ==========================

void gpio_init(void)
{
    // P0.20 en sortie
    nrf_gpio_cfg_output(LED_RECEIVE_PIN);

    // LED éteinte au démarrage
    nrf_gpio_pin_clear(LED_RECEIVE_PIN);
}

// ==========================
// Initialisation ESB en réception
// ==========================

uint32_t esb_init(void)
{
    uint32_t err_code;

    // Adresses identiques à l'exemple PTX Nordic
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
    nrf_esb_config.mode               = NRF_ESB_MODE_PRX;
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
// Programme principal
// ==========================

int main(void)
{
    uint32_t err_code;

    // Initialisation LED
    gpio_init();

    // Initialisation logs
    err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    // Démarrer l'horloge nécessaire à la radio
    clocks_start();

    // Initialiser ESB en mode récepteur
    err_code = esb_init();
    APP_ERROR_CHECK(err_code);

    NRF_LOG_DEBUG("ESB Receiver running\r\n");

    // Démarrer la réception radio
    err_code = nrf_esb_start_rx();
    APP_ERROR_CHECK(err_code);

    while (true)
    {
        if (NRF_LOG_PROCESS() == false)
        {
            __WFE();
        }
    }
}