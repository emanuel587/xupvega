#include <kepler/turbomodule/KeplerTurboModuleRegistration.h>
#include "turbo-modules/XuperDevice.h"
extern "C" {
__attribute__((visibility("default"))) void autoLinkKeplerTurboModulesV1() noexcept {
  KEPLER_REGISTER_TURBO_MODULE(XuperDeviceNative, XuperDevice);
}
}
