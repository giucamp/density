
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "../test_framework/density_test_common.h"
#include "../test_framework/test_allocators.h"
#include "../test_framework/exception_tests.h"
#include <density/lifo.h>
#include <stack>

namespace density_tests
{
    /** Decorator to lifo_allocator that adds debug checks to detect violations of the LIFO order, wrong size passed
        to deallocate or reallocate and leaks of lifo blocks. Functions that can throw call exception_checkpoint()
        to allow exception testing. */
    template <typename UNDERLYING_ALLOCATOR, size_t ALIGNMENT = alignof(void*)>
        class DebugDataStack
    {
    public:

        /** same to lifo_allocator::alignment */
        static constexpr size_t alignment = ALIGNMENT;

        /** same to lifo_allocator::allocate */
        void * allocate(size_t i_size)
        {
            exception_checkpoint();
            auto const block = m_allocator.allocate(i_size);
            notify_alloc(block, i_size);
            return block;
        }

        /** same to lifo_allocator::allocate_empty */
        void * allocate_empty() noexcept
        {
            auto const block = m_allocator.allocate_empty();
            notify_alloc(block, 0);
            return block;
        }
            
        /** same to lifo_allocator::reallocate */
        void * reallocate(void * i_block, size_t i_old_size, size_t i_new_size)
        {
            exception_checkpoint();
            auto const new_block = m_allocator.reallocate(i_block, i_old_size, i_new_size);
            notify_dealloc(i_block, i_old_size);
            notify_alloc(new_block, i_new_size);
            return new_block;
        }

        /** same to lifo_allocator::deallocate */
        void deallocate(void * i_block, size_t i_size) noexcept
        {
            notify_dealloc(i_block, i_size);
            m_allocator.deallocate(i_block, i_size);
        }

        ~DebugDataStack()
        {
            DENSITY_TEST_ASSERT(m_blocks.empty());
        }

    private:

        void notify_alloc(void * i_block, size_t i_size)
        {
            m_blocks.push(Block{i_block, i_size});
        }

        void notify_dealloc(void * i_block, size_t i_size)
        {
            DENSITY_TEST_ASSERT(!m_blocks.empty() &&
                m_blocks.top().m_block == i_block &&
                m_blocks.top().m_size == i_size);
            m_blocks.pop();
        }

    private:
            
        struct Block
        {
            void * m_block;
            size_t m_size;
        };
        std::stack<Block> m_blocks; /**< used to check the LIFO order */

        density::lifo_allocator<UNDERLYING_ALLOCATOR, ALIGNMENT> m_allocator;
    };
} // namespace density_tests

#ifdef DENSITY_USER_DATA_STACK
    /** Override the data stack with a debug user defined data stack that:
            - detects violations of the LIFO constraint and leaks of lifo blocks
            - detects wrong sizes passed to deallocate or reallocate
            - detects leaks of pages or external blocks
            - allows testing the exceptional paths */
    namespace density
    {
        namespace user_data_stack
        {
            namespace
            {
                thread_local density_tests::DebugDataStack<density_tests::DeepTestAllocator<>, alignment> user_data_stack;
                    // alignment is defined in density_config.h (density::user_data_stack::alignment)
            }

            void * allocate(size_t i_size)
            {
                return user_data_stack.allocate(i_size);
            }

            void * allocate_empty() noexcept
            {
                return user_data_stack.allocate_empty();
            }
            
            void * reallocate(void * i_block, size_t i_old_size, size_t i_new_size)
            {
                return user_data_stack.reallocate(i_block, i_old_size, i_new_size);
            }

            void deallocate(void * i_block, size_t i_size) noexcept
            {
                user_data_stack.deallocate(i_block, i_size);
            }

        } // namespace user_data_stack
    
    } // namespace density

#endif // #ifdef DENSITY_USER_DATA_STACK