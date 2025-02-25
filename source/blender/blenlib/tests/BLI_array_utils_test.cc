/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_array_utils.h"
#include "BLI_array_utils.hh"
#include "BLI_utildefines.h"
#include "BLI_utildefines_stack.h"

/* -------------------------------------------------------------------- */
/* tests */

/* BLI_array_reverse */
TEST(array_utils, ReverseStringEmpty)
{
  char data[] = "";
  BLI_array_reverse(data, ARRAY_SIZE(data) - 1);
  EXPECT_STREQ("", data);
}

TEST(array_utils, ReverseStringSingle)
{
  char data[] = "0";
  BLI_array_reverse(data, ARRAY_SIZE(data) - 1);
  EXPECT_STREQ("0", data);
}

TEST(array_utils, ReverseString4)
{
  char data[] = "0123";
  BLI_array_reverse(data, ARRAY_SIZE(data) - 1);
  EXPECT_STREQ("3210", data);
}

TEST(array_utils, ReverseInt4)
{
  const int data_cmp[] = {3, 2, 1, 0};
  int data[] = {0, 1, 2, 3};
  BLI_array_reverse(data, ARRAY_SIZE(data));
  EXPECT_EQ_ARRAY(data_cmp, data, ARRAY_SIZE(data));
}

/* BLI_array_findindex */
TEST(array_utils, FindIndexStringEmpty)
{
  char data[] = "", find = '0';
  EXPECT_EQ(BLI_array_findindex(data, ARRAY_SIZE(data) - 1, &find), -1);
  EXPECT_EQ(BLI_array_rfindindex(data, ARRAY_SIZE(data) - 1, &find), -1);
}

TEST(array_utils, FindIndexStringSingle)
{
  char data[] = "0", find = '0';
  EXPECT_EQ(BLI_array_findindex(data, ARRAY_SIZE(data) - 1, &find), 0);
  EXPECT_EQ(BLI_array_rfindindex(data, ARRAY_SIZE(data) - 1, &find), 0);
}

TEST(array_utils, FindIndexStringSingleMissing)
{
  char data[] = "1", find = '0';
  EXPECT_EQ(BLI_array_findindex(data, ARRAY_SIZE(data) - 1, &find), -1);
  EXPECT_EQ(BLI_array_rfindindex(data, ARRAY_SIZE(data) - 1, &find), -1);
}

TEST(array_utils, FindIndexString4)
{
  char data[] = "0123", find = '3';
  EXPECT_EQ(BLI_array_findindex(data, ARRAY_SIZE(data) - 1, &find), 3);
  EXPECT_EQ(BLI_array_rfindindex(data, ARRAY_SIZE(data) - 1, &find), 3);
}

TEST(array_utils, FindIndexInt4)
{
  int data[] = {0, 1, 2, 3}, find = 3;
  EXPECT_EQ(BLI_array_findindex(data, ARRAY_SIZE(data), &find), 3);
  EXPECT_EQ(BLI_array_rfindindex(data, ARRAY_SIZE(data), &find), 3);
}

TEST(array_utils, FindIndexInt4_DupeEnd)
{
  int data[] = {0, 1, 2, 0}, find = 0;
  EXPECT_EQ(BLI_array_findindex(data, ARRAY_SIZE(data), &find), 0);
  EXPECT_EQ(BLI_array_rfindindex(data, ARRAY_SIZE(data), &find), 3);
}

TEST(array_utils, FindIndexInt4_DupeMid)
{
  int data[] = {1, 0, 0, 3}, find = 0;
  EXPECT_EQ(BLI_array_findindex(data, ARRAY_SIZE(data), &find), 1);
  EXPECT_EQ(BLI_array_rfindindex(data, ARRAY_SIZE(data), &find), 2);
}

TEST(array_utils, FindIndexPointer)
{
  const char *data[4] = {nullptr};
  STACK_DECLARE(data);

  STACK_INIT(data, ARRAY_SIZE(data));

  const char *a = "a", *b = "b", *c = "c", *d = "d";

#define STACK_PUSH_AND_CHECK_FORWARD(v, i) \
  { \
    STACK_PUSH(data, v); \
    EXPECT_EQ(BLI_array_findindex(data, STACK_SIZE(data), &(v)), i); \
  } \
  ((void)0)

#define STACK_PUSH_AND_CHECK_BACKWARD(v, i) \
  { \
    STACK_PUSH(data, v); \
    EXPECT_EQ(BLI_array_rfindindex(data, STACK_SIZE(data), &(v)), i); \
  } \
  ((void)0)

#define STACK_PUSH_AND_CHECK_BOTH(v, i) \
  { \
    STACK_PUSH(data, v); \
    EXPECT_EQ(BLI_array_findindex(data, STACK_SIZE(data), &(v)), i); \
    EXPECT_EQ(BLI_array_rfindindex(data, STACK_SIZE(data), &(v)), i); \
  } \
  ((void)0)

  STACK_PUSH_AND_CHECK_BOTH(a, 0);
  STACK_PUSH_AND_CHECK_BOTH(b, 1);
  STACK_PUSH_AND_CHECK_BOTH(c, 2);
  STACK_PUSH_AND_CHECK_BOTH(d, 3);

  STACK_POP(data);
  STACK_PUSH_AND_CHECK_BACKWARD(a, 3);

  STACK_POP(data);
  STACK_PUSH_AND_CHECK_FORWARD(a, 0);

  STACK_POP(data);
  STACK_POP(data);

  STACK_PUSH_AND_CHECK_BACKWARD(b, 2);
  STACK_PUSH_AND_CHECK_BACKWARD(a, 3);

#undef STACK_PUSH_AND_CHECK_FORWARD
#undef STACK_PUSH_AND_CHECK_BACKWARD
#undef STACK_PUSH_AND_CHECK_BOTH
}

/* BLI_array_binary_and */
#define BINARY_AND_TEST(data_cmp, data_a, data_b, data_combine, length) \
  { \
    BLI_array_binary_and(data_combine, data_a, data_b, length); \
    EXPECT_EQ_ARRAY(data_cmp, data_combine, length); \
  } \
  ((void)0)

TEST(array_utils, BinaryAndInt4Zero)
{
  const int data_cmp[] = {0, 0, 0, 0};
  int data_a[] = {0, 1, 0, 1}, data_b[] = {1, 0, 1, 0};
  int data_combine[ARRAY_SIZE(data_cmp)];
  BINARY_AND_TEST(data_cmp, data_a, data_b, data_combine, ARRAY_SIZE(data_cmp));
}

TEST(array_utils, BinaryAndInt4Mix)
{
  const int data_cmp[] = {1, 0, 1, 0};
  int data_a[] = {1, 1, 1, 1}, data_b[] = {1, 0, 1, 0};
  int data_combine[ARRAY_SIZE(data_cmp)];
  BINARY_AND_TEST(data_cmp, data_a, data_b, data_combine, ARRAY_SIZE(data_cmp));
}
#undef BINARY_AND_TEST

/* BLI_array_binary_or */
#define BINARY_OR_TEST(data_cmp, data_a, data_b, data_combine, length) \
  { \
    BLI_array_binary_or(data_combine, data_a, data_b, length); \
    EXPECT_EQ_ARRAY(data_combine, data_cmp, length); \
  } \
  ((void)0)

TEST(array_utils, BinaryOrInt4Alternate)
{
  int data_a[] = {0, 1, 0, 1}, data_b[] = {1, 0, 1, 0}, data_cmp[] = {1, 1, 1, 1};
  int data_combine[ARRAY_SIZE(data_cmp)];
  BINARY_OR_TEST(data_cmp, data_a, data_b, data_combine, ARRAY_SIZE(data_cmp));
}

TEST(array_utils, BinaryOrInt4Mix)
{
  int data_a[] = {1, 1, 0, 0}, data_b[] = {0, 0, 1, 0}, data_cmp[] = {1, 1, 1, 0};
  int data_combine[ARRAY_SIZE(data_cmp)];
  BINARY_OR_TEST(data_cmp, data_a, data_b, data_combine, ARRAY_SIZE(data_cmp));
}
#undef BINARY_OR_TEST

/* BLI_array_deduplicate_ordered */
#define DEDUPLICATE_ORDERED_TEST(data, data_cmp) \
  { \
    const uint data_len_new = BLI_array_deduplicate_ordered(data, ARRAY_SIZE(data)); \
    EXPECT_EQ(data_len_new, ARRAY_SIZE(data_cmp)); \
    EXPECT_EQ_ARRAY(data, data_cmp, data_len_new); \
    /* Ensure running a second time does nothing. */ \
    const uint data_len_test = BLI_array_deduplicate_ordered(data, data_len_new); \
    EXPECT_EQ(data_len_test, ARRAY_SIZE(data_cmp)); \
    EXPECT_EQ_ARRAY(data, data_cmp, data_len_new); \
  } \
  ((void)0)

TEST(array_utils, DeduplicateOrdered1)
{
  int data[] = {0};
  const int data_cmp[] = {0};
  DEDUPLICATE_ORDERED_TEST(data, data_cmp);
}

TEST(array_utils, DeduplicateOrdered2)
{
  int data[] = {1, 2};
  const int data_cmp[] = {1, 2};
  DEDUPLICATE_ORDERED_TEST(data, data_cmp);
}

TEST(array_utils, DeduplicateOrdered2Same)
{
  int data[] = {1, 1};
  const int data_cmp[] = {1};
  DEDUPLICATE_ORDERED_TEST(data, data_cmp);
}

TEST(array_utils, DeduplicateOrdered3Same)
{
  int data[] = {1, 1, 1};
  const int data_cmp[] = {1};
  DEDUPLICATE_ORDERED_TEST(data, data_cmp);
}

TEST(array_utils, DeduplicateOrdered3)
{
  int data[] = {3, 3, 2, 2, 1, 1};
  const int data_cmp[] = {3, 2, 1};
  DEDUPLICATE_ORDERED_TEST(data, data_cmp);
}

#undef DEDUPLICATE_ORDERED_TEST

static void find_all_ranges_test(const blender::Span<bool> data,
                                 const blender::Span<blender::IndexRange> data_cmp)
{
  using namespace blender;
  Vector<IndexRange> ranges = array_utils::find_all_ranges(data, true);
  EXPECT_EQ(ranges.size(), data_cmp.size());
  EXPECT_EQ_ARRAY(data_cmp.data(), ranges.as_span().data(), data_cmp.size());
}

TEST(array_utils, FindAllRanges1)
{
  using namespace blender;
  const std::array data = {false};
  Vector<IndexRange> ranges = array_utils::find_all_ranges(Span(data.data(), data.size()), true);
  EXPECT_EQ(ranges.size(), 0);
}

TEST(array_utils, FindAllRanges2)
{
  using namespace blender;
  const std::array data = {true, true, true};
  const std::array data_cmp = {IndexRange(0, 3)};
  find_all_ranges_test(data, data_cmp);
}

TEST(array_utils, FindAllRanges3)
{
  using namespace blender;
  const std::array data = {true, false};
  const std::array data_cmp = {IndexRange(0, 1)};
  find_all_ranges_test(data, data_cmp);
}

TEST(array_utils, FindAllRanges4)
{
  using namespace blender;
  const std::array data = {false, true};
  const std::array data_cmp = {IndexRange(1, 1)};
  find_all_ranges_test(data, data_cmp);
}

TEST(array_utils, FindAllRanges5)
{
  using namespace blender;
  const std::array data = {true, false, false, true, true, false, true};
  const std::array data_cmp = {IndexRange(0, 1), IndexRange(3, 2), IndexRange(6, 1)};
  find_all_ranges_test(data, data_cmp);
}
