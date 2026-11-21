// expected-error: different value type
// A key with the name of a grammar key, but another value type, is rejected.
#include "typed_grammars.h"

using namespace OpenSMOKEpp::typed;
#ifdef DICTIONARY_PARSER_SHOULD_FAIL
inline constexpr auto RawT = Key<"@Temperature">(Real(), "Temperature as a number");
#endif

void use(const View<gas::Status> &mixture) {
  (void)mixture.get(gas::T);
#ifdef DICTIONARY_PARSER_SHOULD_FAIL
  (void)mixture.get(RawT);
#endif
}
