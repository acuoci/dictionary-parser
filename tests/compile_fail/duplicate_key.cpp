// expected-error: two keys have the same name
// An incoherent grammar does not compile as soon as it is used.
#include "typed_grammars.h"

using namespace OpenSMOKEpp::typed;
struct Status : Grammar<Status> {
#ifdef DICTIONARY_PARSER_SHOULD_FAIL
  static constexpr auto keys = Keys(gas::T, gas::P, gas::T);
#else
  static constexpr auto keys = Keys(gas::T, gas::P);
#endif
};

void use(OpenSMOKEpp::DictionaryManager &manager) {
  (void)manager.view<Status>("mixture");
}
