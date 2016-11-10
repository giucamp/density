
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <density/void_allocator.h>
#include <atomic>
#include <mutex>

namespace density
{
    namespace detail
    {
        /** An HazardPointer is the mean a thread can use to publish which objects it is accessing.
            A thread declares it is using an hazard pointer by pushing it with push_hazard_ptr. When it has finished
            it pops the pointer from the stack with pop_hazard_ptr. If, for any reason, a thread does not pop
            a pointer when it should, some threads will probably block.
            Lock-free algorithm may exploit the stack of hazard pointer to be re-entrant.
            An HazardPointer must be registered to a HazardPointersContext to be effective. An instance of
            HazardPointer can register only to one HazardPointersContext at a time. Anyway it can be
            reused: after unregistration, it can register to other (or the same) HazardPointersContext.
            A thread can query a HazardPointersContext to check if any thread has a given pointer in its stack.
            HazardPointer is optimized to handle few pointers. Anyway the only limit on the depth of the stack
            is the available memory.
            An instance of HazardPointer must be owned and used by a single thread. A thread can own multiple
            instances of HazardPointer.
            HazardPointersContext is not copyable and not movable. */
        class HazardPointer
        {
        public:

            HazardPointer() noexcept
                : m_pointer(nullptr), m_next(nullptr), m_prev(nullptr)
            {
            }

            HazardPointer(const HazardPointer &) = delete;
            HazardPointer & operator = (const HazardPointer &) = delete;

            sync::atomic<void*> * get() { return &m_pointer; }

        private:
            sync::atomic<void*> m_pointer;
            HazardPointer * m_next, * m_prev; /**< pointers for the intrusive linked list, handled by HazardPointersContext */

            friend class HazardPointersContext;

            #if DENSITY_DEBUG_INTERNAL
                HazardPointersContext * m_dbg_registered_to = nullptr;
            #endif
        };

        /**
            Since HazardPointer is not movable and not copyable, entries are handled with an intrusive doubly-linked list.
            Being intrusive, the remove has constant complexity (while std::list::remove has linear complexity).
            Upon destruction, no stack cam be registered. */
        class HazardPointersContext
        {
        public:

            HazardPointersContext() noexcept
                : m_first_stack(nullptr) {}

            ~HazardPointersContext()
            {
                // stacks must be unregistered before destruction
                DENSITY_ASSERT_INTERNAL(m_first_stack == nullptr);
            }

            /** Register a stack on this context. While registered on a context, a stack cannot be destroyed (otherwise the
                behavior is undefined). If the stack is already registered to a HazardPointersContext, the behavior is undefined.
                register_stack is a locking operation.
                \n\b Complexity: constant.
                \n\b Throws: anything sync::mutex::lock throws. */
            void register_stack(HazardPointer & i_stack)
            {
                std::lock_guard<sync::mutex> lock(m_mutex);

                #if DENSITY_DEBUG_INTERNAL
                    DENSITY_ASSERT_INTERNAL(i_stack.m_dbg_registered_to == nullptr);
                    check_integrity();
                #endif

                i_stack.m_prev = nullptr;
                i_stack.m_next = m_first_stack;
                if (m_first_stack != nullptr)
                {
                    m_first_stack->m_prev = &i_stack;
                }
                m_first_stack = &i_stack;

                #if DENSITY_DEBUG_INTERNAL
                    check_integrity();
                    i_stack.m_dbg_registered_to = this;
                #endif
            }

            /** Unregister a stack on this context. If the stack is not registered to this HazardPointersContext, the behavior is undefined.
                unregister_stack is a locking operation.
                \n\b Complexity: constant.
                \n\b Throws: anything sync::mutex::lock throws. */
            void unregister_stack(HazardPointer & i_stack)
            {
                std::lock_guard<sync::mutex> lock(m_mutex);

                #if DENSITY_DEBUG_INTERNAL
                    DENSITY_ASSERT_INTERNAL(i_stack.m_dbg_registered_to == this);
                    check_integrity();
                #endif

                if (i_stack.m_prev != nullptr)
                {
                    i_stack.m_prev->m_next = i_stack.m_next;
                    DENSITY_ASSERT_INTERNAL(m_first_stack != &i_stack);
                }
                else
                {
                    DENSITY_ASSERT_INTERNAL(m_first_stack == &i_stack);
                    m_first_stack = i_stack.m_next;
                }

                if (i_stack.m_next)
                {
                    i_stack.m_next->m_prev = i_stack.m_prev;
                }

                #if DENSITY_DEBUG_INTERNAL
                    check_integrity();
                    i_stack.m_dbg_registered_to = nullptr;
                #endif
            }

            /** Check if a given pointer is present in any of the stacks of the registered HazardPointer.
                This function scans all the registered stacks. During the scan, the stacks can change (the other
                threads may push and pop their hazard pointers).
                is_hazard_pointer is a locking operation.
                    \n\b Complexity: linear in the number of registered stacks and linear in the depth of the stacks.
                    \n\b Throws: anything sync::mutex::lock throws. */
            bool is_hazard_pointer(void * i_pointer)
            {
                std::lock_guard<sync::mutex> lock(m_mutex);

                for (auto curr = m_first_stack; curr != nullptr; curr = curr->m_next)
                {
                    if (curr->get()->load() == i_pointer)
                    {
                        return true;
                    }
                }
                return false;
            }

        private:
            #if DENSITY_DEBUG_INTERNAL
                void check_integrity() const
                {
                    auto prev = static_cast<HazardPointer *>( nullptr );
                    for (auto curr = m_first_stack; curr != nullptr; curr = curr->m_next)
                    {
                        DENSITY_ASSERT_INTERNAL(curr->m_prev == prev);
                        prev = curr;
                    }
                }
            #endif

        private:
            sync::mutex m_mutex;
            HazardPointer * m_first_stack;
        };

    } // namespace detail

} // namespace detail
