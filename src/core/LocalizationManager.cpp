// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#include "LocalizationManager.hpp"
#include "AppPrefs.hpp"
#include <QCoreApplication>
#include <QHash>
#include <QDebug>

static QHash<QString, QString> s_ruToEn;
static QHash<QString, QString> s_enToRu;

static void ensureDictionariesInitialized() {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

    s_ruToEn = {
        {QStringLiteral("Загрузка..."), QStringLiteral("Downloading...")},
        {QStringLiteral("Не удалось обновить подписку. Сохранённые серверы не изменены."), QStringLiteral("Could not update subscription. Saved servers were kept.")},
        {QStringLiteral("В подписке нет узлов. Сохранённые серверы не изменены."), QStringLiteral("No nodes found in subscription. Saved servers were kept.")},
        {QStringLiteral("Не удалось сохранить обновление подписки."), QStringLiteral("Could not save subscription update.")},
        {QStringLiteral("Подписка обновлена: %1 узлов"), QStringLiteral("Subscription updated: %1 nodes")},
        // Navigation
        {QStringLiteral("Дашборд"), QStringLiteral("Dashboard")},
        {QStringLiteral("Подключение"), QStringLiteral("Dashboard")},
        {QStringLiteral("Узлы"), QStringLiteral("Nodes")},
        {QStringLiteral("Серверы"), QStringLiteral("Servers")},
        {QStringLiteral("Маршрутизация"), QStringLiteral("Routing")},
        {QStringLiteral("Кабинет"), QStringLiteral("Cabinet")},
        {QStringLiteral("Настройки"), QStringLiteral("Settings")},
        {QStringLiteral("Консоль"), QStringLiteral("Console")},

        // Dashboard & Connection
        {QStringLiteral("Сетевая статистика"), QStringLiteral("Network Statistics")},
        {QStringLiteral("Режим маршрутизации"), QStringLiteral("Routing Mode")},
        {QStringLiteral("Настроить ›"), QStringLiteral("Configure ›")},
        {QStringLiteral("Сменить сервер: %1"), QStringLiteral("Change server: %1")},
        {QStringLiteral("Выбрать сервер"), QStringLiteral("Choose a server")},
        {QStringLiteral("Настроить маршрутизацию: %1"), QStringLiteral("Configure routing: %1")},
        {QStringLiteral("Закрыть уведомление"), QStringLiteral("Dismiss notification")},
        {QStringLiteral("Очистить поиск"), QStringLiteral("Clear search")},
        {QStringLiteral("Параметры соединения"), QStringLiteral("Connection Parameters")},
        {QStringLiteral("Сетевой стек:"), QStringLiteral("Network stack:")},
        {QStringLiteral("Удалённый DNS:"), QStringLiteral("Remote DNS:")},
        {QStringLiteral("Изоляция трафика:"), QStringLiteral("Traffic isolation:")},
        {QStringLiteral("Сменить сервер"), QStringLiteral("Change server")},
        {QStringLiteral("Добавить узел"), QStringLiteral("Add node")},
        {QStringLiteral("+ Добавить узел"), QStringLiteral("+ Add Node")},
        {QStringLiteral("Select a server"), QStringLiteral("Select a server")},
        {QStringLiteral("Выберите сервер"), QStringLiteral("Select a server")},
        {QStringLiteral("Ready to connect"), QStringLiteral("Ready to connect")},
        {QStringLiteral("Готов к подключению"), QStringLiteral("Ready to connect")},
        {QStringLiteral("Готов"), QStringLiteral("Ready")},
        {QStringLiteral("Добавьте узел, чтобы подключиться"), QStringLiteral("Add a node to connect")},
        {QStringLiteral("Весь трафик через VPN (Full Tunnel)"), QStringLiteral("All Traffic via VPN (Full Tunnel)")},
        {QStringLiteral("Раздельный туннель (Split Tunneling)"), QStringLiteral("Split Tunneling")},
        {QStringLiteral("Продвинутая маршрутизация (Advanced Routing)"), QStringLiteral("Advanced Routing")},
        {QStringLiteral("Маршрутизация по умолчанию"), QStringLiteral("Default Routing")},
        {QStringLiteral("ПОДКЛЮЧИТЬ"), QStringLiteral("CONNECT")},
        {QStringLiteral("ОТКЛЮЧИТЬ"), QStringLiteral("DISCONNECT")},
        {QStringLiteral("ОТМЕНА"), QStringLiteral("CANCEL")},
        {QStringLiteral("Подключить"), QStringLiteral("Connect")},
        {QStringLiteral("Отключить"), QStringLiteral("Disconnect")},
        {QStringLiteral("Подключено"), QStringLiteral("Connected")},
        {QStringLiteral("Отключено"), QStringLiteral("Disconnected")},
        {QStringLiteral("Подключение..."), QStringLiteral("Connecting...")},
        {QStringLiteral("СЕРВЕР НЕ ВЫБРАН"), QStringLiteral("NO SERVER SELECTED")},
        {QStringLiteral("Нажмите для подключения"), QStringLiteral("Click to connect")},
        {QStringLiteral("Защищенный туннель активен"), QStringLiteral("Secure tunnel active")},
        {QStringLiteral("Защищено — туннель активен"), QStringLiteral("Protected - Tunnel Active")},
        {QStringLiteral("ОТКЛЮЧЕНО"), QStringLiteral("DISCONNECTED")},
        {QStringLiteral("ПОДКЛЮЧЕНИЕ"), QStringLiteral("CONNECTING")},
        {QStringLiteral("ЗАЩИЩЕНО"), QStringLiteral("PROTECTED")},
        {QStringLiteral("TUN АКТИВЕН"), QStringLiteral("TUN ACTIVE")},
        {QStringLiteral("ТОЛЬКО ПРОКСИ"), QStringLiteral("PROXY ONLY")},
        {QStringLiteral("Установка соединения..."), QStringLiteral("Establishing connection...")},
        {QStringLiteral("Подключение к узлу..."), QStringLiteral("Connecting to Node...")},
        {QStringLiteral("Подключение к ядру..."), QStringLiteral("Connecting to core...")},
        {QStringLiteral("Kill switch: соединение потеряно, трафик не защищен"), QStringLiteral("Kill switch: tunnel lost, traffic is unprotected")},
        {QStringLiteral("Выберите узел во вкладке «Узлы»"), QStringLiteral("Select a node in «Nodes» tab")},
        {QStringLiteral("ВХОДЯЩИЙ"), QStringLiteral("DOWNLOAD")},
        {QStringLiteral("ИСХОДЯЩИЙ"), QStringLiteral("UPLOAD")},
        {QStringLiteral("СЕССИЯ"), QStringLiteral("SESSION")},
        {QStringLiteral("СКОРОСТЬ"), QStringLiteral("SPEED")},
        {QStringLiteral("СЕТЕВАЯ АКТИВНОСТЬ"), QStringLiteral("NETWORK ACTIVITY")},
        {QStringLiteral("ЖУРНАЛ ЯДРА"), QStringLiteral("CORE LOG")},
        {QStringLiteral("Очистить"), QStringLiteral("Clear")},
        {QStringLiteral("СКОПИРОВАТЬ"), QStringLiteral("COPY")},
        {QStringLiteral("Логи отсутствуют"), QStringLiteral("No logs available")},

        // Nodes & Subscriptions
        {QStringLiteral("Узлы и подписки"), QStringLiteral("Nodes & Subscriptions")},
        {QStringLiteral("Всего серверов: %1"), QStringLiteral("Total servers: %1")},
        {QStringLiteral("Добавить"), QStringLiteral("Add")},
        {QStringLiteral("+ Добавить"), QStringLiteral("+ Add")},
        {QStringLiteral("Обновить все"), QStringLiteral("Update All")},
        {QStringLiteral("ОБНОВИТЬ ВСЕ"), QStringLiteral("UPDATE ALL")},
        {QStringLiteral("Пинг"), QStringLiteral("Ping")},
        {QStringLiteral("Тест..."), QStringLiteral("Testing...")},
        {QStringLiteral("Поиск узлов и подписок..."), QStringLiteral("Search nodes and subscriptions...")},
        {QStringLiteral("Поиск узла по названию, стране или протоколу..."), QStringLiteral("Search node by name, country or protocol...")},
        {QStringLiteral("Все"), QStringLiteral("All")},
        {QStringLiteral("Серверы не найдены"), QStringLiteral("No servers found")},
        {QStringLiteral("Узлы не найдены"), QStringLiteral("No nodes found")},
        {QStringLiteral("Узлы не найдены по запросу"), QStringLiteral("No nodes found for query")},
        {QStringLiteral("Ничего не найдено"), QStringLiteral("Nothing found")},
        {QStringLiteral("Попробуйте изменить поисковый запрос."), QStringLiteral("Try changing your search query.")},
        {QStringLiteral("Попробуйте изменить поисковый запрос или фильтр"), QStringLiteral("Try changing your search query or filter")},
        {QStringLiteral("Подписка не содержит серверов или не добавлена"), QStringLiteral("Subscription has no servers or is not added")},
        {QStringLiteral("Обновить подписку"), QStringLiteral("Update Subscription")},
        {QStringLiteral("Добавить подписку"), QStringLiteral("Add Subscription")},
        {QStringLiteral("Импорт подписки или ключа"), QStringLiteral("Import Subscription or Key")},
        {QStringLiteral("Вставьте URL подписки или ключ конфигурации:"), QStringLiteral("Paste subscription URL or config key:")},
        {QStringLiteral("Импортировать"), QStringLiteral("Import")},
        {QStringLiteral("Отмена"), QStringLiteral("Cancel")},
        {QStringLiteral("Вставить из буфера"), QStringLiteral("Paste from clipboard")},
        {QStringLiteral("Вставить"), QStringLiteral("Paste")},
        {QStringLiteral("Неверный формат подписки"), QStringLiteral("Invalid subscription format")},
        {QStringLiteral("Подписка успешно импортирована"), QStringLiteral("Subscription imported successfully")},
        {QStringLiteral("Удалить подписку"), QStringLiteral("Delete Subscription")},
        {QStringLiteral("Удалить подписку?"), QStringLiteral("Delete Subscription?")},
        {QStringLiteral("Вы уверены, что хотите удалить подписку?"), QStringLiteral("Are you sure you want to delete subscription?")},
        {QStringLiteral("Вы действительно хотите удалить подписку «%1»? Все связанные с ней серверы будут удалены."), QStringLiteral("Are you sure you want to delete subscription «%1»? All associated servers will be deleted.")},
        {QStringLiteral("Все узлы этой подписки будут удалены."), QStringLiteral("All nodes in this subscription will be deleted.")},
        {QStringLiteral("Удалить"), QStringLiteral("Delete")},
        {QStringLiteral("Удалить группу"), QStringLiteral("Delete Group")},
        {QStringLiteral("Удалить узел"), QStringLiteral("Delete node")},
        {QStringLiteral("Сервер"), QStringLiteral("Server")},
        {QStringLiteral(", выбран"), QStringLiteral(", selected")},
        {QStringLiteral(", selected"), QStringLiteral(", selected")},
        {QStringLiteral("Подписка"), QStringLiteral("Subscription")},
        {QStringLiteral("Активна"), QStringLiteral("Active")},
        {QStringLiteral("АКТИВНО"), QStringLiteral("ACTIVE")},
        {QStringLiteral("%n узлов"), QStringLiteral("%n nodes")},
        {QStringLiteral("Поддержка"), QStringLiteral("Support")},
        {QStringLiteral("Сайт"), QStringLiteral("Website")},
        {QStringLiteral("Скопировать ссылку"), QStringLiteral("Copy Link")},
        {QStringLiteral("Открыть веб-страницу"), QStringLiteral("Open Web Page")},
        {QStringLiteral("Использовано: %1  •  Безлимитный трафик"), QStringLiteral("Used: %1  •  Unlimited Traffic")},
        {QStringLiteral("Использовано: %1 из %2 (%3%)"), QStringLiteral("Used: %1 of %2 (%3%)")},
        {QStringLiteral("БЕЗЛИМИТ"), QStringLiteral("UNLIMITED")},
        {QStringLiteral("Срок: %1  •  %2"), QStringLiteral("Expires: %1  •  %2")},
        {QStringLiteral("Бессрочно"), QStringLiteral("Perpetual")},
        {QStringLiteral("Не обновлялось"), QStringLiteral("Never updated")},
        {QStringLiteral("Пользовательские серверы"), QStringLiteral("Custom Servers")},
        {QStringLiteral("Кастомные серверы"), QStringLiteral("Custom Servers")},
        {QStringLiteral("Кастомные узлы"), QStringLiteral("Custom Nodes")},
        {QStringLiteral("Узлы, добавленные вручную или по отдельным ссылкам"), QStringLiteral("Nodes added manually or via individual links")},
        {QStringLiteral("Нет добавленных узлов или подписок"), QStringLiteral("No nodes or subscriptions added")},
        {QStringLiteral("Нажмите «+ Добавить», чтобы импортировать конфигурацию или ссылку на подписку."), QStringLiteral("Click «+ Add» to import configuration or subscription link.")},
        {QStringLiteral("Нет подписки? Тебе в beaxty VPN!"), QStringLiteral("No subscription? Welcome to beaxty VPN!")},
        {QStringLiteral("Попробуй 3 дня бесплатно! Высокая скорость, обход блокировок и доступ ко всем локациям."), QStringLiteral("Try 3 days for free! High speed, bypass restrictions and access to all locations.")},
        {QStringLiteral("Telegram-бот (@beaxtyvpnbot)"), QStringLiteral("Telegram Bot (@beaxtyvpnbot)")},
        {QStringLiteral("Личный кабинет (cabinet.beaxty.com)"), QStringLiteral("Personal Cabinet (cabinet.beaxty.com)")},
        {QStringLiteral("Переименовать подписку"), QStringLiteral("Rename Subscription")},
        {QStringLiteral("Переименование подписки"), QStringLiteral("Rename Subscription")},
        {QStringLiteral("Введите новое название для подписки:"), QStringLiteral("Enter new name for subscription:")},
        {QStringLiteral("Новое имя подписки:"), QStringLiteral("New subscription name:")},
        {QStringLiteral("Переименовать"), QStringLiteral("Rename")},
        {QStringLiteral("Сохранить"), QStringLiteral("Save")},
        {QStringLiteral("Нажмите для измерения задержки"), QStringLiteral("Click to measure latency")},
        {QStringLiteral("Загружено: %1"), QStringLiteral("Downloaded: %1")},
        {QStringLiteral("Отдано: %1"), QStringLiteral("Uploaded: %1")},
        {QStringLiteral("Всего: %1"), QStringLiteral("Total: %1")},
        {QStringLiteral("Дней осталось: %1"), QStringLiteral("Days left: %1")},
        {QStringLiteral("Срок действия: %1"), QStringLiteral("Expiry date: %1")},
        {QStringLiteral("Истекла"), QStringLiteral("Expired")},
        {QStringLiteral("Обновлено: %1"), QStringLiteral("Updated: %1")},
        {QStringLiteral("Обновление..."), QStringLiteral("Updating...")},
        {QStringLiteral("Только что"), QStringLiteral("Just now")},
        {QStringLiteral("%1 мин. назад"), QStringLiteral("%1 min. ago")},
        {QStringLiteral("%1 ч. назад"), QStringLiteral("%1 hr. ago")},
        {QStringLiteral("более суток назад"), QStringLiteral("more than a day ago")},
        {QStringLiteral("никогда"), QStringLiteral("never")},
        {QStringLiteral("Не удалось обновить подписку"), QStringLiteral("Failed to update subscription")},
        {QStringLiteral("Подписка успешно обновлена"), QStringLiteral("Subscription updated successfully")},
        {QStringLiteral("Подписка \"%1\" обновлена: %2 узлов"), QStringLiteral("Subscription \"%1\" updated: %2 nodes")},
        {QStringLiteral("Подписка переименована: \"%1\""), QStringLiteral("Subscription renamed: \"%1\"")},
        {QStringLiteral("Подписка удалена"), QStringLiteral("Subscription deleted")},
        {QStringLiteral("Подписка скопирована в буфер обмена"), QStringLiteral("Subscription copied to clipboard")},
        {QStringLiteral("Ссылка на поддержку скопирована"), QStringLiteral("Support link copied")},
        {QStringLiteral("Ссылка на страницу скопирована"), QStringLiteral("Web page link copied")},
        {QStringLiteral("Все подписки обновлены"), QStringLiteral("All subscriptions updated")},
        {QStringLiteral("Ошибка импорта: пустая ссылка или конфигурация"), QStringLiteral("Import error: empty link or configuration")},
        {QStringLiteral("Ошибка декодирования подписки"), QStringLiteral("Subscription decode error")},
        {QStringLiteral("Серверы успешно импортированы"), QStringLiteral("Servers imported successfully")},
        {QStringLiteral("Импортировано серверов: %1"), QStringLiteral("Imported servers: %1")},
        {QStringLiteral("Не удалось загрузить подписку"), QStringLiteral("Failed to download subscription")},
        {QStringLiteral("Подписка добавлена: %1"), QStringLiteral("Subscription added: %1")},
        {QStringLiteral("База данных не готова"), QStringLiteral("Database not ready")},
        {QStringLiteral("Обновление подписок..."), QStringLiteral("Updating subscriptions...")},
        {QStringLiteral("Нет подписок по URL для обновления"), QStringLiteral("No URL-based subscriptions found to refresh")},
        {QStringLiteral("Поле ввода пусто"), QStringLiteral("Input is empty")},
        {QStringLiteral("Ошибка сети при загрузке подписки: %1"), QStringLiteral("Network error fetching subscription: %1")},
        {QStringLiteral("Не найдено подходящих серверов в подписке"), QStringLiteral("No valid proxy nodes found in content")},
        {QStringLiteral("Не найдено подходящих узлов в содержимом"), QStringLiteral("No valid proxy nodes found in content")},
        {QStringLiteral("В содержимом не найдено прокси-узлов"), QStringLiteral("No valid proxy nodes found in content")},
        {QStringLiteral("Успешно импортировано %1 серверов в \"%2\""), QStringLiteral("Imported %1 servers successfully into \"%2\"")},
        {QStringLiteral("Ошибка базы данных при импорте: %1"), QStringLiteral("Database error during import: %1")},
        {QStringLiteral("У подписки \"%1\" нет URL адреса"), QStringLiteral("Subscription \"%1\" has no URL address")},
        {QStringLiteral("Обновление подписки \"%1\"..."), QStringLiteral("Updating subscription \"%1\"...")},
        {QStringLiteral("Ошибка обновления \"%1\": %2"), QStringLiteral("Error updating \"%1\": %2")},
        {QStringLiteral("Обновлено %1 серверов в %2 подписках"), QStringLiteral("Updated %1 servers in %2 subscription(s)")},

        // Import Sheet Modal
        {QStringLiteral("Импорт конфигурации"), QStringLiteral("Import Configuration")},
        {QStringLiteral("ИМЯ ГРУППЫ (НЕОБЯЗАТЕЛЬНО)"), QStringLiteral("GROUP NAME (OPTIONAL)")},
        {QStringLiteral("НАЗВАНИЕ ГРУППЫ (ОПЦИОНАЛЬНО)"), QStringLiteral("GROUP NAME (OPTIONAL)")},
        {QStringLiteral("Автоопределение или свое имя"), QStringLiteral("Auto-detect or custom name")},
        {QStringLiteral("Автоопределение или свое название"), QStringLiteral("Auto-detect or custom name")},
        {QStringLiteral("ССЫЛКА НА ПОДПИСКУ ИЛИ КОНФИГ"), QStringLiteral("SUBSCRIPTION LINK OR CONFIG")},
        {QStringLiteral("ССЫЛКА НА ПОДПИСКУ ИЛИ КЛЮЧ"), QStringLiteral("SUBSCRIPTION LINK OR CONFIG")},
        {QStringLiteral("Вставьте ссылку https://... или vless://, vmess://, ss://, trojan://..."), QStringLiteral("Paste link https://... or vless://, vmess://, ss://, trojan://...")},
        {QStringLiteral("Определено:"), QStringLiteral("Detected:")},

        // Latency & Ping
        {QStringLiteral("Таймаут"), QStringLiteral("Timeout")},
        {QStringLiteral("Timeout"), QStringLiteral("Timeout")},
        {QStringLiteral("Задержка %1 мс"), QStringLiteral("Latency %1 ms")},
        {QStringLiteral("Задержка %1 миллисекунд"), QStringLiteral("Latency %1 milliseconds")},
        {QStringLiteral("Latency %1 milliseconds"), QStringLiteral("Latency %1 milliseconds")},
        {QStringLiteral("Узел недоступен"), QStringLiteral("Node unreachable")},
        {QStringLiteral("Node unreachable"), QStringLiteral("Node unreachable")},
        {QStringLiteral("Задержка не измерена"), QStringLiteral("Latency not measured")},
        {QStringLiteral("Latency not measured"), QStringLiteral("Latency not measured")},

        // Routing
        {QStringLiteral("Режим маршрутизации"), QStringLiteral("Routing Mode")},
        {QStringLiteral("Выберите, как направлять трафик приложений и сайтов через защищенный VPN-туннель:"), QStringLiteral("Choose how application and website traffic is routed through the VPN tunnel:")},
        {QStringLiteral("Весь трафик (Full Tunnel)"), QStringLiteral("All Traffic (Full Tunnel)")},
        {QStringLiteral("Абсолютно весь системный трафик и приложения направляются через зашифрованный туннель."), QStringLiteral("All system traffic and applications are routed through the encrypted tunnel.")},
        {QStringLiteral("Раздельный туннель (Split Tunneling)"), QStringLiteral("Split Tunneling")},
        {QStringLiteral("Через VPN идут только определенные домены и сервисы из списка ниже, остальной трафик идет напрямую."), QStringLiteral("Only specified domains and apps go through VPN, other traffic goes directly.")},
        {QStringLiteral("Через VPN направляются только указанные сайты и выбранные приложения. Весь остальной трафик идет напрямую."), QStringLiteral("Only specified websites and selected apps are routed through VPN. Other traffic goes directly.")},
        {QStringLiteral("Продвинутая маршрутизация (Advanced Routing)"), QStringLiteral("Advanced Routing")},
        {QStringLiteral("Пользовательские правила фильтрации по доменам, IP/CIDR, процессам, портам и GeoIP с точечным выбором действия."), QStringLiteral("Custom granular rules by domain, IP/CIDR, process, port, and GeoIP with flexible actions.")},
        {QStringLiteral("Список туннелируемых доменов"), QStringLiteral("Tunnelled Domains List")},
        {QStringLiteral("Туннелируемые домены"), QStringLiteral("Tunnelled Domains")},
        {QStringLiteral("Добавить домен (например, instagram.com)..."), QStringLiteral("Add domain (e.g. instagram.com)...")},
        {QStringLiteral("Список пуст — весь трафик пойдёт напрямую."), QStringLiteral("List is empty — all traffic goes directly.")},
        {QStringLiteral("Список пуст — добавьте сайты, которые должны открываться через VPN."), QStringLiteral("List is empty — add websites to route through VPN.")},
        {QStringLiteral("Роутинг по приложениям (Per-App Routing)"), QStringLiteral("Per-App Routing")},
        {QStringLiteral("Туннелирование только конкретных выбранных программ (Telegram, Firefox, Chrome и др.) или их исключение."), QStringLiteral("Tunnel only selected applications or exclude them from VPN.")},
        {QStringLiteral("Настройка роутинга приложений"), QStringLiteral("Per-App Routing Setup")},
        {QStringLiteral("Туннелируемые приложения"), QStringLiteral("Tunnelled Applications")},
        {QStringLiteral("Только выбранные через VPN"), QStringLiteral("Only selected via VPN")},
        {QStringLiteral("Все, кроме выбранных"), QStringLiteral("All except selected")},
        {QStringLiteral("Имя процесса (например, telegram-desktop)..."), QStringLiteral("Process name (e.g. telegram-desktop)...")},
        {QStringLiteral("Выбрать из списка"), QStringLiteral("Select from list")},
        {QStringLiteral("Выбранные приложения (%1):"), QStringLiteral("Selected applications (%1):")},
        {QStringLiteral("Список приложений пуст. Введите имя процесса или выберите из списка."), QStringLiteral("Application list is empty. Enter process name or pick from list.")},
        {QStringLiteral("Выбор приложения для роутинга"), QStringLiteral("Select Application for Routing")},
        {QStringLiteral("Выберите программы для включения или исключения из туннеля"), QStringLiteral("Select applications to include or exclude from tunnel")},
        {QStringLiteral("Поиск процесса или приложения..."), QStringLiteral("Search process or application...")},
        {QStringLiteral("Приложения не найдены"), QStringLiteral("No applications found")},
        {QStringLiteral("Close"), QStringLiteral("Close")},
        {QStringLiteral("Закрыть"), QStringLiteral("Close")},
        {QStringLiteral("ПРАВИЛА МАРШРУТИЗАЦИИ"), QStringLiteral("ROUTING RULES")},
        {QStringLiteral("Пользовательские правила (%1):"), QStringLiteral("Custom rules (%1):")},
        {QStringLiteral("Добавить правило"), QStringLiteral("Add Rule")},
        {QStringLiteral("Настроить правила..."), QStringLiteral("Manage rules...")},
        {QStringLiteral("Редактор правил маршрутизации"), QStringLiteral("Routing Rules Editor")},
        {QStringLiteral("Создание пользовательского правила"), QStringLiteral("Create Custom Rule")},
        {QStringLiteral("Тип правила"), QStringLiteral("Rule Type")},
        {QStringLiteral("Значение (домен, CIDR, процесс, порт, страна)"), QStringLiteral("Value (domain, CIDR, process, port, country)")},
        {QStringLiteral("Действие"), QStringLiteral("Action")},
        {QStringLiteral("Прямое подключение (Direct)"), QStringLiteral("Direct Connection")},
        {QStringLiteral("Блокировать (Block)"), QStringLiteral("Block")},
        {QStringLiteral("Через VPN (Proxy)"), QStringLiteral("Via VPN (Proxy)")},
        {QStringLiteral("Сохранить правило"), QStringLiteral("Save Rule")},
        {QStringLiteral("Правила отсутствуют. Нажмите «Добавить правило»."), QStringLiteral("No rules configured. Click «Add Rule».")},
        {QStringLiteral("Изменения применяются автоматически."), QStringLiteral("Changes apply automatically.")},
        {QStringLiteral("Изменения применяются при следующем подключении."), QStringLiteral("Changes apply on next connection.")},

        // Settings
        {QStringLiteral("СЕТЕВОЙ ТУННЕЛЬ"), QStringLiteral("NETWORK TUNNEL")},
        {QStringLiteral("TUN Режим (Виртуальный адаптер)"), QStringLiteral("TUN Mode (Virtual Adapter)")},
        {QStringLiteral("РЕКОМЕНДУЕТСЯ"), QStringLiteral("RECOMMENDED")},
        {QStringLiteral("Создает системный сетевой интерфейс. Прямое туннелирование без ручной настройки SOCKS/HTTP прокси."), QStringLiteral("Creates virtual network interface. Direct tunneling without proxy config.")},
        {QStringLiteral("Активно: %1"), QStringLiteral("Active: %1")},
        {QStringLiteral("Аварийная блокировка (Kill Switch)"), QStringLiteral("Kill Switch")},
        {QStringLiteral("Автоматическое переключение на резервный сервер"), QStringLiteral("Automatic fallback server switching")},
        {QStringLiteral("После двух неудачных проверок маршрута приложение попробует выбранные вами серверы. Смена IP может оборвать игру и другие активные соединения. Пока туннель переподключается, трафик может пойти через обычную сеть; Kill Switch приложения не гарантирует системную блокировку."), QStringLiteral("After two failed route checks, the app will try the servers you choose. Changing IP may interrupt games and other active connections. While the tunnel reconnects, traffic may use the normal network; the app's Kill Switch does not guarantee a system-wide block.")},
        {QStringLiteral("РЕЗЕРВНЫЙ ПУЛ · %1 выбрано"), QStringLiteral("FALLBACK POOL · %1 selected")},
        {QStringLiteral("Доступные серверы"), QStringLiteral("Available servers")},
        {QStringLiteral("В резервном пуле"), QStringLiteral("In fallback pool")},
        {QStringLiteral("Поиск сервера"), QStringLiteral("Search servers")},
        {QStringLiteral("Добавить"), QStringLiteral("Add")},
        {QStringLiteral("Убрать"), QStringLiteral("Remove")},
        {QStringLiteral("Переместить выше"), QStringLiteral("Move up")},
        {QStringLiteral("Переместить ниже"), QStringLiteral("Move down")},
        {QStringLiteral("Все серверы уже в резервном пуле"), QStringLiteral("All servers are already in the fallback pool")},
        {QStringLiteral("Добавьте серверы из списка слева"), QStringLiteral("Add servers from the list on the left")},
        {QStringLiteral("Серверы не найдены"), QStringLiteral("No servers found")},
        {QStringLiteral("Текущий маршрут проверяется по нескольким адресам примерно раз в 3 секунды. Один сбой не запускает переключение."), QStringLiteral("The current route is checked against multiple addresses about every 3 seconds. A single failed check never triggers a switch.")},
        {QStringLiteral("Выберите хотя бы один резервный сервер, отличный от текущего."), QStringLiteral("Choose at least one fallback server other than the current one.")},
        {QStringLiteral("По последнему пингу"), QStringLiteral("Last known ping")},
        {QStringLiteral("По порядку"), QStringLiteral("Configured order")},
        {QStringLiteral("Сервер"), QStringLiteral("Server")},
        {QStringLiteral("Сначала добавьте серверы во вкладке «Серверы»."), QStringLiteral("Add servers in the Servers tab first.")},
        {QStringLiteral("Резервный сервер • Сменить сервер"), QStringLiteral("Fallback server • Change server")},
        {QStringLiteral("Связь потеряна. Проверяем резервные серверы..."), QStringLiteral("Connection lost. Checking backup servers...")},
        {QStringLiteral("Текущий маршрут не отвечает. Добавьте резервные серверы в настройках, чтобы включить переключение."), QStringLiteral("The current route is not responding. Add backup servers in Settings to enable automatic switching.")},
        {QStringLiteral("Переключение на резервный сервер..."), QStringLiteral("Switching to a backup server...")},
        {QStringLiteral("Не удалось подключиться ни к одному серверу из резервного пула."), QStringLiteral("Could not connect to any server in the fallback pool.")},
        {QStringLiteral("Сетевое ядро отклонило запуск резервного подключения из-за небезопасных прав."), QStringLiteral("The network core rejected the fallback connection because its permissions are unsafe.")},
        {QStringLiteral("Для резервного подключения не подтверждены права TUN. Проверьте их в настройках."), QStringLiteral("TUN permissions are not verified for the fallback connection. Check them in Settings.")},
        {QStringLiteral("Не удалось запустить сетевое ядро для резервного подключения."), QStringLiteral("Could not start the network core for the fallback connection.")},
        {QStringLiteral("Автоматически переключено на %1"), QStringLiteral("Automatically switched to %1")},
        {QStringLiteral("%1 Повторная попытка через %2 сек."), QStringLiteral("%1 Retrying in %2 seconds.")},
        {QStringLiteral("При разрыве приложение запросит блокировку у работающего ядра. Системный firewall не устанавливается, поэтому блокировка всего трафика после сбоя ядра не гарантируется."), QStringLiteral("When the tunnel drops, the app requests blocking from the running core. No system firewall is installed, so blocking all traffic after a core crash is not guaranteed.")},
        {QStringLiteral("Приложение может запросить у работающего ядра режим блокировки. После сбоя ядра системный firewall не включается, поэтому блокировка всего трафика не гарантируется."), QStringLiteral("The app can request a block mode from a running core. If the core crashes, no system firewall is installed, so system-wide blocking is not guaranteed.")},
        {QStringLiteral("После сбоя ядра системный firewall не включается, поэтому блокировка всего трафика не гарантируется."), QStringLiteral("If the core crashes, no system firewall is enabled, so blocking all traffic is not guaranteed.")},
        {QStringLiteral("DNS СЕРВЕР"), QStringLiteral("DNS SERVER")},
        {QStringLiteral("Выберите DNS-резолвер для защищенных запросов или укажите свой собственный DoH/IP адрес:"), QStringLiteral("Select DNS resolver for encrypted lookups or specify custom DoH/IP address:")},
        {QStringLiteral("Пользовательский"), QStringLiteral("Custom")},
        {QStringLiteral("Пользовательский DNS (URL DoH или IP)..."), QStringLiteral("Custom DNS (DoH URL or IP)...")},
        {QStringLiteral("ИНТЕРФЕЙС И ПОВЕДЕНИЕ"), QStringLiteral("INTERFACE & BEHAVIOR")},
        {QStringLiteral("Сворачивать в трей при закрытии"), QStringLiteral("Minimize to tray on close")},
        {QStringLiteral("При закрытии окна приложение останется активным в области уведомлений."), QStringLiteral("When window is closed, app stays running in system tray.")},
        {QStringLiteral("Тема оформления"), QStringLiteral("Theme")},
        {QStringLiteral("Темная"), QStringLiteral("Dark")},
        {QStringLiteral("Светлая"), QStringLiteral("Light")},
        {QStringLiteral("Системная"), QStringLiteral("System")},
        {QStringLiteral("Язык интерфейса"), QStringLiteral("Interface Language")},
        {QStringLiteral("БЕЗОПАСНОСТЬ И ИДЕНТИФИКАЦИЯ"), QStringLiteral("SECURITY & IDENTITY")},
        {QStringLiteral("Передача идентификатора устройства (HWID)"), QStringLiteral("Send Device Identifier (HWID)")},
        {QStringLiteral("Передает анонимный хэш конфигурации устройства для авторизации подписки."), QStringLiteral("Sends anonymous device config hash for subscription authorization.")},
        {QStringLiteral("Копировать"), QStringLiteral("Copy")},
        {QStringLiteral("Copy HWID"), QStringLiteral("Copy HWID")},
        {QStringLiteral("HWID скопирован в буфер обмена"), QStringLiteral("HWID copied to clipboard")},
        {QStringLiteral("АВТОМАТИЗАЦИЯ"), QStringLiteral("AUTOMATION")},
        {QStringLiteral("Автоподключение при старте"), QStringLiteral("Auto-connect on launch")},
        {QStringLiteral("Автоматически подключаться к последнему выбранному серверу при запуске приложения."), QStringLiteral("Automatically connect to last selected server on app launch.")},
        {QStringLiteral("ПРАВА ДОСТУПА"), QStringLiteral("PRIVILEGES")},
        {QStringLiteral("Права на создание TUN-интерфейса"), QStringLiteral("TUN Interface Capabilities")},
        {QStringLiteral("Для TUN приложение установит отдельную root-owned копию ядра в каталоге текущего пользователя и назначит ей только cap_net_admin. Доступ к копии ограничен ACL; действие требует подтверждения Polkit."), QStringLiteral("For TUN, the app installs a separate root-owned core copy in a per-user system directory and grants only cap_net_admin. ACL limits access to this user; Polkit confirmation is required.")},
        {QStringLiteral("Разрешение для TUN"), QStringLiteral("TUN permission")},
        {QStringLiteral("Для создания системного TUN-интерфейса сетевому ядру нужно право CAP_NET_ADMIN."), QStringLiteral("The network core needs CAP_NET_ADMIN to create a system TUN interface.")},
        {QStringLiteral("После вашего подтверждения приложение один раз установит проверенную копию ядра с этим ограниченным правом и продолжит подключение. SUID-root не используется."), QStringLiteral("After you approve the system prompt, the app will install a verified core copy with this limited permission once and continue connecting. SUID-root is not used.")},
        {QStringLiteral("Отмена"), QStringLiteral("Cancel")},
        {QStringLiteral("Разрешить и подключиться"), QStringLiteral("Allow and connect")},
        {QStringLiteral("Установить права TUN"), QStringLiteral("Install TUN permission")},
        {QStringLiteral("Сначала отключитесь, чтобы изменить права сетевого ядра."), QStringLiteral("Disconnect before changing network core permissions.")},
        {QStringLiteral("Права TUN уже настроены для этого приложения."), QStringLiteral("TUN permissions are already configured for this app.")},
        {QStringLiteral("Для TUN не хватает права CAP_NET_ADMIN. Подтвердите его настройку, чтобы продолжить подключение."), QStringLiteral("TUN is missing CAP_NET_ADMIN. Approve its setup to continue connecting.")},
        {QStringLiteral("Не удалось проверить файл сетевого ядра."), QStringLiteral("Could not verify the network core file.")},
        {QStringLiteral("Обнаружена старая привилегированная копия core в /usr/lib. Удалите у неё SUID и file capabilities."), QStringLiteral("A legacy privileged core copy was found in /usr/lib. Remove its SUID bit and file capabilities.")},
        {QStringLiteral("Обнаружены старые привилегии сетевого ядра. Пересоберите core без SUID и file capabilities."), QStringLiteral("Legacy network core privileges were detected. Rebuild the core without SUID or file capabilities.")},
        {QStringLiteral("Для безопасной настройки TUN нужны pkexec, coreutils, acl и libcap."), QStringLiteral("Safe TUN setup requires pkexec, coreutils, acl and libcap.")},
        {QStringLiteral("Установка права CAP_NET_ADMIN отменена или завершилась ошибкой. SUID-root не применялся."), QStringLiteral("CAP_NET_ADMIN installation was cancelled or failed. SUID-root was not used.")},
        {QStringLiteral("Не удалось проверить установленную копию сетевого ядра."), QStringLiteral("Could not verify the installed network core copy.")},
        {QStringLiteral("Системный каталог прав сетевого ядра небезопасен или недоступен."), QStringLiteral("The network core permissions directory is unsafe or unavailable.")},
        {QStringLiteral("Каталог прав сетевого ядра имеет небезопасные права доступа."), QStringLiteral("The network core permissions directory has unsafe permissions.")},
        {QStringLiteral("Не удалось проверить системный каталог прав сетевого ядра."), QStringLiteral("Could not verify the network core permissions directory.")},
        {QStringLiteral("Не удалось проверить изолированный каталог сетевого ядра."), QStringLiteral("Could not verify the isolated network core directory.")},
        {QStringLiteral("Не удалось ограничить сетевое ядро текущим пользователем."), QStringLiteral("Could not restrict the network core to the current user.")},
        {QStringLiteral("Не удалось запустить системный помощник Polkit."), QStringLiteral("Could not start the system Polkit helper.")},
        {QStringLiteral("Право TUN установлено, но ядро не удалось перезапустить."), QStringLiteral("TUN permission is installed, but the core could not be restarted.")},
        {QStringLiteral("Сетевое ядро установлено с правом CAP_NET_ADMIN без SUID-root."), QStringLiteral("Network core installed with CAP_NET_ADMIN and without SUID-root.")},
        {QStringLiteral("Будет установлена проверяемая копия ядра с CAP_NET_ADMIN, доступная только этому пользователю."), QStringLiteral("A verified core copy will be installed with CAP_NET_ADMIN, accessible only to this user.")},
        {QStringLiteral("Разрешения TUN на этой платформе обрабатываются системой."), QStringLiteral("TUN permissions are managed by the operating system on this platform.")},
        {QStringLiteral("Настроить"), QStringLiteral("Configure")},
        {QStringLiteral("ПОДДЕРЖКА И ДИАГНОСТИКА"), QStringLiteral("SUPPORT & DIAGNOSTICS")},
        {QStringLiteral("Сохранить отчет для поддержки"), QStringLiteral("Save Support Report")},
        {QStringLiteral("Экспорт обезличенного диагностического лога, сетевых маршрутов и состояния системы для службы поддержки."), QStringLiteral("Export redacted diagnostic logs, network routes, and system info for support.")},
        {QStringLiteral("Сохранить отчет"), QStringLiteral("Save Report")},
        {QStringLiteral("Встроенный веб-движок недоступен в данной сборке. Личный кабинет открывается в системном браузере."), QStringLiteral("Built-in web engine is not available in this build. Cabinet will open in system browser.")},
        {QStringLiteral("В текущей сборке встроенный веб-движок не установлен. Кабинет всегда открывается в системном браузере."), QStringLiteral("Built-in web engine is not installed in current build. Cabinet always opens in system browser.")},
        {QStringLiteral("О ПРИЛОЖЕНИИ"), QStringLiteral("ABOUT")},
        {QStringLiteral("Minimalist Desktop VPN • Powered by Throne, sing-box & Xray-core • GPL-3.0"), QStringLiteral("Minimalist Desktop VPN • Powered by Throne, sing-box & Xray-core • GPL-3.0")},

        // Automation & Failover
        {QStringLiteral("Автопереключение при обрыве (Failover)"), QStringLiteral("Connection Failover")},
        {QStringLiteral("Автоматически переподключаться к следующему доступному серверу с минимальным пингом при неожиданном разрыве связи."), QStringLiteral("Automatically reconnect to the best available backup server if the tunnel unexpectedly drops.")},
        {QStringLiteral("Запускать при старте системы"), QStringLiteral("Launch on system startup")},
        {QStringLiteral("Автоматически запускать beaxty VPN при входе в операционную систему."), QStringLiteral("Automatically start beaxty VPN when you log in to your computer.")},
        {QStringLiteral("Автообновление подписок"), QStringLiteral("Subscription Auto-Update")},
        {QStringLiteral("Режим периодического обновления списков серверов и данных профиля:"), QStringLiteral("Subscription server lists and profile update policy:")},
        {QStringLiteral("Никогда"), QStringLiteral("Never")},
        {QStringLiteral("При запуске"), QStringLiteral("On launch")},
        {QStringLiteral("По расписанию"), QStringLiteral("Scheduled")},

        // Server Sorting
        {QStringLiteral("Сортировка"), QStringLiteral("Sort")},
        {QStringLiteral("По умолчанию"), QStringLiteral("Default")},
        {QStringLiteral("По пингу"), QStringLiteral("By ping")},
        {QStringLiteral("По алфавиту"), QStringLiteral("Alphabetical")},
        {QStringLiteral("Пинг ⚡"), QStringLiteral("Ping ⚡")},
        {QStringLiteral("А-Я 🔤"), QStringLiteral("A-Z 🔤")},

        // Tray Menu & Failover messages
        {QStringLiteral("Узел: %1%2 (%3)"), QStringLiteral("Node: %1%2 (%3)")},
        {QStringLiteral("Отменить подключение"), QStringLiteral("Cancel connecting")},
        {QStringLiteral("Сменить сервер"), QStringLiteral("Change server")},
        {QStringLiteral("Нет доступных серверов"), QStringLiteral("No servers available")},
        {QStringLiteral("Не выбран"), QStringLiteral("Not selected")},
        {QStringLiteral("Открыть окно beaxty VPN"), QStringLiteral("Open beaxty VPN")},
        {QStringLiteral("Выход из beaxty VPN"), QStringLiteral("Quit beaxty VPN")},
        {QStringLiteral("Выход"), QStringLiteral("Quit")},
        {QStringLiteral("Связь потеряна. Переключение на «%1»..."), QStringLiteral("Connection lost. Switching to «%1»...")},
        {QStringLiteral("Не удалось восстановить подключение (все серверы недоступны)"), QStringLiteral("Failed to restore connection (all servers unreachable)")}
    };

    // Auto-populate EN -> RU as reverse of RU -> EN
    for (auto it = s_ruToEn.constBegin(); it != s_ruToEn.constEnd(); ++it) {
        if (!s_enToRu.contains(it.value())) {
            s_enToRu.insert(it.value(), it.key());
        }
    }

    // Explicit EN -> RU mappings for variations and direct English keys in code
    const QHash<QString, QString> extraEnToRu = {
        {QStringLiteral("Import Configuration"), QStringLiteral("Импорт конфигурации")},
        {QStringLiteral("GROUP NAME (OPTIONAL)"), QStringLiteral("НАЗВАНИЕ ГРУППЫ (ОПЦИОНАЛЬНО)")},
        {QStringLiteral("Auto-detect or custom name"), QStringLiteral("Автоопределение или свое название")},
        {QStringLiteral("SUBSCRIPTION LINK OR CONFIG"), QStringLiteral("ССЫЛКА НА ПОДПИСКУ ИЛИ КЛЮЧ")},
        {QStringLiteral("Paste link https://... or vless://, vmess://, ss://, trojan://..."), QStringLiteral("Вставьте ссылку https://... или vless://, vmess://, ss://, trojan://...")},
        {QStringLiteral("Paste Clipboard"), QStringLiteral("Вставить")},
        {QStringLiteral("Paste"), QStringLiteral("Вставить")},
        {QStringLiteral("Paste from clipboard"), QStringLiteral("Вставить из буфера")},
        {QStringLiteral("Detected:"), QStringLiteral("Определено:")},
        {QStringLiteral("Import Now"), QStringLiteral("Импортировать")},
        {QStringLiteral("Import"), QStringLiteral("Импортировать")},
        {QStringLiteral("Cancel"), QStringLiteral("Отмена")},
        {QStringLiteral("Delete node"), QStringLiteral("Удалить узел")},
        {QStringLiteral("Refresh subscription"), QStringLiteral("Обновить подписку")},
        {QStringLiteral("Delete subscription"), QStringLiteral("Удалить подписку")},
        {QStringLiteral("Delete Subscription?"), QStringLiteral("Удалить подписку?")},
        {QStringLiteral("All nodes of this subscription will be deleted."), QStringLiteral("Все узлы этой подписки будут удалены.")},
        {QStringLiteral("Perpetual"), QStringLiteral("Бессрочно")},
        {QStringLiteral("Never updated"), QStringLiteral("Не обновлялось")},
        {QStringLiteral("Expires: %1  •  %2"), QStringLiteral("Срок: %1  •  %2")},
        {QStringLiteral("Used: %1  •  Unlimited Traffic"), QStringLiteral("Использовано: %1  •  Безлимитный трафик")},
        {QStringLiteral("Select a server"), QStringLiteral("Выберите сервер")},
        {QStringLiteral("Ready to connect"), QStringLiteral("Готов к подключению")},
        {QStringLiteral("Ready"), QStringLiteral("Готов")},
        {QStringLiteral("Connecting to Node..."), QStringLiteral("Подключение к узлу...")},
        {QStringLiteral("Connecting to core..."), QStringLiteral("Подключение к ядру...")},
        {QStringLiteral("Connecting..."), QStringLiteral("Подключение...")},
        {QStringLiteral("Protected - Tunnel Active"), QStringLiteral("Защищено — туннель активен")},
        {QStringLiteral("Disconnected"), QStringLiteral("Отключено")},
        {QStringLiteral("Kill switch: tunnel lost, traffic is unprotected"), QStringLiteral("Kill switch: соединение потеряно, трафик не защищен")},
        {QStringLiteral("Connect"), QStringLiteral("Подключить")},
        {QStringLiteral("Disconnect"), QStringLiteral("Отключить")},
        {QStringLiteral("Close"), QStringLiteral("Закрыть")},
        {QStringLiteral("Timeout"), QStringLiteral("Таймаут")},
        {QStringLiteral("Node unreachable"), QStringLiteral("Узел недоступен")},
        {QStringLiteral("Latency not measured"), QStringLiteral("Задержка не измерена")},
        {QStringLiteral("Latency %1 milliseconds"), QStringLiteral("Задержка %1 мс")},
        {QStringLiteral("Latency %1 ms"), QStringLiteral("Задержка %1 мс")},
        {QStringLiteral("Database not ready"), QStringLiteral("База данных не готова")},
        {QStringLiteral("Updating subscriptions..."), QStringLiteral("Обновление подписок...")},
        {QStringLiteral("No URL-based subscriptions found to refresh"), QStringLiteral("Нет подписок по URL для обновления")},
        {QStringLiteral("Input is empty"), QStringLiteral("Поле ввода пусто")},
        {QStringLiteral("Empty URL or configuration"), QStringLiteral("Пустая ссылка или конфигурация")},
        {QStringLiteral("Database not initialized"), QStringLiteral("База данных не готова")},
        {QStringLiteral("No valid proxy nodes found in content"), QStringLiteral("В содержимом не найдено прокси-узлов")},
        {QStringLiteral("Servers imported successfully"), QStringLiteral("Серверы успешно импортированы")},
        {QStringLiteral("All subscriptions updated"), QStringLiteral("Все подписки обновлены")},
        {QStringLiteral("Failed to update subscription"), QStringLiteral("Не удалось обновить подписку")},
        {QStringLiteral("Subscription updated successfully"), QStringLiteral("Подписка успешно обновлена")},
        {QStringLiteral("Subscription copied to clipboard"), QStringLiteral("Подписка скопирована в буфер обмена")},
        {QStringLiteral("Support link copied"), QStringLiteral("Ссылка на поддержку скопирована")},
        {QStringLiteral("Web page link copied"), QStringLiteral("Ссылка на страницу скопирована")},
        {QStringLiteral("Subscription deleted"), QStringLiteral("Подписка удалена")},
        {QStringLiteral("Search nodes and subscriptions..."), QStringLiteral("Поиск узлов и подписок...")},
        {QStringLiteral("Update All"), QStringLiteral("Обновить все")},
        {QStringLiteral("Testing..."), QStringLiteral("Тест...")},
        {QStringLiteral("Ping"), QStringLiteral("Пинг")},
        {QStringLiteral("+ Add"), QStringLiteral("+ Добавить")},
        {QStringLiteral("No nodes found for query"), QStringLiteral("Узлы не найдены по запросу")},
        {QStringLiteral("Click to measure latency"), QStringLiteral("Нажмите для измерения задержки")},
        {QStringLiteral("Copy Link"), QStringLiteral("Скопировать ссылку")},
        {QStringLiteral("Open Web Page"), QStringLiteral("Открыть веб-страницу")},
        {QStringLiteral("Support"), QStringLiteral("Поддержка")},
        {QStringLiteral("Website"), QStringLiteral("Сайт")},
        {QStringLiteral("Rename"), QStringLiteral("Переименовать")},
        {QStringLiteral("Rename Subscription"), QStringLiteral("Переименовать подписку")},
        {QStringLiteral("Enter new name for subscription:"), QStringLiteral("Введите новое название для подписки:")},
        {QStringLiteral("Save"), QStringLiteral("Сохранить")},
        {QStringLiteral("Custom Nodes"), QStringLiteral("Кастомные узлы")},
        {QStringLiteral("Custom Servers"), QStringLiteral("Кастомные серверы")},
        {QStringLiteral("No nodes found"), QStringLiteral("Узлы не найдены")},
        {QStringLiteral("Downloaded: %1"), QStringLiteral("Загружено: %1")},
        {QStringLiteral("Uploaded: %1"), QStringLiteral("Отдано: %1")},
        {QStringLiteral("Total: %1"), QStringLiteral("Всего: %1")},
        {QStringLiteral("Days left: %1"), QStringLiteral("Дней осталось: %1")},
        {QStringLiteral("Expiry date: %1"), QStringLiteral("Срок действия: %1")},
        {QStringLiteral("Expired"), QStringLiteral("Истекла")},
        {QStringLiteral("Updated: %1"), QStringLiteral("Обновлено: %1")},
        {QStringLiteral("Updating..."), QStringLiteral("Обновление...")},
        {QStringLiteral("Just now"), QStringLiteral("Только что")},
        {QStringLiteral("%1 min. ago"), QStringLiteral("%1 мин. назад")},
        {QStringLiteral("%1 hr. ago"), QStringLiteral("%1 ч. назад")},
        {QStringLiteral("more than a day ago"), QStringLiteral("более суток назад")},
        {QStringLiteral("never"), QStringLiteral("никогда")},
        {QStringLiteral("%n nodes"), QStringLiteral("%n узлов")},
        {QStringLiteral("Delete Subscription?"), QStringLiteral("Удалить подписку?")},
        {QStringLiteral("Delete subscription?"), QStringLiteral("Удалить подписку?")},
        {QStringLiteral("Are you sure you want to delete subscription «%1»? All associated servers will be deleted."), QStringLiteral("Вы действительно хотите удалить подписку «%1»? Все связанные с ней серверы будут удалены.")},
        {QStringLiteral("No subscription? Welcome to beaxty VPN!"), QStringLiteral("Нет подписки? Тебе в beaxty VPN!")},
        {QStringLiteral("Try 3 days for free! High speed, bypass restrictions and access to all locations."), QStringLiteral("Попробуй 3 дня бесплатно! Высокая скорость, обход блокировок и доступ ко всем локациям.")},
        {QStringLiteral("Telegram Bot (@beaxtyvpnbot)"), QStringLiteral("Telegram-бот (@beaxtyvpnbot)")},
        {QStringLiteral("Personal Cabinet (cabinet.beaxty.com)"), QStringLiteral("Личный кабинет (cabinet.beaxty.com)")},
        {QStringLiteral("Cabinet"), QStringLiteral("Кабинет")},
        {QStringLiteral("Protected"), QStringLiteral("Защищено")},
        {QStringLiteral("In browser"), QStringLiteral("В браузере")},
        {QStringLiteral("Open in browser (cabinet.beaxty.com)"), QStringLiteral("Открыть в браузере (cabinet.beaxty.com)")},
        {QStringLiteral("beaxty VPN Personal Cabinet"), QStringLiteral("Личный кабинет beaxty VPN")},
        {QStringLiteral("Failed to load personal cabinet"), QStringLiteral("Не удалось загрузить личный кабинет")},
        {QStringLiteral("PERSONAL CABINET"), QStringLiteral("ЛИЧНЫЙ КАБИНЕТ")},
        {QStringLiteral("Open cabinet in external browser"), QStringLiteral("Открывать кабинет во внешнем браузере")},
        {QStringLiteral("Cabinet memory saver mode"), QStringLiteral("Режим экономии памяти для кабинета")},
        {QStringLiteral("Connection Failover"), QStringLiteral("Автопереключение при обрыве (Failover)")},
        {QStringLiteral("Fallback server • Change server"), QStringLiteral("Резервный сервер • Сменить сервер")},
        {QStringLiteral("Launch on system startup"), QStringLiteral("Запускать при старте системы")},
        {QStringLiteral("Subscription Auto-Update"), QStringLiteral("Автообновление подписок")},
        {QStringLiteral("Never"), QStringLiteral("Никогда")},
        {QStringLiteral("On launch"), QStringLiteral("При запуске")},
        {QStringLiteral("Scheduled"), QStringLiteral("По расписанию")},
        {QStringLiteral("Sort"), QStringLiteral("Сортировка")},
        {QStringLiteral("Default"), QStringLiteral("По умолчанию")},
        {QStringLiteral("By ping"), QStringLiteral("По пингу")},
        {QStringLiteral("Alphabetical"), QStringLiteral("По алфавиту")},
        {QStringLiteral("Ping ⚡"), QStringLiteral("Пинг ⚡")},
        {QStringLiteral("A-Z 🔤"), QStringLiteral("А-Я 🔤")},
        {QStringLiteral("Change Server"), QStringLiteral("Сменить сервер")},
        {QStringLiteral("Change server"), QStringLiteral("Сменить сервер")},
        {QStringLiteral("No servers available"), QStringLiteral("Нет доступных серверов")},
        {QStringLiteral("Not selected"), QStringLiteral("Не выбран")},
        {QStringLiteral("Open beaxty VPN"), QStringLiteral("Открыть окно beaxty VPN")},
        {QStringLiteral("Quit beaxty VPN"), QStringLiteral("Выход из beaxty VPN")},
        {QStringLiteral("Open Beaxty VPN"), QStringLiteral("Открыть окно beaxty VPN")},
        {QStringLiteral("Quit Beaxty VPN"), QStringLiteral("Выход из beaxty VPN")}
    };

    for (auto it = extraEnToRu.constBegin(); it != extraEnToRu.constEnd(); ++it) {
        s_enToRu.insert(it.key(), it.value());
    }
}

BeaxtyTranslator::BeaxtyTranslator(QObject *parent) : QTranslator(parent) {
    ensureDictionariesInitialized();
}

void BeaxtyTranslator::setLanguage(const QString &lang) {
    m_lang = lang;
}

bool BeaxtyTranslator::isEmpty() const {
    return false;
}

QString BeaxtyTranslator::translate(const char *context, const char *sourceText,
                                    const char *disambiguation, int n) const {
    Q_UNUSED(context);
    Q_UNUSED(disambiguation);
    Q_UNUSED(n);

    if (!sourceText || !sourceText[0]) return QString();
    ensureDictionariesInitialized();

    QString key = QString::fromUtf8(sourceText);

    if (m_lang == QStringLiteral("en")) {
        auto it = s_ruToEn.constFind(key);
        if (it != s_ruToEn.constEnd()) {
            return it.value();
        }
    } else if (m_lang == QStringLiteral("ru")) {
        auto it = s_enToRu.constFind(key);
        if (it != s_enToRu.constEnd()) {
            return it.value();
        }
    }
    return QString(); // default to original source text
}

LocalizationManager *LocalizationManager::s_instance = nullptr;

LocalizationManager::LocalizationManager(QObject *parent) : QObject(parent) {
    s_instance = this;
    m_language = AppPrefs::getString(QStringLiteral("language"), QStringLiteral("ru"));
    m_translator.setLanguage(m_language);
    QCoreApplication::installTranslator(&m_translator);
}

LocalizationManager *LocalizationManager::instance() {
    return s_instance;
}

void LocalizationManager::initialize(QQmlApplicationEngine *engine) {
    m_engine = engine;
}

QString LocalizationManager::language() const {
    return m_language;
}

void LocalizationManager::setLanguage(const QString &lang) {
    if (m_language != lang) {
        m_language = lang;
        AppPrefs::setString(QStringLiteral("language"), lang);
        m_translator.setLanguage(lang);
        emit languageChanged(lang);
        if (m_engine) {
            m_engine->retranslate();
        }
        qDebug() << "[LocalizationManager] Language changed to:" << lang;
    }
}
