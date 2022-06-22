# Описание

## Назначение

Сбор и отображение статистики по трафику на заданном сетевом интерфейсе.

## Состав

### stat_collector 

Утилита для сбора статистики. Реализована в двух вариантах, каждый из которых реализован на двух потоках:

#### stat_collector_v1

Первый поток осуществляет чтение и фильтрацию трафика, второй - суммирование и выдачу данных в stat_displayer по запросу через POSIX Message Queue. 

#### stat_collector_v2

Первый поток читает, фильтрует и суммирует трафик, второй - обеспечивает выдачу данных в stat_displayer по запросу через POSIX Message Queue.

### stat_displayer

При запуске запрашивает статистику у stat_collector и выводит на экран.

# Сборка
    
    git clone https://github.com/bawltuhnuh/metrotek-1.git 
    cd metrotek-1
    make
    sudo make install

# Запуск

## stat_collector_v1

    sudo stat_collector_v1

## stat_collector_v2
    
    sudo stat_collector_v2

## stat_displayer
    
    sudo stat_displayer

# Результаты профилирования

# Авторство и лицензия

Автор: Ярославцева Александра
email: yaroslavceva_sasha@mail.ru  

Лицензия: GNU GPL
