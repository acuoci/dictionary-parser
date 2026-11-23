// expected-error: View::get: the measures of this key take their dimension
// A key declared with Quantity() (no dimension) is read with get(key, dimension).
#include "typed_grammars.h"

namespace runtime {
using namespace OpenSMOKEpp::typed;
inline constexpr auto Value = Key<"@Value">(Quantity(), "A measure of any dimension");
struct Options : Grammar<Options> {
  static constexpr auto keys = Keys(Value);
};
} // namespace runtime

void use(const OpenSMOKEpp::typed::View<runtime::Options> &view) {
  (void)view.get(runtime::Value, OpenSMOKEpp::units::dim::Time);
#ifdef DICTIONARY_PARSER_SHOULD_FAIL
  (void)view.get(runtime::Value);
#endif
}
