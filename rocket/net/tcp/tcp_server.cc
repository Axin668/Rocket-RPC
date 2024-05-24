#include "rocket/net/tcp/tcp_server.h"
#include "rocket/net/eventloop.h"
#include "rocket/net/tcp/tcp_connection.h"
#include "rocket/common/log.h"
#include "rocket/common/config.h"

namespace rocket_rpc {

TcpServer::TcpServer(NetAddr::s_ptr local_addr) : m_local_addr(local_addr) {

  init();

  INFOLOG("rocket TcpServer listen success on [%s]", m_local_addr->toString().c_str());
}

TcpServer::~TcpServer() {
  if (m_main_event_loop) {
    delete m_main_event_loop;
    m_main_event_loop = NULL;
  }
}

void TcpServer::init() {
  
  m_acceptor = std::make_shared<TcpAcceptor>(m_local_addr);

  m_main_event_loop = EventLoop::GetCurrentEventLoop();
  m_io_thread_group = new IOThreadGroup(Config::GetGlobalConfig()->m_io_threads);

  m_listen_fd_event = new FdEvent(m_acceptor->getListenFd());
  m_listen_fd_event->listen(FdEvent::IN_EVENT, std::bind(&TcpServer::onAccept, this));

  m_main_event_loop->addEpollEvent(m_listen_fd_event);

}

void TcpServer::onAccept() {
  // 监听到客户端连接之后, 进行accept, 返回[客户端fd]以及[客户端网络地址]
  auto re = m_acceptor->accept();
  int client_fd = re.first;
  NetAddr::s_ptr peer_addr = re.second;
  m_client_counts ++ ;

  // 把 clientfd 添加到任意 IO 线程里面
  IOThread* io_thread = m_io_thread_group->getIOThread();
  TcpConnection::s_ptr connection = std::make_shared<TcpConnection>(io_thread->getEventLoop(), client_fd, 128, peer_addr, m_local_addr);
  connection->setState(Connected);

  // 客户端连接持久化, 防止析构
  m_client.insert(connection);

  INFOLOG("TcpServer succ get client, fd=%d", client_fd);
}

void TcpServer::start() {
  m_io_thread_group->start();
  m_main_event_loop->loop();
}

}