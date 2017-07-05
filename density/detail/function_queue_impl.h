
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <density/heterogeneous_queue.h>

namespace density
{
	namespace detail
	{
		template < typename CALLABLE >
			class FunctionRuntimeType;

		template < typename RET_VAL, typename... PARAMS >
			class FunctionRuntimeType<RET_VAL (PARAMS...)>
		{
		public:

			using common_type = void;

			template <typename TYPE>
				static FunctionRuntimeType make() noexcept
			{
				return FunctionRuntimeType{ &Impl<TYPE>::align_invoke_destroy, &Impl<TYPE>::align_destroy };
			}

			bool empty() const noexcept
			{ 
				return m_align_invoke_destroy == nullptr; 
			}

			void clear() noexcept
			{
				m_align_invoke_destroy = nullptr;
				m_align_destroy = nullptr;
			}

		    void destroy(void * i_dest) const noexcept
			{
				(*m_align_destroy)(i_dest);
			}

			RET_VAL align_invoke_destroy(void * i_dest, PARAMS &&... i_params) const
			{
				return (*m_align_invoke_destroy)(i_dest, std::forward<PARAMS>(i_params)...);
			}

			size_t alignment() const
			{
				return 1;
			}

			template <typename ACTUAL_TYPE>
				struct Impl
			{
				static RET_VAL align_invoke_destroy(void * i_dest, PARAMS &&... i_params)
				{
					return align_invoke_destroy_impl(std::is_void<RET_VAL>(), i_dest, std::forward<PARAMS>(i_params)...);
				}

				static void align_destroy(void * i_dest) noexcept
				{
					auto const aligned_dest = static_cast<ACTUAL_TYPE*>(address_upper_align(i_dest, alignof(ACTUAL_TYPE)));
                    aligned_dest->ACTUAL_TYPE::~ACTUAL_TYPE();
				}

				static RET_VAL align_invoke_destroy_impl(std::false_type, void * i_dest, PARAMS &&... i_params)
				{
					auto const aligned_dest = static_cast<ACTUAL_TYPE*>(address_upper_align(i_dest, alignof(ACTUAL_TYPE)));
					auto && ret = (*aligned_dest)(std::forward<PARAMS>(i_params)...);
                    aligned_dest->ACTUAL_TYPE::~ACTUAL_TYPE();
					return ret;
				}

				static void align_invoke_destroy_impl(std::true_type, void * i_dest, PARAMS &&... i_params)
				{
					auto const aligned_dest = static_cast<ACTUAL_TYPE*>(address_upper_align(i_dest, alignof(ACTUAL_TYPE)));
					(*aligned_dest)(std::forward<PARAMS>(i_params)...);
                    aligned_dest->ACTUAL_TYPE::~ACTUAL_TYPE();
				}
			};

			using AlignInvokeDestroyFunc = RET_VAL(*)(void * i_dest, PARAMS &&... i_params);
			using AlignDestroyFunc = void(*)(void * i_dest);

			AlignInvokeDestroyFunc m_align_invoke_destroy = nullptr;
			AlignDestroyFunc m_align_destroy = nullptr;
		};


		template < typename QUEUE, typename FUNCTION > class FunctionQueueImpl;

		template < typename QUEUE, typename RET_VAL, typename... PARAMS >
			class FunctionQueueImpl<QUEUE, RET_VAL (PARAMS...)> final
		{
		public:

			using value_type = RET_VAL(PARAMS...);

			template <typename ELEMENT_COMPLETE_TYPE>
				void push(ELEMENT_COMPLETE_TYPE && i_source)
			{
				m_queue.push(std::forward<ELEMENT_COMPLETE_TYPE>(i_source));
			}

			optional_or_bool<RET_VAL> try_consume_front(PARAMS... i_params)
			{
				return consume_front_impl(std::is_void<RET_VAL>(), std::forward<PARAMS>(i_params)...);
			}

			void clear() noexcept
			{
				m_queue.clear();
			}

			bool empty() noexcept
			{
				return m_queue.empty();
			}

		private:

			optional<RET_VAL> consume_front_impl(std::false_type, PARAMS... i_params)
			{
				auto cons = m_queue.start_consume();
				if (cons)
				{
					auto && result = cons.complete_type().align_invoke_destroy(
						cons.unaligned_element_ptr(), std::forward<PARAMS>(i_params)...);
					cons.commit_nodestroy();
					return make_optional<RET_VAL>(std::forward<RET_VAL>(result));
				}
				else
				{
					return optional<RET_VAL>();
				}
			}

			bool consume_front_impl(std::true_type, PARAMS... i_params)
			{
				auto cons = m_queue.start_consume();
				if (cons)
				{
					cons.complete_type().align_invoke_destroy(
						cons.unaligned_element_ptr(), std::forward<PARAMS>(i_params)...);
					cons.commit_nodestroy();
					return true;
				}
				else
				{
					return false;
				}
			}

		private:
			QUEUE m_queue;
		};

	} // namespace detail
    
} // namespace density
