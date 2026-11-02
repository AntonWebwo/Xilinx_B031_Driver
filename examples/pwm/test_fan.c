/**
@file test_fan.c
@brief Универсальный тест управления и мониторинга вентиляторов через новый драйвер
Компиляция:
arm-linux-gnueabihf-g++ -static -std=c++11 -lpthread test_fan.c zynq7007_fpga_driver.c -o test_fan
Использование:
sudo test_fan --monitor          # Мониторинг оборотов (CTRL+C для выхода)
sudo test_fan --set <duty>       # Установить скорость всем вентиляторам (1-100)
sudo test_fan --set <group> <duty> # Установить скорость группе 0 (FAN0-1) или 1 (FAN2-3)
sudo test_fan --auto-test        # Запустить пошаговый тест 1-25-50-75-100%
*/
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <csignal>
#include <atomic>
#include "zynq7007_fpga_driver.h"

static std::atomic<bool> g_running(true);

void signal_handler(int sig) {
    (void)sig;
    g_running.store(false);
}

void print_usage(const char* prog) {
    printf("Usage:\n");
    printf("  %s --monitor                  : Monitor fan speeds in real-time\n", prog);
    printf("  %s --set <duty_cycle>         : Set all fans to <duty_cycle>%%\n", prog);
    printf("  %s --set <group> <duty_cycle> : Set group 0 (FAN0-1) or 1 (FAN2-3)\n", prog);
    printf("  %s --auto-test                : Run automated step test (0-100%%)\n", prog);
}

void monitor_loop() {
    const char* fan_names[] = {"FAN1 (Log 0)", "FAN2 (Log 1)", "FAN3 (Log 2)", "FAN4 (Log 3)"};
    
    printf("================================================================================\n");
    printf(" FAN SPEED MONITORING\n");
    printf(" Press CTRL+C to stop\n");
    printf("================================================================================\n");
    printf("%-20s | %-20s | %-20s | %-20s\n", fan_names[0], fan_names[1], fan_names[2], fan_names[3]);
    printf("--------------------------------------------------------------------------------\n");

    while (g_running.load()) {
        int32_t speeds[4];
        for (int i = 0; i < 4; ++i) {
            speeds[i] = fpga_fan_get_speed(static_cast<fan_id_e>(i));
        }
        // Используем \r для обновления строки на месте
        printf("\r%-20d | %-20d | %-20d | %-20d", speeds[0], speeds[1], speeds[2], speeds[3]);
        fflush(stdout);
        usleep(1000000); // 1 second update rate
    }
    printf("\nStopped.\n");
}

void auto_test() {
    printf("================================================================================\n");
    printf(" AUTOMATED FAN STEP TEST\n");
    printf("================================================================================\n");
    
    const int steps[] = {1, 25, 50, 75, 100};
    const char* fan_names[] = {"FAN1", "FAN2", "FAN3", "FAN4"};

    for (int s : steps) {
        printf("\n--- Setting speed to %d%% ---\n", s);
        
        // Устанавливаем скорость для всех логических каналов
        for (int i = 0; i < 4; ++i) {
            fpga_fan_set_speed(static_cast<fan_id_e>(i), static_cast<uint8_t>(s));
        }
        
        printf("Waiting for stabilization (5 sec)...\n");
        sleep(5);
        
        printf("Current RPM:\n");
        for (int i = 0; i < 4; ++i) {
            int32_t rpm = fpga_fan_get_speed(static_cast<fan_id_e>(i));
            printf("  %s: %d RPM\n", fan_names[i], rpm);
        }
    }
    
    printf("\nTurning off fans...\n");
    for (int i = 0; i < 4; ++i) {
        fpga_fan_set_speed(static_cast<fan_id_e>(i), 0);
    }
    printf("Test complete.\n");
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    if (fpga_driver_init() != 0) {
        fprintf(stderr, "ERROR: Failed to initialize FPGA driver.\n");
        return 1;
    }

    int ret = 0;

    if (strcmp(argv[1], "--monitor") == 0) {
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);
        monitor_loop();

    } else if (strcmp(argv[1], "--auto-test") == 0) {
        auto_test();

    } else if (strcmp(argv[1], "--set") == 0) {
        if (argc == 3) {
            // Format: --set <duty_cycle>
            int duty_cycle = atoi(argv[2]);
            if (duty_cycle < 0 || duty_cycle > 100) {
                fprintf(stderr, "ERROR: Duty cycle must be between 0 and 100.\n");
                ret = 1;
            } else {
                printf("Setting all fans to %d%%...\n", duty_cycle);
                for (int i = 0; i < 4; ++i) {
                    fpga_fan_set_speed(static_cast<fan_id_e>(i), static_cast<uint8_t>(duty_cycle));
                }
            }
        } else if (argc == 4) {
            // Format: --set <group> <duty_cycle>
            int group = atoi(argv[2]);
            int duty_cycle = atoi(argv[3]);
            
            if (duty_cycle < 0 || duty_cycle > 100) {
                fprintf(stderr, "ERROR: Duty cycle must be between 0 and 100.\n");
                ret = 1;
            } else if (group == 0) {
                printf("Setting FAN_CONTROL (FAN0, FAN1) to %d%%...\n", duty_cycle);
                fpga_fan_set_speed(FAN0, static_cast<uint8_t>(duty_cycle));
                fpga_fan_set_speed(FAN1, static_cast<uint8_t>(duty_cycle));
            } else if (group == 1) {
                printf("Setting FAN_CONTROL_1 (FAN2, FAN3) to %d%%...\n", duty_cycle);
                fpga_fan_set_speed(FAN2, static_cast<uint8_t>(duty_cycle));
                fpga_fan_set_speed(FAN3, static_cast<uint8_t>(duty_cycle));
            } else {
                fprintf(stderr, "ERROR: Group must be 0 or 1.\n");
                ret = 1;
            }
        } else {
            fprintf(stderr, "ERROR: Invalid arguments for --set.\n");
            ret = 1;
        }
    } else {
        fprintf(stderr, "ERROR: Unknown command '%s'.\n", argv[1]);
        print_usage(argv[0]);
        ret = 1;
    }

    fpga_driver_uninit();
    return ret;
}
