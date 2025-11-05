#ifndef INPERLOG_H
#define INPERLOG_H
#include"nlohmann/json.hpp"
#include <iostream>
#include <string>
#include <sstream>
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/logger.h"
 
#include "spdlog/sinks/stdout_sinks.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include <spdlog/details/log_msg.h>
#include <spdlog/details/backtracer.h>
#include "spdlog/common.h"
#include "spdlog/sinks/msvc_sink.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/async_logger.h"
#include "spdlog/async.h"
#include <thread>

using json = nlohmann::json;
using namespace spdlog;
// config:
// {
//     "SinkType":["console","msvc","file"],
//     "Pattern":"",
//     "Level":spdlog::level::trace,
//     "LogPath":""//保存的路径
//     "FileType":"Daily/Rotat"
// }
#define INITORNOT if(!isInit)                                \
				  return false;                               \




#define tracef(fmt,...) SPDLOG_LOGGER_TRACE(XuLog::Instance()->getLogger(),fmt,##__VA_ARGS__)
#define debugf(fmt,...) SPDLOG_LOGGER_DEBUG(XuLog::Instance()->getLogger(),fmt,##__VA_ARGS__)
#define infof(fmt,...) SPDLOG_LOGGER_INFO(XuLog::Instance()->getLogger(),fmt,##__VA_ARGS__)
#define warnf(fmt,...) SPDLOG_LOGGER_WARN(XuLog::Instance()->getLogger(),fmt,##__VA_ARGS__)
#define errorf(fmt,...) SPDLOG_LOGGER_ERROR(XuLog::Instance()->getLogger(),fmt,##__VA_ARGS__)
#define criticalf(fmt,...) SPDLOG_LOGGER_CRITICAL(XuLog::Instance()->getLogger(),fmt,##__VA_ARGS__)

typedef std::function<void(int,const std::string&, const std::string &,const std::string&)>logDataCallBackfun;
class __declspec(dllexport) XuLog
{
private:
    /* data */
    XuLog(/* args */);
    XuLog(const XuLog &log){};
   // XuLog &operator = (const XuLog &){return *this};
public:
    ~XuLog();
    static XuLog *Instance();

    void setConfig(const json &config);

    bool init();

    //void run();

	bool getIsInit()
	{
		return isInit;
	}
	std::shared_ptr<spdlog::logger> getLogger()
	{
		return hrg_logger;
	}
	json & getConfig()
	{
		return mConfig;
	}

    int getLogLevel();

    template<typename... Args>
	inline bool print_trace(format_string_t<Args...>  fmt, const Args &... args)
	{
		INITORNOT
		hrg_logger->trace(fmt, args...);
		return true;
	}
	
	template<typename... Args>
	inline bool print_debug(format_string_t<Args...>  fmt, const Args &... args)
	{
		INITORNOT
		hrg_logger->debug(fmt, args...);
		return true;
	}
	
	template<typename... Args>
	inline bool print_info(format_string_t<Args...>  fmt, const Args &... args)
	{
		//SPDLOG_LOGGER_INFO(hrg_logger, fmt, args...);
		hrg_logger->info(fmt, args...);
		return true;
	}

	template<typename... Args>
	inline bool print_warn(format_string_t<Args...> fmt, const Args &... args)
	{
		INITORNOT
		hrg_logger->warn(fmt, args...);
		return true;
	}
	
	template<typename... Args>
	inline bool print_error(format_string_t<Args...> fmt, const Args &... args)
	{
		INITORNOT
		hrg_logger->error(fmt, args...);
		return true;
	}
 
	template<typename... Args>
	inline bool print_critical(format_string_t<Args...> fmt, const Args &... args)
	{
		INITORNOT
		hrg_logger->critical(fmt, args...);
		return true;
    }
    void attachGetLogFun(logDataCallBackfun fun);

    void callBackLog(int type ,const std::string& strType, const std::string &logMessage,const std::string&time);
private:
    json mConfig;
	bool isInit;
    std::shared_ptr<spdlog::logger> hrg_logger;
	std::vector<spdlog::sink_ptr> sinks;
	std::thread *mDailyThread;
    logDataCallBackfun mDataFun;
};

#endif
