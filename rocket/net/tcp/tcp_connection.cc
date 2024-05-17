#include <unistd.h>
#include "rocket/common/log.h"
#include "rocket/net/fd_event_group.h"
#include "rocket/net/tcp/tcp_connection.h"

namespace rocket_rpc {

TcpConnection::TcpConnection(IOThread* io_thread, int fd, int buffer_size, NetAddr::s_ptr peer_addr)
  : m_io_thread(io_thread), m_peer_addr(peer_addr), m_state(NotConnected), m_fd(fd) {

  // 初始化连接的 buffer
  m_in_buffer = std::make_shared<TcpBuffer>(buffer_size);
  m_out_buffer = std::make_shared<TcpBuffer>(buffer_size);

  // 初始化 fd event 以及绑定读入事件
  m_fd_event = FdEventGroup::GetFdEventGroup()->getFdEvent(fd);
  m_fd_event->setNonBlock();
  m_fd_event->listen(FdEvent::IN_EVENT, std::bind(&TcpConnection::onRead, this));

  // 将 fd event 添加至 子线程 eventloop 循环
  io_thread->getEventLoop()->addEpollEvent(m_fd_event);
}

TcpConnection::~TcpConnection() {
  DEBUGLOG("~TcpConnection")
}

void TcpConnection::onRead() {
  // 1. 从 socket 缓冲区, 调用系统的 read 函数读取字节到 in_buffer
  if (m_state != Connected) {
    ERRORLOG("onRead error, client has already disconnected, addr[%s], clientfd[%d]", m_peer_addr->toString().c_str(), m_fd);
    return;
  }

  bool is_read_all = false;
  bool is_close = false;
  while (!is_read_all) { // 尽可能全部读完
    if (m_in_buffer->writeAble() == 0) {
      m_in_buffer->resizeBuffer(2 * m_in_buffer->m_buffer.size());
    }
    int read_count = m_in_buffer->writeAble(); // 表示当前可写的最大字节数
    int write_index = m_in_buffer->writeIndex();

    int rt = read(m_fd, &(m_in_buffer->m_buffer[write_index]), read_count);
    if (rt > 0) {
      DEBUGLOG("success read %d bytes from addr[%s], client fd[%d]", rt, m_peer_addr->toString().c_str(), m_fd);
      m_in_buffer->moveWriteIndex(rt);
      if (rt == read_count) { // 可能没读完
        continue;
      } else if (rt < read_count) { // 已经读完了([实际读回]的比[最大可写]的要少)
        is_read_all = true;
        break;
      }
    } else if (rt == 0) { // 对端连接已关闭
      is_close = true;
      break;
    } else if (rt == -1 && errno == EAGAIN) { // 读不到数据了
      is_read_all = true;
      break;
    }
  }

  if (is_close) {
    // TODO 处理关闭连接
    INFOLOG("peer closed, peer addr [%d], clientfd [%d]", m_peer_addr->toString().c_str(), m_fd);
    clear();
    return; // 不要执行 execute 了
  }

  if (!is_read_all) {
    ERRORLOG("not read all data");
  }

  // TODO: 简单的 echo, 后面补充 RPC 协议解析
  execute();
}

void TcpConnection::execute() {
  // 将 RPC 请求 执行业务逻辑, 获取 RPC 响应, 再把 RPC 响应发送回去
  std::vector<char> tmp;
  int size = m_in_buffer->readAble();
  tmp.resize(size);
  m_in_buffer->readFromBuffer(tmp, size);

  std::string msg;
  for (size_t i = 0; i < tmp.size(); i ++ ) {
    msg += tmp[i];
  }
  
  INFOLOG("success get request[%s] from client[%s]", msg.c_str(), m_peer_addr->toString().c_str());

  m_out_buffer->writeToBuffer(msg.c_str(), msg.length());

  m_fd_event->listen(FdEvent::OUT_EVENT, std::bind(&TcpConnection::onWrite, this));
  m_io_thread->getEventLoop()->addEpollEvent(m_fd_event);

}

void TcpConnection::onWrite() {
  // 将当前 out_buffer 里面的数据全部发送给 Client

  if (m_state != Connected) {
    ERRORLOG("onWrite error, client has already disconnected, addr[%s], clientfd[%d]", m_peer_addr->toString().c_str(), m_fd);
    return;
  }

  bool is_write_all = false;
  while(true) { // 尽可能全部写完
    if (m_out_buffer->readAble() == 0) {
      DEBUGLOG("no data need to send to client [%s]", m_peer_addr->toString().c_str());
      is_write_all = true;
      break;
    }
    int write_size = m_out_buffer->readAble(); // 表示当前可读的最大字节数
    int read_index = m_out_buffer->readIndex();
    int rt = write(m_fd, &(m_out_buffer->m_buffer[read_index]), write_size);

    if (rt >= write_size) { // [实际写入]的比[最大可读]的还要大, 说明写完了 ??????
      DEBUGLOG("no data need to send to client [%s]", m_peer_addr->toString().c_str());
      is_write_all = true;
      break;
    } else if (rt == -1 && errno == EAGAIN) { // 写入 socket 发送缓冲区失败
      // 发送缓冲区已满, 不能再发送了
      // 这种情况下我们等下次 fd 可写的时候再次发送数据即可
      ERRORLOG("write data error, errno==EAGAIN and rt == -1");
      break;
    }
  }
  if (is_write_all) {
    m_fd_event->cancel(FdEvent::OUT_EVENT);
    m_io_thread->getEventLoop()->addEpollEvent(m_fd_event); // 清空可写事件
    // note: 不是 deleteEpollEvent, 否则读写事件都被删除
  }
}

void TcpConnection::setState(const TcpState state) {
  m_state = state;

}

TcpState TcpConnection::getState() {
  return m_state;
}

void TcpConnection::clear() {
  // 处理一些关闭连接后的清理动作
  if (m_state == Closed) {
    return;
  }
  m_fd_event->cancel(FdEvent::IN_EVENT);
  m_fd_event->cancel(FdEvent::OUT_EVENT);

  m_io_thread->getEventLoop()->deleteEpollEvent(m_fd_event);

  m_state = Closed;
}

void TcpConnection::shutdown() {
  if (m_state == Closed || m_state == NotConnected) {
    return;
  }

  // 处于半关闭
  m_state = HalfClosing;

  // 调用 shutdown 关闭读写, 意味着服务器不会再对这个 fd 进行读写操作了
  // 发送 FIN 报文, 触发了四次挥手的第一个阶段
  // 当 fd 发生可读事件, 但是可读的数据为 0, 即 对端也发送了 FIN
  ::shutdown(m_fd, SHUT_RDWR);
}

}