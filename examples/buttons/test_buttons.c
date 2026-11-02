/**
@file test_buttons.c
@brief Скрипт для проверки работы кнопок через новый драйвер
Компиляция:
arm-linux-gnueabihf-g++ -static -std=c++11 -lpthread test_buttons.c zynq7007_fpga_driver.c -o test_buttons
Использование:
sudo test_buttons
*/
#include <cstdio>
#include <unistd.h>
#include <csignal>
#include "zynq7007_fpga_driver.h"

static volatile bool g_running = true;

void signal_handler(int sig) {
    (void)sig;
    g_running = false;
}

int main() {
    printf("================================================================================\n");
    printf(" BUTTONS TEST (Zynq 7007 Driver V9)\n");
    printf("================================================================================\n");
    printf("Press buttons to see events. Press CTRL+C to exit.\n\n");

    if (fpga_driver_init() != 0) {
        fprintf(stderr, "ERROR: Failed to initialize FPGA driver.\n");
        return 1;
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Получаем начальное состояние
    button_state_t prev_state;
    if (fpga_read_buttons(&prev_state) != 0) {
        fprintf(stderr, "ERROR: Failed to read initial button state.\n");
        fpga_driver_uninit();
        return 1;
    }

    while (g_running) {
        button_state_t curr_state;
        if (fpga_read_buttons(&curr_state) == 0) {
            // Детектирование нажатия кнопки RESET (GPIO 921)
            // Логика: переход с высокого уровня (1) на низкий (0)
            if (prev_state.rst == 1 && curr_state.rst == 0) {
                printf("[EVENT] RESET button pressed\n");
            }
            
            // Детектирование нажатия кнопки IP REPORT (GPIO 943)
            // Логика: переход с высокого уровня (1) на низкий (0)
            if (prev_state.ip_report == 1 && curr_state.ip_report == 0) {
                printf("[EVENT] IP REPORT button pressed\n");
            }

            // Обновляем предыдущее состояние
            prev_state = curr_state;
        } else {
            fprintf(stderr, "WARNING: Failed to read button state.\n");
        }

        // Небольшая задержка для снижения нагрузки на CPU и обеспечения стабильности опроса
        usleep(50000); // 50ms
    }

    printf("\nExiting...\n");
    fpga_driver_uninit();
    return 0;
}
