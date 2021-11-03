/*
 *  There are three modes for handling syscalls asynchronously:
 *
 *   a) global io_ring: Maintatain a global io_uring managed by a
 *      dedicated thread that also serves to proxy unsupported system
 *      calls to service threads.
 * 
 *   b) pcpu io_uring - dispatched locally: Maintain an io_uring 
 * 	    ring per-core to dispatch system calls that can be 
 *	    dispatched through the ring and a separately managed pool
 *      of service threads to execute the remaining system calls.
 *      Backlog is flushed locally based on timing and scheduling
 *      heuristics.
 *
 *   c) pcpu io_uring - dispatched globally: As in (b) but a dedicated
 *      service thread polls all of the pcpu rings as well as a non
 *      blocking queue for unsupported calls.
 *
 *
 *  global io_uring
 *  All system call requests feed into a global or per-socket non-blocking
 *  queue that is polled by (a) dedicated service thread(s). All calls that
 *  can be sent to io_ring will be enqueued there while the remainder are
 *  in turn passed on to a thread pool.
 *
 *  PROS:
 *   - dispatch latency limited to time handling atomics on both ends plus
 *     time required to flush backlog on other cores.
 *   - uniform interface between fibers and system call layer
 *   - no changes required to scheduler
 *  CONS:
 *   - global atomic operation for every system call
 *
 *
 *   pcpu io_uring - local flushing
 *   Core adds the following fields:
 *    - ring:      io_uring structure for dispatch
 *    - backlog:   count of undispatched requests
 *    - timestamp: if backlog is non-zero, the cycle counter when the
 *      first entry in the backlog was enqueued.
 *   The backlog and timestamp fields can be used to heuristically
 *   batch calls.
 *   A non-blocking queue to a thread pool will be maintained for passing
 *   requests. The main challenge will be to minimize the overhead of calling
 *   pthread_cond_signal to wakeup a thread in the blocking system call thread
 *   pool.
 *
 *   PROS:
 *    - no atomics required in the common case for io_uring
 *   CONS:
 *    - every cpu has to potentially pay the price of signalling the blocking
 *      system call thread pool.
 *    - system calls are possibly delayed up to threshold microseconds.
 *    - scheduler changes required to support batching
 *    - non-uniform system call interface
 *    - every cpu has to incur the potential latency of making the system call
 *      to flush the backlog.
 *
 *
 *   pcpu io_uring - global flushing
 *   Global service thread polls the pcpus rings, flushing their backlogs as well
 *   as polling the blocking system call queue.
 *   A non-blocking queue to a thread pool will be maintained for passing
 *   requests. The main challenge will be to minimize the overhead of calling
 *   pthread_cond_signal to wakeup a thread in the blocking system call thread
 *   pool.
 *
 *   PROS:
 *    - no atomics required in the common case for io_uring
 *    - dispatch latency limited to time required to poll latency from flushing
 *      other threads' backlog plus potential delay caused pthread_cond_signal
 *      in service thread.
 *    
 *   CONS:
 *    - non-uniform system call interface
 *
 *
 *   CONCLUSION:
 *   Option (c) largely combines the performance benefits of options (a) and (b)
 *   providing sub-microsecond overhead in the common case. Worst case system call
 *   dispatch latency should be in the single digit microseconds. 
 *   
 *   UPDATE:
 *   Pre-5.6 a global io_uring is required as we will otherwise drown the system in
 *   worker queue threads (4 * ncpus) per queue. This would result in up to tens of
 *   thousands of threads.
 *
 *   The current implementation is geared towards 5.11 and later. This simplifies the
 *   initial implementation by having all queues share the same worker queue thread pool
 *   while having the kernel poll for submissions using IORING_SETUP_SQPOLL.
 *
 */

#pragma once
#include <sys/types.h>
#include <sys/socket.h>

#include "intrusive_list.h"
#include "ThreadId.h"
#include "utils.h"
#include "liburing.h"

namespace Arachne {
#define INCOMPLETE_REQUEST -255

    struct syscall_request {
        uint16_t code;
        uint8_t  arg_count;
        uint64_t args[6];
        uint64_t result;
    };

    struct syscall_wait_request : public intrusive_list_base_hook<> {
        ThreadId tid;
        bool cancelled;
        uint8_t opcode;
        uint16_t iovcnt;
        int result;

        int fd;
        uint32_t refcount_local;
        uint32_t *refcount;
        uint64_t offset;
        void * ext_arg;
        struct iovec iov[1];

        DISALLOW_COPY_AND_ASSIGN(syscall_wait_request);
        syscall_wait_request(ThreadContext *context, uint32_t generation) :
            tid(context, generation),
            cancelled(false),
            result(INCOMPLETE_REQUEST),
            offset(0),
            ext_arg(nullptr)
            {
                this->refcount = &this->refcount_local;
            }
    };


    void check_for_completions(struct io_uring *ring);

    /*
     * Functions supported by io_uring on 5.4
     */
    ssize_t preadv(int fd, const struct iovec *iov, int iovcnt, uint64_t off, uint64_t timeout_ms);
    int preadvv(int iocnt, int *fd, const struct iovec **iov, int *iovcnt, uint64_t *off, int *rcs, uint64_t timeout_ms);
    ssize_t pwritev(int fd, const struct iovec *iov, int iovcnt, uint64_t off, uint64_t timeout_ms);
    int pwritevv(int iocnt, int *fds, const struct iovec **iovs, int *iovcnts, uint64_t *offs, int *rcs, uint64_t timeout_ms);
    int fsync(int fd, uint64_t timeout_ms);
    int fsyncv(int iocnt, int *fds, int *rcs, uint64_t timeout_ms);
    int poll(int fd, uint64_t timeout_ms);


    /*
     * supported in 5.6 -- emulated with poll for earlier
     */
    int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

    /*
     * supported in 5.6 -- emulated with poll for earlier
     */
    int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

    /*
     * supported in 5.6 -- dispatched to thread pool prior
     */
    int close(int fd);

    /*
     * supported in 5.6 -- unimplemented prior
     */
    int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);


}
