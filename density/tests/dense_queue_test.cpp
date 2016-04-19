
#include "..\dense_queue.h"
#include "..\testing_utils.h"
#include <iostream>
#include <random>

namespace density
{
	namespace detail
	{
		void queue_test(std::mt19937 & i_random)
		{
			DenseQueue<int> queue(128);
			std::vector<int> ints;
			for (int i = 0; i < 1000; i++)
			{
				auto const queue_dist = std::distance(queue.begin(), queue.end());
				DENSITY_TEST_ASSERT(queue_dist == static_cast<decltype(queue_dist)>(ints.size()));
				DENSITY_TEST_ASSERT(queue.empty() == ints.empty());

				if ( !ints.empty() && std::uniform_int_distribution<int>(0, 6)(i_random) <= 2)
				{
					queue.pop();
					ints.erase(ints.begin());
				}
				queue.push(i);
				ints.push_back(i);
			}

			for (auto i : queue)
			{
				std::cout << i << std::endl;
			}
		}
	}

	void queue_test()
	{
		std::mt19937 random;
		detail::queue_test(random);
	}
}