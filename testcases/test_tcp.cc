#include "rocket/common/log.h"
#include "rocket/net/tcp/net_addr.h"

int main() {
  rocket_rpc::Config::SetGlobalConfig("../conf/rocket_rpc.xml");
  rocket_rpc::Logger::InitGlobalLogger();

  rocket_rpc::IPNetAddr addr("127.0.0.1", 12345);
  DEBUGLOG("create addr %s", addr.toString().c_str());
}