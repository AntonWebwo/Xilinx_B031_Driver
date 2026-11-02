/**
@file test_uart_tx.c
@brief Прямая отправка данных по UART с выбором скорости
Компиляция:
arm-linux-gnueabihf-g++ -static -std=c++11 -lpthread test_uart_tx.cpp zynq7007_fpga_driver.c -o test_uart_tx
Использование:
sudo test_uart_tx <chain_id> <hex_data> [baud_index]
Пример (115200 bps, индекс 26):
sudo test_uart_tx 0 520500000A 26
*/
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include "zynq7007_fpga_driver.h"

int hex_to_bytes(const char *hex, uint8_t *bytes, int max_len) {
    int len = strlen(hex);
    if (len % 2 != 0) return -1;
    int byte_len = len / 2;
    if (byte_len > max_len) return -1;
    for (int i = 0; i < byte_len; i++) {
        unsigned int val;
        if (sscanf(hex + i * 2, "%2x", &val) != 1) return -1;
        bytes[i] = (uint8_t)val;
    }
    return byte_len;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <chain_id> <hex_data> [baud_index 0-26]\n", argv[0]);
        fprintf(stderr, "Note: baud_index 26 (0x1A) = 115200 bps\n");
        return 1;
    }

    int chain = atoi(argv[1]);
    const char *hex_str = argv[2];
    int baud_index = 26; // Default to 115200

    if (argc >= 4) {
        baud_index = atoi(argv[3]);
        if (baud_index < 0 || baud_index > 26) {
            fprintf(stderr, "Error: Baud index must be between 0 and 26\n");
            return 1;
        }
    }

    if (chain < 0 || chain > 3) {
        fprintf(stderr, "Error: Chain ID must be 0-3\n");
        return 1;
    }

    uint8_t data[256];
    int len = hex_to_bytes(hex_str, data, sizeof(data));
    if (len <= 0) {
        fprintf(stderr, "Error: Invalid hex string or empty data\n");
        return 1;
    }

    if (fpga_driver_init() != 0) {
        fprintf(stderr, "Error: Driver init failed\n");
        return 1;
    }

    // 1. Установка скорости через прямой доступ к регистру FPGA_BT8D
    printf("Setting UART Baud Index: %d (0x%02X) in FPGA_BT8D...\n", baud_index, baud_index);
    fpga_reg_write(FPGA_BT8D, (uint32_t)baud_index);
    usleep(10000);

    // 2. Инициализация таймаута (необходимо для работы UART контроллера)
    fpga_reg_write(TIME_OUT_CONTROL, 0x8000020C);
    usleep(10000);

    // 3. Включаем режим VIL, так как BC буфер (который мы используем для отправки) работает только в нем
    // Даже если мы шлем "сырые" данные, механизм доставки у FPGA завязан на этот бит
    uint32_t ctrl = 0;
    fpga_reg_read(DHASH_ACC_CONTROL, &ctrl);
    ctrl |= (1 << 15); // Set VIL_MODE
    fpga_reg_write(DHASH_ACC_CONTROL, ctrl);
    usleep(1000);

    printf("Sending %d bytes to Chain %d: %s\n", len, chain, hex_str);

    // 4. Отправка данных. 
    // Внимание: fpga_uart_bc_send требует минимум 4 байта и добавляет CRC.
    // Если нужно передать меньше 4 байт, дополняем нулями.
    uint8_t buf[4] = {0};
    memcpy(buf, data, len < 4 ? len : 4);
    
    // Используем проверенную функцию отправки через BC буфер
    int ret = fpga_uart_bc_send(chain, buf, 4);
    
    if (ret == 0) {
        printf("Done. Data sent via BC Buffer.\n");
    } else {
        fprintf(stderr, "Error: Failed to send data.\n");
    }

    fpga_driver_uninit();
    return 0;
}
