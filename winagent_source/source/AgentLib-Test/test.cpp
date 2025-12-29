#include "pch.h"

// Basic test to verify the test framework
TEST(SanityCheck, BasicAssertion) {
  EXPECT_EQ(1, 1);
  EXPECT_TRUE(true);
}

// Main function provided by Google Test