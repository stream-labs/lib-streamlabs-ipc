#include "async_op.hpp"
// #include "overlapped.hpp"
#include <semaphore.h>

namespace os {
    namespace apple {
        class named_pipe;

        class async_request: public os::async_op {
			protected:
			sem_t *sem;

			void set_sem(sem_t *sem);

			void set_valid(bool valid);

			public:
			virtual ~async_request();

			virtual bool is_valid() override;

			virtual void invalidate() override;

			virtual bool is_complete() override;

			virtual bool cancel() override;

			virtual void call_callback() override;

			virtual void call_callback(os::error ec, size_t length) override;

			// os::waitable
			virtual void *get_waitable() override;

			public:
			friend class os::apple::named_pipe;
			friend class os::waitable;
        };
    }
}
