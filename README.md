# Zynq7007 FPGA Driver

Драйвер для взаимодействия с AXI FPGA на ПЛИС Xilinx Zynq 7007, используемый в качестве контрольной платы Bitmain Antminer S19 и выше (платформа V9, 0xB031).

## Описание

Этот драйвер предоставляет программный интерфейс для управления аппаратными ресурсами FPGA, включая:
- Управление вентиляторами (PWM)
- Чтение температурных датчиков
- Работа с I2C (EEPROM, PIC контроллеры)
- UART коммуникация с ASIC чипами (Broadcast mode)
- Управление светодиодами и кнопками
- Проверка статуса подключения хэш-плат
- Аппаратный сброс цепей
- Таймеры и низкоуровневый доступ к регистрам

## Авторство

Разработан **Антоном Виноградовым** ([@AntonWebwo](https://github.com/AntonWebwo)) при помощи различного реверс-инженеринга кода `bmminer` и `single-board-test` с применением логического анализатора **Saleae Logic 16**.

## Требования к системе

### Обязательные устройства

Для корректной работы драйвера в системе должны быть загружены следующие kernel-модули Bitmain:

1. **`bitmain_axi.ko`** - создаёт устройство `/dev/axi_fpga_dev`
2. **`fpga_mem_driver.ko`** - создаёт устройство `/dev/fpga_mem`

Проверьте наличие устройств:
```bash
ls -la /dev/axi_fpga_dev /dev/fpga_mem
```

Если устройства отсутствуют, загрузите модули:
```bash
insmod bitmain_axi.ko
insmod fpga_mem_driver.ko
```

### Зависимости

- Linux-based система (Antminer или совместимая, ARM архитектура)
- Кросс-компилятор `arm-linux-gnueabihf-g++`
- POSIX threads (`-lpthread`)

## Структура проекта

```
zynq7007-fpga-driver/
├── zynq7007_fpga_driver.h    # Заголовочный файл с определениями регистров и API
├── zynq7007_fpga_driver.c    # Реализация драйвера
├── examples/                 # Примеры использования, разделенные по функционалу
│   ├── buttons/              # Примеры работы с кнопками (RST, IP_REPORT)
│   ├── fpga/                 # Примеры прямого чтения/записи регистров FPGA
│   ├── gpio/                 # Примеры работы с GPIO (сброс RESET цепей)
│   ├── i2c/                  # Примеры работы с I2C (чтение/запись EEPROM, PIC)
│   ├── led/                  # Примеры управления светодиодами (RED, GREEN)
│   ├── plug/                 # Примеры детекта наличия хэш-платы на канале
│   ├── pwm/                  # Примеры управления ШИМ (кулеры) и чтения RPM
│   ├── uart/                 # Примеры работы с Serial через регистры (ASIC comm)
│   └── version/              # Примеры получения версии прошивки FPGA
└── README.md                 # Этот файл
```

## Архитектура драйвера

### Карта регистров FPGA (V9 Platform, 0xB031)

Драйвер работает с 187 логическими регистрами FPGA, которые маппятся на физические адреса через таблицу `v9_fpga_map`. Доступ к регистрам осуществляется через memory-mapped I/O после открытия `/dev/axi_fpga_dev`.

**Важно:** Программный доступ ограничен регистрами с индексом 0-115. Попытка доступа к регистрам выше 115 вернёт ошибку `-2`.

### Основные компоненты API

#### 1. Инициализация
```c
int fpga_driver_init(void);      // Инициализация драйвера (mmap устройств)
void fpga_driver_uninit(void);   // Деинициализация и освобождение ресурсов
```

#### 2. Низкоуровневый доступ к регистрам
```c
int fpga_reg_read(uint32_t address, uint32_t* val);   // Чтение регистра по логическому адресу
int fpga_reg_write(uint32_t address, uint32_t val);   // Запись в регистр
```

#### 3. Управление вентиляторами (PWM)
```c
int fpga_fan_init(void);
int32_t fpga_fan_get_speed(fan_id_e fan_id);           // Получение RPM (FAN0-FAN3)
int fpga_fan_set_speed(fan_id_e fan_id, uint8_t duty); // Установка скорости (0-100%)
int fpga_fan_turn_on(fan_side_e side);                 // Включение всех вентиляторов
int fpga_fan_turn_off(fan_side_e side);                // Выключение всех вентиляторов
```

#### 4. I2C коммуникация
```c
int fpga_i2c_init(uint32_t master_index);              // Инициализация контекста
bool pic_send_byte(uint8_t chain_id, uint8_t dev_addr, uint8_t cmd);
uint8_t pic_read_byte(uint8_t chain_id, uint8_t dev_addr);
bool eeprom_write_byte(uint8_t chain_id, uint8_t dev_addr, uint8_t mem_addr, uint8_t data);
uint8_t eeprom_read_byte(uint8_t chain_id, uint8_t dev_addr, uint8_t mem_addr);
```
Все I2C операции защищены мьютексом `i2c_mutex`.

#### 5. UART коммуникация с ASIC (Broadcast Mode)
```c
int fpga_uart_full_init(void);                         // Инициализация UART (Baud 115200 via FPGA_BT8D)
int fpga_uart_bc_send(uint32_t chain_id, const uint8_t *data, uint32_t size);
int fpga_uart_bc_receive(uint32_t chain_id, fpga_uart_rx_result_t *result, int32_t timeout_ms);
int fpga_reset_chain_hw(uint32_t chain_id);            // Аппаратный сброс цепи (10ms pulse)
int fpga_uart_clear_fifo(uint32_t chain_id);           // Очистка FIFO и счетчика CRC ошибок
```
**Особенности:**
- CRC5 рассчитывается автоматически для первых 4 байт команды функцией `bitmain_crc5()`.
- Поддержка до 4 цепей (chain_id: 0-3) в базовом режиме.
- Все UART операции защищены мьютексом `uart_mutex`.

#### 6. Кнопки и светодиоды (SysFS GPIO)
```c
int fpga_read_buttons(button_state_t* state);          // Чтение состояния кнопок (RST, IP_REPORT)
int fpga_led_control(int led_id, int on);              // Управление LED (0=Red, 1=Green)
int fpga_led_blink(int led_id, int count, int period_ms); // Мигание LED
```

#### 7. Статус подключения хэш-плат
```c
int fpga_check_plug_status(int chain_index, plug_status_t* status);
int fpga_scan_all_plugs(plug_status_t* statuses);
```

#### 8. Версия FPGA
```c
uint32_t fpga_get_version_id(void);
int fpga_get_version(fpga_version_info_t* info);
```

## Сборка

### Компиляция библиотеки

```bash
arm-linux-gnueabihf-g++ -shared -fPIC -std=c++11 -o libzynq7007_fpga.so zynq7007_fpga_driver.c -lpthread
```

### Компиляция примеров

Примеры скомпилированы в статические бинарные файлы для удобства запуска на устройстве без зависимостей.

```bash
cd examples

# Buttons
cd buttons && arm-linux-gnueabihf-g++ -static -std=c++11 -lpthread test_buttons.c ../../zynq7007_fpga_driver.c -o test_buttons

# FPGA Registers
cd fpga && arm-linux-gnueabihf-g++ -static -std=c++11 -lpthread fpga_reg_tool.c ../../zynq7007_fpga_driver.c -o fpga_reg_tool

# GPIO (Reset)
cd gpio && arm-linux-gnueabihf-g++ -static -std=c++11 -lpthread reset_chain.c ../../zynq7007_fpga_driver.c -o reset_chain

# I2C
cd i2c && arm-linux-gnueabihf-g++ -static -std=c++11 -lpthread test_i2c_read.c ../../zynq7007_fpga_driver.c -o test_i2c_read

# LED
cd led && arm-linux-gnueabihf-g++ -static -std=c++11 -lpthread test_led.c ../../zynq7007_fpga_driver.c -o test_led

# Plug Detection
cd plug && arm-linux-gnueabihf-g++ -static -std=c++11 -lpthread test_plug.c ../../zynq7007_fpga_driver.c -o test_plug

# PWM (Fans)
cd pwm && arm-linux-gnueabihf-g++ -static -std=c++11 -lpthread test_fan.c ../../zynq7007_fpga_driver.c -o test_fan

# UART
cd uart && arm-linux-gnueabihf-g++ -static -std=c++11 -lpthread test_uart_rx.c ../../zynq7007_fpga_driver.c -o test_uart_rx

# Version
cd version && arm-linux-gnueabihf-g++ -static -std=c++11 -lpthread test_version.c ../../zynq7007_fpga_driver.c -o test_version
```

## Важные замечания

1. **Права доступа**: Для работы с `/dev/axi_fpga_dev` и `/dev/fpga_mem` требуются права root.
2. **Платформа V9**: Драйвер разработан специально для платформы V9 (0xB031). Использование на других платформах может привести к некорректной работе.
3. **CRC5**: Функция `bitmain_crc5()` реализует алгоритм CRC5, используемый Bitmain. CRC рассчитывается на первые 4 байта (32 бита) данных.
4. **Таймауты**: При работе с UART рекомендуется использовать таймауты для избежания блокировок.
5. **Ограничение регистров**: Доступ к регистрам ограничен индексами 0-115, несмотря на то что карта содержит 187 элементов.

## Лицензия

Этот проект распространяется под лицензией MIT.

## Контакты

- **Автор**: Антон Виноградов
- **GitHub**: [@AntonWebwo](https://github.com/AntonWebwo)

## Благодарности

- Saleae Logic 16 - за возможность детального анализа протоколов
- Сообщество разработчиков Antminer - за документацию и примеры кода
- Daniel Sokil [@s0kil](https://github.com/s0kil) - за примеры кода
