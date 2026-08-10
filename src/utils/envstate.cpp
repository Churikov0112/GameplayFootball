#include "envstate.hpp"

#include "../base/math/vector3.hpp"
#include "../base/math/quaternion.hpp"

// EnvState overloads for Vector3/Quaternion: these classes have virtual
// destructors (vptr), so memcpy would capture the vtable address which is
// stable within one binary but changes between rebuilds. Serialize only the
// data members instead.

void EnvState::process(blunted::Vector3 &value) {
  process(value.coords[0]);
  process(value.coords[1]);
  process(value.coords[2]);
}

void EnvState::process(blunted::Quaternion &value) {
  process(value.elements[0]);
  process(value.elements[1]);
  process(value.elements[2]);
  process(value.elements[3]);
}
