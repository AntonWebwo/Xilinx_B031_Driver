/**
@file test_vil_bc_tx.c
@brief Тест отправки пакета в UART в режиме VIL BC
Логика строго из driver-btm-c5.c: chain_inactive / set_BC_command_buffer
Компиляция:
arm-linux-gnueabihf-g++ -static -std=c++11 -lpthread test_vil_bc_tx.c zynq7007_fpga_driver.c -o test_vil_bc_tx
Использование:
sudo ./test_vil_bc_tx <chain_id> <hex_data>
*/
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include "zynq7007_fpga_driver.h"

// Биты управления из driver-btm-c5.h
#define VIL_MODE                (1 << 15)
#define BC_COMMAND_BUFFER_READY (1 << 31)
#define BC_COMMAND_EN_CHAIN_ID  (1 << 23)

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
        fprintf(stderr, "Usage: %s <chain_id> <hex_data>\n", argv[0]);
        return 1;
    }

    int chain = atoi(argv[1]);
    if (chain < 0 || chain > 3) {
        fprintf(stderr, "Error: Chain ID must be 0-3\n");
        return 1;
    }

    const char *hex_str = argv[2];
    uint8_t data[64];
    int len = hex_to_bytes(hex_str, data, sizeof(data));
    if (len < 0) {
        fprintf(stderr, "Error: Invalid hex string\n");
        return 1;
    }

    if (fpga_driver_init() != 0) {
        fprintf(stderr, "Error: Failed to initialize FPGA driver\n");
        return 1;
    }

    // Инициализация UART (скорость и таймауты)
    fpga_uart_full_init();

    // Строгая логика из driver-btm-c5: Установка режима VIL
    // В оригинале это делается через set_dhash_acc_control
    printf("Setting VIL Mode...\n");
    uint32_t ctrl = 0;
    fpga_reg_read(DHASH_ACC_CONTROL, &ctrl);
    // Устанавливаем бит VIL_MODE (15) и сбрасываем OPERATION_MODE (5)
    ctrl = (ctrl & ~(1 << 5)) | VIL_MODE; 
    fpga_reg_write(DHASH_ACC_CONTROL, ctrl);
    usleep(1000);

    printf("Sending VIL BC Packet to Chain %d...\n", chain);
    
    // Логика из driver-btm-c5: set_BC_command_buffer + set_BC_write_command
    // Формируем буфер из первых 4 байт данных + CRC5
    uint8_t buf[5];
    memcpy(buf, data, len < 4 ? len : 4);
    // Если данных меньше 4, дополняем нулями для расчета CRC (как в тестовых скриптах)
    if (len < 4) memset(buf + len, 0, 4 - len);
    
    // Расчет CRC5 для первых 4 байт (32 бита)
    uint8_t crc = bitmain_crc5(buf, 32);
    
    uint32_t cmd_buf[3] = {0};
    cmd_buf[0] = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
    cmd_buf[1] = crc << 24;
    cmd_buf[2] = 0;

    fpga_reg_write(BC_COMMAND_BUFFER, cmd_buf[0]);
    fpga_reg_write(BC_COMMAND_BUFFER_1, cmd_buf[1]);
    fpga_reg_write(BC_COMMAND_BUFFER_2, cmd_buf[2]);

    uint32_t ret_val = 0;
    fpga_reg_read(BC_WRITE_COMMAND, &ret_val);
    
    // Триггер отправки: BC_COMMAND_BUFFER_READY | BC_COMMAND_EN_CHAIN_ID | (chain << 16) | (old_val & 0xfff0ffff)
    uint32_t trigger = BC_COMMAND_BUFFER_READY | BC_COMMAND_EN_CHAIN_ID | ((chain & 0xFF) << 16) | (ret_val & 0xfff0ffff);
    fpga_reg_write(BC_WRITE_COMMAND, trigger);

    printf("Packet sent.\n");
    fpga_driver_uninit();
    return 0;
}
