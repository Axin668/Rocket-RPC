#include <memory>
#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>

#include "rocket/net/rpc/rpc_channel.h"
#include "rocket/net/rpc/rpc_controller.h"
#include "rocket/net/coder/tinypb_protocol.h"
#include "rocket/net/tcp/tcp_client.h"
#include "rocket/common/log.h"
#include "rocket/common/msg_id_util.h"
#include "rocket/common/error_code.h"

namespace rocket_rpc {

RpcChannel::RpcChannel(NetAddr::s_ptr peer_addr) : m_peer_addr(peer_addr) {
  m_client = std::make_shared<TcpClient>(m_peer_addr);
}

RpcChannel::~RpcChannel() {
  INFOLOG("~RpcChannel");
}

void RpcChannel::Init(controller_s_ptr controller, message_s_ptr req, message_s_ptr resp, closure_s_ptr done) {
  if (m_is_init) {
    return;
  }
  m_controller = controller;
  m_request = req;
  m_response = resp;
  m_closure = done;
  m_is_init = true;
}

void RpcChannel::CallMethod(const google::protobuf::MethodDescriptor* method,
                      google::protobuf::RpcController* controller, const google::protobuf::Message* request,
                      google::protobuf::Message* response, google::protobuf::Closure* done) {
  std::shared_ptr<TinyPBProtocol> req_protocol = std::make_shared<TinyPBProtocol>();
  
  RpcController* my_controller = dynamic_cast<RpcController*>(controller);
  if (my_controller == NULL) {
    ERRORLOG("failed callmethod, RpcController convert error");
    return;
  }

  if (my_controller->GetMsgId().empty()) {
    req_protocol->m_msg_id = MsgIDUtil::GenMsgID();
    my_controller->SetMsgId(req_protocol->m_msg_id);
  } else {
    req_protocol->m_msg_id = my_controller->GetMsgId();
  }

  req_protocol->m_method_name = method->full_name();
  INFOLOG("%s | call method name [%s]", req_protocol->m_msg_id.c_str(), req_protocol->m_method_name.c_str());

  if (!m_is_init) {

    std::string err_info = "RpcChannel not init";
    my_controller->SetError(ERROR_RPC_CHANNEL_INIT, err_info);
    ERRORLOG("%s | %s, RpcChannel not init", req_protocol->m_msg_id.c_str(), err_info.c_str());
    return;
  }

  // request 序列化
  if (!request->SerializeToString(&(req_protocol->m_pb_data))) {
    std::string err_info = "failed to serialize";
    my_controller->SetError(ERROR_FAILED_SERIALIZE, err_info);
    ERRORLOG("%s | %s, origin request [%s]", req_protocol->m_msg_id.c_str(), err_info.c_str(), request->ShortDebugString().c_str());
    return;
  }

  s_ptr channel = shared_from_this();  // 只可用智能指针构造, 不用裸指针 or 栈对象
  m_client->connect([req_protocol, channel]() mutable {
    channel->getTcpClient()->writeMessage(req_protocol, [req_protocol, channel](AbstractProtocol::s_ptr) mutable {
      INFOLOG("%s | send request success. method_name[%s]", 
        req_protocol->m_msg_id.c_str(), req_protocol->m_method_name.c_str());
        
      channel->getTcpClient()->readMessage(req_protocol->m_msg_id, [channel](AbstractProtocol::s_ptr msg) mutable {
        std::shared_ptr<TinyPBProtocol> resp_protocol = std::dynamic_pointer_cast<TinyPBProtocol>(msg);
        INFOLOG("%s | success get rpc response, call method name[%s]", 
          resp_protocol->m_msg_id.c_str(), resp_protocol->m_method_name.c_str());

        RpcController* my_controller = dynamic_cast<RpcController*>(channel->getController());
        if (!(channel->getResposne()->ParseFromString(resp_protocol->m_pb_data))) {
          ERRORLOG("%s | serialize error", resp_protocol->m_msg_id.c_str());
          my_controller->SetError(ERROR_FAILED_SERIALIZE, "serialize error");
          return;
        }
        
        if (resp_protocol->m_err_code != 0) {
          ERRORLOG("%s | call rpc method[%s] failed, error code[%d], error info [%s]", 
            resp_protocol->m_msg_id.c_str(), resp_protocol->m_method_name, 
            resp_protocol->m_err_code, resp_protocol->m_err_info);
          my_controller->SetError(resp_protocol->m_err_code, resp_protocol->m_err_info);
          return;
        }

        if (channel->getClosure()) {  // 执行 rpc 回调函数
          channel->getClosure()->Run();
        }

        channel.reset();  // 将 rpc channel 的引用计数减一, 方便析构
      });
    });
  });

}

google::protobuf::RpcController* RpcChannel::getController() {
  return m_controller.get();
}

google::protobuf::Message* RpcChannel::getRequest() {
  return m_request.get();
}

google::protobuf::Message* RpcChannel::getResposne() {
  return m_response.get();
}

google::protobuf::Closure* RpcChannel::getClosure() {
  return m_closure.get();
}

TcpClient* RpcChannel::getTcpClient() {
  return m_client.get();
}

}