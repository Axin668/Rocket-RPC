#ifndef ROCKET_RPC_COMMON_RUN_TIME_H
#define ROCKET_RPC_COMMON_RUN_TIME_H

#include <string>

namespace rocket_rpc {

class RunTime {
  public:
  
  public:
    static RunTime* GetRunTime();
  
  public:
    std::string m_msgid;
    std::string m_method_name;

};

}

#endif
