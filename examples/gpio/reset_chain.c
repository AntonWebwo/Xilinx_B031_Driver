/**
 * @file reset_chain.c
 * @brief Утилита для управления сбросом хэш-цепей на базе Zynq7007 FPGA Driver (V9)
 * 
 * Компиляция:
 * arm-linux-gnueabihf-g++ -static -std=c++11 -o reset_chain reset_chain.c zynq7007_fpga_driver.c -lpthread
 */

#include "zynq7007_fpga_driver.h"
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <getopt.h>
#include <string>
#include <cstring>

void print_usage(const char *prog) {
    printf("Usage: %s -c <chain_id> [--ri <level>]\n", prog);
    printf("Options:\n");
    printf("  -c, --chain <id>     Chain ID to control (0-3 for S19/V9)\n");
    printf("  --ri <level>         Manual RI pin control (Active Low logic on PIC/ASIC side):\n");
    printf("                       '1' or 'UP'   - Release Reset (Write 0 to bit in RESET_HASH_BOARD)\n");
    printf("                       '0' or 'DOWN' - Assert Reset (Write 1 to bit in RESET_HASH_BOARD)\n");
    printf("                       If omitted, performs a standard hardware reset pulse via API.\n");
    printf("  -h, --help           Show this help\n");
}

int main(int argc, char *argv[]) {
    int chain = -1;
    std::string ri_arg;
    bool manual_ri = false;
    int ri_level = 0; // 0 = Down (Assert), 1 = Up (Release)

    static struct option long_options[] = {
        {"chain", required_argument, 0, 'c'},
        {"ri",    required_argument, 0, 'r'},
        {"help",  no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "c:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 'c':
                chain = atoi(optarg);
                break;
            case 'r': // --ri
                ri_arg = optarg;
                manual_ri = true;
                if (ri_arg == "1" || ri_arg == "UP" || ri_arg == "up") {
                    ri_level = 1;
                } else if (ri_arg == "0" || ri_arg == "DOWN" || ri_arg == "down") {
                    ri_level = 0;
                } else {
                    fprintf(stderr, "Error: Invalid RI level '%s'. Use 1/UP or 0/DOWN.\n", optarg);
                    return 1;
                }
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    // В платформе V9 обычно 4 цепи (0-3)
    if (chain < 0 || chain > 3) {
        fprintf(stderr, "Error: Invalid Chain ID. Must be 0-3 for V9 Platform.\n");
        print_usage(argv[0]);
        return 1;
    }

    // Инициализация драйвера
    if (fpga_driver_init() != 0) {
        fprintf(stderr, "Error: Failed to initialize FPGA driver. Check /dev/axi_fpga_dev and /dev/fpga_mem.\n");
        return 1;
    }

    uint32_t mask = (1 << chain);

    if (manual_ri) {
        // --- РУЧНОЕ УПРАВЛЕНИЕ ---
        // В V9 платформе бит в регистре RESET_HASH_BOARD установлен в 1 = Сброс активен (Low на пине)
        
        uint32_t current_reg = 0;
        if (fpga_reg_read(RESET_HASH_BOARD, &current_reg) != 0) {
            fprintf(stderr, "Error: Failed to read RESET_HASH_BOARD register.\n");
            fpga_driver_uninit();
            return 1;
        }

        if (ri_level == 1) {
            // UP: Снимаем сброс (Clear bit)
            uint32_t new_val = current_reg & ~mask; 
            
            printf("[INFO] Releasing Reset for Chain %d (RI -> HIGH)...\n", chain);
            fpga_reg_write(RESET_HASH_BOARD, new_val);
            printf("[INFO] RI is now UP. Board should start initializing.\n");
        } else {
            // DOWN: Активируем сброс (Set bit)
            uint32_t new_val = current_reg | mask;

            printf("[INFO] Asserting Reset for Chain %d (RI -> LOW)...\n", chain);
            fpga_reg_write(RESET_HASH_BOARD, new_val);
            printf("[INFO] RI is now DOWN. Board is held in reset state.\n");
        }
    } else {
        // --- АВТОМАТИЧЕСКИЙ АППАРАТНЫЙ СБРОС ЧЕРЕЗ API ---
        printf("[INFO] Starting automatic hardware reset sequence for Chain %d...\n", chain);
        
        // Используем строго реализованную функцию из драйвера
        // Она выполняет: Set Bit -> Wait 10ms -> Clear Bit -> Wait 50ms
        int res = fpga_reset_chain_hw(chain);
        
        if (res == 0) {
            printf("[INFO] Hardware reset pulse completed successfully.\n");
            
            // Дополнительная диагностика: проверка статуса подключения
            plug_status_t status;
            if (fpga_check_plug_status(chain, &status) == 0) {
                printf("[INFO] Plug Status for Chain %d: %s\n", 
                       chain, status.is_plugged ? "CONNECTED" : "DISCONNECTED");
            }
        } else {
            fprintf(stderr, "[ERROR] Hardware reset failed with code %d\n", res);
        }
    }

    fpga_driver_uninit();
    return 0;
}
