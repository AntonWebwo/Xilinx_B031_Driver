/**
@file test_uart_vil_tx.c
@brief Тест отправки данных по UART в режиме BC VIL (Строго по логике init_asic_stasic.cpp)
Компиляция:
arm-linux-gnueabihf-g++ -static -std=c++11 -lpthread test_uart_vil_tx.c zynq7007_fpga_driver.c -o test_uart_vil_tx
Использование:
sudo ./test_uart_vil_tx -c <chain> -x <hex_data>
Пример:
sudo ./test_uart_vil_tx -c 0 -x 5205000A
*/
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <getopt.h>
#include <vector>
#include "zynq7007_fpga_driver.h"

// Регистры FPGA (Индексы для V9/Zynq7007 согласно 7007_fpga.h / driver)
#define BC_COMMAND_BUFFER       28 // BC_COMMAND_BUFFER
#define BC_WRITE_COMMAND        26 // BC_WRITE_COMMAND
#define DHASH_ACC_CONTROL       35 // DHASH_ACC_CONTROL
#define REG_TIME_OUT_CONTROL    20 // TIME_OUT_CONTROL

// Биты управления
#define BC_COMMAND_BUFFER_READY (1 << 31)
#define BC_COMMAND_EN_CHAIN_ID  (1 << 23)
#define VIL_MODE                (1 << 15)
#define TIME_OUT_VALID          (1 << 31)

void print_usage(const char* prog) {
    printf("Usage:\n");
    printf("  %s -c <chain> -x <hex_data>\n", prog);
    printf("  -c, --chain : Chain ID (0-3)\n");
    printf("  -x, --hex   : Hex string of data to send\n");
}

int hex_to_bytes(const char* hex, uint8_t* bytes, int max_len) {
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

// Функция отправки, строго копирующая логику из init_asic_stasic.cpp
void send_raw_packet(int chain, const std::vector<uint8_t> &data) {
    unsigned int bc_buf[3] = {0, 0, 0};

    // Заполняем буфер побайтно (Big Endian внутри слов)
    for (size_t i = 0; i < data.size() && i < 12; ++i) {
        int word_idx = i / 4;
        int byte_pos = 3 - (i % 4);
        bc_buf[word_idx] |= ((unsigned int)data[i]) << (byte_pos * 8);
    }

    fpga_reg_write(BC_COMMAND_BUFFER, bc_buf[0]);
    fpga_reg_write(BC_COMMAND_BUFFER + 1, bc_buf[1]);
    fpga_reg_write(BC_COMMAND_BUFFER + 2, bc_buf[2]);

    uint32_t ret_val = 0;
    fpga_reg_read(BC_WRITE_COMMAND, &ret_val);
    
    // ИСПРАВЛЕНО: Корректная установка битов цепи [23:16] и маскировка
    uint32_t trigger_val = BC_COMMAND_BUFFER_READY | BC_COMMAND_EN_CHAIN_ID | ((chain & 0xFF) << 16) | (ret_val & 0xff00ffff);
    fpga_reg_write(BC_WRITE_COMMAND, trigger_val);
}

/**
 * @brief Установка таймаута (аналог set_time_out_control)
 */
void set_timeout(uint32_t val) {
    fpga_reg_write(REG_TIME_OUT_CONTROL, val | TIME_OUT_VALID);
    usleep(1000);
}

int main(int argc, char* argv[]) {
    int chain = 0;
    const char* hex_data = nullptr;

    static struct option long_options[] = {
        {"chain", required_argument, 0, 'c'},
        {"hex",   required_argument, 0, 'x'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "c:x:", long_options, nullptr)) != -1) {
        switch (opt) {
            case 'c': chain = atoi(optarg); break;
            case 'x': hex_data = optarg; break;
            default: print_usage(argv[0]); return 1;
        }
    }

    if (chain < 0 || chain > 3 || !hex_data) {
        fprintf(stderr, "ERROR: Missing or invalid arguments.\n");
        print_usage(argv[0]);
        return 1;
    }

    uint8_t raw_bytes[64];
    int raw_len = hex_to_bytes(hex_data, raw_bytes, sizeof(raw_bytes));
    if (raw_len <= 0) {
        fprintf(stderr, "ERROR: Invalid hex string.\n");
        return 1;
    }

    if (fpga_driver_init() != 0) {
        fprintf(stderr, "ERROR: Failed to initialize FPGA driver.\n");
        return 1;
    }

    printf("================================================================================\n");
    printf(" UART BC VIL TX TEST (Strictly from init_asic_stasic.cpp)\n");
    printf("================================================================================\n");
    printf(" Chain      : %d\n", chain);
    printf(" Data Len   : %d bytes\n", raw_len);
    printf(" Data (Hex) : %s\n", hex_data);
    printf("--------------------------------------------------------------------------------\n");

    // 1. Включаем режим передачи VIL
    uint32_t ctrl = 0;
    fpga_reg_read(DHASH_ACC_CONTROL, &ctrl);
    fpga_reg_write(DHASH_ACC_CONTROL, ctrl | VIL_MODE);
    usleep(1000);

    // 2. Устанавливаем таймаут перед началом работы
    set_timeout(0x1FFFF);

    // 3. Формируем вектор данных и отправляем
    std::vector<uint8_t> packet(raw_bytes, raw_bytes + raw_len);
    send_raw_packet(chain, packet);

    printf("SUCCESS: Packet sent to Chain %d\n", chain);
    printf("Waiting for ASIC response...\n");

    fpga_driver_uninit();
    return 0;
}
