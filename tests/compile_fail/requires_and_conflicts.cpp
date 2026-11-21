// expected-error: requires a key it conflicts with
#include "typed_grammars.h"

using namespace OpenSMOKEpp::typed;
struct Status : Grammar<Status> {
  static constexpr auto keys = Keys(gas::T, gas::P, gas::Rho);
#ifdef DICTIONARY_PARSER_SHOULD_FAIL
  static constexpr auto constraints = Rules(Requires(gas::T, gas::P), Conflicts(gas::P, gas::T));
#else
  static constexpr auto constraints = Rules(Requires(gas::T, gas::P), Conflicts(gas::P, gas::Rho));
#endif
};

void use(OpenSMOKEpp::DictionaryManager &manager) {
  (void)manager.view<Status>("mixture");
}
