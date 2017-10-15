
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "../test_framework/density_test_common.h"
#include "../test_framework/test_allocators.h"
#include "../test_framework/exception_tests.h"
#include <density/lifo.h>
#include <vector>
#include <iostream>
#include <algorithm>

namespace density_tests
{
    /** Decorator to lifo_allocator that adds debug checks to detect violations of the LIFO order, wrong size passed
        to deallocate or reallocate and leaks of lifo blocks. Functions that can throw call exception_checkpoint()
        to allow exception testing. */
    template <typename UNDERLYING_ALLOCATOR, size_t ALIGNMENT = alignof(void*)>
        class DebugDataStack : private IAllocatorHook
    {
    public:

        /** same to lifo_allocator::alignment */
        static constexpr size_t alignment = ALIGNMENT;

        DebugDataStack()
        {
            m_allocator.underlying_allocator_ref().add_hook(this);
        }

        /** same to lifo_allocator::allocate */
        void * allocate(size_t i_size)
        {
            exception_checkpoint();
            auto const block = m_allocator.allocate(i_size);
            notify_alloc(block, i_size);
            if(m_check_consinstency)
                check_consinstency();
            return block;
        }

        /** same to lifo_allocator::allocate_empty */
        void * allocate_empty() noexcept
        {
            auto const block = m_allocator.allocate_empty();
            notify_alloc(block, 0);
            if(m_check_consinstency)
                check_consinstency();
            return block;
        }

        /** same to lifo_allocator::reallocate */
        void * reallocate(void * i_block, size_t i_old_size, size_t i_new_size)
        {
            exception_checkpoint();
            auto const new_block = m_allocator.reallocate(i_block, i_old_size, i_new_size);
            notify_dealloc(i_block, i_old_size);
            notify_alloc(new_block, i_new_size);
            if(m_check_consinstency)
                check_consinstency();
            return new_block;
        }

        /** same to lifo_allocator::deallocate */
        void deallocate(void * i_block, size_t i_size) noexcept
        {
            notify_dealloc(i_block, i_size);
            m_allocator.deallocate(i_block, i_size);
            if(m_check_consinstency)
                check_consinstency();
        }

        ~DebugDataStack()
        {
            m_allocator.underlying_allocator_ref().remove_hook(this);
            DENSITY_TEST_ASSERT(m_blocks.empty());
        }

    private:

        bool m_check_consinstency = false;

        void check_consinstency() const
        {
            auto top = m_allocator.top_pointer();
            if (top == decltype(m_allocator)::virgin_top())
            {
                DENSITY_TEST_ASSERT(m_pages.empty());
                for (const auto & block : m_blocks)
                {
                    DENSITY_TEST_ASSERT(!decltype(m_allocator)::is_internal_block(block.m_size));
                }
            }
            else
            {
                void * curr_address = nullptr;
                for (const auto & block : m_blocks)
                {
                    if (decltype(m_allocator)::is_internal_block(block.m_size))
                    {
                        if (curr_address != nullptr)
                        {
                            if (same_page(curr_address, block.m_block))
                            {
                                DENSITY_TEST_ASSERT(curr_address == block.m_block);
                            }
                            else
                            {
                                DENSITY_TEST_ASSERT(density::address_is_aligned(block.m_block, UNDERLYING_ALLOCATOR::page_alignment));
                            }
                        }
                        curr_address = density::address_add(block.m_block, block.m_size);
                    }
                }

                if (curr_address == nullptr)
                {
                    DENSITY_TEST_ASSERT(density::address_is_aligned(top, UNDERLYING_ALLOCATOR::page_alignment));
                }
                else
                {
                    DENSITY_TEST_ASSERT(top == curr_address || density::address_is_aligned(top, UNDERLYING_ALLOCATOR::page_alignment));
                }
            }

            for (auto page : m_pages)
            {
                auto blocks = std::count_if(m_blocks.begin(), m_blocks.end(), [page](Block i_block){
                    return i_block.belongs_to_page(page);
                });

                auto const top_page = density::address_lower_align(top, UNDERLYING_ALLOCATOR::page_alignment);
                bool const top_is_there = top_page == page;

                DENSITY_TEST_ASSERT(top_is_there || blocks > 0);
            }
        }

    private:

        static bool same_page(const void * i_first, const void * i_second) noexcept
        {
            auto const page_mask = UNDERLYING_ALLOCATOR::page_alignment - 1;
            return ((reinterpret_cast<uintptr_t>(i_first) ^ reinterpret_cast<uintptr_t>(i_second)) & ~page_mask) == 0;
        }

        void notify_alloc(void * i_block, size_t i_size)
        {
            m_blocks.push_back(Block{i_block, i_size});
        }

        void notify_dealloc(void * i_block, size_t i_size)
        {
            DENSITY_TEST_ASSERT(!m_blocks.empty() &&
                m_blocks.back().m_block == i_block &&
                m_blocks.back().m_size == i_size);
            m_blocks.pop_back();
        }

        void on_allocated_page(void * i_page) override
        {
            m_pages.push_back(i_page);
        }

        void on_deallocating_page(void * i_page) override
        {
            auto it = std::find(m_pages.begin(), m_pages.end(), i_page);
            DENSITY_TEST_ASSERT(it != m_pages.end());
            m_pages.erase(it);
        }

    private:

        struct Block
        {
            void * m_block;
            size_t m_size;

            bool belongs_to_page(void * i_page) noexcept
            {
                auto const page_start = density::address_lower_align(i_page, UNDERLYING_ALLOCATOR::page_alignment);
                auto const page_end = density::address_add(page_start, UNDERLYING_ALLOCATOR::page_size);
                auto const block_end = density::address_add(m_block, m_size);
                return m_block >= page_start && block_end < page_end;
            }
        };
        std::vector<Block> m_blocks; /**< used to check the LIFO order */
        std::vector<void*> m_pages;

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
