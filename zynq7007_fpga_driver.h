#ifndef ZYNQ7007_FPGA_DRIVER_H
#define ZYNQ7007_FPGA_DRIVER_H

#include <cstdint>
#include <cstddef>
#include <stdbool.h>

// ============================================================================
// Константы и определения (Строго V9)
// ============================================================================
#define AXI_FPGA_DEV "/dev/axi_fpga_dev"
#define FPGA_MEM_DEV "/dev/fpga_mem"
#define AXI_MAP_SIZE 0x1200
#define HAL_MAP_SIZE 0x1000000
#define MAX_FAN_COUNT 4

#define IIC_READ_FLAG           (1 << 25)
#define IIC_REG_ADDR_VALID      (1 << 24)
#define IIC_READY_BIT           (1 << 31)

// Макросы для BC_COMMAND (из тестовых скриптов)
#define BC_COMMAND_BUFFER_READY (1 << 31)
#define BC_COMMAND_EN_CHAIN_ID  (1 << 23)
#define NONCE_INDICATOR         (1 << 7)

// ============================================================================
// Протоколы ASIC (Строго из driver-bitmain.h)
// ============================================================================
#define BITMAIN_TOKEN_TYPE_TXCONFIG 0x51
#define BITMAIN_TOKEN_TYPE_TXTASK   0x52
#define BITMAIN_TOKEN_TYPE_RXSTATUS 0x53
#define BITMAIN_DATA_TYPE_RXSTATUS  0xA1
#define BITMAIN_DATA_TYPE_RXNONCE   0xA2
#define VIL_COMMAND_TYPE            (0x02 << 5) // 0x40
#define VIL_ALL                     (0x01 << 4) // 0x10
#define SET_CONFIG                  0x08
#define CHAIN_INACTIVE              0x05
#define COMMAND_FOR_ALL             0x80

// Биты управления DHASH_ACC_CONTROL (добавлено)
#define VIL_MODE                    (1 << 15)
#define OPERATION_MODE              (1 << 5)
#define RUN_BIT                     (1 << 6)
#define NEW_BLOCK                   (1 << 7)
#define VIL_MIDSTATE_NUMBER(x)      ((x & 0x0f) << 8)

// ============================================================================
// Типы данных и перечисления
// ============================================================================
typedef enum fpga_register_e {
    HARDWARE_VERSION = 0,
    FAN_SPEED = 1,
    HASH_ON_PLUG = 2,
    WORK_FIFO_STATE = 3,
    RETURN_NONCE = 4,
    RETURN_NONCE_HI = 5,
    NONCE_NUMBER_IN_FIFO = 6,
    NONCE_FIFO_INTERRUPT = 7,
    TEMPERATURE_0_3 = 8,
    TEMPERATURE_4_7 = 9,
    TEMPERATURE_8_11 = 10,
    TEMPERATURE_12_15 = 11,
    IIC_COMMAND = 12,
    RESET_HASH_BOARD = 13,
    BMC_COUNTER = 14,
    FPGA_BT8D = 15,
    TW_WRITE_COMMAND = 16,
    QN_WRITE_DATA_COMMAND = 17,
    RESET_CHAIN = 18,
    FAN_CONTROL = 19,
    TIME_OUT_CONTROL = 20,
    TICKET_MASK_FPGA = 21,
    HASH_COUNTING_NUMBER_FPGA = 22,
    FAN_CONTROL_1 = 23,
    SNO = 24,
    SOCKET_ID = 25,
    BC_WRITE_COMMAND = 26,
    FRIST_WORD = 27,
    BC_COMMAND_BUFFER = 28,
    BC_COMMAND_BUFFER_1 = 29,
    BC_COMMAND_BUFFER_2 = 30,
    FPGA_CHIP_ID_ADDR = 31,
    FPGA_CHIP_ID_HI = 32,
    CRC_ERROR_CNT = 33,
    I2C_SW_ID = 34,
    DHASH_ACC_CONTROL = 35,
    COINBASE_AND_NONCE2_LENGTH = 36,
    WORK_NONCE_2 = 37,
    WORK_NONCE_2_HI = 38,
    NONCE2_AND_JOBID_STORE_ADDRESS = 39,
    MERKLE_BIN_NUMBER = 40,
    JOB_START_ADDRESS = 41,
    JOB_LENGTH = 42,
    JOB_DATA_READY = 43,
    JOB_ID = 44,
    WORK_ENABLE = 45,
    BLOCK_HEADER_VERSION = 46,
    TIME_STAMP = 47,
    TARGET_BITS = 48,
    PRE_HEADER_HASH = 49,
    PRE_HEADER_HASH1 = 50,
    PRE_HEADER_HASH2 = 51,
    PRE_HEADER_HASH3 = 52,
    PRE_HEADER_HASH4 = 53,
    PRE_HEADER_HASH5 = 54,
    PRE_HEADER_HASH6 = 55,
    PRE_HEADER_HASH7 = 56,
    BLOCK_VERSION_0 = 57,
    BLOCK_VERSION_1 = 58,
    BLOCK_VERSION_2 = 59,
    BLOCK_VERSION_3 = 60,
    BLOCK_VERSION_4 = 61,
    BLOCK_VERSION_5 = 62,
    BLOCK_VERSION_6 = 63,
    BLOCK_VERSION_7 = 64,
    CHAIN0_3_SEND_FIFO_STATUS = 65,
    CHAIN4_7_SEND_FIFO_STATUS = 66,
    CHAIN8_11_SEND_FIFO_STATUS = 67,
    CHAIN12_13_SEND_FIFO_STATUS = 68,
    CHAIN0_SEND_READY = 69,
    CHAIN0_SEND_BUFFER = 70,
    CHAIN1_SEND_READY = 71,
    CHAIN1_SEND_BUFFER = 72,
    CHAIN2_SEND_READY = 73,
    CHAIN2_SEND_BUFFER = 74,
    CHAIN3_SEND_READY = 75,
    CHAIN3_SEND_BUFFER = 76,
    CHAIN4_SEND_READY = 77,
    CHAIN4_SEND_BUFFER = 78,
    CHAIN5_SEND_READY = 79,
    CHAIN5_SEND_BUFFER = 80,
    CHAIN6_SEND_READY = 81,
    CHAIN6_SEND_BUFFER = 82,
    CHAIN7_SEND_READY = 83,
    CHAIN7_SEND_BUFFER = 84,
    CHAIN8_SEND_READY = 85,
    CHAIN8_SEND_BUFFER = 86,
    CHAIN9_SEND_READY = 87,
    CHAIN9_SEND_BUFFER = 88,
    RECEIVE_FIFO_0_1_STATUS = 89,
    RECEIVE_FIFO_2_3_STATUS = 90,
    RECEIVE_FIFO_4_5_STATUS = 91,
    RECEIVE_FIFO_6_7_STATUS = 92,
    RECEIVE_FIFO_8_9_STATUS = 93,
    RECEIVE_FIFO_10_11_STATUS = 94,
    RECEIVE_FIFO_12_13_STATUS = 95,
    CHAIN0_READ_ENABLE = 96,
    CHAIN0_READ_DATA = 97,
    CHAIN1_READ_ENABLE = 98,
    CHAIN1_READ_DATA = 99,
    CHAIN2_READ_ENABLE = 100,
    CHAIN2_READ_DATA = 101,
    CHAIN3_READ_ENABLE = 102,
    CHAIN3_READ_DATA = 103,
    CHAIN4_READ_ENABLE = 104,
    CHAIN4_READ_DATA = 105,
    CHAIN5_READ_ENABLE = 106,
    CHAIN5_READ_DATA = 107,
    CHAIN6_READ_ENABLE = 108,
    CHAIN6_READ_DATA = 109,
    CHAIN7_READ_ENABLE = 110,
    CHAIN7_READ_DATA = 111,
    CHAIN8_READ_ENABLE = 112,
    CHAIN8_READ_DATA = 113,
    CHAIN9_READ_ENABLE = 114,
    CHAIN9_READ_DATA = 115,
    CHAIN10_SEND_READY = 116,
    CHAIN10_SEND_BUFFER = 117,
    CHAIN11_SEND_READY = 118,
    CHAIN11_SEND_BUFFER = 119,
    CHAIN12_SEND_READY = 120,
    CHAIN12_SEND_BUFFER = 121,
    CHAIN13_SEND_READY = 122,
    CHAIN13_SEND_BUFFER = 123,
    CHAIN10_READ_ENABLE = 124,
    CHAIN10_READ_DATA = 125,
    CHAIN11_READ_ENABLE = 126,
    CHAIN11_READ_DATA = 127,
    CHAIN12_READ_ENABLE = 128,
    CHAIN12_READ_DATA = 129,
    CHAIN13_READ_ENABLE = 130,
    CHAIN13_READ_DATA = 131,
    SEED_ADDRESS_0 = 132,
    SEED_ADDRESS_1 = 133,
    SEED_ADDRESS_2 = 134,
    SEED_ADDRESS_3 = 135,
    SEED_ADDRESS_4 = 136,
    SEED_ADDRESS_5 = 137,
    SEED_ADDRESS_6 = 138,
    SEED_ADDRESS_7 = 139,
    FULL_NODES_ADDR = 140,
    NUM_MODE_ADDR = 141,
    CACHE_SAVE_ADDR = 142,
    CACHE_START_ADDR = 143,
    DS_START_ADDR = 144,
    DS_DONE_ADDR = 145,
    DATA_SAVE_ADDR = 146,
    UART0_STATUS_ADDR = 147,
    UART1_STATUS_ADDR = 148,
    UART2_STATUS_ADDR = 149,
    UART3_STATUS_ADDR = 150,
    UART4_STATUS_ADDR = 151,
    UART5_STATUS_ADDR = 152,
    UART6_STATUS_ADDR = 153,
    UART7_STATUS_ADDR = 154,
    UART8_STATUS_ADDR = 155,
    UART9_STATUS_ADDR = 156,
    UART10_STATUS_ADDR = 157,
    UART11_STATUS_ADDR = 158,
    UART12_STATUS_ADDR = 159,
    UART13_STATUS_ADDR = 160,
    UART14_STATUS_ADDR = 161,
    UART15_STATUS_ADDR = 162,
    UART16_STATUS_ADDR = 163,
    UART17_STATUS_ADDR = 164,
    UART18_STATUS_ADDR = 165,
    UART19_STATUS_ADDR = 166,
    UART20_STATUS_ADDR = 167,
    UART21_STATUS_ADDR = 168,
    UART22_STATUS_ADDR = 169,
    UART23_STATUS_ADDR = 170,
    UART24_STATUS_ADDR = 171,
    UART25_STATUS_ADDR = 172,
    UART26_STATUS_ADDR = 173,
    UART27_STATUS_ADDR = 174,
    UART28_STATUS_ADDR = 175,
    UART29_STATUS_ADDR = 176,
    UART30_STATUS_ADDR = 177,
    UART31_STATUS_ADDR = 178,
    UART_WORK_FLAG_ADDR = 179,
    DATA_SIZE_ADDR = 180,
    DATA_STORE_ADDR = 181,
    UART_CTRL_STATE_ADDR = 182,
    UART_DBG_WR_ADDR = 183,
    UART_DBG_RD_ADDR = 184,
    UART_DBG_STATUS_ADDR = 185,
    BIT_DIV_ADDR = 186,
    MAX_FPGA_REGISTERS = 187
} fpga_register_e;

typedef enum fan_side_e {
    FRONT_SIDE = 0,
    REAR_SIDE = 1,
    BOTH_SIDES = 2
} fan_side_e;

typedef enum fan_id_e {
    FAN0 = 0,
    FAN1 = 1,
    FAN2 = 2,
    FAN3 = 3,
    FAN_MAX = 4
} fan_id_e;

typedef struct button_state_s {
    int rst;
    int ip_report;
} button_state_t;

typedef struct plug_status_s {
    int chain_index;
    int is_plugged;
} plug_status_t;

typedef struct fpga_version_info_s {
    uint32_t version_id;
    char firmware_ver[32];
} fpga_version_info_t;

typedef struct fpga_uart_rx_result_s {
    uint32_t reg_value;
    uint8_t crc;
    uint8_t chain_number;
    int is_nonce;
} fpga_uart_rx_result_t;

// ============================================================================
// Функции драйвера
// ============================================================================
int fpga_driver_init(void);
void fpga_driver_uninit(void);
int fpga_driver_is_initialized(void);

int fpga_read_buttons(button_state_t* state);
int fpga_read_button(const char* button_name);

int fpga_led_control(int led_id, int on);
int fpga_led_blink(int led_id, int count, int period_ms);

int fpga_fan_init(void);
int32_t fpga_fan_get_speed(fan_id_e fan_id);
int fpga_fan_set_speed(fan_id_e fan_id, uint8_t duty_cycle);
int fpga_fan_turn_on(fan_side_e side);
int fpga_fan_turn_off(fan_side_e side);
int fpga_fan_get_max_count(void);

int fpga_check_plug_status(int chain_index, plug_status_t* status);
int fpga_scan_all_plugs(plug_status_t* statuses);

int fpga_get_version(fpga_version_info_t* info);
uint32_t fpga_get_version_id(void);

int fpga_i2c_init(uint32_t master_index);
int fpga_i2c_select(int ctx, uint32_t master_index);
void fpga_i2c_uninit(int ctx);
bool iic_wait_ready(void);
uint8_t iic_xfer(uint8_t chain_id, uint8_t dev_addr_7bit, bool read_flag, bool reg_addr_valid, uint8_t reg_addr, uint8_t data);
bool pic_send_byte(uint8_t chain_id, uint8_t dev_addr, uint8_t cmd_byte);
uint8_t pic_read_byte(uint8_t chain_id, uint8_t dev_addr);
bool eeprom_write_byte(uint8_t chain_id, uint8_t dev_addr, uint8_t mem_addr, uint8_t data);
uint8_t eeprom_read_byte(uint8_t chain_id, uint8_t dev_addr, uint8_t mem_addr);

int fpga_uart_full_init(void);
int fpga_uart_clear_fifo(uint32_t chain_id);
int fpga_reset_chain_hw(uint32_t chain_id);

int fpga_uart_bc_send(uint32_t chain_id, const uint8_t *data, uint32_t size);
int fpga_uart_bc_receive(uint32_t chain_id, fpga_uart_rx_result_t *result, int32_t timeout_ms);

uint8_t bitmain_crc5(const uint8_t *data, uint32_t len_bits);

int fpga_timer_init(uint32_t timer_id, uint32_t period_ms);
int fpga_timer_start(uint32_t timer_id);
int fpga_timer_stop(uint32_t timer_id);
uint32_t fpga_timer_get_value(uint32_t timer_id);
int fpga_timer_reset(uint32_t timer_id);
void fpga_timer_uninit(uint32_t timer_id);

int fpga_reg_read(uint32_t address, uint32_t* val);
int fpga_reg_write(uint32_t address, uint32_t val);
uint32_t* fpga_get_hal_mem_ptr(void);
uint32_t get_fpga_physical_address(uint32_t logical_address);

#endif // ZYNQ7007_FPGA_DRIVER_H
