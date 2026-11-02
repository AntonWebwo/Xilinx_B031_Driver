/**
@file test_i2c_read.c
@brief Тест чтения данных по I2C (PIC или EEPROM)
Компиляция:
arm-linux-gnueabihf-g++ -static -std=c++11 -lpthread test_i2c_read.c zynq7007_fpga_driver.c -o test_i2c_read
Использование:
sudo ./test_i2c_read -c <chain> -a <addr> -l <len> --mode <pic|eeprom> [-m <mem_addr>]
*/
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <getopt.h>
#include "zynq7007_fpga_driver.h"

void print_usage(const char* prog) {
    printf("Usage:\n");
    printf("  %s -c <chain> -a <addr> -l <len> --mode <pic|eeprom> [-m <mem_addr>]\n", prog);
    printf("  -c, --chain    : Chain ID (0-3)\n");
    printf("  -a, --addr     : Device 7-bit address (hex, e.g., 0x20)\n");
    printf("  -l, --len      : Number of bytes to read\n");
    printf("  -m, --mem-addr : Memory address for EEPROM mode (default 0)\n");
    printf("  --mode         : 'pic' or 'eeprom'\n");
}

int main(int argc, char* argv[]) {
    int chain = -1;
    uint8_t addr = 0;
    int len = 0;
    int mem_addr = 0;
    const char* mode_str = nullptr;

    static struct option long_options[] = {
        {"chain",    required_argument, 0, 'c'},
        {"addr",     required_argument, 0, 'a'},
        {"len",      required_argument, 0, 'l'},
        {"mem-addr", required_argument, 0, 'm'},
        {"mode",     required_argument, 0, 'M'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "c:a:l:m:M:", long_options, nullptr)) != -1) {
        switch (opt) {
            case 'c': chain = atoi(optarg); break;
            case 'a': addr = strtol(optarg, nullptr, 16); break;
            case 'l': len = atoi(optarg); break;
            case 'm': mem_addr = atoi(optarg); break;
            case 'M': mode_str = optarg; break;
            default: print_usage(argv[0]); return 1;
        }
    }

    if (chain < 0 || chain > 3 || addr == 0 || len <= 0 || !mode_str) {
        fprintf(stderr, "ERROR: Missing or invalid arguments.\n");
        print_usage(argv[0]);
        return 1;
    }

    bool is_eeprom = (strcmp(mode_str, "eeprom") == 0);
    bool is_pic = (strcmp(mode_str, "pic") == 0);

    if (!is_eeprom && !is_pic) {
        fprintf(stderr, "ERROR: Mode must be 'pic' or 'eeprom'.\n");
        return 1;
    }

    if (fpga_driver_init() != 0) {
        fprintf(stderr, "ERROR: Failed to initialize FPGA driver.\n");
        return 1;
    }

    printf("================================================================================\n");
    printf(" I2C READ TEST (%s MODE)\n", is_eeprom ? "EEPROM" : "PIC");
    printf("================================================================================\n");
    printf(" Chain      : %d\n", chain);
    printf(" Address    : 0x%02X\n", addr);
    printf(" Length     : %d bytes\n", len);
    if (is_eeprom) printf(" Mem Start  : 0x%02X\n", mem_addr);
    printf("--------------------------------------------------------------------------------\n");

    uint8_t* buffer = new uint8_t[len];
    memset(buffer, 0, len);

    for (int i = 0; i < len; ++i) {
        uint8_t val = 0;
        if (is_eeprom) {
            // EEPROM чтение требует указания адреса памяти
            val = eeprom_read_byte(chain, addr, static_cast<uint8_t>(mem_addr + i));
        } else {
            // PIC чтение просто считывает байт
            val = pic_read_byte(chain, addr);
        }
        buffer[i] = val;
        usleep(1000); // Небольшая задержка между операциями
    }

    printf(" Received   : ");
    for (int i = 0; i < len; ++i) {
        printf("%02X ", buffer[i]);
    }
    printf("\n================================================================================\n");

    delete[] buffer;
    fpga_driver_uninit();
    return 0;
}
