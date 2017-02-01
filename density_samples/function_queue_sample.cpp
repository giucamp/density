
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <density/function_queue.h>
#include <density/lifo.h>
#include <string>
#include <functional> // for std::bind
#include <iostream>
#include <utility>

namespace function_queue_sample
{
    using namespace density;

	// if this struct is defined within run(), in visual studio 2015 (update 2) clang crashes while compiling
    struct Renderer { int m_draw_calls = 0; };

    void run()
    {
        {
            auto print_func = [](const char * i_str) { std::cout << i_str; };

            function_queue<void()> queue_1;
            #ifndef __GNUC__ /* with gcc, the result of std::bind is not nothrow-move-constructible even if the original
                            callable object and the capture is nothrow-move-constructible. */
                queue_1.push(std::bind(print_func, "hello "));
            #endif
            queue_1.push([print_func]() { print_func("world!"); });
            queue_1.push([]() { std::cout << std::endl; });
            queue_1.consume_front();
            while (!queue_1.empty())
                queue_1.consume_front();

            function_queue<int(double, double)> queue_2;
            queue_2.push([](double a, double b) { return static_cast<int>(a + b); });
            std::cout << "a + b = " << queue_2.consume_front(40., 2.) << std::endl;
        }

        {


function_queue<bool (Renderer & )> render_commands;

// post a command that draws a circle
float x = 5, y = 6, radius = 3;
render_commands.push( [x, y, radius] (Renderer & i_renderer) {
    std::cout << "drawing a circle at (" << x << ", " << y <<
        ") with radius = " << radius << std::endl;
    i_renderer.m_draw_calls++;
    return true;
});

// post a command that loads a texture
std::string file_name = "hello_world.png";
int flags = 42;
render_commands.push([file_name, flags](Renderer & i_renderer) {
    std::cout << "loading " << file_name << " with flags " << flags << std::endl;
    i_renderer.m_draw_calls++;
    return true;
});

// execute the commands
Renderer renderer;
while (!render_commands.empty())
{
    if (!render_commands.consume_front(renderer))
    {
        std::cerr << "command failed" << std::endl;
    }
}
        }
    }
}
