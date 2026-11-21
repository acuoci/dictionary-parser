// expected-error: View::read_if: the variable does not have the type declared
// read_if(key, out) requires the exact type of the key, as read(key, out).
#include "typed_grammars.h"

void use(const OpenSMOKEpp::typed::View<reactor::Batch> &batch) {
  int steps = 0;
  batch.read_if(reactor::Steps, steps);
#ifdef DICTIONARY_PARSER_SHOULD_FAIL
  unsigned int narrowed = 0; // @Steps is an int
  batch.read_if(reactor::Steps, narrowed);
#endif
}
