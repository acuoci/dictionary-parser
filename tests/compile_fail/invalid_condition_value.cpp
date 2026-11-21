// expected-error: never have
// A condition on an enumeration must use one of its values.
#include "typed_grammars.h"

using namespace OpenSMOKEpp::typed;
struct Batch : Grammar<Batch> {
  static constexpr auto keys = Keys(reactor::Type, reactor::Profile);
#ifdef DICTIONARY_PARSER_SHOULD_FAIL
  static constexpr auto constraints =
      Rules(When(Equals(reactor::Type, "NonIsotermal"), Required(reactor::Profile)));
#else
  static constexpr auto constraints =
      Rules(When(Equals(reactor::Type, "NonIsothermal"), Required(reactor::Profile)));
#endif
};

void use(OpenSMOKEpp::DictionaryManager &manager) {
  (void)manager.view<Batch>("batch");
}
