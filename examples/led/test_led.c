/**
@file test_led.c
@brief Универсальный тест управления LED через новый драйвер
Компиляция:
arm-linux-gnueabihf-g++ -static -std=c++11 -lpthread test_led.c zynq7007_fpga_driver.c -o test_led
Использование:
sudo test_led --red 1          # Включить красный
sudo test_led --green 0        # Выключить зеленый
sudo test_led --blink red 100 5 # Мигать красным 5 раз с периодом 100мс
sudo test_led --police         # Запустить "полицейский" режим (CTRL+C для выхода)
*/
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <csignal>
#include <pthread.h>
#include <ctime>
#include "zynq7007_fpga_driver.h"

// Глобальные переменные для многопоточного режима
static volatile bool g_police_running = false;
static pthread_t g_police_thread;

void signal_handler(int sig) {
    (void)sig;
    g_police_running = false;
}

// --- Логика "Полицейского" режима ---
void police_pattern() {
    fpga_led_control(0, 1); // Red ON
    fpga_led_control(1, 0); // Green OFF
    usleep(80000);          // 80ms
    fpga_led_control(0, 0); // Red OFF
    fpga_led_control(1, 1); // Green ON
    usleep(80000);          // 80ms
}

void accent_blink(int led_id, int count) {
    for (int i = 0; i < count; ++i) {
        if (!g_police_running) return;
        fpga_led_control(led_id, 1);
        usleep(60000);
        fpga_led_control(led_id, 0);
        usleep(60000);
    }
}

void* police_loop(void* arg) {
    (void)arg;
    srand(time(nullptr));
    while (g_police_running) {
        // Основной стробоскоп
        for (int i = 0; i < 4; ++i) {
            if (!g_police_running) break;
            police_pattern();
        }
        // Случайный акцент (30% шанс)
        if ((rand() % 10) < 3) {
            int accent_type = rand() % 2; 
            int flashes = 2 + (rand() % 3); 
            if (accent_type == 0) accent_blink(0, flashes); // Red
            else accent_blink(1, flashes);                  // Green
        }
    }
    // Гарантированное выключение при выходе
    fpga_led_control(0, 0);
    fpga_led_control(1, 0);
    return nullptr;
}

// --- Утилиты ---
void print_usage(const char* prog) {
    printf("Usage:\n");
    printf("  %s --red [0|1]             : Set Red LED state\n", prog);
    printf("  %s --green [0|1]           : Set Green LED state\n", prog);
    printf("  %s --blink <color> <ms> <n>: Blink color (red/green) n times with period ms\n", prog);
    printf("  %s --police                : Start dynamic police strobe mode\n", prog);
}

int get_led_id(const char* color) {
    if (strcmp(color, "red") == 0) return 0;
    if (strcmp(color, "green") == 0) return 1;
    return -1;
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

    // 1. Police Mode
    if (strcmp(argv[1], "--police") == 0) {
        printf("================================================================================\n");
        printf(" POLICE MODE ACTIVE\n");
        printf(" Press CTRL+C to stop\n");
        printf("================================================================================\n");
        
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);
        
        g_police_running = true;
        pthread_create(&g_police_thread, nullptr, police_loop, nullptr);
        pthread_join(g_police_thread, nullptr);
        
        printf("\nPolice mode deactivated.\n");

    } 
    // 2. Simple Blink Mode
    else if (strcmp(argv[1], "--blink") == 0) {
        if (argc < 5) {
            fprintf(stderr, "ERROR: --blink requires <color> <period_ms> <count>\n");
            ret = 1;
        } else {
            int led_id = get_led_id(argv[2]);
            int period = atoi(argv[3]);
            int count = atoi(argv[4]);
            
            if (led_id == -1 || period <= 0 || count <= 0) {
                fprintf(stderr, "ERROR: Invalid blink parameters.\n");
                ret = 1;
            } else {
                printf("Blinking %s LED %d times (period: %d ms)...\n", argv[2], count, period);
                fpga_led_blink(led_id, count, period);
            }
        }
    } 
    // 3. Direct Control (--red / --green)
    else {
        int led_id = -1;
        int state = -1;

        if (strcmp(argv[1], "--red") == 0) led_id = 0;
        else if (strcmp(argv[1], "--green") == 0) led_id = 1;
        else {
            fprintf(stderr, "ERROR: Unknown command '%s'. Use --red, --green, --blink or --police.\n", argv[1]);
            ret = 1;
        }

        if (led_id != -1) {
            if (argc < 3) {
                fprintf(stderr, "ERROR: Missing state value (0 or 1).\n");
                ret = 1;
            } else {
                state = atoi(argv[2]);
                if (state != 0 && state != 1) {
                    fprintf(stderr, "ERROR: State must be 0 (OFF) or 1 (ON).\n");
                    ret = 1;
                } else {
                    const char* color = (led_id == 0) ? "Red" : "Green";
                    const char* action = (state == 1) ? "ON" : "OFF";
                    printf("%s LED turned %s\n", color, action);
                    fpga_led_control(led_id, state);
                }
            }
        }
    }

    fpga_driver_uninit();
    return ret;
}
