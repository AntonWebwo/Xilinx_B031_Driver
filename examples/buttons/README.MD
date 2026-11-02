# Buttons Examples

Примеры работы с аппаратными кнопками контрольной платы Bitmain (RST, IP_REPORT) через драйвер Zynq7007 FPGA.

## Описание

Кнопки на контрольной плате Antminer S19/V9 подключены к GPIO портам FPGA и доступны через SysFS интерфейс Linux. Драйвер предоставляет функции для чтения состояния кнопок с детектированием событий нажатия.

**Поддерживаемые кнопки:**
- **RST** (Reset) - GPIO 921
- **IP_REPORT** - GPIO 943

## API Функции

```c
// Чтение состояния всех кнопок
int fpga_read_buttons(button_state_t* state);

// Чтение конкретной кнопки по имени
int fpga_read_button(const char* button_name); // "RST" или "IP_REPORT"
```

Структура состояния:
```c
typedef struct button_state_s {
    int rst;        // 1 = не нажата, 0 = нажата (Active Low)
    int ip_report;  // 1 = не нажата, 0 = нажата (Active Low)
} button_state_t;
```

## Примеры

### test_buttons.c

Программа мониторинга кнопок в реальном времени с детектированием событий нажатия (переход из состояния 1 в 0).

**Компиляция:**
```bash
arm-linux-gnueabihf-g++ -static -std=c++11 -lpthread test_buttons.c ../zynq7007_fpga_driver.c -o test_buttons
```

**Использование:**
```bash
sudo ./test_buttons
```

**Вывод:**
```
================================================================================
 BUTTONS TEST (Zynq 7007 Driver V9)
================================================================================
Press buttons to see events. Press CTRL+C to exit.

[EVENT] RESET button pressed
[EVENT] IP REPORT button pressed
```

## Особенности реализации

1. **SysFS GPIO**: Кнопки читаются через `/sys/class/gpio/gpio<N>/value`
2. **Автоматический экспорт**: Драйвер автоматически экспортирует GPIO порты при первом обращении
3. **Active Low Logic**: Нажатие кнопки соответствует логическому `0`
4. **Debouncing**: Рекомендуется программная задержка между опросами (50ms в примере)
5. **Потокобезопасность**: Чтение кнопок не требует мьютексов, так как не использует общие ресурсы FPGA

## Требования

- Права root для доступа к SysFS GPIO
- Загруженные модули ядра: `bitmain_axi.ko`, `fpga_mem_driver.ko`
