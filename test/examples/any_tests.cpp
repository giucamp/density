
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "any.h"
#include <iostream>

namespace density_examples
{
    void any_tests()
    {
        using namespace density;
        using MyAny = any<default_type_features, f_equal>;

        MyAny var;
        assert(var == MyAny());
        assert(!var.has_value());
        assert(var.type() == typeid(void));

        static_assert(has_features<MyAny, f_copy_construct>::value, "");
        static_assert(!has_features<MyAny, f_istream>::value, "");

        var = MyAny(4);
        assert(var != MyAny());
        assert(var != MyAny(4.0));
        assert(var != MyAny(7));
        assert(var == MyAny(4));
        assert(var.has_value());
        assert(var.type() == typeid(int));
        assert(any_cast<int>(var) == 4);
        assert(*any_cast<int>(&var) == 4);

        var = 4;
        assert(var != MyAny());
        assert(var != MyAny(4.0));
        assert(var != MyAny(7));
        assert(var == MyAny(4));
        assert(var.has_value());
        assert(var.type() == typeid(int));
        assert(any_cast<int>(var) == 4);
        assert(*any_cast<int>(&var) == 4);

        auto const var1 = var;
        assert(var1 != MyAny());
        assert(var1 != MyAny(4.0));
        assert(var1 != MyAny(7));
        assert(var1 == MyAny(4));
        assert(var1.has_value());
        assert(var1.type() == typeid(int));
        assert(any_cast<int>(var1) == 4);
        assert(*any_cast<int>(&var1) == 4);

        using summable_any = any<default_type_features, f_istream, f_ostream, f_sum>;
        summable_any a = 3, b = 4;
        std::cout << a << " + " << b << " = " << a + b << std::endl;
    }

} // namespace density_examples
