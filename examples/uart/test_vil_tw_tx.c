/**
@file test_vil_tw_tx.c
@brief Тест отправки пакета конфигурации VIL (TW mode logic) через BC буфер
Логика строго из driver-btm-c5.c: формирование пакета SET_CONFIG + CRC5
Компиляция:
arm-linux-gnueabihf-g++ -static -std=c++11 -lpthread test_vil_tw_tx.c zynq7007_fpga_driver.c -o test_vil_tw_tx
Использование:
sudo ./test_vil_tw_tx <chain_id> <reg_addr_hex> <data_hex>
*/
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include "zynq7007_fpga_driver.h"

// Константы
#define VIL_COMMAND_TYPE            (0x02 << 5) // 0x40
#define VIL_ALL                     (0x01 << 4) // 0x10
#define SET_CONFIG                  0x08
#define BC_COMMAND_BUFFER_READY     (1 << 31)
#define BC_COMMAND_EN_CHAIN_ID      (1 << 23)

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <chain_id> <reg_addr_hex> <data_hex>\n", argv[0]);
        return 1;
    }

    int chain = atoi(argv[1]);
    uint8_t reg_addr = strtol(argv[2], NULL, 16);
    uint32_t reg_data = strtol(argv[3], NULL, 16);

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

    // Установка режима VIL
    uint32_t ctrl = 0;
    fpga_reg_read(DHASH_ACC_CONTROL, &ctrl);
    ctrl |= (1 << 15); // VIL_MODE
    ctrl &= ~(1 << 5); // Сброс OPERATION_MODE
    fpga_reg_write(DHASH_ACC_CONTROL, ctrl);
    usleep(1000);

    printf("Sending VIL CONFIG Packet to Chain %d...\n", chain);
    printf("Register : 0x%02X\n", reg_addr);
    printf("Data     : 0x%08X\n", reg_data);

    // Формирование пакета VIL SET_CONFIG (9 байт)
    uint8_t pkt[9];
    memset(pkt, 0, sizeof(pkt));
    
    pkt[0] = VIL_COMMAND_TYPE | VIL_ALL | SET_CONFIG; // 0x58
    pkt[1] = 0x09;                                    // Length
    pkt[2] = 0x00;                                    // Chip Addr
    pkt[3] = reg_addr;                                // Register Address
    
    // Данные в Big Endian
    pkt[4] = (reg_data >> 24) & 0xFF;
    pkt[5] = (reg_data >> 16) & 0xFF;
    pkt[6] = (reg_data >> 8) & 0xFF;
    pkt[7] = reg_data & 0xFF;
    
    // Расчет CRC5 по первым 8 байтам (64 бита)
    // ВАЖНО: передаем 8 * 8 = 64 бита
    pkt[8] = bitmain_crc5(pkt, 64);

    printf("Packet   : ");
    for(int i=0; i<9; i++) printf("%02X ", pkt[i]);
    printf("\n");
    printf("CRC5 Calc: 0x%02X\n", pkt[8]);

    // Отправка через BC_COMMAND_BUFFER
    unsigned int bc_buf[3] = {0};
    
    bc_buf[0] = (pkt[0] << 24) | (pkt[1] << 16) | (pkt[2] << 8) | pkt[3];
    bc_buf[1] = (pkt[4] << 24) | (pkt[5] << 16) | (pkt[6] << 8) | pkt[7];
    bc_buf[2] = pkt[8] << 24;

    fpga_reg_write(BC_COMMAND_BUFFER, bc_buf[0]);
    fpga_reg_write(BC_COMMAND_BUFFER_1, bc_buf[1]);
    fpga_reg_write(BC_COMMAND_BUFFER_2, bc_buf[2]);

    // Триггер отправки
    uint32_t ret_val = 0;
    fpga_reg_read(BC_WRITE_COMMAND, &ret_val);
    
    uint32_t trigger = BC_COMMAND_BUFFER_READY | 
                       BC_COMMAND_EN_CHAIN_ID | 
                       ((chain & 0xFF) << 16) | 
                       (ret_val & 0xfff0ffff);
                       
    fpga_reg_write(BC_WRITE_COMMAND, trigger);

    printf("Success: Packet triggered on Chain %d\n", chain);
    
    fpga_driver_uninit();
    return 0;
}
