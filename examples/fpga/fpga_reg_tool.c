/**
@file fpga_reg_tool.c
@brief Работа с регистрами AXI FPGA чтение и запись
Компиляция:
arm-linux-gnueabihf-g++ -static -std=c++11 -lpthread fpga_reg_tool.c zynq7007_fpga_driver.c -o fpga_reg_tool
Использование: fpga_reg_tool <command> [arguments]
Команды:
  dump                  - Read and display FPGA registers (0-115)
  read <reg>            - Read specific register (dec or 0xhex)
  write <reg> <value>   - Write value to specific register
  help                  - Show this help message

Примеры:
  fpga_reg_tool dump
  fpga_reg_tool read 15
  fpga_reg_tool read 0x0F
  fpga_reg_tool write 15 0x1A
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include "zynq7007_fpga_driver.h"

// Массив имен регистров (только для используемых 0-115)
static const char *fpga_reg_names[] = {
    "HARDWARE_VERSION", "FAN_SPEED", "HASH_ON_PLUG", "WORK_FIFO_STATE",
    "RETURN_NONCE", "RETURN_NONCE_HI", "NONCE_NUMBER_IN_FIFO", "NONCE_FIFO_INTERRUPT",
    "TEMPERATURE_0_3", "TEMPERATURE_4_7", "TEMPERATURE_8_11", "TEMPERATURE_12_15",
    "IIC_COMMAND", "RESET_HASH_BOARD", "BMC_COUNTER", "FPGA_BT8D",
    "TW_WRITE_COMMAND", "QN_WRITE_DATA_COMMAND", "RESET_CHAIN", "FAN_CONTROL",
    "TIME_OUT_CONTROL", "TICKET_MASK_FPGA", "HASH_COUNTING_NUMBER_FPGA", "FAN_CONTROL_1",
    "SNO", "SOCKET_ID", "BC_WRITE_COMMAND", "FRIST_WORD",
    "BC_COMMAND_BUFFER", "BC_COMMAND_BUFFER_1", "BC_COMMAND_BUFFER_2", "FPGA_CHIP_ID_ADDR",
    "FPGA_CHIP_ID_HI", "CRC_ERROR_CNT", "I2C_SW_ID", "DHASH_ACC_CONTROL",
    "COINBASE_AND_NONCE2_LENGTH", "WORK_NONCE_2", "WORK_NONCE_2_HI", "NONCE2_AND_JOBID_STORE_ADDRESS",
    "MERKLE_BIN_NUMBER", "JOB_START_ADDRESS", "JOB_LENGTH", "JOB_DATA_READY",
    "JOB_ID", "WORK_ENABLE", "BLOCK_HEADER_VERSION", "TIME_STAMP",
    "TARGET_BITS", "PRE_HEADER_HASH", "PRE_HEADER_HASH1", "PRE_HEADER_HASH2",
    "PRE_HEADER_HASH3", "PRE_HEADER_HASH4", "PRE_HEADER_HASH5", "PRE_HEADER_HASH6",
    "PRE_HEADER_HASH7", "BLOCK_VERSION_0", "BLOCK_VERSION_1", "BLOCK_VERSION_2",
    "BLOCK_VERSION_3", "BLOCK_VERSION_4", "BLOCK_VERSION_5", "BLOCK_VERSION_6",
    "BLOCK_VERSION_7", "CHAIN0_3_SEND_FIFO_STATUS", "CHAIN4_7_SEND_FIFO_STATUS", "CHAIN8_11_SEND_FIFO_STATUS",
    "CHAIN12_13_SEND_FIFO_STATUS", "CHAIN0_SEND_READY", "CHAIN0_SEND_BUFFER", "CHAIN1_SEND_READY",
    "CHAIN1_SEND_BUFFER", "CHAIN2_SEND_READY", "CHAIN2_SEND_BUFFER", "CHAIN3_SEND_READY",
    "CHAIN3_SEND_BUFFER", "CHAIN4_SEND_READY", "CHAIN4_SEND_BUFFER", "CHAIN5_SEND_READY",
    "CHAIN5_SEND_BUFFER", "CHAIN6_SEND_READY", "CHAIN6_SEND_BUFFER", "CHAIN7_SEND_READY",
    "CHAIN7_SEND_BUFFER", "CHAIN8_SEND_READY", "CHAIN8_SEND_BUFFER", "CHAIN9_SEND_READY",
    "CHAIN9_SEND_BUFFER", "RECEIVE_FIFO_0_1_STATUS", "RECEIVE_FIFO_2_3_STATUS", "RECEIVE_FIFO_4_5_STATUS",
    "RECEIVE_FIFO_6_7_STATUS", "RECEIVE_FIFO_8_9_STATUS", "RECEIVE_FIFO_10_11_STATUS", "RECEIVE_FIFO_12_13_STATUS",
    "CHAIN0_READ_ENABLE", "CHAIN0_READ_DATA", "CHAIN1_READ_ENABLE", "CHAIN1_READ_DATA",
    "CHAIN2_READ_ENABLE", "CHAIN2_READ_DATA", "CHAIN3_READ_ENABLE", "CHAIN3_READ_DATA",
    "CHAIN4_READ_ENABLE", "CHAIN4_READ_DATA", "CHAIN5_READ_ENABLE", "CHAIN5_READ_DATA",
    "CHAIN6_READ_ENABLE", "CHAIN6_READ_DATA", "CHAIN7_READ_ENABLE", "CHAIN7_READ_DATA",
    "CHAIN8_READ_ENABLE", "CHAIN8_READ_DATA", "CHAIN9_READ_ENABLE", "CHAIN9_READ_DATA",
    "CHAIN10_SEND_READY", "CHAIN10_SEND_BUFFER", "CHAIN11_SEND_READY", "CHAIN11_SEND_BUFFER",
    "CHAIN12_SEND_READY", "CHAIN12_SEND_BUFFER", "CHAIN13_SEND_READY", "CHAIN13_SEND_BUFFER",
    "CHAIN10_READ_ENABLE", "CHAIN10_READ_DATA", "CHAIN11_READ_ENABLE", "CHAIN11_READ_DATA",
    "CHAIN12_READ_ENABLE", "CHAIN12_READ_DATA", "CHAIN13_READ_ENABLE", "CHAIN13_READ_DATA"
};

#define DUMP_MAX_REG 115

// Парсинг числа из строки (поддержка десятичного и 0xHEX форматов)
static uint32_t parse_value(const char *str) {
    uint32_t val = 0;
    if (strncasecmp(str, "0x", 2) == 0) {
        sscanf(str, "%" SCNx32, &val);
    } else {
        sscanf(str, "%" SCNu32, &val);
    }
    return val;
}

static void print_usage(const char *prog) {
    printf("Usage: %s <command> [arguments]\n", prog);
    printf("Commands:\n");
    printf("  dump                  - Read and display FPGA registers (0-%d)\n", DUMP_MAX_REG);
    printf("  read <reg>            - Read specific register (dec or 0xhex)\n");
    printf("  write <reg> <value>   - Write value to specific register\n");
    printf("  help                  - Show this help message\n");
    printf("\nExamples:\n");
    printf("  %s dump\n", prog);
    printf("  %s read 15\n", prog);
    printf("  %s read 0x0F\n", prog);
    printf("  %s write 15 0x1A\n", prog);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "-h") == 0 || strcmp(cmd, "--help") == 0) {
        print_usage(argv[0]);
        return 0;
    }

    // Инициализация драйвера
    if (fpga_driver_init() != 0) {
        fprintf(stderr, "ERROR: Failed to initialize FPGA driver. Check /dev/axi_fpga_dev permissions.\n");
        return 1;
    }

    int ret = 0;

    if (strcmp(cmd, "dump") == 0) {
        printf("========================================================================================================\n");
        printf(" FPGA REGISTER DUMP (Registers 0 to %d)\n", DUMP_MAX_REG);
        printf("========================================================================================================\n");
        printf(" %-30s | ID  | Logical | Physical | Value (Hex)   \n", "Register Name");
        printf("--------------------------------|-----|---------|----------|---------------\n");
        
        for (uint32_t i = 0; i <= DUMP_MAX_REG; i++) {
            uint32_t val = 0;
            uint32_t phys = get_fpga_physical_address(i);
            const char *name = (i < sizeof(fpga_reg_names)/sizeof(fpga_reg_names[0])) ? fpga_reg_names[i] : "UNKNOWN";
            
            if (phys == 0xFFFFFFFF) {
                printf(" %-30s | %3u |  0x%03X  |   N/A    |     N/A       \n", name, i, i);
                continue;
            }

            if (fpga_reg_read(i, &val) == 0) {
                printf(" %-30s | %3u |  0x%03X  |  0x%03X  |  0x%08X  \n", name, i, i, phys, val);
            } else {
                printf(" %-30s | %3u |  0x%03X  |  0x%03X  |   ERROR     \n", name, i, i, phys);
            }
        }
        printf("========================================================================================================\n");

    } else if (strcmp(cmd, "read") == 0) {
        if (argc < 3) {
            fprintf(stderr, "ERROR: Missing register address.\n");
            ret = 1;
        } else {
            uint32_t reg = parse_value(argv[2]);
            uint32_t val = 0;
            uint32_t phys = get_fpga_physical_address(reg);
            const char *name = (reg < sizeof(fpga_reg_names)/sizeof(fpga_reg_names[0])) ? fpga_reg_names[reg] : "UNKNOWN";
            
            if (phys == 0xFFFFFFFF) {
                fprintf(stderr, "ERROR: Register %s (0x%X) is unmapped or out of bounds.\n", name, reg);
                ret = 1;
            } else if (fpga_reg_read(reg, &val) == 0) {
                printf("Register %-30s | ID: %3u (0x%03X) [Physical: 0x%03X] = 0x%08X\n", name, reg, reg, phys, val);
            } else {
                fprintf(stderr, "ERROR: Failed to read register %s (0x%X).\n", name, reg);
                ret = 1;
            }
        }

    } else if (strcmp(cmd, "write") == 0) {
        if (argc < 4) {
            fprintf(stderr, "ERROR: Missing register address or value.\n");
            ret = 1;
        } else {
            uint32_t reg = parse_value(argv[2]);
            uint32_t val = parse_value(argv[3]);
            uint32_t phys = get_fpga_physical_address(reg);
            const char *name = (reg < sizeof(fpga_reg_names)/sizeof(fpga_reg_names[0])) ? fpga_reg_names[reg] : "UNKNOWN";

            if (phys == 0xFFFFFFFF) {
                fprintf(stderr, "ERROR: Register %s (0x%X) is unmapped or out of bounds.\n", name, reg);
                ret = 1;
            } else if (fpga_reg_write(reg, val) == 0) {
                printf("SUCCESS: Wrote 0x%08X to Register %-30s | ID: %3u (0x%03X) [Physical: 0x%03X]\n", val, name, reg, reg, phys);
            } else {
                fprintf(stderr, "ERROR: Failed to write to register %s (0x%X).\n", name, reg);
                ret = 1;
            }
        }
    } else {
        fprintf(stderr, "ERROR: Unknown command '%s'.\n", cmd);
        print_usage(argv[0]);
        ret = 1;
    }

    fpga_driver_uninit();
    return ret;
}
