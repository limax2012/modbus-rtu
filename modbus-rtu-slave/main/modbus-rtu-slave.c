#include <stdio.h>
#include <string.h>
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define UART_NUM UART_NUM_1
#define TXD_PIN 17
#define RXD_PIN 16
#define BUF_SIZE 256

static const char *TAG = "SLAVE";

// Simulated Modbus holding registers (values stored in memory)
uint16_t holding_registers[] = {12, 24, 36, 48, 60}; // Example values

void send_modbus_response(uint8_t slave_id, uint16_t register_value) {
    uint8_t response[7];

    response[0] = slave_id;
    response[1] = 0x03; // Function Code (Read Holding Register)
    response[2] = 0x02; // Byte count
    response[3] = (register_value >> 8) & 0xFF; // High byte
    response[4] = register_value & 0xFF; // Low byte
    response[5] = 0x00; // Placeholder CRC
    response[6] = 0x00; // Placeholder CRC

    uart_write_bytes(UART_NUM, (const char *)response, sizeof(response));
    ESP_LOGI(TAG, "Sent Modbus response with value: %d", register_value);
}

void process_modbus_request(uint8_t *data, int length) {
    if (length >= 8 && data[0] == 0x01 && data[1] == 0x03) { 
        uint16_t register_address = (data[2] << 8) | data[3]; 

        if (register_address < (sizeof(holding_registers) / sizeof(holding_registers[0]))) {
            send_modbus_response(data[0], holding_registers[register_address]);
        } else {
            ESP_LOGW(TAG, "Invalid register address: %d", register_address);
        }
    } else {
        ESP_LOGW(TAG, "Invalid or unrecognized Modbus request.");
    }
}

void slave_task(void *arg) {
    uint8_t data[BUF_SIZE];

    while (1) {
        int len = uart_read_bytes(UART_NUM, data, BUF_SIZE, pdMS_TO_TICKS(500));
        if (len > 0) {
            ESP_LOGI(TAG, "Received %d bytes:", len);
            for (int i = 0; i < len; i++) {
                printf("%02X ", data[i]);
            }
            printf("\n");

            process_modbus_request(data, len);
        }
    }
}

void app_main() {
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM, &uart_config);
    uart_set_pin(UART_NUM, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    xTaskCreate(slave_task, "slave_task", 4096, NULL, 5, NULL);
}
