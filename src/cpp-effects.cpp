// C++ Effects library
// Maciej Pirog, Huawei Edinburgh Research Centre, maciej.pirog@huawei.com
// License: MIT

#include "cpp-effects/cpp-effects.h"

namespace CppEffects {

std::list<MetaframeBase*> OneShot::Metastack;
TransferBase* OneShot::transferBuffer;
std::optional<TailAnswer> OneShot::tailAnswer = {};
InitMetastack OneShot::im;
int64_t OneShot::freshCounter = -2;

}
