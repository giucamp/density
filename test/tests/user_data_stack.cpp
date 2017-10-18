
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "../test_framework/density_test_common.h"
#include "../test_framework/test_allocators.h"
#include "../test_framework/exception_tests.h"
#include "../test_framework/statistics.h"
#include <density/lifo.h>
#include <vector>
#include <iostream>
#include <algorithm>

namespace density_tests
{
    /** Decorator to lifo_allocator that adds debug checks to detect violations of the LIFO order, wrong size passed
        to deallocate or reallocate and leaks of lifo blocks. Functions that can throw call exception_checkpoint()
        to allow exception testing. */
    template <size_t ALIGNMENT = alignof(void*)>
        class DebugDataStack
    {
    public:

        using underlying_allocator = density_tests::DeepTestAllocator<>;

        /** same to lifo_allocator::alignment */
        static constexpr size_t alignment = ALIGNMENT;

        DebugDataStack()
        {
        }

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
            DENSITY_TEST_ASSERT(m_lifo_blocks.empty());
        }

        struct Stats
        {
            Statistics m_page_count;
            Statistics m_page_block_count;
            Statistics m_page_used_space;

            Statistics m_external_block_count;
            Statistics m_external_block_size;

            friend std::ostream & operator << (std::ostream & i_stream, const Stats & i_statistics)
            {
                i_stream << "page_count: " << i_statistics.m_page_count;
                i_stream << "\tpage_block_count: " << i_statistics.m_page_block_count;
                i_stream << "\tpage_used_space: " << i_statistics.m_page_used_space;
                i_stream << "\nexternal_block_count: " << i_statistics.m_external_block_count;
                i_stream << "\texternal_block_size: " << i_statistics.m_external_block_size;
                return i_stream;
            }
        };

        const Stats & stats() const noexcept
        {
            return m_stats;
        }

        void stat_sample()
        {
            // construct pages, a vector of pages sorted by age
            struct Page
            {
                void * m_page = nullptr;
                size_t m_progressive = 0;
                size_t m_used_size = 0;
                size_t m_blocks = 0;
            };
            std::vector<Page> pages;
            m_allocator.underlying_allocator_ref().for_each_page([&pages](void * i_page, size_t i_progressive) {
                Page page;
                page.m_page = i_page;
                page.m_progressive = i_progressive;
                pages.push_back(page);
            });
            std::sort(pages.begin(), pages.end(), [](const Page & i_first, const Page & i_second) {
                return i_first.m_progressive < i_second.m_progressive;
            });

            // construct external_blocks, a vector of external blocks sorted by age
            struct ExternalBlock
            {
                void * m_block = nullptr;
                underlying_allocator::BlockInfo m_block_info;
            };
            std::vector<ExternalBlock> external_blocks;
            m_allocator.underlying_allocator_ref().for_each_block([&external_blocks](void * i_block, const underlying_allocator::BlockInfo & i_info) {
                ExternalBlock block;
                block.m_block = i_block;
                block.m_block_info = i_info;
                external_blocks.push_back(block);
            });
            std::sort(external_blocks.begin(), external_blocks.end(), [](const ExternalBlock & i_first, const ExternalBlock & i_second) {
                return i_first.m_block_info.m_progressive < i_second.m_block_info.m_progressive;
            });

            // update stats about count
            m_stats.m_page_count.sample(static_cast<double>(pages.size()));
            m_stats.m_external_block_count.sample(static_cast<double>(external_blocks.size()));

            // check the consistency
            void * const current_top = get_top_pointer();
            void * const virgin_top = get_virgin_top();
            if (current_top == virgin_top)
            {
                // virgin data-stack: only external blocks must exist. external_blocks and m_lifo_blocks must match exactly
                DENSITY_TEST_ASSERT(pages.size() == 0);
                DENSITY_TEST_ASSERT(external_blocks.size() == m_lifo_blocks.size());
                for (size_t index = 0; index < external_blocks.size(); index++)
                {
                    auto const & lifo_block = m_lifo_blocks[index];
                    auto const & external_block = external_blocks[index];
                    DENSITY_TEST_ASSERT(external_block.m_block == lifo_block.m_block);
                    DENSITY_TEST_ASSERT(external_block.m_block_info.m_size == lifo_block.m_size);

                    m_stats.m_external_block_size.sample(static_cast<double>(external_block.m_block_info.m_size));
                }
            }
            else
            {
                DENSITY_TEST_ASSERT(!pages.empty()); // at least one page must exist

                // iterate m_lifo_blocks
                Block prev_block;
                size_t page_index = 0, external_block_index = 0;
                size_t lifo_block_index = 0;
                for (; lifo_block_index < m_lifo_blocks.size() && m_lifo_blocks[lifo_block_index].m_block == virgin_top; lifo_block_index++)
                {
                    // skip initial virgin blocks
                }
                for( ; lifo_block_index < m_lifo_blocks.size(); lifo_block_index++)
                {
                    const auto & block = m_lifo_blocks[lifo_block_index];
                    if (external_block_index < external_blocks.size() && external_blocks[external_block_index].m_block == block.m_block)
                    {
                        // consume an external block
                        DENSITY_TEST_ASSERT(external_blocks[external_block_index].m_block_info.m_size == block.m_size);
                        m_stats.m_external_block_size.sample(static_cast<double>(external_blocks[external_block_index].m_block_info.m_size));
                        external_block_index++;
                    }
                    else
                    {
                        if (!same_page(block.m_block, prev_block.m_block))
                        {
                            // page switch
                            if (prev_block.m_block == nullptr)
                            {
                                // first page
                            }
                            else
                            {
                                m_stats.m_page_block_count.sample(static_cast<double>(pages[page_index].m_blocks));
                                m_stats.m_page_used_space.sample(static_cast<double>(pages[page_index].m_used_size));
                                page_index++;
                                DENSITY_TEST_ASSERT(page_index < pages.size());
                            }
                        }
                        DENSITY_TEST_ASSERT(same_page(block.m_block, pages[page_index].m_page));                        

                        // update the page
                        pages[page_index].m_blocks++;
                        pages[page_index].m_used_size += block.m_size;

                        prev_block = block;
                    }
                }

                // we must have consumed all the elements of pages and external_blocks
                DENSITY_TEST_ASSERT(page_index + 1 == pages.size());
                DENSITY_TEST_ASSERT(external_block_index == external_blocks.size());
            }
        }

    private:

        static bool same_page(const void * i_first, const void * i_second) noexcept
        {
            auto const page_mask = underlying_allocator::page_alignment - 1;
            return ((reinterpret_cast<uintptr_t>(i_first) ^ reinterpret_cast<uintptr_t>(i_second)) & ~page_mask) == 0;
        }

        void * get_top_pointer() const
        {
            /* this function makes the assumption that allocate_empty just returns the top pointer
                without altering the state of the allocator. */
            return const_cast<DebugDataStack*>(this)->m_allocator.allocate_empty();
        }

        void * get_virgin_top() const
        {
            /* this function makes the assumption that allocate_empty just returns the top pointer. Even the
                notion of virgin allocator is an implementation detail of density::lifo_allocator. */
            return density::lifo_allocator<underlying_allocator, ALIGNMENT>().allocate_empty();
        }

        void notify_alloc(void * i_block, size_t i_size)
        {
            // virgin blocks can be only at the bottom
            void * const virgin_top = get_virgin_top();
            if (!m_lifo_blocks.empty() && m_lifo_blocks.back().m_block != virgin_top)
            {
                DENSITY_TEST_ASSERT(i_block != virgin_top);
            }

            Block new_block;
            new_block.m_block = i_block;
            new_block.m_size = i_size;
            m_lifo_blocks.push_back(new_block);
        }

        void notify_dealloc(void * i_block, size_t i_size)
        {
            DENSITY_TEST_ASSERT(!m_lifo_blocks.empty() &&
                m_lifo_blocks.back().m_block == i_block &&
                m_lifo_blocks.back().m_size == i_size);
            m_lifo_blocks.pop_back();
        }

    private:

        struct Block
        {
            void * m_block = nullptr;
            size_t m_size = 0;

            bool belongs_to_page(void * i_page) const noexcept
            {
                auto const page_start = density::address_lower_align(i_page, underlying_allocator::page_alignment);
                auto const page_end = density::address_add(page_start, underlying_allocator::page_size);
                auto const block_end = density::address_add(m_block, m_size);
                return m_block >= page_start && block_end < page_end;
            }
        };
        std::vector<Block> m_lifo_blocks; /**< used to check the LIFO order */        
        density::lifo_allocator<underlying_allocator, ALIGNMENT> m_allocator;
        Stats m_stats;
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
                thread_local density_tests::DebugDataStack<alignment> user_data_stack;
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


            /* If DENSITY_USER_DATA_STACK is defined, this test program implements this function
                that make a complete check of the data-stack, and updates the internal statistics. */
            void stat_sample()
            {
                user_data_stack.stat_sample();
            }

            /* Prints the internal statistics. */
            void stats_print(std::ostream & i_dest)
            {
                i_dest << user_data_stack.stats();
            }

        } // namespace user_data_stack

    } // namespace density

#endif // #ifdef DENSITY_USER_DATA_STACK
