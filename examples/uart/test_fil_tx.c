/**
@file test_fil_tx.cpp
@brief Тест отправки пакета в UART в режиме FIL
Логика строго из driver-btm-c5.c: read_asic_register (FIL mode part)
Компиляция:
arm-linux-gnueabihf-g++ -static -std=c++11 -lpthread test_fil_tx.c zynq7007_fpga_driver.c -o test_fil_tx
Использование:
sudo test_fil_tx <chain_id> <cmd_hex> <addr_hex> <reg_hex>
Пример (GET_STATUS на все чипы, регистр 0x00):
sudo test_fil_tx 0 0x04 0x00 0x00
*/
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include "zynq7007_fpga_driver.h"

// Биты управления
#define BC_COMMAND_BUFFER_READY (1 << 31)
#define BC_COMMAND_EN_CHAIN_ID  (1 << 23)
#define COMMAND_FOR_ALL         0x80

int main(int argc, char *argv[]) {
    // Проверяем количество аргументов: prog chain cmd addr reg
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <chain_id> <cmd_hex> <addr_hex> <reg_hex>\n", argv[0]);
        fprintf(stderr, "Example: %s 0 0x04 0x00 0x00\n", argv[0]);
        return 1;
    }

    int chain = atoi(argv[1]);
    uint8_t cmd = strtol(argv[2], NULL, 16);
    uint8_t addr = strtol(argv[3], NULL, 16);
    uint8_t reg = strtol(argv[4], NULL, 16);

    if (chain < 0 || chain > 3) {
        fprintf(stderr, "Error: Chain ID must be 0-3\n");
        return 1;
    }

    if (fpga_driver_init() != 0) {
        fprintf(stderr, "Error: Failed to initialize FPGA driver\n");
        return 1;
    }

    // Инициализация UART
    fpga_uart_full_init();

    // Для FIL режима бит VIL_MODE должен быть сброшен
    printf("Setting FIL Mode...\n");
    uint32_t ctrl = 0;
    fpga_reg_read(DHASH_ACC_CONTROL, &ctrl);
    ctrl &= ~(1 << 15); // Clear VIL_MODE
    fpga_reg_write(DHASH_ACC_CONTROL, ctrl);
    usleep(1000);

    printf("Sending FIL Packet to Chain %d...\n", chain);
    printf("CMD: 0x%02X, ADDR: 0x%02X, REG: 0x%02X\n", cmd, addr, reg);

    // Логика из driver-btm-c5: FIL packet formation
    uint8_t buf[4];
    buf[0] = cmd;
    // В driver-btm-c5 для опроса всех чипов обычно ставится COMMAND_FOR_ALL
    buf[0] |= COMMAND_FOR_ALL; 
    
    buf[1] = addr;
    buf[2] = reg;
    
    // CRC5 считается по 27 битам (3 байта + 3 бита команды)
    // В driver-btm-c5: CRC5(buf, 4*8 - 5)
    buf[3] = bitmain_crc5(buf, 27); 

    printf("Packet Bytes: %02X %02X %02X %02X\n", buf[0], buf[1], buf[2], buf[3]);

    unsigned int bc_buf[3] = {0};
    for (int i = 0; i < 4; i++) {
        int word_idx = i / 4;
        int byte_pos = 3 - (i % 4);
        bc_buf[word_idx] |= ((unsigned int)buf[i]) << (byte_pos * 8);
    }

    fpga_reg_write(BC_COMMAND_BUFFER, bc_buf[0]);
    fpga_reg_write(BC_COMMAND_BUFFER_1, bc_buf[1]);
    fpga_reg_write(BC_COMMAND_BUFFER_2, bc_buf[2]);

    uint32_t ret_val = 0;
    fpga_reg_read(BC_WRITE_COMMAND, &ret_val);
    uint32_t trigger = BC_COMMAND_BUFFER_READY | BC_COMMAND_EN_CHAIN_ID | ((chain & 0xFF) << 16) | (ret_val & 0xfff0ffff);
    fpga_reg_write(BC_WRITE_COMMAND, trigger);

    printf("FIL Packet sent.\n");
    fpga_driver_uninit();
    return 0;
}
