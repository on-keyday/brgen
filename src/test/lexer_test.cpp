/*license*/
#include <core/lexer/lexer.h>
#include <gtest/gtest.h>

TEST(LexerTest, LexerTest) {
    GTEST_ASSERT_TRUE(brgen::lexer::internal::check_lexer());
}
