/**
@file test_fil_rx.cpp
@brief Тест приема ответов в режиме FIL
Логика строго из driver-btm-c5.c: get_nonce_and_register / check_asic_reg
Компиляция:
arm-linux-gnueabihf-g++ -static -std=c++11 -lpthread test_fil_rx.c zynq7007_fpga_driver.c -o test_fil_rx
Использование:
sudo test_fil_rx <chain_id> [timeout_sec]
*/
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <csignal>
#include <atomic>
#include <chrono> // <--- ДОБАВЛЕНО
#include "zynq7007_fpga_driver.h"

static std::atomic<bool> g_running(true);

void signal_handler(int sig) {
    (void)sig;
    g_running.store(false);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <chain_id> [timeout_sec]\n", argv[0]);
        return 1;
    }

    int chain = atoi(argv[1]);
    int timeout_sec = 10;
    if (argc > 2) timeout_sec = atoi(argv[2]);

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

    // Очистка FIFO перед началом
    fpga_uart_clear_fifo(chain);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    printf("================================================================================\n");
    printf(" FIL RX TEST (Listening on Chain %d)\n", chain);
    printf(" Press CTRL+C to stop or wait %d seconds\n", timeout_sec);
    printf("================================================================================\n");

    int response_count = 0;
    auto start_time = std::chrono::steady_clock::now();

    while (g_running.load()) {
        // Проверка таймаута
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count() >= timeout_sec) {
            printf("\nTimeout reached.\n");
            break;
        }

        uint32_t nonce_count = 0;
        fpga_reg_read(NONCE_NUMBER_IN_FIFO, &nonce_count);
        uint32_t num = nonce_count & 0xFFFF;

        if (num > 0) {
            for (uint32_t i = 0; i < num; i++) {
                uint32_t word0 = 0, word1 = 0;
                fpga_reg_read(RETURN_NONCE, &word0);
                fpga_reg_read(RETURN_NONCE_HI, &word1);

                uint8_t resp_chain = word0 & 0x0F;
                int is_nonce = (word0 & NONCE_INDICATOR) ? 1 : 0;

                // В режиме FIL ответы от регистров приходят без бита NONCE_INDICATOR
                if (resp_chain == chain && !is_nonce) {
                    response_count++;
                    // В FIL режиме word1 содержит значение регистра, а старшие биты word0 - CRC
                    uint8_t crc = (word0 >> 24) & 0x7F;
                    uint32_t reg_val = word1;
                    
                    printf("[#%03d] Chain: %d | RegVal: 0x%08X | CRC: 0x%02X\n", 
                           response_count, resp_chain, reg_val, crc);
                }
            }
        }
        usleep(10000); // 10ms poll interval
    }

    printf("\nStopped. Total responses: %d\n", response_count);
    
    // Проверка ошибок CRC на стороне FPGA
    uint32_t crc_err = 0;
    fpga_reg_read(CRC_ERROR_CNT, &crc_err);
    printf("FPGA CRC Error Count: %u\n", crc_err & 0xFFFF);

    fpga_driver_uninit();
    return 0;
}
