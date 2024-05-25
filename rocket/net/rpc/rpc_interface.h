#ifndef ROCKET_RPC_NET_RPC_RPC_INTERFACE_H
#define ROCKET_RPC_NET_RPC_RPC_INTERFACE_H

#include "rocket/net/rpc/rpc_closure.h"
#include "rocket/net/rpc/rpc_controller.h"

namespace rocket_rpc {

/**
 * Rpc Interface Base Class
 * All interfaces should extend this abstract class
*/

class RpcInterface {
  public:

    RpcInterface(RpcClosure* done, RpcController* controller);

    virtual ~RpcInterface();

    // core business deal method
    virtual void run() = 0;

    // set error code and error into to response message
    virtual void setError(long long code, const std::string& err_info) = 0;

    // reply to client
    // you should call it when you want to set response back
    // it means this rpc method done
    virtual void reply();
  
  protected:
    RpcClosure* m_done {NULL};  // callback

    RpcController* m_controller {NULL};

};

}

#endif