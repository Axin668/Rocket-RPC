#ifndef ROCKET_RPC_NET_WAKEUP_FDEVENT_H
#define ROCKET_RPC_NET_WAKEUP_FDEVNET_H

#include "rocket/net/fd_event.h"

namespace rocket_rpc {

class WakeUpFdEvent : public FdEvent {
  public:
    WakeUpFdEvent(int fd);

    ~WakeUpFdEvent();

    void wakeup();

  private:
};

}

#endif