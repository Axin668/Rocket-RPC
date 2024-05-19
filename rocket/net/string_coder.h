#ifndef ROCKET_RPC_NET_STRING_CODER_H
#define ROCKET_RPC_NET_STRING_CODER_H

#include "rocket/net/abstract_coder.h"
#include "rocket/net/abstract_protocol.h"

namespace rocket_rpc {

class StringProtocol : public AbstractProtocol {

  public:
    std::string info;
};

class StringCoder : public AbstractCoder {
  // 将 message 对象转化为字节流, 写入到 buffer
  void encode(std::vector<AbstractProtocol*>& messages, TcpBuffer::s_ptr out_buffer) {
    for (size_t i = 0; i < messages.size(); i ++ ) {
      StringProtocol* msg = dynamic_cast<StringProtocol*>(messages[i]);
      out_buffer->writeToBuffer(msg->info.c_str(), msg->info.length());
    }
  }

  // 将 buffer 里面的字节流转换为 message 对象
  void decode(std::vector<AbstractProtocol*>& out_messages, TcpBuffer::s_ptr buffer) {
    std::vector<char> re;
    buffer->readFromBuffer(re, buffer->readAble());
    std::string info;
    for (size_t i = 0; i < re.size(); i ++ ) {
      info += re[i];
    }

    StringProtocol* msg = new StringProtocol();
    msg->info = info;
    out_messages.push_back(msg);
  }

};

}

#endif