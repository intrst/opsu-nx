#pragma once
#include "opsu/util/Color.h"

namespace opsu {
namespace colors {

// The constants of fluddokt.opsu.fake.Color (Color.white etc.), same values.
inline const Color black{0.f, 0.f, 0.f};
inline const Color white{1.f, 1.f, 1.f};
inline const Color transparent{0.f, 0.f, 0.f, 0.5f};  // sic: the Java constant has alpha 0.5
inline const Color lightGray{0.8f, 0.8f, 0.8f};
inline const Color green{0.f, 1.f, 0.f};
inline const Color red{1.f, 0.f, 0.f};
inline const Color orange{1.f, 0.5f, 0.f};
inline const Color blue{0.f, 0.f, 1.f};
inline const Color lightgreen{0.5f, 1.f, 0.5f};
inline const Color lightred{1.f, 0.5f, 0.5f};
inline const Color lightorange{1.f, 0.75f, 0.5f};
inline const Color lightblue{0.5f, 0.5f, 1.f};
inline const Color gray{0.5f, 0.5f, 0.5f};
inline const Color magenta{1.f, 0.f, 1.f};
inline const Color cyan{0.f, 1.f, 1.f};
inline const Color darkGray{0.2f, 0.2f, 0.2f};
inline const Color yellow{1.f, 1.f, 0.f};

}  // namespace colors
}  // namespace opsu
