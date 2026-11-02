// Force-included when building with Microsoft's compiler (see CMakeLists.txt).
// The Peaks/stmlib code uses GCC-style "__attribute__((always_inline))"
// annotations, which MSVC doesn't understand. They are only optimisation hints,
// so it is safe to drop them.
#pragma once

#ifdef _MSC_VER
#define __attribute__(x)
#endif
