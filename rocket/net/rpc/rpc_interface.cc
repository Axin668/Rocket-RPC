#include "rocket/net/rpc/rpc_interface.h"
#include "rocket/net/rpc/rpc_closure.h"
#include "rocket/net/rpc/rpc_controller.h"
#include "rocket/common/log.h"

namespace rocket_rpc {

RpcInterface::RpcInterface(const google::protobuf::Message* req, google::protobuf::Message* resp, RpcClosure* done, RpcController* controller) 
  : m_req_base(req), m_resp_base(resp), m_done(done), m_controller(controller) {
  INFOLOG("RpcInterface");
}

RpcInterface::~RpcInterface() {
  INFOLOG("~RpcInterface");

  reply();
  destroy();

}

void RpcInterface::reply() {
  // reply to client
  // you should call it when you want to set response back
  // it means this rpc method done
  if (m_done) {
    m_done->Run();
  }
}

std::shared_ptr<RpcClosure> RpcInterface::newRpcClosure(std::function<void()>& cb) {
  return std::make_shared<RpcClosure>(shared_from_this(), cb);
}

void RpcInterface::destroy() {
  if (m_req_base) {
    delete m_req_base;
    m_req_base = NULL;
  }

  if (m_resp_base) {
    delete m_resp_base;
    m_resp_base = NULL;
  }

  if (m_done) {
    delete m_done;
    m_done = NULL;
  }

  if (m_controller) {
    delete m_controller;
    m_controller = NULL;
  }
}

}