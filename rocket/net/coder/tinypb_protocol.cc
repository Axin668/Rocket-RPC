#include "rocket/net/coder/tinypb_protocol.h"

namespace rocket_rpc {

char TinyPBProtocol::PB_START = 0x02;
char TinyPBProtocol::PB_END = 0x03;

}