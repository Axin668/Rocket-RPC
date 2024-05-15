#include "rocket/net/io_thread_group.h"
#include "rocket/common/log.h"

namespace rocket_rpc {

IOThreadGroup::IOThreadGroup(int size) : m_size(size) {
  m_io_thread_groups.resize(size);
  for (size_t i = 0; i < size; i ++ ) {
    m_io_thread_groups[i] = new IOThread();
  }
}

IOThreadGroup::~IOThreadGroup() {

}

void IOThreadGroup::start() {
  for (size_t i = 0; i < m_size; i ++ ) {
    m_io_thread_groups[i]->start();
  }
}

void IOThreadGroup::join() {
  for (size_t i = 0; i < m_size; i ++ ) {
    m_io_thread_groups[i]->join();
  }
}

IOThread* IOThreadGroup::getIOThread() {
  if (m_index == m_io_thread_groups.size() || m_index == -1) {
    m_index = 0;
  }
  return m_io_thread_groups[m_index ++ ];
}

}