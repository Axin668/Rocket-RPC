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
      APPDEBUGLOG("start sleep 3s");
      sleep(3);
      APPDEBUGLOG("end sleep 3s");
      if (request->price() < 10) {
        response->set_ret_code(-1);
        response->set_res_info("short balance");
        return;
      }
      response->set_order_id("2024-05-21");
      APPDEBUGLOG("call makeOrder success");
      if (done) {
        done->Run();
        delete done;
        done = NULL;
      }
    }
};

int main(int argc, char* argv[]) {

  if (argc != 2) {
    printf("Start test_rpc_server error, argc not 2\n");
    printf("Start like this: \n");
    printf("/test_rpc_server ../conf/rocket_rpc.xml \n");
    return 0;
  }

  rocket_rpc::Config::SetGlobalConfig(argv[1]);

  rocket_rpc::Logger::InitGlobalLogger();

  std::shared_ptr<OrderImpl> service = std::make_shared<OrderImpl>();
  rocket_rpc::RpcDispatcher::GetRpcDispatcher()->registerService(service);

  rocket_rpc::IPNetAddr::s_ptr addr = std::make_shared<rocket_rpc::IPNetAddr>("127.0.0.1", rocket_rpc::Config::GetGlobalConfig()->m_port);

  rocket_rpc::TcpServer tcp_server(addr);

  tcp_server.start();

  return 0;
}