#ifndef ROCKET_RPC_COMMON_MSG_ID_UTIL_H
#define ROCKET_RPC_COMMON_MSG_ID_UTIL_H

#include <string>

namespace rocket_rpc {

class MsgIDUtil {

  public:
    static std::string GenMsgID();

};

}

#endif