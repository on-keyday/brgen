/*license*/
#include "lexer.h"
static_assert(brgen::lexer::internal::check_lexer());
int main() {
    return brgen::lexer::internal::check_lexer() ? 0 : 1;
}