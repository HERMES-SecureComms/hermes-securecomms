#pragma once
#include "hermes_security.h"
// PUBLIC TEST KEYS: compiled only with the explicit secure-mock environment.
namespace hermes { namespace test_fixture {
inline security::Bytes key() {
  security::Bytes result(32); for(size_t i=0;i<32;++i) result[i]=i; return result;
}
inline security::Bytes publicKey() {
  return {0xd7,0x5a,0x98,0x01,0x82,0xb1,0x0a,0xb7,0xd5,0x4b,0xfe,0xd3,0xc9,0x64,0x07,0x3a,
          0x0e,0xe1,0x72,0xf3,0xda,0xa6,0x23,0x25,0xaf,0x02,0x1a,0x68,0xf7,0x07,0x51,0x1a};
}
inline security::PersistentState state() {
  security::PersistentState s; s.deviceId="HPTT-001";
  s.credentials.push_back({"GLOVE-001","OPERATOR-001","KEY-GLOVE-001-v1",security::Status::Active,0,{"HPTT-001","HPTT-002"},1});
  return s;
}
} }
