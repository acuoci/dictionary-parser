// expected-error: must be a typed grammar
// Views exist only for typed grammars.
#include "typed_grammars.h"

struct Plain : OpenSMOKEpp::DictionaryGrammar {};

void use(OpenSMOKEpp::DictionaryManager &manager) {
  (void)manager.view<gas::Status>("mixture");
#ifdef DICTIONARY_PARSER_SHOULD_FAIL
  (void)manager.view<Plain>("mixture");
#endif
}
