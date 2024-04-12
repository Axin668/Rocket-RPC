#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>
#include "rocket/common/util.h"

namespace rocket_rpc {

static int g_pid = 0;

static thread_local int g_thread_id = 0;

pid_t getPid() {
  if (g_pid != 0) {
    return g_pid;
  }
  return g_pid = getpid();
}

pid_t getThreadId() {
  if (g_thread_id != 0) {
    return g_thread_id;
  }
  return g_thread_id = syscall(SYS_gettid);
}

}