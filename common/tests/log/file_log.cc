/*******************************************
   Author        : Jun Zhang
   Email         : ewalker.zj@gmail.com
   Last Modified : 2019-05-03 11:59
   Filename      : file_log.cc
   Description   : 
*******************************************/

#include <iostream>
#include <chrono>
#include "spdlog/spdlog.h"
//#include "spdlog/sinks/sink.h"
//#include "spdlog/sinks/basic_file_sink.h"
//#include "spdlog/sinks/daily_file_sink.h"
//#include "spdlog/sinks/rotating_file_sink.h"
#include "hour_rotate_sink.h"

int main(int, char* [])
{
    //auto sharedFileSink = std::make_shared<spdlog::sinks::hour_file_sink>("basic.txt");
    auto hour_file_logger = spdlog::hour_logger_mt("hour_logger", "hour_log");

    auto sharedFileSink = std::make_shared<spdlog::sinks::hour_file_sink_mt>("basic.txt", 0);
    auto firstLogger = std::make_shared<spdlog::logger>("loggerOne", sharedFileSink);
    auto secondLogger = std::make_shared<spdlog::logger>("loggerTwo", sharedFileSink);
    

    int count  = 0;
    while( count++ < 10){
    hour_file_logger -> info("hour log");
    for(int i = 0; i < 10; i ++)
    {
        firstLogger->info("[loggerOne]: Hello {}.", i);
    }

    for(int j = 0; j < 10; j ++)
    {
        secondLogger->info("[loggerTwo]: Hello {}. ", j);
    }

    //spdlog::set_pattern("[%Y%m%d %H:%M:%S] [thread %t][%l] %v");
    spdlog::info("A conn down...");


    firstLogger -> flush_on(spdlog::level::err);
    firstLogger -> error("[loggerOne]: write immediately Hello {}. ", "first");

    secondLogger -> flush_on(spdlog::level::err);
    secondLogger -> error("[loggerTwo]: write immediately Hello {}. ", "second");
    
    hour_file_logger -> flush_on(spdlog::level::err);
    hour_file_logger -> error("hour log");

    std::this_thread::sleep_for(std::chrono::seconds(300));
    }

    double x = 4.1234567890;
    SPDLOG_INFO("Test: {}", x);
    SPDLOG_INFO("Test: {0:.8f}", x);

    return 0;
}
