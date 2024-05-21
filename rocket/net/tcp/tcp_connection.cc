#include <unistd.h>
#include "rocket/common/log.h"
#include "rocket/net/fd_event_group.h"
#include "rocket/net/tcp/tcp_connection.h"
#include "rocket/net/coder/string_coder.h"
#include "rocket/net/coder/tinypb_coder.h"

namespace rocket_rpc {

TcpConnection::TcpConnection(EventLoop* event_loop, int fd, int buffer_size, NetAddr::s_ptr peer_addr, NetAddr::s_ptr local_addr, TcpConnectionType type /*=TcpConnectionByServer*/)
  : m_event_loop(event_loop), m_local_addr(local_addr), m_peer_addr(peer_addr), m_state(NotConnected), m_fd(fd), m_connection_type(type) {

  // 初始化连接的 buffer
  m_in_buffer = std::make_shared<TcpBuffer>(buffer_size);
  m_out_buffer = std::make_shared<TcpBuffer>(buffer_size);

  // 初始化 fd event 以及绑定读入事件
  m_fd_event = FdEventGroup::GetFdEventGroup()->getFdEvent(fd);
  m_fd_event->setNonBlock();

  m_coder = new TinyPBCoder();

  if (m_connection_type == TcpConnectionByServer) {
    // 如果是服务端的连接, 直接将 fd event 添加至 子线程 eventloop 循环进行监听
    listenRead();
  }
}

TcpConnection::~TcpConnection() {
  DEBUGLOG("TcpConnection::~TcpConnection")
  if (m_coder) {
    delete m_coder;
    m_coder = NULL;
  }
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
  if (m_connection_type == TcpConnectionByServer) { // 服务端读逻辑(主动)
    // 将 RPC 请求 执行业务逻辑, 获取 RPC 响应, 再把 RPC 响应发送回去
    std::vector<AbstractProtocol::s_ptr> result;
    std::vector<AbstractProtocol::s_ptr> reply_messages;
    m_coder->decode(result, m_in_buffer);
    for (size_t i = 0; i < result.size(); i ++ ) {
      // 1. 针对每一个请求, 调用 rpc 方法, 获取响应 message
      // 2. 将响应 message 放入到发送缓冲区, 监听可写事件进行回包
      INFOLOG("success get request[%s] from client[%s]", result[i]->m_req_id.c_str(), m_peer_addr->toString().c_str());

      std::shared_ptr<TinyPBProtocol> message = std::make_shared<TinyPBProtocol>();
      // message->m_pb_data = "hello, this is rocket rpc test data";
      // message->m_req_id = result[i]->m_req_id;

      RpcDispatcher::GetRpcDispatcher()->dispatch(result[i], message, this);
      reply_messages.emplace_back(message);
    }
  
    m_coder->encode(reply_messages, m_out_buffer);
    listenWrite();
    
  } else { // 客户端读逻辑(被动)
    // 从 buffer 里 decode 得到 message 对象, 并执行其回调
    std::vector<AbstractProtocol::s_ptr> result;
    m_coder->decode(result, m_in_buffer);

    for (size_t i = 0; i < result.size(); i ++ ) { // 执行客户端连接所有的读回调, 执行后清空
      std::string req_id = result[i]->m_req_id;
      auto it = m_read_dones.find(req_id);
      if (it != m_read_dones.end()) {
        it->second(result[i]);
      }
      // TODO 清空
    }
  }

}

void TcpConnection::onWrite() {
  // 将当前 out_buffer 里面的数据全部发送给 Client

  if (m_state != Connected) {
    ERRORLOG("onWrite error, client has already disconnected, addr[%s], clientfd[%d]", m_peer_addr->toString().c_str(), m_fd);
    return;
  }

  if (m_connection_type == TcpConnectionByClinet) { // 客户端写逻辑(主动)
    // 1. 将 message encode 得到字节流
    // 2. 将数据写入到 buffer 里面, 然后全部发送

    std::vector<AbstractProtocol::s_ptr> messages;
    
    for (size_t i = 0; i < m_write_dones.size(); i ++ ) {
      messages.push_back(m_write_dones[i].first);
    }

    m_coder->encode(messages, m_out_buffer);
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
    m_event_loop->addEpollEvent(m_fd_event); // 清空可写事件
    // note: 不是 deleteEpollEvent, 否则读写事件都被删除
  }

  if (m_connection_type == TcpConnectionByClinet) { // 执行客户端连接所有的写回调, 执行后清空
    for (size_t i = 0; i < m_write_dones.size(); i ++ ) {
      m_write_dones[i].second(m_write_dones[i].first);
    }
    m_write_dones.clear();
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

  m_event_loop->deleteEpollEvent(m_fd_event);

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

void TcpConnection::setConnectionType(TcpConnectionType type) {
  m_connection_type = type;
}

void TcpConnection::listenWrite() {
  
  m_fd_event->listen(FdEvent::OUT_EVENT, std::bind(&TcpConnection::onWrite, this));
  m_event_loop->addEpollEvent(m_fd_event);
}

void TcpConnection::listenRead() {

  m_fd_event->listen(FdEvent::IN_EVENT, std::bind(&TcpConnection::onRead, this));
  m_event_loop->addEpollEvent(m_fd_event);
}

void TcpConnection::pushSendMessage(AbstractProtocol::s_ptr message, std::function<void(AbstractProtocol::s_ptr)> done) {
  m_write_dones.push_back(std::make_pair(message, done));
}

void TcpConnection::pushReadMessage(const std::string& req_id, std::function<void(AbstractProtocol::s_ptr)> done) {
  m_read_dones.insert(std::make_pair(req_id, done));
}

NetAddr::s_ptr TcpConnection::getLocalAddr() {
  return m_local_addr;
}

NetAddr::s_ptr TcpConnection::getPeerAddr() {
  return m_peer_addr;
}

}