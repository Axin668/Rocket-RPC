#include <sys/timerfd.h>
#include <string.h>
#include "rocket/net/timer.h"
#include "rocket/common/log.h"
#include "rocket/common/util.h"

namespace rocket_rpc {

Timer::Timer() : FdEvent() {

  m_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
  DEBUGLOG("timer fd=%d", m_fd);

  // 把 fd 可读事件放到了 eventloop 上监听
  listen(FdEvent::IN_EVENT, std::bind(&Timer::onTimer, this));
}

Timer::~Timer() {
}

void Timer::onTimer() {

  // DEBUGLOG("onTimer");

  // 处理缓冲区数据, 防止下一次继续触发可读事件
  char buf[8];
  while (1) {
    if ((read(m_fd, buf, 8) == -1) && errno == EAGAIN) {
      break;
    }
  }

  // 执行定时任务
  int64_t now = getNowMs();

  std::vector<TimerEvent::s_ptr> tmps;
  std::vector<std::pair<int64_t, std::function<void()>>> tasks;

  ScopeMutex<Mutex> lock(m_mutex);
  auto it = m_pending_events.begin();

  for (it = m_pending_events.begin(); it != m_pending_events.end(); ++it) {
    if ((*it).first > now) {
      break;
    }
    if (!(*it).second->isCanceled()) {
      tmps.push_back((*it).second);
      tasks.push_back(std::make_pair((*it).second->getArriveTime(), (*it).second->getCallBack()));
    }
  }

  m_pending_events.erase(m_pending_events.begin(), it);
  lock.unlock();

  // 需要把重复的 Event 再次添加进去
  for (auto i = tmps.begin(); i != tmps.end(); ++i) {
    if ((*i)->isRepeated()) {
      // 调整 arriveTime
      (*i)->resetArriveTime();
      addTimerEvent(*i);
    }
  }

  // 确保定时器的有效, 即[维护距离它最近执行的任务的差值interval作为超时时间]
  resetArriveTime();

  // 执行任务
  for (auto i : tasks) {
    if (i.second) {
      i.second();
    }
  }
}

void Timer::resetArriveTime() {
  ScopeMutex<Mutex> lock(m_mutex);
  auto tmp = m_pending_events;
  lock.unlock();

  if (tmp.size() == 0) {
    return;
  }

  int64_t now = getNowMs();

  auto it = tmp.begin();
  int64_t interval = 0;
  // 距离当前时间最近的任务还未超时
  if (it->second->getArriveTime() > now) {
    interval = it->second->getArriveTime() - now;
  } else { // 已超时, 给一个默认最小间隔时间(100ms), 赶紧执行
    interval = 100;
  }

  timespec ts;
  memset(&ts, 0, sizeof(ts));
  ts.tv_sec = interval / 1000;
  ts.tv_nsec = (interval % 1000) * 1000000;

  itimerspec value;
  memset(&value, 0, sizeof(value));
  value.it_value = ts;

  // 设置定时器下一次到期时间为value
  int rt = timerfd_settime(m_fd, 0, &value, NULL);
  if (rt != 0) {
    ERRORLOG("timerfd_settime error, errno=%d, error=%s", errno, strerror(errno));
  }
  // DEBUGLOG("timer reset to %lld", now + interval);
}

void Timer::addTimerEvent(TimerEvent::s_ptr event) {
  bool is_reset_timerfd = false;

  ScopeMutex<Mutex> lock(m_mutex);
  if (m_pending_events.empty()) {
    is_reset_timerfd = true;
  } else {
    auto it = m_pending_events.begin();
    // 如果把当前定时任务加入阻塞队列, 可能它会最先执行, 此时需要重设定时器(调整interval间隔)
    // 因为定时器只会维护距离它最近的任务的超时时间
    if ((*it).second->getArriveTime() > event->getArriveTime()) {
      is_reset_timerfd = true;
    }
  }
  m_pending_events.emplace(event->getArriveTime(), event);
  lock.unlock();

  if (is_reset_timerfd) {
    resetArriveTime();
  }
}

void Timer::deleteTimerEvent(TimerEvent::s_ptr event) {
  event->setCanceled(true);

  ScopeMutex<Mutex> lock(m_mutex);

  auto begin = m_pending_events.lower_bound(event->getArriveTime());
  auto end = m_pending_events.upper_bound(event->getArriveTime());

  auto it = begin;
  for (it = begin; it != end; ++it ) {
    if (it->second == event) {
      break;
    }
  }

  if (it != end) {
    m_pending_events.erase(it);
  }
  lock.unlock();
  DEBUGLOG("success delete TimerEvent at arrive time %lld", event->getArriveTime());
}

}