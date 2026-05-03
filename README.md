### WEB_AGENT

<div align="center">
  
[![Русский](https://img.shields.io/badge/Язык-Русский-blue.svg)](#-русская-версия)
[![English](https://img.shields.io/badge/Language-English-red.svg)](#-english-version)
  
</div>

## 🇷🇺 Русская версия

## Программирование на основе классов и шаблонов (ТОП ИТ)

**Web-Agent** — это легковесное, кроссплатформенное приложение, которое работает в фоновом режиме и связывает действия на веб-странице с выполнением программ на локальном компьютере. Агент опрашивает удаленный сервер, получает инструкции (запустить программу, переместить файлы) и отправляет результаты обратно. Проект спроектирован для параллельной обработки задач от множества пользователей.

## Описание
Данный проект был реализован на основе технического задания (ТЗ) и представляет собой фоновый сервис (веб-агент), который выступает в роли исполнителя команд от удаленного сервера.

**Основная бизнес-логика:**
1.  Опрос сервера: Агент периодически обращается к серверу по HTTP/HTTPS.
2.  Получение задачи: Сервер возвращает инструкцию (например, какую программу запустить).
3.  Выполнение: Агент запускает программу, ожидает её завершения и обрабатывает результаты (перемещает файлы).
4.  Отчет: Агент отправляет результаты (файлы, коды выполнения) обратно на сервер и логирует каждое действие.

## Библиотеки:
1) HTTP Library: [CPR](https://github.com/libcpr/cpr)
2) JSON Parser: [nlohmann/json](https://github.com/nlohmann/json)
3) TESTS: [Google Test](https://github.com/google/googletest)
4) Dotenv: [dotenv-cpp](https://github.com/laserpants/dotenv-cpp)

## Сваггер:
Сваггер находиться в [этом](https://github.com/yarikpd/web_agent/blob/main/openapi.yaml) файле

## Установка из готового исполняемого файла (GitHub Releases)

1. Скачайте исполняемый файл для вашей платформы со страницы [Releases](https://github.com/yarikpd/web_agent/releases):
   - `web_agent-linux` — для Linux
   - `web_agent-macos` — для macOS
   - `web_agent-windows` — для Windows

2. Скачайте файл `.env.example` из репозитория и переименуйте его в `.env`, либо создайте `.env` вручную со следующим содержимым:
   ```
   API_URL=<url вашего сервера>
   AGENT_UID=<идентификатор агента>
   AGENT_DESCR=<описание агента>
   JOBS_DIR=jobs
   POLL_INTERVAL_SECONDS=5
   THREAD_COUNT=5
   SHOW_LOGS_IN_CONSOLE=FALSE
   LOG_FILE_PATH="./logs.txt"
   ```
   Заполните `API_URL`, `AGENT_UID` и `AGENT_DESCR` реальными значениями.

3. Создайте папку `jobs` рядом с исполняемым файлом. Скачайте примеры job-файлов из папки [`jobs/`](https://github.com/yarikpd/web_agent/tree/main/jobs) репозитория или создайте собственные.

4. **Linux / macOS**: сделайте файл исполняемым:
   ```bash
   chmod +x web_agent-linux    # или web_agent-macos
   ```
   **macOS**: если Gatekeeper блокирует запуск, снимите атрибут карантина:
   ```bash
   xattr -d com.apple.quarantine web_agent-macos
   ```
   Либо откройте файл через ПКМ → «Открыть» в Finder.

5. Запустите агента:
   ```bash
   ./web_agent-linux           # Linux
   ./web_agent-macos           # macOS
   web_agent-windows.exe       # Windows (командная строка)
   ```

## Инструкия по сборке (из исходников)
Перед созданием проекта cmake убедитесь, что у вас установлен shure [meson](https://mesonbuild.com/):
1) Для MacOs:
```bash
brew install meson ninja
```
Или если у вас нет homebrew, ледуйте инструкциям в [official documentations](https://mesonbuild.com/SimpleStart.html#without-homebrew).

2) Для Debian, Ubuntu и производных дистрибутивов:
```bash
sudo apt install meson ninja-build libmbedtls-dev libpsl-dev
```

3) Для Fedora, Centos, RHEL и производных дистрибутивов: 
```bash
sudo dnf install meson ninja-build
```

4) Для Arch: 
```bash
sudo pacman -S meson
```

5) Для Windows: 
Follow instructions in [official documentations](https://mesonbuild.com/SimpleStart.html#windows1)

## Роли
Team Lead: Полосухин Ярослав

Архитектор: Ваков Егор

Разработчик 1 (Developer 1): Канаев Андрей 

Разработчик 2 (Developer 2): Дубинина Лиза 

Разработчик 3 (Developer 3): Ефименко Кирилл 

Тестировщик (QA Engineer): Пенов Михаил

Технический писатель (Tech Writer): Ицхакина Валентина

## 🇬🇧 English version

## Object-Oriented and Template-Based Programming (IT TOP)

**Web-Agent** is a lightweight, cross-platform application that runs in the background and bridges actions on a web page with the execution of programs on a local computer. The agent polls a remote server, receives instructions (run a program, move files), and sends results back. The project is designed for parallel processing of tasks from multiple users.

## Description
This project was implemented based on technical specifications and represents a background service (web agent) that acts as an executor of commands from a remote server.

**Core Business Logic:**
1.  Server Polling: The agent periodically contacts the server via HTTP/HTTPS.
2.  Task Acquisition: The server returns an instruction (e.g., which program to run).
3.  Execution: The agent launches the program, waits for its completion, and processes the results (moves files).
4.  Reporting: The agent sends the results (files, execution codes) back to the server and logs every action.

### Libraries:
1) HTTP Library: [CPR](https://github.com/libcpr/cpr)
2) JSON Parser: [nlohmann/json](https://github.com/nlohmann/json)
3) TESTS: [Google Test](https://github.com/google/googletest)
4) Dotenv: [dotenv-cpp](https://github.com/laserpants/dotenv-cpp)

## Swagger:
Swagger is in [this](https://github.com/yarikpd/web_agent/blob/main/openapi.yaml ) file

## Installing from pre-built binary (GitHub Releases)

1. Download the executable for your platform from the [Releases](https://github.com/yarikpd/web_agent/releases) page:
   - `web_agent-linux` — for Linux
   - `web_agent-macos` — for macOS
   - `web_agent-windows.exe` — for Windows

2. Download the `.env.example` file from the repository and rename it to `.env`, or create `.env` manually with the following content:
   ```
   API_URL=<your server URL>
   AGENT_UID=<agent identifier>
   AGENT_DESCR=<agent description>
   JOBS_DIR=jobs
   POLL_INTERVAL_SECONDS=5
   THREAD_COUNT=5
   SHOW_LOGS_IN_CONSOLE=FALSE
   LOG_FILE_PATH="./logs.txt"
   ```
   Fill in `API_URL`, `AGENT_UID` and `AGENT_DESCR` with actual values.

3. Create a `jobs` folder next to the executable. Download example job files from the [`jobs/`](https://github.com/yarikpd/web_agent/tree/main/jobs) folder in the repository or create your own.

4. **Linux / macOS**: make the file executable:
   ```bash
   chmod +x web_agent-linux    # or web_agent-macos
   ```
   **macOS**: if Gatekeeper blocks the launch, remove the quarantine attribute:
   ```bash
   xattr -d com.apple.quarantine web_agent-macos
   ```
   Or open the file via Right Click → "Open" in Finder.

5. Run the agent:
   ```bash
   ./web_agent-linux           # Linux
   ./web_agent-macos           # macOS
   web_agent-windows.exe       # Windows (command prompt)
   ```

### Building (from source)
Before building cmake project, make shure you have installed [meson](https://mesonbuild.com/):
1) For MacOs:
```bash
brew install meson ninja
```
Or if you don't have homebrew, follow instructions in [official documentations](https://mesonbuild.com/SimpleStart.html#without-homebrew).

2) For Debian, Ubuntu and derivatives:
```bash
sudo apt install meson ninja-build  libmbedtls-dev libpsl-dev
```

3) For Fedora, Centos, RHEL and derivatives: 
```bash
sudo dnf install meson ninja-build
```

4) For Arch: 
```bash
sudo pacman -S meson
```

5) For Windows:
Follow instructions in [official documentations](https://mesonbuild.com/SimpleStart.html#windows1)

## Роли
Team Lead: Polosukhin Yaroslav

System Architect: Vakov Egor

Developer 1: Kanaev Andrey

Developer 2: Dubinina Yelizaveta

Developer 3: Yefimenko Kirill 

QA Engineer: Penov Mikhail

Tech Writer: Itskhakina Valentina
