// expected-error: a key can never be used
// @Temperature requires @Pressure and @Density, which exclude each other.
#include "typed_grammars.h"

using namespace OpenSMOKEpp::typed;
struct Status : Grammar<Status> {
  static constexpr auto keys = Keys(gas::T, gas::P, gas::Rho);
#ifdef DICTIONARY_PARSER_SHOULD_FAIL
  static constexpr auto constraints =
      Rules(Requires(gas::T, gas::P, gas::Rho), AtMostOneOf(gas::P, gas::Rho));
#else
  static constexpr auto constraints =
      Rules(RequiresAnyOf(gas::T, gas::P, gas::Rho), AtMostOneOf(gas::P, gas::Rho));
#endif
};

void use(OpenSMOKEpp::DictionaryManager &manager) {
  (void)manager.view<Status>("mixture");
}
