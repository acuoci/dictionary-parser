// expected-error: starts with '@'
// Keyword names are checked when the key is declared.
#include "typed_grammars.h"

using namespace OpenSMOKEpp::typed;
inline constexpr auto Good = Key<"@Temperature">(Real(), "Temperature");
#ifdef DICTIONARY_PARSER_SHOULD_FAIL
inline constexpr auto Bad = Key<"Temperature">(Real(), "Temperature");
#endif
