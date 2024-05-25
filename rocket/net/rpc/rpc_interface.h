#ifndef ROCKET_RPC_NET_RPC_RPC_INTERFACE_H
#define ROCKET_RPC_NET_RPC_RPC_INTERFACE_H

#include <memory>
#include <google/protobuf/message.h>
#include "rocket/net/rpc/rpc_controller.h"

namespace rocket_rpc {

class RpcClosure;

/**
 * Rpc Interface Base Class
 * All interfaces should extend this abstract class
*/

class RpcInterface : public std::enable_shared_from_this<RpcInterface> {
  public:

    RpcInterface(const google::protobuf::Message* req, google::protobuf::Message* resp, RpcClosure* done, RpcController* controller);

    virtual ~RpcInterface();

    // core business deal method
    virtual void run() = 0;

    // reply to client
    // you should call it when you want to set response back
    // it means this rpc method done
    void reply();

    // free resourse
    void destroy();

    // alloc a closure object which handle by this interface
    std::shared_ptr<RpcClosure> newRpcClosure(std::function<void()>& cb);

    // set error code and error into to response message
    virtual void setError(int code, const std::string& err_info) = 0;
  
  protected:

    const google::protobuf::Message* m_req_base {NULL};

    google::protobuf::Message* m_resp_base {NULL};

    RpcClosure* m_done {NULL};  // callback

    RpcController* m_controller {NULL};

};

}

#endif