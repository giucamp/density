//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef DENSITY_INCLUDING_CONC_QUEUE_DETAIL
	#error "THIS IS A PRIVATE HEADER OF DENSITY. DO NOT INCLUDE IT DIRECTLY"
#endif // ! DENSITY_INCLUDING_CONC_QUEUE_DETAIL

#include "..\void_allocator.h"

namespace density
{
	namespace detail
	{
		/** An HazardPointersStack is the mean a thread can use to publish which objects it is accessing.
			A thread declares it is using an hazard pointer by pushing it with push_hazard_ptr. When it has finished
			it pops the pointer from the stack with pop_hazard_ptr. If, for any reason, a thread does not pop
			a pointer when it should, some threads will probably block.
			Lock-free algorithm may exploit the stack of hazard pointer to be re-entrant.
			An HazardPointersStack must be registered to a HazardPointersContext to be effective. An instance of 
			HazardPointersStack can register only to one HazardPointersContext at a time. Anyway it can be 
			reused: after unregistration, it can register to other (or the same) HazardPointersContext.
			A thread can query a HazardPointersContext to check if any thread has a given pointer in its stack.
			HazardPointersStack is optimized to handle few pointers. Anyway the only limit on the depth of the stack 
			is the available memory.
			An instance of HazardPointersStack must be owned and used by a single thread. A thread can own multiple
			instances of HazardPointersStack.
			HazardPointersContext is not copyable and not movable. */
		class alignas(64) HazardPointersStack
		{
		public:

			HazardPointersStack() noexcept
				: m_pointer_count(0), m_next(nullptr), m_prev(nullptr), m_inner_stack(nullptr)
			{

			}

			HazardPointersStack(const HazardPointersStack &) = delete;
			HazardPointersStack & operator = (const HazardPointersStack &) = delete;

			/** Destructor. The stack must be empty. */
			~HazardPointersStack()
			{
				DENSITY_ASSERT_INTERNAL(m_pointer_count == 0);
				void_allocator().delete_object(m_inner_stack.load());
			}

			/** Pushes a pointer in this stack. If multiple threads try to push to and/or pop from
				the same stack without external synchronization, a data-race occurs, and the behavior is undefined.
					@param i_pointer pointer to add on the top of the stack. Can't be nullptr. HazardPointersStack does
						not access the memory pointed by i_pointer.
				\n\b Complexity: linear in the depth of the stack.
				\n\b Throws: any exception thrown by operator new. */
			void push_hazard_ptr(void * i_pointer)
			{
				DENSITY_ASSERT_INTERNAL(i_pointer != nullptr);

				auto const new_index = m_pointer_count++;
				if (new_index < s_inplace_count)
				{
					m_inplace_pointers[new_index].store(i_pointer);
				}
				else
				{
					// no place on the local array. Recurse on the inner stack.
					auto inner = m_inner_stack.load();
					if (inner == nullptr)
					{
						/* the built-in new of the language would not respect the over-alignment of HazardPointersStack, 
							so we use a temporary void_allocator */
						m_inner_stack.store(inner = void_allocator().new_object<HazardPointersStack>());
					}
					inner->push_hazard_ptr(i_pointer);
				}
			}

			/** Pushes a pointer in this stack, but does not initialize it. A pointer to the entry in the stack is returned,
				so that the caller can initialize it. If multiple threads try to push to and/or pop from
				the same stack without external synchronization, a data-race occurs, and the behavior is undefined.
					@param i_pointer pointer to add on the top of the stack. Can't be nullptr. HazardPointersStack does
						not access the memory pointed by i_pointer.
				\n\b Complexity: linear in the depth of the stack.
				\n\b Throws: any exception thrown by operator new. */
			std::atomic<void*> * push_hazard_ptr()
			{
				auto const new_index = m_pointer_count++;
				if (new_index < s_inplace_count)
				{
					return &m_inplace_pointers[new_index];
				}
				else
				{
					// no place on the local array. Recurse on the inner stack.
					auto inner = m_inner_stack.load();
					if (inner == nullptr)
					{
						/* the built-in new of the language would not respect the over-alignment of HazardPointersStack, 
							so we use a temporary void_allocator */
						m_inner_stack.store(inner = void_allocator().new_object<HazardPointersStack>());
					}
					return inner->push_hazard_ptr();
				}
			}

			/** Removes the most-recently pushed pointer from this stack. If multiple threads try to push to and/or pop from
				the same stack without external synchronization, a data-race occurs, and the behavior is undefined.
				\n\b Complexity: linear in the depth of the stack.
				\n\b Throws: nothing. */
			void pop_hazard_ptr() noexcept
			{
				DENSITY_ASSERT_INTERNAL(m_pointer_count > 0);

				auto count = m_pointer_count;
				HazardPointersStack * stack = this;
				while(count >= s_inplace_count)
				{					
					DENSITY_ASSERT_INTERNAL(stack->m_inner_stack.load() != nullptr);

					count -= s_inplace_count;
					stack = stack->m_inner_stack.load();
				}

				DENSITY_ASSERT_INTERNAL(stack->m_inplace_pointers[count].load() != nullptr);
				stack->m_inplace_pointers[count].store(nullptr);

				m_pointer_count--;
			}


		private:

			/** Checks whether a pointer is present on the stack. Stacks (de)registration on a context
					must be externally synchronized with is_hazard_pointer, or a data race occurs.
				@return whether the pointer is present on the stack.
				\n\b Complexity: linear in the depth of the stack.
				\n\b Throws: nothing. */
			bool is_hazard_pointer(void * i_pointer) const noexcept
			{
				for (size_t index = 0; index < s_inplace_count; index++)
				{
					if (m_inplace_pointers[index].load() == i_pointer)
					{
						return true;
					}
				}

				// the pointer is not present on the local array: check on the inner stack (if any)
				auto const inner = m_inner_stack.load();
				if (inner == nullptr)
				{
					return false;
				}
				else
				{
					return inner->is_hazard_pointer(i_pointer);
				}
			}
			
		private:
			static constexpr size_t s_inplace_count = 4; /**< this is the maximum number of pointers that can be pushed (without popping) 
																before an inner stack is created. */
			std::atomic<void*> m_inplace_pointers[s_inplace_count];
			size_t m_pointer_count; /**< number of pointers stored in this stack and in all the inner stacks. */
			HazardPointersStack * m_next, * m_prev; /**< pointers for the intrusive linked list, handled by HazardPointersContext */
			std::atomic<HazardPointersStack*> m_inner_stack; /**< Inner stack or nullptr. Once an inner stack is created, it stays alive until
																	HazardPointersStack gets destroyed. */
			
			friend class HazardPointersContext;

			#if DENSITY_DEBUG_INTERNAL
				HazardPointersContext * m_dbg_registered_to = nullptr;
			#endif			
		};

		/** 
			Since HazardPointersStack is not movable and not copyable, entries are handled with an intrusive doubly-linked list. 
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
				\n\b Throws: anything std::mutex::lock throws. */
			void register_stack(HazardPointersStack & i_stack)
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				
				#if DENSITY_DEBUG_INTERNAL
					DENSITY_ASSERT_INTERNAL(i_stack.m_dbg_registered_to == nullptr);
					check_integrity();					
				#endif

				i_stack.m_prev = nullptr;
				i_stack.m_next = m_first_stack;
				m_first_stack->m_prev = &i_stack;
				m_first_stack = &i_stack;
				
				#if DENSITY_DEBUG_INTERNAL
					check_integrity();
					i_stack.m_dbg_registered_to = this;
				#endif
			}

			/** Unregister a stack on this context. If the stack is not registered to this HazardPointersContext, the behavior is undefined.
				unregister_stack is a locking operation.
				\n\b Complexity: constant.
				\n\b Throws: anything std::mutex::lock throws. */
			void unregister_stack(HazardPointersStack & i_stack)
			{
				std::lock_guard<std::mutex> lock(m_mutex);

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

			/** Check if a given pointer is present in any of the stacks of the registered HazardPointersStack.
				This function scans all the registered stacks. During the scan, the stacks can change (the other 
				threads may push and pop their hazard pointers). 
				is_hazard_pointer is a locking operation.
					\n\b Complexity: linear in the number of registered stacks and linear in the depth of the stacks.
					\n\b Throws: anything std::mutex::lock throws. */
			bool is_hazard_pointer(void * i_pointer)
			{
				std::lock_guard<std::mutex> lock(m_mutex);

				for (auto curr = m_first_stack; curr != nullptr; curr = curr->m_next)
				{
					if (curr->is_hazard_pointer(i_pointer))
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
					auto prev = static_cast<HazardPointersStack *>( nullptr );
					for (auto curr = m_first_stack; curr != nullptr; curr = curr->m_next)
					{
						DENSITY_ASSERT_INTERNAL(curr->m_prev == prev);
						prev = curr;
					}
				}
			#endif		

		private:
			std::mutex m_mutex;
			HazardPointersStack * m_first_stack;
		};

	} // namespace detail

} // namespace detail