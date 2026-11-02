/**
@file test_i2c_write.c
@brief Тест записи данных по I2C (PIC или EEPROM)
Компиляция:
arm-linux-gnueabihf-g++ -static -std=c++11 -lpthread test_i2c_write.c zynq7007_fpga_driver.c -o test_i2c_write
Использование:
sudo ./test_i2c_write -c <chain> -a <addr> -x <hex_data> --mode <pic|eeprom> [-m <mem_addr>]
*/
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <getopt.h>
#include "zynq7007_fpga_driver.h"

void print_usage(const char* prog) {
    printf("Usage:\n");
    printf("  %s -c <chain> -a <addr> -x <hex_data> --mode <pic|eeprom> [-m <mem_addr>]\n", prog);
    printf("  -c, --chain    : Chain ID (0-3)\n");
    printf("  -a, --addr     : Device 7-bit address (hex, e.g., 0x20)\n");
    printf("  -x, --hex      : Hex string of data to send (e.g., 55AA0407007B)\n");
    printf("  -m, --mem-addr : Starting memory address for EEPROM mode (default 0)\n");
    printf("  --mode         : 'pic' or 'eeprom'\n");
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

int main(int argc, char* argv[]) {
    int chain = -1;
    uint8_t addr = 0;
    const char* hex_data = nullptr;
    int mem_addr = 0;
    const char* mode_str = nullptr;

    static struct option long_options[] = {
        {"chain",    required_argument, 0, 'c'},
        {"addr",     required_argument, 0, 'a'},
        {"hex",      required_argument, 0, 'x'},
        {"mem-addr", required_argument, 0, 'm'},
        {"mode",     required_argument, 0, 'M'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "c:a:x:m:M:", long_options, nullptr)) != -1) {
        switch (opt) {
            case 'c': chain = atoi(optarg); break;
            case 'a': addr = strtol(optarg, nullptr, 16); break;
            case 'x': hex_data = optarg; break;
            case 'm': mem_addr = atoi(optarg); break;
            case 'M': mode_str = optarg; break;
            default: print_usage(argv[0]); return 1;
        }
    }

    if (chain < 0 || chain > 3 || addr == 0 || !hex_data || !mode_str) {
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

    uint8_t bytes[256];
    int len = hex_to_bytes(hex_data, bytes, sizeof(bytes));
    if (len <= 0) {
        fprintf(stderr, "ERROR: Invalid hex string.\n");
        return 1;
    }

    if (fpga_driver_init() != 0) {
        fprintf(stderr, "ERROR: Failed to initialize FPGA driver.\n");
        return 1;
    }

    printf("================================================================================\n");
    printf(" I2C WRITE TEST (%s MODE)\n", is_eeprom ? "EEPROM" : "PIC");
    printf("================================================================================\n");
    printf(" Chain      : %d\n", chain);
    printf(" Address    : 0x%02X\n", addr);
    printf(" Data Len   : %d bytes\n", len);
    printf(" Data       : %s\n", hex_data);
    if (is_eeprom) printf(" Mem Start  : 0x%02X\n", mem_addr);
    printf("--------------------------------------------------------------------------------\n");

    int success_count = 0;
    for (int i = 0; i < len; ++i) {
        bool res = false;
        if (is_eeprom) {
            // EEPROM запись требует указания адреса памяти
            res = eeprom_write_byte(chain, addr, static_cast<uint8_t>(mem_addr + i), bytes[i]);
        } else {
            // PIC отправка байта команды/данных
            res = pic_send_byte(chain, addr, bytes[i]);
        }
        
        if (res) {
            success_count++;
        } else {
            fprintf(stderr, "WARNING: Failed to write byte %d (0x%02X)\n", i, bytes[i]);
        }
        usleep(5000); // Задержка для стабильности шины
    }

    printf(" Result     : %d/%d bytes sent successfully.\n", success_count, len);
    printf("================================================================================\n");

    fpga_driver_uninit();
    return 0;
}
