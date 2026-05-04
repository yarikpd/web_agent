# Документация по Job-файлам

Job-файлы — это JSON-файлы, которые описывают задачу, выполняемую агентом. Каждый файл определяет команду для запуска, её параметры и поведение после выполнения.

Job-файлы хранятся в папке, указанной в `JOBS_DIR` (по умолчанию `jobs/`). Имя файла используется как идентификатор задачи (например, `TASK.json` → задача `TASK`).

## Структура файла

```json
{
  "name": "my_job",
  "description": "Описание задачи",
  "command": {
    "linux": "команда для Linux",
    "macos": "команда для macOS",
    "windows": "команда для Windows"
  },
  "parameters": {
    "param_name": {
      "description": "Описание параметра",
      "required": true,
      "type": "string"
    }
  },
  "return_files": false,
  "restart_after": false,
  "return": {
    "description": "Описание возвращаемого значения",
    "type": "string",
    "source": "command_output"
  }
}
```

## Поля

### `name` (строка, обязательно)
Уникальное имя задачи. Должно совпадать с именем файла (без `.json`).

### `description` (строка, опционально)
Человекочитаемое описание задачи.

### `command` (объект, обязательно)
Объект, содержащий команды для разных операционных систем. Ключи: `linux`, `macos`, `windows`. Значение — строка команды, которая будет выполнена через shell.

Команда может содержать плейсхолдеры вида `{param_name}`, которые будут заменены на значения переданных параметров. Если параметр необязательный и не был передан, плейсхолдер заменяется на пустую строку.

Поддерживаются все три ОС (`linux`, `macos`, `windows`). Агент выбирает команду, соответствующую текущей платформе.

### `parameters` (объект, обязательно)
Определяет параметры, которые принимает задача. Ключи — имена параметров (должны совпадать с плейсхолдерами в команде). Значения — объекты со следующими полями:

| Поле | Тип | Обязательно | Описание |
|------|-----|-------------|----------|
| `description` | string | нет | Описание параметра |
| `required` | bool | нет | Обязательный ли параметр (по умолчанию `false`) |
| `type` | string | да | Тип параметра (см. ниже) |

**Поддерживаемые типы параметров:**
- `string` — произвольная строка
- `int` / `integer` — целое число
- `float` / `double` / `number` — число с плавающей точкой
- `bool` / `boolean` — булево значение (`true`/`false` или `1`/`0`)

Если передан аргумент, не объявленный в `parameters`, выполнение завершится ошибкой. Если не передан обязательный параметр — аналогично.

### `return_files` (bool, опционально, по умолчанию `false`)
Если `true`, агент ищет в переданных аргументах пути к файлам, ожидает их появления/стабилизации на диске (до 3 секунд) и отправляет на сервер вместе с результатом выполнения.

Полезно для задач, которые создают или изменяют файлы.

### `restart_after` (bool, опционально, по умолчанию `false`)
Если `true`, после успешного выполнения задачи и отправки результата агент перезапускается. Используется для задач, изменяющих конфигурацию агента (например, `CONF`, `TIMEOUT`), чтобы применить новые настройки.

### `return` (объект, опционально)
Описывает тип возвращаемого задачей значения.

| Поле | Тип | Описание |
|------|-----|----------|
| `type` | string | Тип возвращаемого значения: `string`, `bool`, `int`, `float` |
| `source` | string | Источник значения: `command_output` (stdout) или `command_success` (успешность выхода) |
| `description` | string | Описание возвращаемого значения |

По умолчанию тип — `string`.

## Примеры

### Простая задача (directory.json)
```json
{
  "name": "directory",
  "description": "Get files in a directory",
  "command": {
    "linux": "ls -l {directory}",
    "macos": "ls -l {directory}",
    "windows": "dir {directory}"
  },
  "parameters": {
    "directory": {
      "description": "The directory to list",
      "required": true,
      "type": "string"
    }
  },
  "return": {
    "type": "string",
    "source": "command_output"
  }
}
```

Вызов: `f("directory", {{"directory", "/home/user"}})` — выполнит `ls -l /home/user`.

### Задача с возвратом файлов (TASK.json)
```json
{
  "name": "TASK",
  "description": "Run a program with optional arguments and return its output",
  "command": {
    "linux": "{program} {args}",
    "macos": "{program} {args}",
    "windows": "start /wait \"\" \"{program}\" {args}"
  },
  "return_files": true,
  "parameters": {
    "program": {
      "description": "Program or command to run",
      "required": true,
      "type": "string"
    },
    "args": {
      "description": "Optional command-line arguments",
      "required": false,
      "type": "string"
    }
  },
  "return": {
    "type": "string",
    "source": "command_output"
  }
}
```

Вызов: `f("TASK", {{"program", "gcc"}, {"args", "-o prog main.c"}})` — выполнит `gcc -o prog main.c`.

### Задача с перезапуском (TIMEOUT.json)
```json
{
  "name": "TIMEOUT",
  "description": "Update polling interval in .env",
  "command": {
    "linux": "sed -i 's/^POLL_INTERVAL_SECONDS=.*/POLL_INTERVAL_SECONDS={timeout}/' .env",
    "macos": "sed -i.bak 's/^POLL_INTERVAL_SECONDS=.*/POLL_INTERVAL_SECONDS={timeout}/' .env && rm -f .env.bak",
    "windows": "powershell -Command \"...\""
  },
  "return_files": false,
  "restart_after": true,
  "parameters": {
    "timeout": {
      "description": "Polling interval in seconds",
      "required": true,
      "type": "int"
    }
  },
  "return": {
    "type": "bool",
    "source": "command_success"
  }
}
```

Вызов: `f("TIMEOUT", {{"timeout", "10"}})` — обновит интервал опроса и перезапустит агента.
