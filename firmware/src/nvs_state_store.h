#pragma once
#include <nvs.h>
#include "hermes_security.h"
namespace hermes {
class NvsStateStore : public security::StateStore {
 public:
  bool load(security::PersistentState& out) override {
    nvs_handle_t handle;
    lastError_ = nvs_open("hermes-v2", NVS_READONLY, &handle);
    if (lastError_ != ESP_OK) return false;
    size_t size = 0;
    lastError_ = nvs_get_blob(handle, "state", nullptr, &size);
    if (lastError_ != ESP_OK || !size || size > 65535) { nvs_close(handle); return false; }
    security::Bytes bytes(size);
    lastError_ = nvs_get_blob(handle, "state", bytes.data(), &size);
    nvs_close(handle);
    return lastError_ == ESP_OK && security::decodeState(bytes, out);
  }
  bool save(const security::PersistentState& state) override {
    security::PersistentState checked;
    auto bytes = security::encodeState(state);
    if (!security::decodeState(bytes, checked)) return false;
    nvs_handle_t handle;
    lastError_ = nvs_open("hermes-v2", NVS_READWRITE, &handle);
    if (lastError_ != ESP_OK) return false;
    lastError_ = nvs_set_blob(handle, "state", bytes.data(), bytes.size());
    if (lastError_ == ESP_OK) lastError_ = nvs_commit(handle);
    nvs_close(handle);
    return lastError_ == ESP_OK;
  }
  bool missing() const { return lastError_ == ESP_ERR_NVS_NOT_FOUND; }
 private:
  esp_err_t lastError_ = ESP_OK;
};
}
