#ifndef ROCKET_RPC_NET_RPC_RPC_CLOSURE_H
#define ROCKET_RPC_NET_RPC_RPC_CLOSURE_H

#include <google/protobuf/stubs/callback.h>
#include <functional>

namespace rocket_rpc {

class RpcClousre : public google::protobuf::Closure {
  public:

    RpcClousre(std::function<void()> cb) : m_cb(cb) {}

    void Run() override {
      if (m_cb != nullptr) {
        m_cb();;
      }
    }
  
  private:
    std::function<void()> m_cb {nullptr};
};

}

#endif