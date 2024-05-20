#include <vector>
#include <string.h>
#include "rocket/net/coder/tinypb_coder.h"
#include "rocket/net/coder/tinypb_protocol.h"
#include "rocket/common/util.h"
#include "rocket/common/log.h"

namespace rocket_rpc {

// 将 message 对象转化为字节流, 写入到 buffer
void TinyPBCoder::encode(std::vector<AbstractProtocol::s_ptr>& messages, TcpBuffer::s_ptr out_buffer) {

}

// 将 buffer 里面的字节流转换为 message 对象
void TinyPBCoder::decode(std::vector<AbstractProtocol::s_ptr>& out_messages, TcpBuffer::s_ptr buffer) {
  
  while (1) {
    // 遍历 buffer, 找到 PB_START, 找到之后, 解析出整包的长度. 然后得到结束符的位置, 判断是否为 PB_END
    std::vector<char> tmp = buffer->m_buffer;
    int start_index = buffer->readIndex();
    int end_index = -1;

    int pk_len = 0;
    bool parse_success = false;
    int i = 0;
    for (i = start_index; i < buffer->writeIndex(); i ++ ) {
      if (tmp[i] == TinyPBProtocol::PB_START) {
        // 从下取四个字节, 由于是网络字节序, 需要转化为主机字节序
        if (i + 1 < buffer->writeIndex()) {
          pk_len = getInt32FromNetByte(&tmp[i + 1]);
          DEBUGLOG("get pk_len = %d", pk_len);

          // 结束符的索引
          int j = i + pk_len - 1;
          if (j >= buffer->writeIndex()) {
            continue;
          }
          if (tmp[j] == TinyPBProtocol::PB_END) {
            start_index = i;
            end_index = j;
            parse_success = true;
            break;
          }
        }
      }
    }

    if (parse_success) {
      buffer->moveReadIndex(end_index - start_index + 1);
      std::shared_ptr<TinyPBProtocol> message = std::make_shared<TinyPBProtocol>();
      message->m_pk_len = pk_len;

      int req_id_len_index = start_index + sizeof(char) + sizeof(message->m_pk_len);
      if (req_id_len_index >= end_index) {
        message->parse_success = false;
        ERRORLOG("parse error, req_id_len_index[%d] >= end_index[%d]", req_id_len_index, end_index);
      }
      message->m_req_id_len = getInt32FromNetByte(&tmp[req_id_len_index]);
      DEBUGLOG("parse req_id_len=%d", message->m_req_id_len);

      int req_id_index = req_id_len_index + sizeof(message->m_req_id_len);
      char req_id[100] = {0};
      memcpy(&req_id[0], &tmp[req_id_index], message->m_req_id_len);
      message->m_req_id = std::string(req_id);
      DEBUGLOG("parse req_id=%s", message->m_req_id.c_str());

      int method_name_len_index = req_id_index + message->m_req_id_len;
      if (method_name_len_index >= end_index) {
        message->parse_success = false;
        ERRORLOG("parse error, method_name_len_index[%d] >= end_index[%d]", method_name_len_index, end_index);
      }
      message->m_method_name_len = getInt32FromNetByte(&tmp[method_name_len_index]);

      int method_name_index = method_name_len_index + sizeof(message->m_method_name_len);
      char method_name[512] = {0};
      memcpy(&method_name[0], &tmp[method_name_index], message->m_method_name_len);
      message->m_method_name = std::string(method_name);
      DEBUGLOG("parse method_name=%s", message->m_method_name.c_str());

      int err_code_index = method_name_index + message->m_method_name_len;
      if (err_code_index >= end_index) {
        message->parse_success = false;
        ERRORLOG("parse error, err_code_index[%d] >= end_index[%d]", err_code_index, end_index);
      }
      message->m_err_code = getInt32FromNetByte(&tmp[err_code_index]);

      int error_info_len_index = err_code_index + sizeof(message->m_err_code);
      if (error_info_len_index >= end_index) {
        message->parse_success = false;
        ERRORLOG("parse error, error_info_len_index[%d] >= end_index[%d]", error_info_len_index, end_index);
      }
      message->m_err_info_len = getInt32FromNetByte(&tmp[error_info_len_index]);

      int error_info_index = error_info_len_index + sizeof(message->m_err_info_len);
      char error_info[512] = {0};
      memcpy(&error_info[0], &tmp[error_info_index], message->m_err_info_len);
      message->m_err_info = std::string(error_info);
      DEBUGLOG("parse error_info=%s", message->m_err_info.c_str());

      int pb_data_len = message->m_pk_len - message->m_method_name_len - message->m_req_id_len - message->m_err_info_len - 2 - 24;
      int pb_data_index = error_info_index + message->m_err_info_len;
      message->m_pb_data = std::string(&tmp[pb_data_index], pb_data_len);

      // 这里去解析校验和
      int check_sum_index = pb_data_index + pb_data_len;
      if (check_sum_index >= end_index) {
        message->parse_success = false;
        ERRORLOG("parse error, check_sum_index[%d] >= end_index[%d]", check_sum_index, end_index);
      }
      message->m_check_sum = getInt32FromNetByte(&tmp[check_sum_index]);

      message->parse_success = true;

      out_messages.push_back(message);
    }

    if (i >= buffer->writeIndex()) { // 如果读完所有buffer, 直接返回即可
      DEBUGLOG("decode end, read all buffer data");
      return;
    }

  }
}

}