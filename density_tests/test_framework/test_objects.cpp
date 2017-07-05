
#include "test_objects.h"

namespace density_tests
{
	std::atomic<size_t> InstanceCounted::s_instance_counter{0};

	#if DENSITY_TEST_INSTANCE_PROGRESSIVE
		std::atomic<uint64_t> InstanceCounted::s_next_instance_progr{0};
	#endif

} // namespace density_tests
