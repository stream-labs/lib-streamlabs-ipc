#ifndef OS_APPLE_SEMAPHORE_HPP
#define OS_APPLE_SEMAPHORE_HPP

#include "../../include/semaphore.hpp"
#include <fcntl.h> 
#include <sys/stat.h>
#include <semaphore.h>

namespace os {
	namespace apple {
		class semaphore : public os::semaphore {
            sem_t *sem;

			public:
			semaphore(int32_t initial_count = 0, int32_t maximum_count = std::numeric_limits<int32_t>::max());
			virtual ~semaphore();

			virtual os::error signal(uint32_t count = 1) override;

			// os::waitable
			protected:
			virtual void *get_waitable() override;
		};
	} // namespace apple
} // namespace os

#endif OS_APPLE_SEMAPHORE_HPP