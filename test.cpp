#include <stdint.h>
#include <algorithm>
#include <gtest/gtest.h>

#define allocator_warn(x) fprintf(stderr, "allocator warn: %s", x)

#include "FirstFitAllocator.h"

namespace {

class FirstFitAllocatorTest : public ::testing::Test {
  protected:
    enum {
        MEMORY_SIZE = 1 * 1024 * 1024
    };
    void* memory_;
    FirstFitAllocator allocator_;

  FirstFitAllocatorTest() :
      memory_((void*)malloc(MEMORY_SIZE)),
      allocator_(FirstFitAllocator((uintptr_t)memory_, (uintptr_t)memory_ + MEMORY_SIZE))
  {
  }

  virtual ~FirstFitAllocatorTest() {
      free(memory_);
  }

};

TEST_F(FirstFitAllocatorTest, AllocateOnce) {
    void* p = allocator_.allocate(FirstFitAllocator::HEADER_SIZE);
    EXPECT_EQ(FirstFitAllocator::HEADER_SIZE, allocator_.getSize(p));
    EXPECT_EQ((uintptr_t)allocator_.getStart() + FirstFitAllocator::HEADER_SIZE, (uintptr_t)p);
}

TEST_F(FirstFitAllocatorTest, AllocateTwice) {
    void* first = allocator_.allocate(FirstFitAllocator::HEADER_SIZE);
    EXPECT_EQ(FirstFitAllocator::HEADER_SIZE, allocator_.getSize(first));
    void* second = allocator_.allocate(FirstFitAllocator::HEADER_SIZE);
    EXPECT_EQ((uintptr_t)allocator_.getStart() + FirstFitAllocator::HEADER_SIZE * 3, (uintptr_t)second);
}

TEST_F(FirstFitAllocatorTest, AllocateAndFree) {
    void* p = allocator_.allocate(FirstFitAllocator::HEADER_SIZE);
    EXPECT_EQ(MEMORY_SIZE - FirstFitAllocator::HEADER_SIZE * 2, allocator_.getFreeSize());
    EXPECT_TRUE(allocator_.free_no_compact(p));
    EXPECT_EQ(MEMORY_SIZE, allocator_.getFreeSize());
}

TEST_F(FirstFitAllocatorTest, AllocateAndFree2) {
    void* first = allocator_.allocate(FirstFitAllocator::HEADER_SIZE);
    void* second = allocator_.allocate(FirstFitAllocator::HEADER_SIZE);
    EXPECT_TRUE(allocator_.free(first));
    void* third = allocator_.allocate(FirstFitAllocator::HEADER_SIZE * 2);
    EXPECT_EQ(MEMORY_SIZE - FirstFitAllocator::HEADER_SIZE * 5, allocator_.getFreeSize());

    EXPECT_TRUE(allocator_.free(second));
    EXPECT_TRUE(allocator_.free(third));
    EXPECT_EQ(MEMORY_SIZE, allocator_.getFreeSize());
}

TEST_F(FirstFitAllocatorTest, AllocateAndFree3) {
    void* first = allocator_.allocate(FirstFitAllocator::HEADER_SIZE);
    void* second = allocator_.allocate(FirstFitAllocator::HEADER_SIZE);
    EXPECT_TRUE(allocator_.free(first));
    void* third = allocator_.allocate(MEMORY_SIZE - FirstFitAllocator::HEADER_SIZE * 5);
    ASSERT_TRUE(third != NULL);
    EXPECT_EQ(1, allocator_.getFreeListSize());
    EXPECT_EQ(FirstFitAllocator::HEADER_SIZE * 2, allocator_.getFreeSize());
    EXPECT_TRUE(allocator_.free(second));
    EXPECT_TRUE(allocator_.free(third));
    EXPECT_EQ(MEMORY_SIZE, allocator_.getFreeSize());
    EXPECT_EQ(1, allocator_.getFreeListSize());
}

TEST_F(FirstFitAllocatorTest, ExtraSizeIsEqualToHeaderSize) {
    void* first = allocator_.allocate(FirstFitAllocator::HEADER_SIZE * 2);
    void* second = allocator_.allocate(FirstFitAllocator::HEADER_SIZE);
    EXPECT_TRUE(allocator_.free(first));
    void* third = allocator_.allocate(FirstFitAllocator::HEADER_SIZE);
    // check there is no sizeByte=0 block
    EXPECT_EQ(1, allocator_.getFreeListSize());
    EXPECT_TRUE(allocator_.free(second));
    EXPECT_TRUE(allocator_.free(third));
    EXPECT_EQ(MEMORY_SIZE, allocator_.getFreeSize());
}

TEST_F(FirstFitAllocatorTest, FreeMiddle) {
    void* first = allocator_.allocate(FirstFitAllocator::HEADER_SIZE);
    void* second = allocator_.allocate(FirstFitAllocator::HEADER_SIZE);
    EXPECT_TRUE(allocator_.free_no_compact(first));
    EXPECT_EQ(MEMORY_SIZE - FirstFitAllocator::HEADER_SIZE *2, allocator_.getFreeSize());

    EXPECT_TRUE(allocator_.free_no_compact(second));
    EXPECT_EQ(MEMORY_SIZE, allocator_.getFreeSize());
    EXPECT_EQ(3, allocator_.getFreeListSize());
    allocator_.compact();
    EXPECT_EQ(1, allocator_.getFreeListSize());
    EXPECT_EQ(MEMORY_SIZE, allocator_.getFreeSize());
}

TEST_F(FirstFitAllocatorTest, FreeRight) {
    void* first = allocator_.allocate(FirstFitAllocator::HEADER_SIZE);
    void* second = allocator_.allocate(MEMORY_SIZE - FirstFitAllocator::HEADER_SIZE * 3);
    EXPECT_EQ(0, allocator_.getFreeSize());
    ASSERT_TRUE(second != NULL);
    EXPECT_TRUE(allocator_.free_no_compact(first));
    EXPECT_TRUE(allocator_.free_no_compact(second));
    allocator_.compact();
    EXPECT_EQ(1, allocator_.getFreeListSize());
}

TEST_F(FirstFitAllocatorTest, AllocateHuge) {
    void* p = allocator_.allocate(0x10000000);
    EXPECT_EQ(NULL, p);
}

TEST_F(FirstFitAllocatorTest, FreeNULL) {
    EXPECT_EQ(false, allocator_.free_no_compact(NULL));
}

TEST_F(FirstFitAllocatorTest, OverWrite) {
    void* first = allocator_.allocate(FirstFitAllocator::HEADER_SIZE);
    void* second = allocator_.allocate(FirstFitAllocator::HEADER_SIZE);
    // woops overwrite!
    memset(first, 0xff, 100);
    EXPECT_EQ(false, allocator_.free(second));
}

TEST_F(FirstFitAllocatorTest, OverWrite2) {
    void* first = allocator_.allocate(FirstFitAllocator::HEADER_SIZE);
    // woops overwrite!
    EXPECT_EQ(true, allocator_.free(first));
    EXPECT_EQ(false, allocator_.free(first));
}


TEST_F(FirstFitAllocatorTest, AllocateZero) {
    EXPECT_EQ(NULL, allocator_.allocate(0));
}

TEST_F(FirstFitAllocatorTest, Random) {
    for (int j = 0; j < 100; j++) {
        srand(time(NULL));
        std::vector<void*> allocated;
        for(int i = 0; i < 1000; i++) {
            int sizeToAlloc = rand() % 100 + 1;
            void* p = allocator_.allocate(sizeToAlloc);
            ASSERT_TRUE(p != NULL);
            allocated.push_back(p);
        }
        random_shuffle(allocated.begin(), allocated.end());
        for (std::vector<void*>::const_iterator it = allocated.begin();
             it != allocated.end(); ++it) {
            EXPECT_TRUE(allocator_.free(*it));
        }
        EXPECT_EQ(1, allocator_.getFreeListSize());
    }
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
