#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include "rocket/common/log.h"
#include "rocket/net/tcp/tcp_client.h"
#include "rocket/net/eventloop.h"
#include "rocket/net/fd_event_group.h"

namespace rocket_rpc {

TcpClient::TcpClient(NetAddr::s_ptr peer_addr) : m_peer_addr(peer_addr) {
  m_event_loop = EventLoop::GetCurrentEventLoop();
  m_fd = socket(peer_addr->getFamily(), SOCK_STREAM, 0);

  if (m_fd < 0) {
    ERRORLOG("TcpClient::TcpClient() error, failed to create fd");
    return;
  }

  m_fd_event = FdEventGroup::GetFdEventGroup()->getFdEvent(m_fd);
  m_fd_event->setNonBlock();
  
  m_connection = std::make_shared<TcpConnection>(m_event_loop, m_fd, 128, peer_addr, TcpConnectionByClinet);
  m_connection->setConnectionType(TcpConnectionByClinet);
}

TcpClient::~TcpClient() {
  DEBUGLOG("TcpClient::~TcpClient")
  if (m_fd > 0) {
    close(m_fd);
  }
}

// 异步地进行 connect
// 如果 connect 成功, done 会被执行
void TcpClient::connect(std::function<void()> done) {
  int rt = ::connect(m_fd, m_peer_addr->getSockAddr(), m_peer_addr->getSockLen());
  if (rt == 0) {
    DEBUGLOG("connect [%s] success", m_peer_addr->toString().c_str());
    // m_connection->setState(Connected);
    if (done) {
      done();
    }
  } else if (rt == -1) {
    if (errno == EINPROGRESS) {
      // epoll 监听可写事件, 然后判断错误码
      m_fd_event->listen(FdEvent::OUT_EVENT, [this, done]() {
        int error = 0;
        socklen_t error_len = sizeof(error);
        getsockopt(m_fd, SOL_SOCKET, SO_ERROR, &error, &error_len);
        bool is_connect_succ = false;
        if (error == 0) {
          DEBUGLOG("connect [%s] success", m_peer_addr->toString().c_str());
          is_connect_succ = true;
          m_connection->setState(Connected);
        } else {
          ERRORLOG("connect error, errno=%d, error=%s", errno, strerror(errno));
        }
        // 只有一次机会
        // 连接完之后需要去掉可写事件的监听, 不然会一直触发
        m_fd_event->cancel(FdEvent::OUT_EVENT);
        m_event_loop->addEpollEvent(m_fd_event);

        if (is_connect_succ && done) { // 如果连接成功, 执行连接的回调
          done();
        }
      });
      m_event_loop->addEpollEvent(m_fd_event);

      if (!m_event_loop->isLooping()) {
        m_event_loop->loop();
      }

    } else {
      ERRORLOG("connect error, errno=%d, error=%s", errno, strerror(errno));
    }
  }

}

// 异步地发送 Message
// 如果发送 message 成功, 会调用 done 函数, 函数的入参就是 message 对象
void TcpClient::writeMessage(AbstractProtocol::s_ptr message, std::function<void(AbstractProtocol::s_ptr)> done) {
  // 1. 把 message 对象写入到 Connection 的 buffer, done 也写入
  // 2. 启动 connection 可写事件监听
  m_connection->pushSendMessage(message, done);
  m_connection->listenWrite();
}

// 异步地读取 message
// 如果读取 message 成功, 会调用 done 函数, 函数的入参就是 message 对象
void TcpClient::readMessage(AbstractProtocol::s_ptr message, std::function<void(AbstractProtocol::s_ptr)> done) {

}

}