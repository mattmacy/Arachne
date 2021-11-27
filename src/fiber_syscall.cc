#include "fiber_syscall.h"
#include "Arachne.h"

namespace Arachne {

template<uint8_t opcode>
int
uring_syscall(int fd, void *argptr, uint64_t argint,
              uint64_t off, int flags, uint64_t timeout_ms)
{
    struct io_uring_sqe *sqe;
    struct iovec *iovp = nullptr;
    struct iovec *iovin = nullptr;
    int iovcnt = 0;
    int rc;

    static_assert(opcode == IORING_OP_WRITEV || opcode == IORING_OP_READV ||
                  opcode == IORING_OP_FSYNC || opcode == IORING_OP_SEND ||
                  opcode == IORING_OP_SENDMSG || opcode == IORING_OP_ACCEPT ||
                  opcode == IORING_OP_CONNECT || opcode == IORING_OP_CLOSE);
    switch (opcode) {
        case IORING_OP_WRITEV:
        case IORING_OP_READV:
            iovin = (struct iovec *)argptr;
            iovcnt = argint;
            break;
    }

    while ((sqe = io_uring_get_sqe(std::addressof(core.sys_io_ring))) == nullptr) {
        yield();
    }
    syscall_wait_request *request = new syscall_wait_request(core.loadedContext, core.loadedContext->generation);

    request->fd = fd;
    request->offset = off;
    request->iovcnt = iovcnt;
    request->opcode = opcode;
    request->refcount_local = 1;
    request->refcount = &request->refcount_local;
    if (iovcnt) {
        iovp = &request->iov[0];

        if (iovcnt > 1) {
            request->ext_arg = malloc(sizeof(*iovin)*iovcnt);
            iovp = static_cast<struct iovec *>(request->ext_arg);
        }
        memcpy(iovp, iovin, sizeof(*iovin)*iovcnt);
    }
    uint64_t min_delay = 1;
    uint64_t wakeup_time = -1ULL;
    if (timeout_ms != -1ULL) {
        wakeup_time = Cycles::rdtsc() + Cycles::fromMilliseconds(std::max(timeout_ms, min_delay));
    }
    core.loadedContext->wakeupTimeInCycles = wakeup_time;
     switch (opcode) {
        case IORING_OP_WRITEV:
        case IORING_OP_READV:
            io_uring_prep_rw(opcode, sqe, fd, iovp, iovcnt, off);
            break;
         case IORING_OP_SEND:
             io_uring_prep_send(sqe, fd, argptr, argint, flags);
            break;
         case IORING_OP_SENDMSG:
             io_uring_prep_sendmsg(sqe, fd, (const struct msghdr *)argptr, flags);
             break;
         case IORING_OP_ACCEPT:
             io_uring_prep_accept(sqe, fd, (struct sockaddr *)argptr, (socklen_t *) argint, flags);
             break;
         case IORING_OP_CONNECT:
             io_uring_prep_connect(sqe, fd, (struct sockaddr *)argptr, argint);
             break;
         case IORING_OP_CLOSE:
             io_uring_prep_close(sqe, fd);
             break;
     }
    io_uring_sqe_set_data(sqe, request);
    io_uring_sqe_set_flags(sqe, IOSQE_ASYNC);
    core.pending_requests.push_back(*request);
    io_uring_submit(std::addressof(core.sys_io_ring));
    dispatch();
    if (unlikely(request->result == INCOMPLETE_REQUEST)) {
        /* We were either interrupted or timed out.
         * The scheduler will free the syscall request for us.
         */
        request->cancelled = true;
        if (Cycles::rdtsc() >= wakeup_time) {
            return -ETIMEDOUT;
        } else {
            return -EINTR;
        }
    }
    request->unlink();
    rc = request->result;
    if (request->ext_arg) {
        free(request->ext_arg);
    }
    delete request;
    return rc;
}

template<uint8_t opcode>
int
uring_syscallv(int opcount, int *fds, struct iovec **iovs, int *iovcnts, uint64_t *offs, int *rcs, uint64_t timeout_ms)
{
    struct io_uring_sqe *sqe;
    struct iovec *iovp = nullptr;
    uint32_t *refcount;
    int rc, iovcnt;
    uint64_t off;

    static_assert(opcode == IORING_OP_WRITEV || opcode == IORING_OP_READV || opcode == IORING_OP_FSYNC);
    off = rc = iovcnt = 0;
    syscall_wait_request **requests = (syscall_wait_request **)alloca(sizeof(void *) * opcount);
    for (int i = 0; i < opcount; i++) {
        while ((sqe = io_uring_get_sqe(std::addressof(core.sys_io_ring))) == nullptr) {
            yield();
        }
        syscall_wait_request *request = new syscall_wait_request(core.loadedContext, core.loadedContext->generation);
        requests[i] = request;
        if (i == 0) {
            refcount = request->refcount = &request->refcount_local;
            *refcount = opcount;
        }
        request->refcount = refcount;
        request->fd = fds[i];
        if (offs)
            off = request->offset = offs[i];
        if (iovcnts)
            iovcnt = request->iovcnt = iovcnts[i];
        request->opcode = opcode;
        if (iovcnt) {
            iovp = &request->iov[0];
            if (iovcnt > 1) {
                request->ext_arg = malloc(sizeof(*iovp)*iovcnt);
                iovp = static_cast<struct iovec *>(request->ext_arg);
            }
            memcpy(iovp, iovs[i], sizeof(*iovp)*iovcnt);
        }
        io_uring_prep_rw(opcode, sqe, fds[i], iovp, iovcnt, off);
        io_uring_sqe_set_data(sqe, request);
        io_uring_sqe_set_flags(sqe, IOSQE_ASYNC);
        core.pending_requests.push_back(*request);
        io_uring_submit(std::addressof(core.sys_io_ring));
    }
    uint64_t min_delay = 1;
    uint64_t wakeup_time = -1ULL;
    if (timeout_ms != -1ULL) {
        wakeup_time = Cycles::rdtsc() + Cycles::fromMilliseconds(std::max(timeout_ms, min_delay));
    }
    core.loadedContext->wakeupTimeInCycles = wakeup_time;

    dispatch();
    if (unlikely(*requests[0]->refcount != 0)) {
        /* We were either interrupted or timed out.
         * The scheduler will free the syscall request for us.
         */
        for (int i = 0; i < opcount; i++) {
            requests[i]->cancelled = true;
        }
        if (Cycles::rdtsc() >= wakeup_time) {
            return -ETIMEDOUT;
        } else {
            return -EINTR;
        }
    }
    for (int i = 0; i < opcount; i++) {
        syscall_wait_request *request = requests[i];
        rcs[i] = request->result;
        if (rcs[i] < 0)
            rc = rcs[i];
        request->unlink();
        rc = request->result;
        if (request->ext_arg) {
            free(request->ext_arg);
        }
        delete request;
    }
    return rc;
}

ssize_t
preadv(int fd, const struct iovec *iov, int iovcnt, uint64_t off, uint64_t timeout_ms)
{
    return uring_syscall<IORING_OP_READV>(fd, (void *)(uintptr_t)iov, iovcnt, off, /* flags */ 0, timeout_ms);
}

int
preadvv(int iocnt, int *fds, struct iovec **iovs, int *iovcnts, uint64_t *offs, int *rcs, uint64_t timeout_ms)
{
    return uring_syscallv<IORING_OP_READV>(iocnt, fds, iovs, iovcnts, offs, rcs, timeout_ms);
}

ssize_t
pwritev(int fd, struct iovec *iov, int iovcnt, uint64_t off, uint64_t timeout_ms)
{
    return uring_syscall<IORING_OP_WRITEV>(fd, iov, iovcnt, off, /* flags */ 0, timeout_ms);
}

int
pwritevv(int iocnt, int *fds, struct iovec **iovs, int *iovcnts, uint64_t *offs, int *rcs, uint64_t timeout_ms)
{
    return uring_syscallv<IORING_OP_WRITEV>(iocnt, fds, iovs, iovcnts, offs, rcs, timeout_ms);
}

int
fsync(int fd, uint64_t timeout_ms)
{
    return uring_syscall<IORING_OP_FSYNC>(fd, nullptr, 0, 0, /* flags */ 0, timeout_ms);
}

int
fsyncv(int iocnt, int *fds, int *rcs, uint64_t timeout_ms)
{
    return uring_syscallv<IORING_OP_FSYNC>(iocnt, fds, nullptr, nullptr, nullptr, rcs, timeout_ms);
}

ssize_t
send(int sockfd, const void *buf, size_t len, int flags, uint64_t timeout_ms)
{
    return uring_syscall<IORING_OP_SEND>(sockfd, (void *)(uintptr_t)buf, len, /* off */0, flags, timeout_ms);
}

ssize_t
sendmsg(int sockfd, const struct msghdr *msg, int flags, uint64_t timeout_ms)
{
    return uring_syscall<IORING_OP_SENDMSG>(sockfd, (void *)(uintptr_t)msg, /* len */ 0, /* off */ 0, flags, timeout_ms);
}

int
accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    return uring_syscall<IORING_OP_ACCEPT>(sockfd, addr, (uint64_t) addrlen, /* off */ 0, 0, -1);
}

int
connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen, uint64_t timeout_ms)
{
    return uring_syscall<IORING_OP_CONNECT>(sockfd, (void *)(uintptr_t)addr, addrlen, /* off */ 0, /* flags */ 0, timeout_ms);
}

int
close(int fd)
{
    return uring_syscall<IORING_OP_CLOSE>(fd, nullptr, 0, /* off */ 0, /* flags */ 0, -1);
}

void
check_for_completions(struct io_uring *ring)
{
    struct io_uring_cqe *cqe;

    while (io_uring_peek_cqe(ring, &cqe) == 0) {
        syscall_wait_request *request = (syscall_wait_request *)io_uring_cqe_get_data(cqe);
        request->result = cqe->res;

        io_uring_cqe_seen(ring, cqe);

        if (unlikely(request->cancelled)) {
            request->unlink();
            if (request->ext_arg) {
                free(request->ext_arg);
            }
            delete request;
            continue;
        }
        (*request->refcount)--;
        if (*request->refcount == 0) {
            schedule(request->tid);
        }
    }
}

} // namespace Arachne
