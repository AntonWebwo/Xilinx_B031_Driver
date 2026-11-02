/**
@file zynq7007_fpga_driver.c
@brief Реализация драйвера FPGA Xilinx Zynq 7007 (V9 Only, B031)
*/
#include "zynq7007_fpga_driver.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <pthread.h>
#include <map>
#include <sys/stat.h>

// ============================================================================
// Глобальные переменные
// ============================================================================
static int32_t fpga_initialized = 0;
static int32_t fpga_fd = -1;
static uint32_t* axi_fpga_addr = nullptr;
static int32_t fd_fpga_mem_hal = -1;
static uint32_t* fpga_mem_addr_hal = nullptr;

// Мьютексы
static pthread_mutex_t i2c_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t uart_mutex = PTHREAD_MUTEX_INITIALIZER;

// Контексты
static std::map<int, void*> i2c_context_map;

// V9 Map (Строго 187 элементов, дополнено нулями до MAX_FPGA_REGISTERS)
static const uint32_t v9_fpga_map[] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x11, 0x20, 0x21, 0x22, 0x23, 0x24, 0x28,
    0x25, 0x26, 0x30, 0x30, 0x31, 0x32, 0x33, 0x3C,
    0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44,
    0x45, 0x46, 0x47, 0x48, 0x49, 0x4B, 0x4C, 0x4D,
    0x4E, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56,
    0x57, 0x58, 0x59, 0x5A, 0x5B, 0x11C, 0x11D, 0x11E,
    0x11F, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
    0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E,
    0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76,
    0x77, 0x7C, 0x7D, 0x7E, 0x7F, 0x80, 0x81, 0x82,
    0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B,
    0x8C, 0x8D, 0x8E, 0x8F, 0x90, 0x91, 0x92, 0x93,
    0x94, 0x95, 0x96, 0x97, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00
};

// ============================================================================
// Вспомогательные функции
// ============================================================================
uint32_t get_fpga_physical_address(uint32_t logical_address) {
    if (logical_address >= MAX_FPGA_REGISTERS) return 0xFFFFFFFF;
    return v9_fpga_map[logical_address];
}

static void log_message(const char* message) {
    fprintf(stderr, "%s\n", message);
}

// ============================================================================
// Init/Uninit
// ============================================================================
int fpga_driver_init(void) {
    if (fpga_initialized) return 0;
    
    fpga_fd = open(AXI_FPGA_DEV, O_RDWR);
    if (fpga_fd < 0) {
        log_message("Ошибка: не удалось открыть /dev/axi_fpga_dev");
        return -1;
    }
    
    axi_fpga_addr = static_cast<uint32_t*>(mmap(nullptr, AXI_MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fpga_fd, 0));
    if (axi_fpga_addr == MAP_FAILED || !axi_fpga_addr) {
        close(fpga_fd);
        log_message("Ошибка: mmap axi_fpga_addr failed");
        return -2;
    }
    
    fd_fpga_mem_hal = open(FPGA_MEM_DEV, O_RDWR);
    if (fd_fpga_mem_hal < 0) {
        munmap(axi_fpga_addr, AXI_MAP_SIZE);
        close(fpga_fd);
        log_message("Ошибка: не удалось открыть /dev/fpga_mem");
        return -1;
    }
    
    fpga_mem_addr_hal = static_cast<uint32_t*>(mmap(nullptr, HAL_MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd_fpga_mem_hal, 0));
    if (fpga_mem_addr_hal == MAP_FAILED || !fpga_mem_addr_hal) {
        munmap(axi_fpga_addr, AXI_MAP_SIZE);
        close(fpga_fd);
        close(fd_fpga_mem_hal);
        log_message("Ошибка: mmap fpga_mem_addr_hal failed");
        return -1;
    }
    
    pthread_mutex_init(&uart_mutex, nullptr);
    fpga_initialized = 1;
    return 0;
}

void fpga_driver_uninit(void) {
    if (!fpga_initialized) return;
    if (axi_fpga_addr) munmap(axi_fpga_addr, AXI_MAP_SIZE);
    if (fpga_mem_addr_hal) munmap(fpga_mem_addr_hal, HAL_MAP_SIZE);
    if (fpga_fd >= 0) close(fpga_fd);
    if (fd_fpga_mem_hal >= 0) close(fd_fpga_mem_hal);
    pthread_mutex_destroy(&uart_mutex);
    fpga_initialized = 0;
}

int fpga_driver_is_initialized(void) {
    return fpga_initialized;
}

// ============================================================================
// Low-level Reg Access
// ============================================================================
int fpga_reg_read(uint32_t address, uint32_t* val) {
    if (!val || !fpga_initialized) return -1;
    // Блокируем доступ к регистрам с индексом выше 115
    if (address > 115) return -2; 
    
    uint32_t phys = get_fpga_physical_address(address);
    if (phys == 0xFFFFFFFF) return -2;
    *val = axi_fpga_addr[phys];
    return 0;
}

int fpga_reg_write(uint32_t address, uint32_t val) {
    if (!fpga_initialized) return -1;
    // Блокируем доступ к регистрам с индексом выше 115
    if (address > 115) return -2;

    uint32_t phys = get_fpga_physical_address(address);
    if (phys == 0xFFFFFFFF) return -2;
    axi_fpga_addr[phys] = val;
    return 0;
}

uint32_t* fpga_get_hal_mem_ptr(void) {
    return fpga_mem_addr_hal;
}

// ============================================================================
// Buttons (SysFS GPIO)
// ============================================================================
static int gpio_sysfs_read(int gpio_num) {
    char path[64];
    char val_str[2] = {0};
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", gpio_num);
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    read(fd, val_str, 1);
    close(fd);
    return (val_str[0] == '1') ? 1 : 0;
}

static void gpio_ensure_input(int gpio_num) {
    char path[64];
    struct stat st;
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d", gpio_num);
    if (stat(path, &st) != 0) {
        int fd = open("/sys/class/gpio/export", O_WRONLY);
        if (fd >= 0) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%d", gpio_num);
            write(fd, buf, strlen(buf));
            close(fd);
            usleep(200000);
        }
    }
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", gpio_num);
    int fd = open(path, O_WRONLY);
    if (fd >= 0) {
        write(fd, "in", 2);
        close(fd);
    }
}

int fpga_read_buttons(button_state_t* state) {
    if (!state) return -3;
    gpio_ensure_input(921);
    gpio_ensure_input(943);
    state->rst = gpio_sysfs_read(921);
    state->ip_report = gpio_sysfs_read(943);
    return 0;
}

int fpga_read_button(const char* button_name) {
    if (!button_name) return -3;
    if (strcmp(button_name, "RST") == 0) {
        gpio_ensure_input(921);
        return gpio_sysfs_read(921);
    }
    if (strcmp(button_name, "IP_REPORT") == 0) {
        gpio_ensure_input(943);
        return gpio_sysfs_read(943);
    }
    return -2;
}

// ============================================================================
// LED (SysFS GPIO)
// ============================================================================
static int gpio_sysfs_write(int gpio_num, int value) {
    char path[64];
    char val_str[2];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", gpio_num);
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;
    val_str[0] = value ? '1' : '0';
    write(fd, val_str, 1);
    close(fd);
    return 0;
}

static void gpio_ensure_output(int gpio_num) {
    char path[64];
    struct stat st;
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d", gpio_num);
    if (stat(path, &st) != 0) {
        int fd = open("/sys/class/gpio/export", O_WRONLY);
        if (fd >= 0) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%d", gpio_num);
            write(fd, buf, strlen(buf));
            close(fd);
            usleep(200000);
        }
    }
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", gpio_num);
    int fd = open(path, O_WRONLY);
    if (fd >= 0) {
        write(fd, "out", 3);
        close(fd);
    }
}

int fpga_led_control(int led_id, int on) {
    int gpio = -1;
    if (led_id == 0) gpio = 941; // Red
    else if (led_id == 1) gpio = 942; // Green
    else return -3;
    
    gpio_ensure_output(gpio);
    return gpio_sysfs_write(gpio, on);
}

int fpga_led_blink(int led_id, int count, int period_ms) {
    for (int i = 0; i < count; ++i) {
        fpga_led_control(led_id, 1);
        usleep(period_ms * 1000);
        fpga_led_control(led_id, 0);
        usleep(period_ms * 1000);
    }
    return 0;
}

// ============================================================================
// FAN (Strictly from set_fpga_fan.cpp)
// ============================================================================
static int32_t get_fan_rpm_raw(int physical_id) {
    int retries = 10;
    uint32_t speed_reg;
    while (retries-- > 0) {
        fpga_reg_read(FAN_SPEED, &speed_reg);
        if (((speed_reg & 0x700) >> 8) == (uint32_t)physical_id) {
            uint8_t raw = speed_reg & 0xFF;
            return raw * 0x78; // Strictly 0xB031
        }
        usleep(50000);
    }
    return -1;
}

int fpga_fan_init(void) { return 0; }

int32_t fpga_fan_get_speed(fan_id_e fan_id) {
    int physical_id = -1;
    switch (fan_id) {
        case FAN0: physical_id = 1; break;
        case FAN1: physical_id = 0; break;
        case FAN2: physical_id = 2; break;
        case FAN3: physical_id = 3; break;
        default: return -1;
    }
    return get_fan_rpm_raw(physical_id);
}

int fpga_fan_set_speed(fan_id_e fan_id, uint8_t duty_cycle) {
    if (duty_cycle > 100) duty_cycle = 100;
    uint32_t reg_val = (100 - duty_cycle) | ((uint32_t)duty_cycle << 16);
    if (duty_cycle == 0) reg_val = 0;
    
    if (fan_id == FAN0 || fan_id == FAN1) {
        fpga_reg_write(FAN_CONTROL, reg_val);
    } else if (fan_id == FAN2 || fan_id == FAN3) {
        fpga_reg_write(FAN_CONTROL_1, reg_val);
    }
    return 0;
}

int fpga_fan_turn_on(fan_side_e side) {
    (void)side; // Параметр используется для совместимости интерфейса, но логика включает все вентиляторы
    fpga_fan_set_speed(FAN0, 100);
    fpga_fan_set_speed(FAN2, 100);
    return 0;
}

int fpga_fan_turn_off(fan_side_e side) {
    (void)side; // Параметр используется для совместимости интерфейса, но логика выключает все вентиляторы
    fpga_fan_set_speed(FAN0, 0);
    fpga_fan_set_speed(FAN2, 0);
    return 0;
}

int fpga_fan_get_max_count(void) { return 4; }

// ============================================================================
// PLUG
// ============================================================================
int fpga_check_plug_status(int chain_index, plug_status_t* status) {
    if (!status || chain_index < 0 || chain_index > 3) return -3;
    uint32_t plug_reg = 0;
    int ret = fpga_reg_read(HASH_ON_PLUG, &plug_reg);
    if (ret != 0) return ret;
    status->chain_index = chain_index;
    status->is_plugged = (plug_reg >> chain_index) & 1;
    return 0;
}

int fpga_scan_all_plugs(plug_status_t* statuses) {
    if (!statuses) return -3;
    int connected_count = 0;
    for (int i = 0; i < 4; ++i) {
        plug_status_t status;
        int ret = fpga_check_plug_status(i, &status);
        if (ret == 0) {
            statuses[i] = status;
            if (status.is_plugged) connected_count++;
        }
    }
    return connected_count;
}

// ============================================================================
// Version
// ============================================================================
uint32_t fpga_get_version_id(void) {
    uint32_t ver = 0;
    fpga_reg_read(HARDWARE_VERSION, &ver);
    return ver & 0xFFFF;
}

int fpga_get_version(fpga_version_info_t* info) {
    if (!info) return -3;
    uint32_t ver = 0;
    fpga_reg_read(HARDWARE_VERSION, &ver);
    info->version_id = ver & 0xFFFF;
    snprintf(info->firmware_ver, sizeof(info->firmware_ver), "V9 Platform (0xB031)");
    return 0;
}

// ============================================================================
// I2C
// ============================================================================
bool iic_wait_ready(void) {
    int timeout = 100;
    uint32_t status;
    while (timeout > 0) {
        if (fpga_reg_read(IIC_COMMAND, &status) != 0) return false;
        if (status & IIC_READY_BIT) return true;
        usleep(5000);
        timeout--;
    }
    return false;
}

uint8_t iic_xfer(uint8_t chain_id, uint8_t dev_addr_7bit, bool read_flag, bool reg_addr_valid, uint8_t reg_addr, uint8_t data) {
    if (chain_id > 15) return 0xFF;
    uint32_t value = 0;
    if (read_flag) value |= IIC_READ_FLAG;
    if (reg_addr_valid) value |= ((uint32_t)reg_addr << 8) | IIC_REG_ADDR_VALID;
    
    uint32_t addr_high_nibble = (dev_addr_7bit >> 3) & 0x0F;
    uint32_t chain_field = ((uint32_t)(chain_id & 0x0F)) << 16;
    uint32_t iic_cmd = (uint32_t)data | value | (addr_high_nibble << 20) | chain_field;
    
    fpga_reg_write(IIC_COMMAND, iic_cmd & 0x3FFFFFFF);
    if (!iic_wait_ready()) return 0xFF;
    
    uint32_t status;
    fpga_reg_read(IIC_COMMAND, &status);
    return (uint8_t)status;
}

int fpga_i2c_init(uint32_t master_index) {
    pthread_mutex_lock(&i2c_mutex);
    int ctx = static_cast<int>(i2c_context_map.size());
    i2c_context_map[ctx] = reinterpret_cast<void*>(master_index);
    pthread_mutex_unlock(&i2c_mutex);
    return ctx;
}

int fpga_i2c_select(int ctx, uint32_t master_index) {
    pthread_mutex_lock(&i2c_mutex);
    auto it = i2c_context_map.find(ctx);
    if (it != i2c_context_map.end()) it->second = reinterpret_cast<void*>(master_index);
    pthread_mutex_unlock(&i2c_mutex);
    return 0;
}

void fpga_i2c_uninit(int ctx) {
    pthread_mutex_lock(&i2c_mutex);
    i2c_context_map.erase(ctx);
    pthread_mutex_unlock(&i2c_mutex);
}

bool pic_send_byte(uint8_t chain_id, uint8_t dev_addr, uint8_t cmd_byte) {
    pthread_mutex_lock(&i2c_mutex);
    uint8_t res = iic_xfer(chain_id, dev_addr, false, false, 0, cmd_byte);
    pthread_mutex_unlock(&i2c_mutex);
    return (res != 0xFF);
}

uint8_t pic_read_byte(uint8_t chain_id, uint8_t dev_addr) {
    pthread_mutex_lock(&i2c_mutex);
    uint8_t res = iic_xfer(chain_id, dev_addr, true, false, 0, 0);
    pthread_mutex_unlock(&i2c_mutex);
    return res;
}

bool eeprom_write_byte(uint8_t chain_id, uint8_t dev_addr, uint8_t mem_addr, uint8_t data) {
    pthread_mutex_lock(&i2c_mutex);
    iic_xfer(chain_id, dev_addr, false, true, mem_addr, data);
    pthread_mutex_unlock(&i2c_mutex);
    usleep(5000);
    return true;
}

uint8_t eeprom_read_byte(uint8_t chain_id, uint8_t dev_addr, uint8_t mem_addr) {
    pthread_mutex_lock(&i2c_mutex);
    uint8_t res = iic_xfer(chain_id, dev_addr, true, true, mem_addr, 0);
    pthread_mutex_unlock(&i2c_mutex);
    return res;
}

// ============================================================================
// UART Advanced Logic (Строго из fpga_test_bc_tx.c и fpga_test_bc_rx.c)
// ============================================================================
int fpga_uart_full_init(void) {
    // 1. Установка скорости 115200 (0x1A) через FPGA_BT8D
    fpga_reg_write(FPGA_BT8D, 0x1A);
    usleep(10000);
    
    // 2. Настройка таймаута ожидания ответа
    fpga_reg_write(TIME_OUT_CONTROL, 0x8000020C);
    usleep(10000);
    return 0;
}

int fpga_uart_clear_fifo(uint32_t chain_id) {
    (void)chain_id; // Очищаем весь FIFO для надёжности
    
    uint32_t nonce_count = 0;
    fpga_reg_read(NONCE_NUMBER_IN_FIFO, &nonce_count);
    uint32_t num = nonce_count & 0xFFFF;
    
    uint32_t dummy;
    if (num > 0) {
        for (uint32_t i = 0; i < num; i++) {
            fpga_reg_read(RETURN_NONCE, &dummy);
            fpga_reg_read(RETURN_NONCE_HI, &dummy);
        }
    }
    
    // Сбрасываем счетчик ошибок CRC записью 0
    fpga_reg_write(CRC_ERROR_CNT, 0);
    return 0;
}

int fpga_reset_chain_hw(uint32_t chain_id) {
    if (chain_id > 3) return -1;
    uint32_t reset_reg = 0;
    fpga_reg_read(RESET_HASH_BOARD, &reset_reg);
    
    reset_reg |= (1 << chain_id);
    fpga_reg_write(RESET_HASH_BOARD, reset_reg);
    usleep(10000); // 10ms удержание сброса
    
    reset_reg &= ~(1 << chain_id);
    fpga_reg_write(RESET_HASH_BOARD, reset_reg);
    usleep(50000); // 50ms на стабилизацию ASIC после сброса
    return 0;
}

int fpga_uart_bc_send(uint32_t chain_id, const uint8_t *data, uint32_t size) {
    if (chain_id > 3 || !data || size < 4) return -1;
    
    pthread_mutex_lock(&uart_mutex);
    
    // Расчет CRC5 строго по спецификации (на первые 4 байта = 32 бита)
    uint8_t crc = bitmain_crc5(data, 32);
    
    uint32_t cmd_buf[3] = {0};
    cmd_buf[0] = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
    cmd_buf[1] = crc << 24;
    cmd_buf[2] = 0;
    
    // Запись в буферные регистры
    fpga_reg_write(BC_COMMAND_BUFFER, cmd_buf[0]);
    fpga_reg_write(BC_COMMAND_BUFFER_1, cmd_buf[1]);
    fpga_reg_write(BC_COMMAND_BUFFER_2, cmd_buf[2]);
    
    // Формирование триггера отправки
    uint32_t bc_cmd = 0;
    fpga_reg_read(BC_WRITE_COMMAND, &bc_cmd);
    uint32_t trigger = BC_COMMAND_BUFFER_READY | BC_COMMAND_EN_CHAIN_ID | (chain_id << 16) | (bc_cmd & 0xFFF0FFFF);
    fpga_reg_write(BC_WRITE_COMMAND, trigger);
    
    pthread_mutex_unlock(&uart_mutex);
    return 0;
}

int fpga_uart_bc_receive(uint32_t chain_id, fpga_uart_rx_result_t *result, int32_t timeout_ms) {
    if (!result || chain_id > 3) return -1;
    
    int retries = timeout_ms / 10; // Проверка каждые 10 мс
    if (retries < 1) retries = 1;
    
    pthread_mutex_lock(&uart_mutex);
    
    for (int i = 0; i < retries; i++) {
        uint32_t nonce_count = 0;
        fpga_reg_read(NONCE_NUMBER_IN_FIFO, &nonce_count);
        uint32_t num = nonce_count & 0xFFFF;
        
        if (num > 0) {
            uint32_t word0 = 0, word1 = 0;
            fpga_reg_read(RETURN_NONCE, &word0);
            fpga_reg_read(RETURN_NONCE_HI, &word1);
            
            uint8_t resp_chain = word0 & 0x0F;
            int is_nonce = (word0 & NONCE_INDICATOR) ? 1 : 0;
            
            if (resp_chain == chain_id && !is_nonce) {
                result->reg_value = word1;
                result->crc = (word0 >> 24) & 0x7F;
                result->chain_number = resp_chain;
                result->is_nonce = 0;
                pthread_mutex_unlock(&uart_mutex);
                return 0; // Успешный приём
            }
            // Если это nonce или чужая цепь, мы просто вычитали его из FIFO и продолжаем ждать
        }
        usleep(10000); // 10 мс задержка между проверками
    }
    
    pthread_mutex_unlock(&uart_mutex);
    return -1; // Таймаут
}

uint8_t bitmain_crc5(const uint8_t *ptr, uint32_t len_bits) {
    uint32_t i;
    uint8_t j = 0x80, k = 0;
    uint8_t crcin[5] = {1, 1, 1, 1, 1};
    uint8_t crcout[5];
    uint8_t din, crc = 0;
    for (i = 0; i < len_bits; i++) {
        din = (*ptr & j) ? 1 : 0;
        crcout[0] = crcin[4] ^ din;
        crcout[1] = crcin[0];
        crcout[2] = crcin[1] ^ crcin[4] ^ din;
        crcout[3] = crcin[2];
        crcout[4] = crcin[3];
        j >>= 1; 
        k++;
        if (k == 8) { j = 0x80; k = 0; ptr++; }
        memcpy(crcin, crcout, 5);
    }
    crc = 0;
    if(crcin[4]) crc |= 0x10;
    if(crcin[3]) crc |= 0x08;
    if(crcin[2]) crc |= 0x04;
    if(crcin[1]) crc |= 0x02;
    if(crcin[0]) crc |= 0x01;
    return crc;
}

// ============================================================================
// Timers
// ============================================================================
int fpga_timer_init(uint32_t timer_id, uint32_t period_ms) {
    fpga_reg_write(0x300 + timer_id, period_ms);
    return 0;
}

int fpga_timer_start(uint32_t timer_id) {
    fpga_reg_write(0x310 + timer_id, 1);
    return 0;
}

int fpga_timer_stop(uint32_t timer_id) {
    fpga_reg_write(0x310 + timer_id, 0);
    return 0;
}

uint32_t fpga_timer_get_value(uint32_t timer_id) {
    uint32_t val = 0;
    fpga_reg_read(0x320 + timer_id, &val);
    return val;
}

int fpga_timer_reset(uint32_t timer_id) {
    fpga_reg_write(0x330 + timer_id, 1);
    usleep(1000);
    fpga_reg_write(0x330 + timer_id, 0);
    return 0;
}

void fpga_timer_uninit(uint32_t timer_id) {
    fpga_timer_stop(timer_id);
}
