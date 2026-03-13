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

## Инструкия по сборке
Перед созданием проекта cmake убедитесь, что у вас установлен shure [meson](https://mesonbuild.com/):
1) Для MacOs:
```bash
brew install meson ninja
```
Или если у вас нет homebrew, ледуйте инструкциям в [official documentations](https://mesonbuild.com/SimpleStart.html#without-homebrew).

2) Для Debian, Ubuntu и производных дистрибутивов:
```bash
sudo apt install meson ninja-build
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

## Библиотеки:
1) HTTP Library: [CPR](https://github.com/libcpr/cpr)
2) JSON Parser: [nlohmann/json](https://github.com/nlohmann/json)
3) TESTS: [Google Test](https://github.com/google/googletest)

## Роли
Team Lead: Полосухин Ярослав

Архитектор: Ваков Егор

Разработчик 1 (Developer 1): Канаев Андрей 

Разработчик 2 (Developer 2): Дубинина Лиза 

Разработчик 3 (Developer 3): Ефименко Кирилл 

Тестировщик (QA Engineer): Пенов Михаил

Технический писатель (Tech Writer): Ицхакина Валентина

## 🇬🇧 English version

пупупупу