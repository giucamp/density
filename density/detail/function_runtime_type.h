
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <density/density_common.h>
#include <type_traits>

namespace density
{
    namespace detail
    {
        /** \internal - implementation of invoke for functions */
        template < typename FUNCTION, typename... PARAMS >
            inline typename std::enable_if< !std::is_member_pointer<typename std::decay<FUNCTION>::type>::value,
                    typename std::result_of<FUNCTION&&(PARAMS&&...) >::type >::type
                invoke(FUNCTION && i_functor, PARAMS &&... i_params )
                    noexcept(noexcept((std::forward<FUNCTION>)(i_functor)(std::forward<PARAMS>(i_params)...)))
        {
            return std::forward<FUNCTION>(i_functor)(std::forward<PARAMS>(i_params)...);
        }

        /** \internal - implementation of invoke for pointers to members */
        template < typename FUNCTION, typename... PARAMS >
            inline typename std::enable_if< std::is_member_pointer<typename std::decay<FUNCTION>::type>::value,
                    typename std::result_of<FUNCTION&&(PARAMS&&...) >::type >::type
                invoke(FUNCTION && i_functor, PARAMS &&... i_params )
                    noexcept(noexcept(std::mem_fn(i_functor)(std::forward<PARAMS>(i_params)...)))
        {
            return std::mem_fn(i_functor)(std::forward<PARAMS>(i_params)...);
        }

        /** \internal Private class template used as runtime type for function queues */
        template < function_type_erasure MODE, typename CALLABLE >
            class FunctionRuntimeType;

        /** \internal Private class template used as runtime type for function queues with function_standard_erasure */
        template < typename RET_VAL, typename... PARAMS >
            class FunctionRuntimeType<function_standard_erasure, RET_VAL (PARAMS...)>
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

            RET_VAL align_invoke_destroy(void * i_dest, PARAMS... i_params) const
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
                static RET_VAL align_invoke_destroy(void * i_dest, PARAMS... i_params)
                {
                    return align_invoke_destroy_impl(std::is_void<RET_VAL>(), i_dest, std::forward<PARAMS>(i_params)...);
                }

                static void align_destroy(void * i_dest) noexcept
                {
                    auto const aligned_dest = static_cast<ACTUAL_TYPE*>(address_upper_align(i_dest, alignof(ACTUAL_TYPE)));
                    aligned_dest->ACTUAL_TYPE::~ACTUAL_TYPE();
                }

                static RET_VAL align_invoke_destroy_impl(std::false_type, void * i_dest, PARAMS... i_params)
                {
                    auto const aligned_dest = static_cast<ACTUAL_TYPE*>(address_upper_align(i_dest, alignof(ACTUAL_TYPE)));
                    auto && ret = density::detail::invoke(*aligned_dest, std::forward<PARAMS>(i_params)...);
                    aligned_dest->ACTUAL_TYPE::~ACTUAL_TYPE();
                    return ret;
                }

                static void align_invoke_destroy_impl(std::true_type, void * i_dest, PARAMS... i_params)
                {
                    auto const aligned_dest = static_cast<ACTUAL_TYPE*>(address_upper_align(i_dest, alignof(ACTUAL_TYPE)));
                    density::detail::invoke(*aligned_dest, std::forward<PARAMS>(i_params)...);
                    aligned_dest->ACTUAL_TYPE::~ACTUAL_TYPE();
                }
            };

            using AlignInvokeDestroyFunc = RET_VAL(*)(void * i_dest, PARAMS... i_params);
            using AlignDestroyFunc = void(*)(void * i_dest);

            AlignInvokeDestroyFunc m_align_invoke_destroy = nullptr;
            AlignDestroyFunc m_align_destroy = nullptr;
        };

        /** \internal Private class template used as runtime type for function queues with function_manual_clear */
        template < typename RET_VAL, typename... PARAMS >
            class FunctionRuntimeType<function_manual_clear, RET_VAL (PARAMS...)>
        {
        public:

            using common_type = void;

            template <typename TYPE>
                static FunctionRuntimeType make() noexcept
            {
                return FunctionRuntimeType{ &Impl<TYPE>::align_invoke_destroy };
            }

            bool empty() const noexcept
            {
                return m_align_invoke_destroy == nullptr;
            }

            void clear() noexcept
            {
                m_align_invoke_destroy = nullptr;
            }

            void destroy(void * i_dest) const noexcept
            {
                // with function_manual_clear calling destroy causes undefined behavior
                DENSITY_ASSERT(false);
                (void)i_dest;
            }

            RET_VAL align_invoke_destroy(void * i_dest, PARAMS... i_params) const
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
                static RET_VAL align_invoke_destroy(void * i_dest, PARAMS... i_params)
                {
                    return align_invoke_destroy_impl(std::is_void<RET_VAL>(), i_dest, std::forward<PARAMS>(i_params)...);
                }

                static RET_VAL align_invoke_destroy_impl(std::false_type, void * i_dest, PARAMS... i_params)
                {
                    auto const aligned_dest = static_cast<ACTUAL_TYPE*>(address_upper_align(i_dest, alignof(ACTUAL_TYPE)));
                    auto && ret = density::detail::invoke(*aligned_dest, std::forward<PARAMS>(i_params)...);;
                    aligned_dest->ACTUAL_TYPE::~ACTUAL_TYPE();
                    return ret;
                }

                static void align_invoke_destroy_impl(std::true_type, void * i_dest, PARAMS... i_params)
                {
                    auto const aligned_dest = static_cast<ACTUAL_TYPE*>(address_upper_align(i_dest, alignof(ACTUAL_TYPE)));
                    density::detail::invoke(*aligned_dest, std::forward<PARAMS>(i_params)...);
                    aligned_dest->ACTUAL_TYPE::~ACTUAL_TYPE();
                }
            };

            using AlignInvokeDestroyFunc = RET_VAL(*)(void * i_dest, PARAMS... i_params);
            AlignInvokeDestroyFunc m_align_invoke_destroy = nullptr;
        };

    } // namespace detail

} // namespace density
