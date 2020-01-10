#include "DmcNode.hpp"
#include <cassert>


uint8_t DMCNode::getState() const { return uint8_t(((_nx0 & 0xFU) << 4U) | (_nx1 & 0xFU)); }

void DMCNode::setState(const uint8_t state) {
  _nx0 = (_nx0 & 0xfffffff0U) | (state >> 4U);
  _nx1 = (_nx1 & 0xfffffff0U) | (state & 0xFU);
}

uint32_t DMCNode::getNx0() const { return _nx0 >> 4U; }

void DMCNode::setNx0(const uint32_t nx0) {
  assert((nx0 >> 28) == 0);
  _nx0 = (_nx0 & 0xFU) | (nx0 << 4U);
}

uint32_t DMCNode::getNx1() const { return _nx1 >> 4U; }

void DMCNode::setNx1(const uint32_t nx1) {
  assert((nx1 >> 28) == 0);
  _nx1 = (_nx1 & 0xFU) | (nx1 << 4U);
}
