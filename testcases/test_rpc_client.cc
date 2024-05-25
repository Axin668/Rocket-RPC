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
  // 创建 channel 
  // rocket_rpc::IPNetAddr::s_ptr addr = std::make_shared<rocket_rpc::IPNetAddr>("127.0.0.1", 12345);
  // std::shared_ptr<rocket_rpc::RpcChannel> channel = std::make_shared<rocket_rpc::RpcChannel>(addr);
  NEWRPCCHANNEL("127.0.0.1:12346", channel);

  // 创建请求和响应
  // std::shared_ptr<makeOrderRequest> request = std::make_shared<makeOrderRequest>();
  // std::shared_ptr<makeOrderResponse> response = std::make_shared<makeOrderResponse>();
  NEWMESSAGE(makeOrderRequest, request);
  NEWMESSAGE(makeOrderResponse, response);
  request->set_price(100);
  request->set_goods("apple");

  // 创建 controller
  // std::shared_ptr<rocket_rpc::RpcController> controller = std::make_shared<rocket_rpc::RpcController>();
  NEWRPCCONTROLLER(controller);
  controller->SetMsgId("999999888888");
  controller->SetTimeout(10000);

  // 创建回调函数, 业务逻辑
  std::shared_ptr<rocket_rpc::RpcClosure> closure = std::make_shared<rocket_rpc::RpcClosure>(nullptr, [request, response, channel, controller]() mutable {
    if (controller->GetErrorCode() == 0) {
      INFOLOG("call rpc success, request[%s], resposne[%s]", request->ShortDebugString().c_str(), response->ShortDebugString().c_str());
      // 业务逻辑
      if (response->order_id() == "xxx") {
        // xx
      }
    } else {
      ERRORLOG("call rpc failed, request[%s], error code[%d], error info[%s]", 
        request->ShortDebugString().c_str(), 
        controller->GetErrorCode(), controller->GetErrorInfo().c_str());
    }
    INFOLOG("now exit eventloop");
    // channel->getTcpClient()->stop();
    channel.reset();
  });

  // 调用 RPC 请求
  // channel->Init(controller, request, response, closure);
  // Order_Stub stub(channel.get());
  // stub.makeOrder(controller.get(), request.get(), response.get(), closure.get());
  CALLRPC("127.0.0.1:12346", Order_Stub, makeOrder, controller, request, response, closure);
}

int main() {
  rocket_rpc::Config::SetGlobalConfig(NULL);

  // 设置为1表示需要读取配置文件(一般为server端), 设置为0表示不需要读取配置文件(一般为client端)
  rocket_rpc::Logger::InitGlobalLogger(0);

  // test_tcp_client();

  test_rpc_channel();

  INFOLOG("test_rpc_channel end");

  return 0;
}