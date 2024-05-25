#include "rocket/net/rpc/rpc_interface.h"
#include "rocket/net/rpc/rpc_closure.h"
#include "rocket/net/rpc/rpc_controller.h"
#include "rocket/common/log.h"

namespace rocket_rpc {

RpcInterface::RpcInterface(RpcClosure* done, RpcController* controller) : m_done(done), m_controller(controller) {

}

RpcInterface::~RpcInterface() {

}

void RpcInterface::reply() {
  // reply to client
  // you should call it when you want to set response back
  // it means this rpc method done
  if (m_done) {
    m_done->Run();
  }
}

}