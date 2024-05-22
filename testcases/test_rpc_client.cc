#include <assert.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <memory>
#include <unistd.h>
#include "rocket/common/log.h"
#include "rocket/common/config.h"
#include "rocket/net/tcp/tcp_client.h"
#include "rocket/net/tcp/net_addr.h"
#include "rocket/net/coder/string_coder.h"
#include "rocket/net/coder/abstract_protocol.h"
#include "rocket/net/coder/tinypb_protocol.h"
#include "rocket/net/rpc/rpc_dispatcher.h"
#include "rocket/net/rpc/rpc_controller.h"
#include "rocket/net/rpc/rpc_channel.h"
#include "rocket/net/rpc/rpc_closure.h"

#include "order.pb.h"

void test_tcp_client() {

  rocket_rpc::IPNetAddr::s_ptr addr = std::make_shared<rocket_rpc::IPNetAddr>("127.0.0.1", 12345);
  rocket_rpc::TcpClient client(addr);
  client.connect([addr, &client]() {
    DEBUGLOG("connect to [%s] success", addr->toString().c_str());
    std::shared_ptr<rocket_rpc::TinyPBProtocol> message = std::make_shared<rocket_rpc::TinyPBProtocol>();
    message->m_msg_id = "99999888888";
    message->m_pb_data = "test pb data";

    makeOrderRequest request;
    request.set_price(100);
    request.set_goods("apple");

    if (!request.SerializeToString(&(message->m_pb_data))) {
      ERRORLOG("serialize error");
      return;
    }

    message->m_method_name = "Order.makeOrder";

    client.writeMessage(message, [request](rocket_rpc::AbstractProtocol::s_ptr msg_ptr) {
      DEBUGLOG("send message success, request[%s]", request.ShortDebugString().c_str());
    });

    client.readMessage("99999888888", [](rocket_rpc::AbstractProtocol::s_ptr msg_ptr) {
      // 将父类的指针转化为子类的指针
      std::shared_ptr<rocket_rpc::TinyPBProtocol> message = std::dynamic_pointer_cast<rocket_rpc::TinyPBProtocol>(msg_ptr);
      DEBUGLOG("msg_id[%s], get response %s", message->m_msg_id.c_str(), message->m_pb_data.c_str());
      makeOrderResponse response;

      if (!response.ParseFromString(message->m_pb_data)) {
        ERRORLOG("deserialize error");
        return;
      }
      DEBUGLOG("get response success, respons[%s]", response.ShortDebugString().c_str());
    });
  });
}

void test_rpc_channel() {
  rocket_rpc::IPNetAddr::s_ptr addr = std::make_shared<rocket_rpc::IPNetAddr>("127.0.0.1", 12345);
  std::shared_ptr<rocket_rpc::RpcChannel> channel = std::make_shared<rocket_rpc::RpcChannel>(addr);

  std::shared_ptr<makeOrderRequest> request = std::make_shared<makeOrderRequest>();
  request->set_price(100);
  request->set_goods("apple");

  std::shared_ptr<makeOrderResponse> response = std::make_shared<makeOrderResponse>();

  std::shared_ptr<rocket_rpc::RpcController> controller = std::make_shared<rocket_rpc::RpcController>();
  controller->SetMsgId("999999888888");

  std::shared_ptr<rocket_rpc::RpcClousre> closure = std::make_shared<rocket_rpc::RpcClousre>([request, response, channel]() mutable {
    INFOLOG("call rpc successm request[%s], resposne[%s]", request->ShortDebugString().c_str(), response->ShortDebugString().c_str());
    INFOLOG("now exit eventloop");
    channel->getTcpClient()->stop();
    channel.reset();
  });

  channel->Init(controller, request, response, closure);

  Order_Stub stub(channel.get());

  stub.makeOrder(controller.get(), request.get(), response.get(), closure.get());
}

int main() {
  rocket_rpc::Config::SetGlobalConfig("../conf/rocket_rpc.xml");

  rocket_rpc::Logger::InitGlobalLogger();

  // test_tcp_client();

  test_rpc_channel();

  INFOLOG("test_rpc_channel end");

  return 0;
}