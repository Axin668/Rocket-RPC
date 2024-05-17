#include <memory>
#include "rocket/common/log.h"
#include "rocket/net/tcp/net_addr.h"
#include "rocket/net/tcp/tcp_server.h"

void test_tcp_server() {

  rocket_rpc::IPNetAddr::s_ptr addr = std::make_shared<rocket_rpc::IPNetAddr>("127.0.0.1", 12345);

  DEBUGLOG("create addr %s", addr->toString().c_str());

  rocket_rpc::TcpServer tcp_server(addr);

  tcp_server.start();
}

int main() {
  rocket_rpc::Config::SetGlobalConfig("../conf/rocket_rpc.xml");
  rocket_rpc::Logger::InitGlobalLogger();

  test_tcp_server();
}