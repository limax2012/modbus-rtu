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

static const char *TAG = "MASTER";

// Modbus request to read a specific holding register
void send_modbus_request(uint16_t register_address) {
    uint8_t modbus_request[8];

    modbus_request[0] = 0x01; // Slave ID
    modbus_request[1] = 0x03; // Function Code (Read Holding Register)
    modbus_request[2] = (register_address >> 8) & 0xFF; // High byte of Register Address
    modbus_request[3] = register_address & 0xFF; // Low byte of Register Address
    modbus_request[4] = 0x00; // Number of registers to read (high byte)
    modbus_request[5] = 0x01; // Number of registers to read (low byte)
    modbus_request[6] = 0x00; // Placeholder CRC
    modbus_request[7] = 0x00; // Placeholder CRC

    uart_write_bytes(UART_NUM, (const char *)modbus_request, sizeof(modbus_request));
    ESP_LOGI(TAG, "Sent Modbus request for register %d.", register_address);
}

void receive_modbus_response() {
    uint8_t data[BUF_SIZE];
    int len = uart_read_bytes(UART_NUM, data, BUF_SIZE, pdMS_TO_TICKS(500));

    if (len > 0) {
        ESP_LOGI(TAG, "Received %d bytes:", len);
        for (int i = 0; i < len; i++) {
            printf("%02X ", data[i]);
        }
        printf("\n");

        if (len >= 7 && data[1] == 0x03) {
            uint16_t value = (data[3] << 8) | data[4];
            ESP_LOGI(TAG, "Holding Register Value: %d", value);
        }
    } else {
        ESP_LOGW(TAG, "No response received.");
    }
}

void master_task(void *arg) {
    vTaskDelay(pdMS_TO_TICKS(2000)); // Delay for stabilization
    uint16_t register_addresses[] = {0x0001, 0x0003, 0x0004}; // Registers to read
    int register_count = sizeof(register_addresses) / sizeof(register_addresses[0]);

    while (1) {
        for (int i = 0; i < register_count; i++) {
            send_modbus_request(register_addresses[i]);
            vTaskDelay(pdMS_TO_TICKS(500));
            receive_modbus_response();
            vTaskDelay(pdMS_TO_TICKS(2000)); // Wait before next request
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

    xTaskCreate(master_task, "master_task", 4096, NULL, 5, NULL);
}
