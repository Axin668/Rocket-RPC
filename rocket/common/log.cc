#include <sys/time.h>
#include <sstream>
#include <stdio.h>
#include <assert.h>
#include "rocket/common/log.h"
#include "rocket/common/util.h"
#include "rocket/common/config.h"
#include "rocket/net/eventloop.h"

namespace rocket_rpc {

static Logger* g_logger = nullptr;

Logger* Logger::GetGlobalLogger() {
  return g_logger;
}

Logger::Logger(LogLevel level) : m_set_level(level) {

}

void Logger::init() {
  printf("开始初始化日志器\n");
  m_async_logger = std::make_shared<AsyncLogger>(
    Config::GetGlobalConfig()->m_log_file_name,
    Config::GetGlobalConfig()->m_log_file_path,
    Config::GetGlobalConfig()->m_log_max_file_size);
  printf("异步日志器构造完成\n");
  // 注意解决循环依赖问题(构建Logger需要TimerEvent, TimerEvent又引用了Logger::DEBUGLOG, 所以需要二段构造)
  m_timer_event = std::make_shared<TimerEvent>(Config::GetGlobalConfig()->m_log_sync_interval, true, std::bind(&Logger::syncLoop, this)); 
  EventLoop::GetCurrentEventLoop()->addTimerEvent(m_timer_event);
  printf("添加定时事件完成\n");
}

void Logger::syncLoop() {
  // 同步 m_buffer 到 async_logger 的 buffer 队尾
  printf("开始同步数据到异步日志器\n");
  std::vector<std::string> tmp_vec;
  ScopeMutex<Mutex> lock(m_mutex);
  tmp_vec.swap(m_buffer);
  lock.unlock();

  printf("axin akak\n");
  printf("push tmp.size(%d)\n", tmp_vec.size());  
  if (!tmp_vec.empty()) {
    printf("push tmp.size(%d)\n", tmp_vec.size());
    m_async_logger->pushLogBuffer(tmp_vec);
  }
}

void Logger::InitGlobalLogger() {
  LogLevel global_log_level = StringToLogLevel(Config::GetGlobalConfig()->m_log_level);
  printf("Init log level [%s]\n", LogLevelToString(global_log_level).c_str());
  g_logger = new Logger(global_log_level);
  g_logger->init();
}

std::string LogLevelToString(LogLevel level) {
  switch (level) {
  case Debug:
    return "DEBUG";

  case Info:
    return "INFO";

  case Error:
    return "ERROR"; 
  default:
    return "UNKNOWN";
  }
}

LogLevel StringToLogLevel(const std::string& log_level) {
  if (log_level == "DEBUG") {
    return Debug;
  } else if (log_level == "INFO") {
    return Info;
  } else if (log_level == "ERROR") {
    return Error;
  }
  return Unknown;
}

std::string LogEvent::toString() {
  struct timeval now_time;

  gettimeofday(&now_time, nullptr);

  struct tm now_time_t;
  localtime_r(&(now_time.tv_sec), &now_time_t);

  char buf[128];
  strftime(&buf[0], 128, "%y-%m-%d %H:%M:%S", &now_time_t);
  std::string time_str(buf);
  int ms = now_time.tv_usec / 1000;
  time_str = time_str + "." + std::to_string(ms);

  m_pid = getPid();
  m_thread_id = getThreadId();

  std::stringstream ss;

  ss << "[" << LogLevelToString(m_level) << "]\t"
    << "[" << time_str << "]\t"
    << "[" << m_pid << ":" << m_thread_id << "]\t";
  
  return ss.str();
}

void Logger::pushLog(const std::string& msg) {
  ScopeMutex<Mutex> lock(m_mutex);
  m_buffer.push_back(msg);
  lock.unlock();
}

void Logger::log() {

}

AsyncLogger::AsyncLogger(const std::string& file_name, const std::string& file_path, int max_file_size)
  : m_file_name(file_name), m_file_path(file_path), m_max_file_size(max_file_size) {

  sem_init(&m_semaphore, 0, 0);

  assert(pthread_create(&m_thread, NULL, &AsyncLogger::Loop, this) == 0);

  // assert(pthread_cond_init(&m_condition, NULL) == 0);
  
  sem_wait(&m_semaphore);
  printf("异步构造被唤醒\n");
}

void* AsyncLogger::Loop(void * arg) {
  // 将 buffer 里边的全部数据打印到文件中, 然后线程睡眠, 直到有新的数据再重复这个过程

  AsyncLogger* logger = reinterpret_cast<AsyncLogger*>(arg);

  assert(pthread_cond_init(&logger->m_condition, NULL) == 0);

  sem_post(&logger->m_semaphore);
  printf("开始唤起\n");

  while (1) {
    ScopeMutex<Mutex> lock(logger->m_mutex);
    while (logger->m_buffer.empty()) {
      printf("begin pthread_cond_wait back \n");
      pthread_cond_wait(&(logger->m_condition), logger->m_mutex.getMutex());
    }
    printf("pthread_cond_wait back \n");

    std::vector<std::string> tmp;
    tmp.swap(logger->m_buffer.front());
    logger->m_buffer.pop();

    lock.unlock();

    timeval now;
    gettimeofday(&now, NULL);

    struct tm now_time;
    localtime_r(&(now.tv_sec), &now_time);

    const char* format = "%Y%m%d";
    char date[32];
    strftime(date, sizeof(date), format, &now_time);

    if (std::string(date) != logger->m_date) {
      logger->m_no = 0;
      logger->m_reopen_flag = true;
      logger->m_date = std::string(date);
    }
    if (logger->m_file_handler == NULL) {
      logger->m_reopen_flag = true;
    }

    std::stringstream ss;
    ss << logger->m_file_path << logger->m_file_name << "_"
      << std::string(date) << "_log.";
    std::string log_file_name = ss.str() + std::to_string(logger->m_no);

    if (logger->m_reopen_flag) {
      if (logger->m_file_handler) {
        fclose(logger->m_file_handler);
      }
      logger->m_file_handler = fopen(log_file_name.c_str(), "a");  // 追加写入
      logger->m_reopen_flag = false;
    }

    if (ftell(logger->m_file_handler) > logger->m_max_file_size) {
      fclose(logger->m_file_handler);

      log_file_name = ss.str() + std::to_string(logger->m_no++);
      logger->m_file_handler = fopen(log_file_name.c_str(), "a");  // 追加写入
      logger->m_reopen_flag = false;
    }

    for (auto& i : tmp) {
      if (!i.empty()) {
        fwrite(i.c_str(), 1, i.length(), logger->m_file_handler);
      }
    }
    fflush(logger->m_file_handler);

    if (logger->m_stop_flag) {
      // logger->flush();
      return NULL;
    }
  }

  return NULL;
}

void AsyncLogger::stop() {
  m_stop_flag = true;
}

void AsyncLogger::flush() {
  if (m_file_handler) {
    fflush(m_file_handler);
  }
}

void AsyncLogger::pushLogBuffer(std::vector<std::string>& vec) {
  ScopeMutex<Mutex> lock(m_mutex);
  m_buffer.push(vec);
  // 这时候需要唤醒异步日志线程(异步线程没有 buffer 数据打印就会沉睡)
  pthread_cond_signal(&m_condition);
  lock.unlock();

  printf("pthread_cond_signal\n");
}

}