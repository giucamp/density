
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "../test_framework/density_test_common.h"
//

#include <density/runtime_type.h>
#include <mutex>

namespace density_tests
{
    void feature_list_examples()
    {
        using namespace density;

        //! [feature_list example 1]
        using Features1 = feature_list<f_size, f_alignment, f_copy_construct>;
        using Tuple     = Features1::tuple<void>;
        //! [feature_list example 1]
        //! [feature_list example 2]
        // Features1, Features2 and Features3 produce the same feature tuple
        using Features2 =
          feature_list<feature_list<f_size>, feature_list<f_alignment, f_copy_construct>>;
        using Features3 = feature_list<
          feature_list<f_size, f_none>,
          feature_list<feature_list<f_none>>,
          feature_list<f_size, f_alignment, f_copy_construct, f_none, f_copy_construct, f_size>>;
        //! [feature_list example 2]
        //! [feature_list example 3]
        /* If two fetaure_list priduce the same tuple, they are similar. We can't compare class 
           templates with std::is_same, so we compare a specialization */
        static_assert(std::is_same<Features1::tuple<int>, Features2::tuple<int>>::value, "");
        static_assert(std::is_same<Features2::tuple<int>, Features3::tuple<int>>::value, "");
        //! [feature_list example 3]

        {
            using Rt1 = runtime_type<void, f_size, f_alignment>;
            using Rt2 = runtime_type<void, feature_list<f_size>, f_none, f_alignment>;
            Rt1 t1 = Rt1::make<int>();
            Rt1 t2 = t1;
            t1 = t2;
            assert(t1 == t2);

            using Rt3 = runtime_type<void, feature_list<f_size, f_default_construct>, f_none, f_alignment>;
            static_assert(!std::is_constructible<Rt1, Rt3>::value, "");
            static_assert(!std::is_assignable<Rt1, Rt3>::value, "");
        }

        {
            using Rt1 = runtime_type<void>;
            using Rt2 = runtime_type<void, default_type_features<void>>;
            Rt1 t1 = Rt1::make<int>();
            Rt1 t2 = t1;
            t1 = t2;
            assert(t1 == t2);

            using Rt3 = runtime_type<void, f_default_construct, f_destroy>;
            static_assert(!std::is_constructible<Rt1, Rt3>::value, "");
            static_assert(!std::is_assignable<Rt1, Rt3>::value, "");
        }

    }

    void runtime_type_examples()
    {
        using namespace density;
        //! [feature_list example 1]

        using MyFeatures    = feature_list<f_size, f_alignment, f_copy_construct>;
        using MyRuntimeType = runtime_type<void, MyFeatures>;

        // this is ok: int supports sizeof, alignof, and copy construction
        auto IntType = MyRuntimeType::make<int>();

        // this fails to compile: std::mutex doesen't allow copy construction
        // auto MutexType = MyRuntimeType::make<std::mutex>();

        // MyFeatures::tuple<void> = std::tuple<f_size::type<void>, f_alignment::type<void>>
        /*static_assert(
          std::is_same<
            MyFeatures::tuple<void>,
            std::tuple<f_size::type<void>, f_alignment::type<void>>>::value,
          "");*/

        //! [feature_list example 1]
    }

    //! [runtime_type example 2]

    /* This feature calls an update function on any object. The update has not to be virtual, as
        type erasure already is a kind of virtualization. */
    struct feature_call_update
    {
        template <typename COMMON_ANCESTOR> struct type
        {
            void (*m_func)(COMMON_ANCESTOR * i_object, float i_elapsed_time);

            void operator()(COMMON_ANCESTOR * i_object, float i_elapsed_time) const
            {
                m_func(i_object, i_elapsed_time);
            }

            /** Creates an instance of this feature bound to the specified target type */
            template <typename TARGET_TYPE> constexpr static type make() noexcept
            {
                static_assert(
                  std::is_convertible<TARGET_TYPE *, COMMON_ANCESTOR *>::value,
                  "TARGET_TYPE must derive from COMMON_ANCESTOR, or COMMON_TYPE must be void");

                return type{&invoke<TARGET_TYPE>};
            }

          private:
            template <typename TARGET_TYPE>
            static void invoke(COMMON_ANCESTOR * i_object, float i_elapsed_time) noexcept
            {
                static_cast<TARGET_TYPE *>(i_object)->update(i_elapsed_time);
            }
        };
    };
    //! [runtime_type example 2]

} // namespace density_tests
