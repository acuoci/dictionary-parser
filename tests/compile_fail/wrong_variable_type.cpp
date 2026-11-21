// expected-error: does not have the type declared
// read(key, out) requires the exact type of the key (no silent narrowing).
#include "typed_grammars.h"

void use(const OpenSMOKEpp::typed::View<reactor::Batch> &batch) {
  int steps = 0;
  batch.read(reactor::Steps, steps);
#ifdef DICTIONARY_PARSER_SHOULD_FAIL
  float end_time = 0.f; // @EndTime is a double
  batch.read(reactor::EndTime, end_time);
#endif
}
