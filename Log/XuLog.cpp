#include "XuLog.h"
#include <fstream>
#include<QFileInfo>
#include<QDate>
#include<QDateTime>
#include <QCoreApplication>
#define LOGNAME "inperlog"
#include <QString>
#include <QDir>
class CustomDailyFileSink : public spdlog::sinks::sink {
public:
    // 构造函数：接收文件路径、日期和轮换时间
    CustomDailyFileSink(const std::string& filename, int hour, int minute)
        : daily_sink_(filename, hour, minute) {}

protected:
    void log(const spdlog::details::log_msg& msg) override {

        std::lock_guard<std::mutex> lock(daily_sink_mutex_);
        daily_sink_.log(msg);  // 将日志传递给原始的 daily_file_sink_mt

        if(msg.level == spdlog::level::err || msg.level == spdlog::level::info || msg.level == spdlog::level::warn)
        {
            std::time_t time_t_tp = std::chrono::system_clock::to_time_t(msg.time);
            std::tm* tm = std::localtime(&time_t_tp);
            char time_str[64];
            auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(msg.time.time_since_epoch()) % 1000;
            std::strftime(time_str, sizeof(time_str), "%Y-%m-%dT%H:%M:%S", tm);
            std::snprintf(time_str + std::strlen(time_str), sizeof(time_str) - std::strlen(time_str), ".%03d", static_cast<int>(milliseconds.count()));
            QString file =  msg.source.filename;
            QStringList fileList = file.split("/");
//            std::string formatted_log = fmt::format("{:s}[{}]: {}\n",
//                                                fileList.size()>=2? fileList.last().toStdString():msg.source.filename,msg.source.line, msg.payload);
            std::string formatted_log = fmt::format("{}\n",msg.payload);
            string_view_t type = spdlog::level::to_string_view(msg.level);
            XuLog::Instance()->callBackLog(msg.level,std::string(type.data(), type.size()),formatted_log,std::string(time_str));
        }
    }

    void flush() override
    {
        std::lock_guard<std::mutex> lock(daily_sink_mutex_);
        daily_sink_.flush();
    }
    void set_pattern(const std::string &pattern) override
    {
        daily_sink_.set_pattern(pattern);
    };

    void set_formatter(std::unique_ptr<spdlog::formatter> sink_formatter) override
    {
        daily_sink_.set_formatter(std::move(sink_formatter));
    };
private:
    spdlog::sinks::daily_file_sink_mt daily_sink_;  // 组合 daily_file_sink_mt
    std::mutex daily_sink_mutex_;  // 保护对 daily_sink_ 的访问
};

std::mutex mConfigMutex;
static bool stop = false;
int toNextDay = 0;
XuLog::XuLog(/* args */):
isInit(false),
mDailyThread(nullptr)
{
}

XuLog::~XuLog()
{
    //spdlog::drop("multi_sink");
    stop = true;
    if(isInit)
    {
        spdlog::shutdown();
    }
}

XuLog *XuLog::Instance()
{
    static XuLog log;
    return &log;
}

void XuLog::setConfig(const json &config)
{
	mConfigMutex.lock();
    mConfig = config;
	mConfigMutex.unlock();
}
void dailyDeleteLogs()
{
    std::thread::id id = std::this_thread::get_id();
    std::stringstream sin;
    sin << id;
    infof("current thread id {}",sin.str());
    while (!stop)
    {
		mConfigMutex.lock();
        json config = XuLog::Instance()->getConfig();
		mConfigMutex.unlock();
		if (!config.contains("LogPath"))
		{
			continue;
        }
        if(toNextDay == 0)
        {
            int days = config.contains("SaveDays") ? config["SaveDays"].get<int>() : 10;
            QString runpath = QCoreApplication::applicationDirPath();
            QString logfile = QFileInfo(runpath).absoluteFilePath()+"/"+QString::fromStdString(config["LogPath"]);
            QStringList fileList;
            QDir sourceDir(QFileInfo(logfile).absolutePath());
            QFileInfoList fileInfoList = sourceDir.entryInfoList();
            foreach(QFileInfo fileInfo, fileInfoList)
            {
                //如果
                if(fileInfo.fileName() == "." || fileInfo.fileName() == "..")
                    continue;
                if(fileInfo.isDir())
                {
                    continue;
                }
                else if (fileInfo.isFile())
                {
                    fileList.push_back(fileInfo.absoluteFilePath());
                }
            }
            QDate currentDate = QDate::currentDate();
            for (size_t i = 0; i < fileList.size(); i++)
            {	
                QString tmp = QFileInfo(fileList.at(i)).baseName();
                QStringList tmplist = tmp.split("_");
                if (tmplist.size()!=2)
                {
                    continue;
                }
                QDate tmpDate = QDate::fromString(tmplist.at(1),"yyyy-MM-dd");
                if (tmpDate.daysTo(currentDate) > days)
                {
                    QFile tmpfile(fileList.at(i));
                    tmpfile.remove();
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
            }
            QDateTime now = QDateTime::currentDateTime();
            QDateTime now_jia1=now.addDays(1);
            now_jia1.setTime(QTime(0, 0));
            toNextDay = now.secsTo(now_jia1);
        }
        else
        {
            toNextDay --;
            if(toNextDay < 0)
            {
                toNextDay =0;
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }  
}
bool XuLog::init()
{
    //if (!mConfig.is_object())
    //{
    //  //  return false;
    //}
	//ifstream fin;
    if(isInit)
    {
        return true;
    }
    QString logPath = QCoreApplication::applicationDirPath()+"/log.config";
	std::ifstream ifs;
    ifs.open(logPath.toStdString(),std::ios::in);
    if (!ifs.is_open())
    {
        /* code */
        std::ofstream MyFile(logPath.toStdString());
        mConfig["SinkType"] = {"console","msvc","file"};//s
        mConfig["Pattern"] = "[%Y-%m-%d %H:%M:%S.%e][%l][%s][%#]%v";
        mConfig["Level"] = spdlog::level::info; //日志等级
        mConfig["FileType"] = "Daily"; //日志使用每天的还是 滚动的 Daily/Rotat
        mConfig["SaveDays"] = 30; //保存的时间
        mConfig["LogPath"] = "log/multisink.txt"; //保存的路径
        MyFile <<mConfig.dump(2);
        MyFile.close();
    }
    else
    {
        std::string buff;
		std::string config;
        while (getline(ifs,buff))
        {
			config += buff;
        }
        try
        {
            mConfig = json::parse(config);
        }
        catch (const nlohmann::json::parse_error& e) {
        }
    }
    
	spdlog::flush_every(std::chrono::seconds(1));
    spdlog::init_thread_pool(8192,1);	//spdlog::flush_on(spdlog::level::trace);
    bool needNewThread = false;
    if (mConfig.contains("SinkType") && mConfig["SinkType"].is_array())
    {  
        json &typearray = mConfig["SinkType"];
        std::string pattern = mConfig["Pattern"];
        for (size_t i = 0; i < typearray.size(); i++)
        {
            spdlog::sink_ptr sink;
            if (typearray[i] == "console")
            {
               sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            }
            if (typearray[i] == "msvc")
            {
               #ifdef WIN32
               sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
               #else
               continue;
               #endif
            }
            if (typearray[i] == "file")
            {
				std::string logpath = mConfig.contains("LogPath") ? mConfig["LogPath"] : "log/multisink.txt";
                QString logFullPath = QCoreApplication::applicationDirPath() + "/" + QString::fromStdString(logpath);
               if(mConfig.contains("FileType"))
               {
                    if(mConfig["FileType"] == "Daily")
                    {
                        //sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(logFullPath.toStdString(), 0, 0);//默认24点重新创建日志
                        sink = std::make_shared<CustomDailyFileSink>(logFullPath.toStdString(), 0, 0);
                        needNewThread = true;
                    }
                    else
                    {
                        sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(logFullPath.toStdString(), 1024*3,3);
                    }
               }
               else
               {
                    sink = std::make_shared<CustomDailyFileSink>(logFullPath.toStdString(), 0, 0);
                    //sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(logFullPath.toStdString(), 0,0);
                    needNewThread = true;
               }
            }    

			//sink->flush_every(std::chrono::seconds(1));
            sink->set_pattern(pattern);
            sinks.push_back(sink);
        }
        hrg_logger = std::make_shared<spdlog::async_logger>(LOGNAME, begin(sinks), end(sinks),spdlog::thread_pool(), spdlog::async_overflow_policy::block);
        //hrg_logger = std::make_shared<spdlog::logger>(LOGNAME, begin(sinks), end(sinks));
		hrg_logger->flush_on(spdlog::level::trace);
		if (mConfig.contains("Level"))
		{
			hrg_logger->set_level(mConfig["Level"]);
		}
		else
		{
			hrg_logger->set_level(spdlog::level::trace);
		}
		spdlog::register_logger(hrg_logger);
        if(needNewThread)
        {
            mDailyThread = new std::thread(dailyDeleteLogs);
            mDailyThread->detach();
        }
    }
    else
    {
        return false;
    }
    isInit = true;
    return true;
}

int XuLog::getLogLevel()
{
    if(hrg_logger)
    {
        return hrg_logger->level();
    }
    return -1;
}

void XuLog::attachGetLogFun(logDataCallBackfun fun)
{
    mDataFun = fun;
}

void XuLog::callBackLog(int type, const std::string &strType, const std::string &logMessage, const std::string &time)
{
    if(mDataFun)
    {
        mDataFun(type,strType,logMessage,time);
    }
}


