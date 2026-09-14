#include <cassert>
#include <cerrno>

extern "C" {
#include "../components/microlink/src/ml_io_policy.h"
}

int main() {
    assert(ml_io_errno_is_transient(EAGAIN));
    assert(ml_io_errno_is_transient(EWOULDBLOCK));
    assert(ml_io_errno_is_transient(EINTR));

    assert(!ml_io_errno_is_transient(ENOMEM));
    assert(!ml_io_errno_is_transient(ECONNRESET));
    assert(!ml_io_errno_is_transient(EPIPE));

    assert(ml_io_send_should_retry(EAGAIN, 0, 0));
    assert(ml_io_send_should_retry(EINTR, ML_IO_SEND_MAX_RETRIES - 1,
                                   ML_IO_SEND_RETRY_WINDOW_MS - 1));
    assert(!ml_io_send_should_retry(EAGAIN, ML_IO_SEND_MAX_RETRIES, 0));
    assert(!ml_io_send_should_retry(EAGAIN, 0, ML_IO_SEND_RETRY_WINDOW_MS));
    assert(!ml_io_send_should_retry(ENOMEM, 0, 0));
}
