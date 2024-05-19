#ifndef ROCKET_RPC_NET_TCP_TCP_CLIENT_H
#define ROCKET_RPC_NET_TCP_TCP_CLIENT_H

#include "rocket/net/tcp/net_addr.h"
#include "rocket/net/eventloop.h"
#include "rocket/net/tcp/tcp_connection.h"
#include "rocket/net/abstract_protocol.h"

namespace rocket_rpc {

class TcpClient {
  public:
    TcpClient(NetAddr::s_ptr peer_addr);

    ~TcpClient();

    // 异步地进行 connect
    // 如果 connect 成功, done 会被执行
    void connect(std::function<void()> done);

    // 异步地发送 Message
    // 如果发送 message 成功, 会调用 done 函数, 函数的入参就是 message 对象
    void writeMessage(AbstractProtocol::s_ptr request, std::function<void(AbstractProtocol::s_ptr)> done);

    // 异步地读取 message
    // 如果读取 message 成功, 会调用 done 函数, 函数的入参就是 message 对象
    void readMessage(AbstractProtocol::s_ptr request, std::function<void(AbstractProtocol::s_ptr)> done);

  private:
    NetAddr::s_ptr m_peer_addr;
    EventLoop* m_event_loop {NULL};

    int m_fd {-1};
    FdEvent* m_fd_event {NULL};

    TcpConnection::s_ptr m_connection;

};

}

#endif