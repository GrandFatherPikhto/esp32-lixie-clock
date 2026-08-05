# Тесты

Как настроить окружение и запускать тесты проекта на **Windows** и **Linux / WSL2**, а также что покрывает каждый уровень. Контекст архитектуры — в [architect_ru.md](architect_ru.md).

## Обзор — три уровня тестов

| Уровень | Что тестируется | Инструменты | Нужна ли плата? |
|---------|----------------|-------------|-----------------|
| 1 | Python-инструменты: `configure_clock.py`, `build_target.py` | pytest (мок серийного порта / YAML) | Нет |
| 2 | Чистая логика `core/` (конфигурация, время, дисплей) | Unity (нативный C) | Нет, но нужен Linux/WSL |
| 3 | Сборки прошивки для всех поддерживаемых чипов | `idf.py` / `build_target.py` | Нет для сборки; да для прошивки |

Workflow CI (`.github/workflows/ci.yml`) запускает все три уровня на Ubuntu при каждом пуше / pull request — зелёная галочка означает, что всё проходит без локальной настройки.

## Настройка окружения

### Windows

1. **Установите ESP-IDF v6.0.2** (установщик Espressif). Откройте PowerShell ESP-IDF и выполните `export.ps1` (или используйте ярлык терминала «ESP-IDF»), чтобы `idf.py` оказался в `PATH`.
2. **Виртуальное окружение Python** для инструментов:
   ```powershell
   python -m venv .venv
   .venv\Scripts\activate
   pip install -r tools/requirements.txt   # pyserial, pyyaml, pytest
   ```

### Linux / WSL2

1. **Установите ESP-IDF v6.0.2** и тулчейн (`$IDF_PATH/install.sh`, затем в bash `. $IDF_PATH/export.sh`). Поставляемый Unity-фреймворк для тестов лежит внутри ESP-IDF.
2. **Компилятор C** для host-тестов: `sudo apt-get install -y gcc` (Unity-тесты требуют Linux-хоста — на Windows запускайте их в WSL2).
3. **Виртуальное окружение Python** для инструментов:
   ```bash
   python3 -m venv .venv
   source .venv/bin/activate
   pip install -r tools/requirements.txt
   ```

> **Эта рабочая копия:** ESP-IDF в `~/esp/esp-idf/v6.0.2` — dev-срез v6.1-dev, у которого сгенерированные picolibc-specs конфликтуют с установленным тулчейном. Поэтому локальный (в .gitignore) `sdkconfig` переключён на `CONFIG_LIBC_NEWLIB=y`, чтобы обычный `idf.py build` работал. CI использует чистый v6.0.2 + тулчейн.

## Запуск тестов

### Уровень 1 — Python-инструменты (pytest)

```bash
.venv\Scripts\python -m pytest tests -v   # Windows
.venv/bin/python -m pytest tests -v       # Linux / WSL
```

- **Покрытие:** `tests/test_configure_clock.py` (протокол через стаб `FakeSerial`, применение YAML, маскировка пароля, автоопределение порта, включая числовые VID/PID), `tests/test_build_target.py` (точная сборка команды `idf.py` из YAML/CLI; `subprocess.run` замокан — сборка не выполняется).
- **Ожидаемый результат:** `48 passed`.

### Уровень 2 — Host C юнит-тесты (Unity, только Linux/WSL)

```bash
. $IDF_PATH/export.sh          # задаёт IDF_PATH для поставляемого Unity
bash tests/c/run_tests.sh
```

- Скрипт компилирует `core/src/*.c` + Unity (из `$IDF_PATH/components/unity/unity`) + `tests/c/test_core.c` в `build_host/` и запускает бинарник.
- **Покрытие:** `core_config` (разбор/валидация всех ключей и диапазонов, включая `night_*`, `cross_fade`, `slot_machine_interval`, `wifi_power_save`, `hue_shift`, `hue_2`), `core_time` (формат часового пояса, UTC→ч/м/с, `core_time_is_night_hour`), `core_display` (значение цифры, GRB-размещение, масштабирование яркости, границы, HSV→RGB).
- **Ожидаемый результат:** `15 Tests 0 Failures 0 Ignored OK`.

### Уровень 3 — Сборка прошивки (любая ОС с активным ESP-IDF)

```bash
. $IDF_PATH/export.sh                       # Linux/macOS   (Windows: export.ps1)
python tools/build_target.py --target esp32
# или вручную:
idf.py set-target esp32
idf.py build
```

- Поддерживаемые цели: `esp32`, `esp32s2`, `esp32s3`, `esp32c3`, `esp32c6`, `esp32h2`.
- Прошивка и мониторинг подключённой платы: `idf.py -p /dev/ttyUSB0 flash monitor` (Linux) или `idf.py -p COM5 flash monitor` (Windows).

## Сводка типов тестов

| Тип | Инструмент | Где | Хост |
|-----|-----------|-----|------|
| Python unit-тесты (мок серийного порта/YAML) | pytest | `tests/test_*.py` | Windows + Linux |
| Нативные C unit-тесты (чистая логика) | Unity | `tests/c/test_core.c` | только Linux/WSL |
| Компиляция + линковка прошивки | `idf.py` | весь проект | любая (активный ESP-IDF) |
| Полный регрессионный шлюз | GitHub Actions | `.github/workflows/ci.yml` | Ubuntu (CI) |

## Устранение неполадок

- **`IDF_PATH is not set` / Unity не найден** (Уровень 2): сначала выполните `export.sh`, чтобы существовал `$IDF_PATH/components/unity/unity`.
- **`pytest` не найден:** `pip install -r tools/requirements.txt` в `.venv` проекта.
- **`idf.py: Permission denied` на Linux:** выполняйте export под **bash**, а не `/bin/sh` — `bash -c '. $IDF_PATH/export.sh && idf.py build'`.
- **Ошибка picolibc specs при сборке:** несоответствие установленного IDF и тулчейна; переключение локального `sdkconfig` на `CONFIG_LIBC_NEWLIB=y` решает проблему (см. примечание выше).
