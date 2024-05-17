#ifndef ROCKET_RPC_NET_FD_EVENT_GROUP_H
#define ROCKET_RPC_NET_FD_EVENT_GROUP_H

#include <vector>
#include "rocket/common/mutex.h"
#include "rocket/net/fd_event.h"

namespace rocket_rpc {

// 用于快速取出某个 fd event 来和 tcp server 接收 accept 之后生成的 fd 进行绑定
class FdEventGroup {
  public:
    FdEventGroup(int size);

    ~FdEventGroup();

    FdEvent* getFdEvent(int fd);

  public:
    static FdEventGroup* GetFdEventGroup();

  private:
    int m_size {0};
    std::vector<FdEvent*> m_fd_group;
    Mutex m_mutex;

};

}

#endif