// expected-error: a rule names a key that is not in 'keys'
#include "typed_grammars.h"

using namespace OpenSMOKEpp::typed;
struct Status : Grammar<Status> {
  static constexpr auto keys = Keys(gas::T, gas::P);
#ifdef DICTIONARY_PARSER_SHOULD_FAIL
  static constexpr auto constraints = Rules(ExactlyOneOf(gas::T, gas::Rho));
#else
  static constexpr auto constraints = Rules(ExactlyOneOf(gas::T, gas::P));
#endif
};

void use(OpenSMOKEpp::Dictionary &dictionary) {
  (void)attach<Status>(dictionary);
}
