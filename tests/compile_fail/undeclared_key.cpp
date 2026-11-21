// expected-error: the key is not declared in this grammar
// Reading a key that the grammar does not declare does not compile.
#include "typed_grammars.h"

void use(const OpenSMOKEpp::typed::View<gas::Status> &mixture) {
  (void)mixture.get(gas::T);
#ifdef DICTIONARY_PARSER_SHOULD_FAIL
  (void)mixture.get(reactor::EndTime); // @EndTime is a key of reactor::Batch
#endif
}
