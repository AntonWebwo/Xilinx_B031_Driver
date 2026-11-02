/**
@file test_uart_rx.c
@brief Прямой прием данных по UART с выбором скорости
Компиляция:
arm-linux-gnueabihf-g++ -static -std=c++11 -lpthread test_uart_rx.cpp zynq7007_fpga_driver.c -o test_uart_rx
Использование:
sudo test_uart_rx <chain_id> [timeout_sec] [baud_index 0-26]
Пример (115200 bps, 30 сек):
sudo test_uart_rx 0 30 26
*/
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <csignal>
#include <atomic>
#include <chrono>
#include "zynq7007_fpga_driver.h"

static std::atomic<bool> g_running(true);

void signal_handler(int sig) {
    (void)sig;
    g_running.store(false);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <chain_id> [timeout_sec] [baud_index 0-26]\n", argv[0]);
        fprintf(stderr, "Note: baud_index 26 (0x1A) = 115200 bps\n");
        return 1;
    }

    int chain = atoi(argv[1]);
    int timeout_sec = 10;
    int baud_index = 26; // Default to 115200

    if (argc > 2) timeout_sec = atoi(argv[2]);
    if (argc > 3) {
        baud_index = atoi(argv[3]);
        if (baud_index < 0 || baud_index > 26) {
            fprintf(stderr, "Error: Baud index must be between 0 and 26\n");
            return 1;
        }
    }

    if (chain < 0 || chain > 3) {
        fprintf(stderr, "Error: Chain ID must be 0-3\n");
        return 1;
    }

    if (fpga_driver_init() != 0) {
        fprintf(stderr, "Error: Driver init failed\n");
        return 1;
    }

    // 1. Установка скорости через прямой доступ к регистру FPGA_BT8D
    printf("Setting UART Baud Index: %d (0x%02X) in FPGA_BT8D...\n", baud_index, baud_index);
    fpga_reg_write(FPGA_BT8D, (uint32_t)baud_index);
    usleep(10000);
    
    // Настройка таймаута
    fpga_reg_write(TIME_OUT_CONTROL, 0x8000020C);
    usleep(10000);

    // Включаем режим VIL для корректной работы FIFO
    uint32_t ctrl = 0;
    fpga_reg_read(DHASH_ACC_CONTROL, &ctrl);
    ctrl |= (1 << 15); // Set VIL_MODE
    fpga_reg_write(DHASH_ACC_CONTROL, ctrl);
    usleep(1000);

    // Очистка FIFO
    fpga_uart_clear_fifo(chain);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    printf("================================================================================\n");
    printf(" UART RX TEST (Listening on Chain %d, Baud Index %d)\n", chain, baud_index);
    printf(" Press CTRL+C to stop or wait %d seconds\n", timeout_sec);
    printf("================================================================================\n");

    auto start_time = std::chrono::steady_clock::now();
    int total_packets = 0;

    while (g_running.load()) {
        // Проверка таймаута
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count() >= timeout_sec) {
            printf("\nTimeout reached.\n");
            break;
        }

        // Чтение количества слов в FIFO
        uint32_t nonce_count = 0;
        fpga_reg_read(NONCE_NUMBER_IN_FIFO, &nonce_count);
        uint32_t num = nonce_count & 0xFFFF;

        if (num > 0) {
            // В режиме Raw каждое слово (32 бита) содержит 4 байта данных
            char line_buf[128];
            int line_idx = 0;
            
            for (uint32_t i = 0; i < num; i++) {
                uint32_t word = 0;
                fpga_reg_read(RETURN_NONCE, &word);
                
                // Извлекаем 4 байта из слова (Big Endian)
                uint8_t b0 = (word >> 24) & 0xFF;
                uint8_t b1 = (word >> 16) & 0xFF;
                uint8_t b2 = (word >> 8) & 0xFF;
                uint8_t b3 = word & 0xFF;

                // Форматируем вывод
                line_idx += sprintf(line_buf + line_idx, "%02X %02X %02X %02X ", b0, b1, b2, b3);
                
                // Если накопили достаточно или конец пакета, выводим
                if (line_idx > 40) { 
                     printf("%s\n", line_buf);
                     line_idx = 0;
                     memset(line_buf, 0, sizeof(line_buf));
                }
            }
            
            if (line_idx > 0) {
                printf("%s\n", line_buf);
            }
            
            total_packets++;
        } else {
            usleep(10000); // 10ms poll
        }
    }

    printf("\nStopped. Total read cycles: %d\n", total_packets);
    fpga_driver_uninit();
    return 0;
}
