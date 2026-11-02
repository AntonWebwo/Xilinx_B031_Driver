/**
@file test_plug.c
@brief Скрипт для проверки наличия хеш-плат в коннекторах через новый драйвер
Компиляция:
arm-linux-gnueabihf-g++ -static -std=c++11 -lpthread test_plug.c zynq7007_fpga_driver.c -o test_plug
Использование:
sudo test_plug
*/
#include <cstdio>
#include "zynq7007_fpga_driver.h"

int main() {
    printf("================================================================================\n");
    printf(" HASH BOARD PLUG DETECTION TEST (Zynq 7007 Driver V9)\n");
    printf("================================================================================\n");

    int ret = fpga_driver_init();
    if (ret != 0) {
        fprintf(stderr, "ERROR: Failed to initialize FPGA driver (code: %d).\n", ret);
        fprintf(stderr, "Check if /dev/axi_fpga_dev exists and permissions are correct.\n");
        return 1;
    }

    printf("\n--- Scanning Connectors (Chain 0-3) ---\n");
    
    plug_status_t statuses[4];
    int count = fpga_scan_all_plugs(statuses);
    
    printf(" Detected boards: %d out of 4\n\n", count);

    for (int i = 0; i < 4; ++i) {
        const char* status_str = statuses[i].is_plugged ? "CONNECTED" : "DISCONNECTED";
        const char* icon = statuses[i].is_plugged ? "[+]" : "[ ]";
        printf(" %s Chain %d: %-12s\n", icon, i, status_str);
    }

    printf("\n--- Detailed Raw Status Check ---\n");
    for (int i = 0; i < 4; ++i) {
        plug_status_t single_status;
        ret = fpga_check_plug_status(i, &single_status);
        if (ret == 0) {
            printf(" Chain %d: Raw Bit Value = %d\n", i, single_status.is_plugged);
        } else {
            printf(" Chain %d: Read Error (code: %d)\n", i, ret);
        }
    }

    printf("\n================================================================================\n");
    
    fpga_driver_uninit();
    return 0;
}
