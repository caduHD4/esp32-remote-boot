#pragma once

// SinricPro 3.3.1 leaves these virtual base methods non-pure and undefined.
// The current upstream declares them pure virtual. Keeping the compatibility
// definitions in main.cpp's translation unit also avoids that release's
// non-inline queue definitions being emitted twice.
namespace SINRICPRO_NAMESPACE {

void SinricProInterface::sendMessage(JsonDocument&) {}

JsonDocument SinricProInterface::prepareEvent(String, const char*, const char*) {
  return JsonDocument();
}

unsigned long SinricProInterface::getTimestamp() {
  return 0;
}

bool SinricProInterface::isConnected() {
  return false;
}

unsigned long SinricProDeviceInterface::getTimestamp() {
  return 0;
}

}  // namespace SINRICPRO_NAMESPACE
