/**
@file test_version.c
@brief Скрипт для вывода версии FPGA через новый универсальный драйвер
Компиляция:
arm-linux-gnueabihf-g++ -static -std=c++11 -lpthread test_version.c zynq7007_fpga_driver.c -o test_version
Использование:
sudo test_version
*/
#include <cstdio>
#include "zynq7007_fpga_driver.h"

int main() {
    printf("================================================================================\n");
    printf(" FPGA VERSION CHECK (Zynq 7007 Driver V9)\n");
    printf("================================================================================\n");

    int ret = fpga_driver_init();
    if (ret != 0) {
        fprintf(stderr, "ERROR: Failed to initialize FPGA driver (code: %d).\n", ret);
        fprintf(stderr, "Check if /dev/axi_fpga_dev exists and permissions are correct.\n");
        return 1;
    }

    fpga_version_info_t info;
    ret = fpga_get_version(&info);
    
    if (ret == 0) {
        printf(" Version ID   : 0x%04X\n", info.version_id);
        printf(" Firmware     : %s\n", info.firmware_ver);
        
        if (info.version_id == 0xB031) {
            printf(" Platform     : V9 (Zynq 7007) [MATCH]\n");
            printf(" Status       : OK\n");
        } else {
            printf(" Platform     : Unknown / Legacy\n");
            printf(" Warning      : Expected 0xB031 for V9 platform\n");
        }
    } else {
        fprintf(stderr, "ERROR: Failed to read version information (code: %d).\n", ret);
    }

    printf("================================================================================\n");
    
    fpga_driver_uninit();
    return 0;
}
