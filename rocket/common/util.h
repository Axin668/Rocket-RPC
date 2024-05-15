#ifndef ROCKET_RPC_COMMON_UTIL_H
#define ROCKET_RPC_COMMON_UTIL_H

#include <sys/types.h>
#include <unistd.h>

namespace rocket_rpc {

pid_t getPid();

pid_t getThreadId();

int64_t getNowMs();

}

#endif