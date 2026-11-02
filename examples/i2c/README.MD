# I2C Examples

Примеры работы с шиной I2C для взаимодействия с периферийными устройствами хэш-плат: микроконтроллерами PIC16F1704 и EEPROM памятью.

## Описание

Драйвер реализует программный мастер I2C через регистры FPGA (`IIC_COMMAND`). Все операции защищены мьютексом `i2c_mutex` для обеспечения потокобезопасности при одновременном доступе из разных частей приложения.

**Поддерживаемые режимы:**
- **PIC Mode**: Отправка команд и чтение данных от контроллеров управления питанием/мониторингом на хэш-платах.
- **EEPROM Mode**: Чтение и запись байтов в энергонезависимую память с указанием адреса ячейки.

## API Функции

### Базовые функции
```c
// Инициализация контекста I2C
int fpga_i2c_init(uint32_t master_index);

// Отправка байта команды в PIC (без адреса памяти)
bool pic_send_byte(uint8_t chain_id, uint8_t dev_addr, uint8_t cmd_byte);

// Чтение байта из PIC
uint8_t pic_read_byte(uint8_t chain_id, uint8_t dev_addr);

// Запись байта в EEPROM по указанному адресу памяти
bool eeprom_write_byte(uint8_t chain_id, uint8_t dev_addr, uint8_t mem_addr, uint8_t data);

// Чтение байта из EEPROM по указанному адресу памяти
uint8_t eeprom_read_byte(uint8_t chain_id, uint8_t dev_addr, uint8_t mem_addr);
```

### Низкоуровневый трансфер
```c
// Универсальная функция транзакции I2C
uint8_t iic_xfer(uint8_t chain_id, uint8_t dev_addr_7bit, bool read_flag, 
                 bool reg_addr_valid, uint8_t reg_addr, uint8_t data);
```

## Примеры

### 1. test_i2c_read.c

Утилита для чтения данных из устройств I2C. Поддерживает пакетное чтение нескольких байт.

**Компиляция:**
```bash
arm-linux-gnueabihf-g++ -static -std=c++11 -lpthread test_i2c_read.c ../zynq7007_fpga_driver.c -o test_i2c_read
```

**Использование:**

Чтение 16 байт из EEPROM (адрес устройства 0x50) на цепи 0, начиная с адреса памяти 0x00:
```bash
sudo ./test_i2c_read -c 0 -a 0x50 -l 16 --mode eeprom -m 0
```

Чтение статуса из PIC (адрес устройства 0x20) на цепи 1:
```bash
sudo ./test_i2c_read -c 1 -a 0x20 -l 1 --mode pic
```

### 2. test_i2c_write.c

Утилита для записи данных в устройства I2C. Принимает данные в виде HEX-строки.

**Компиляция:**
```bash
arm-linux-gnueabihf-g++ -static -std=c++11 -lpthread test_i2c_write.c ../zynq7007_fpga_driver.c -o test_i2c_write
```

**Использование:**

Запись последовательности байт `55 AA 04 07` в EEPROM на цепи 0, начиная с адреса 0x10:
```bash
sudo ./test_i2c_write -c 0 -a 0x50 -x 55AA0407 --mode eeprom -m 0x10
```

Отправка команды `0x01` в PIC на цепи 2:
```bash
sudo ./test_i2c_write -c 2 -a 0x20 -x 01 --mode pic
```

## Особенности реализации

1. **Адресация устройств**: Используется 7-битная адресация I2C.
2. **Задержки**: 
   - При чтении EEPROM добавляется задержка `usleep(1000)` между байтами.
   - При записи EEPROM добавляется задержка `usleep(5000)` для завершения цикла программирования ячейки.
3. **Безопасность шины**: Все высокоуровневые функции (`pic_send_byte`, `eeprom_read_byte` и т.д.) автоматически захватывают и освобождают `i2c_mutex`.
4. **Цепи (Chains)**: Поддерживаются цепи с ID от 0 до 3. Выбор цепи осуществляется через битовые поля в регистре `IIC_COMMAND`.
5. **Статус готовности**: Перед каждой операцией драйвер опрашивает бит `IIC_READY_BIT` (31-й бит регистра `IIC_COMMAND`) с таймаутом.
