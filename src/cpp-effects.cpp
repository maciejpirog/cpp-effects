// C++ Effects library
// Maciej Pirog, Huawei Edinburgh Research Centre, maciej.pirog@huawei.com
// License: MIT

#include "cpp-effects/cpp-effects.h"

namespace CppEffects {

std::optional<TailAnswer> OneShot::tailAnswer = {};
int64_t OneShot::freshCounter = -2;

}
