#include <memory>
#include <unistd.h>
#include <pthread.h>
#include <google/protobuf/service.h>
#include "rocket/common/log.h"
#include "rocket/net/tcp/net_addr.h"
#include "rocket/net/tcp/tcp_server.h"
#include "rocket/net/rpc/rpc_dispatcher.h"
#include "order.pb.h"

class OrderImpl : public Order {
  public:
    void makeOrder(google::protobuf::RpcController* controller,
                        const ::makeOrderRequest* request,
                        ::makeOrderResponse* response,
                        ::google::protobuf::Closure* done) {
      APPDEBUGLOG("start sleep 5s");
      sleep(5);
      APPDEBUGLOG("end sleep 5s");
      if (request->price() < 10) {
        response->set_ret_code(-1);
        response->set_res_info("short balance");
        return;
      }
      response->set_order_id("2024-05-21");
      APPDEBUGLOG("call makeOrder success");
    }
};

void test_tcp_server() {

  rocket_rpc::IPNetAddr::s_ptr addr = std::make_shared<rocket_rpc::IPNetAddr>("127.0.0.1", 12346);

  DEBUGLOG("create addr %s", addr->toString().c_str());

  rocket_rpc::TcpServer tcp_server(addr);

  tcp_server.start();
}

int main() {
  rocket_rpc::Config::SetGlobalConfig("../conf/rocket_rpc.xml");

  rocket_rpc::Logger::InitGlobalLogger();

  std::shared_ptr<OrderImpl> service = std::make_shared<OrderImpl>();
  rocket_rpc::RpcDispatcher::GetRpcDispatcher()->registerService(service);

  test_tcp_server();

  return 0;
}