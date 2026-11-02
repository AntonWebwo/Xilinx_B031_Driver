/**
@file test_uart_vil_rx.c
@brief Тест приёма данных по UART в режиме BC VIL
Компиляция:
arm-linux-gnueabihf-g++ -static -std=c++11 -lpthread test_uart_vil_rx.c zynq7007_fpga_driver.c -o test_uart_vil_rx
Использование:
sudo ./test_uart_vil_rx -c <chain> [-t <timeout_ms>]
Пример:
sudo ./test_uart_vil_rx -c 0 -t 5000
*/
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <getopt.h>
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
    printf("  %s -c <chain> [-t <timeout_ms>]\n", prog);
    printf("  -c, --chain   : Chain ID (0-3)\n");
    printf("  -t, --timeout : Timeout per packet in ms (default 1000)\n");
}

int main(int argc, char* argv[]) {
    int chain = -1;
    int timeout = 1000;

    static struct option long_options[] = {
        {"chain",   required_argument, 0, 'c'},
        {"timeout", required_argument, 0, 't'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "c:t:", long_options, nullptr)) != -1) {
        switch (opt) {
            case 'c': chain = atoi(optarg); break;
            case 't': timeout = atoi(optarg); break;
            default: print_usage(argv[0]); return 1;
        }
    }

    if (chain < 0 || chain > 3) {
        fprintf(stderr, "ERROR: Chain must be 0-3.\n");
        print_usage(argv[0]);
        return 1;
    }

    if (fpga_driver_init() != 0) {
        fprintf(stderr, "ERROR: Failed to initialize FPGA driver.\n");
        return 1;
    }

    // Инициализация UART
    fpga_uart_full_init();
    
    // Очистка FIFO перед началом прослушки
    fpga_uart_clear_fifo(chain);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    printf("================================================================================\n");
    printf(" UART BC VIL RX TEST (Listening on Chain %d)\n", chain);
    printf(" Press CTRL+C to stop\n");
    printf("================================================================================\n");

    int response_count = 0;
    while (g_running.load()) {
        fpga_uart_rx_result_t result;
        int ret = fpga_uart_bc_receive(chain, &result, timeout);
        
        if (ret == 0) {
            response_count++;
            printf("[#%03d] Chain: %d | Value: 0x%08X | CRC: 0x%02X\n", 
                   response_count, result.chain_number, result.reg_value, result.crc);
        }
    }

    printf("\nStopped. Total responses: %d\n", response_count);
    fpga_driver_uninit();
    return 0;
}
