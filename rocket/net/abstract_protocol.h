#ifndef ROCKET_RPC_NET_ABSTRACT_PROTOCOL_H
#define ROCKET_RPC_NET_ABSTRACT_PROTOCOL_H

#include <memory>

namespace rocket_rpc {

class AbstractProtocol {
  public:
    typedef std::shared_ptr<AbstractProtocol> s_ptr;

  private:

};

}

#endif

