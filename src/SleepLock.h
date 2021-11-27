/* Copyright (c) 2015-2018 Stanford University
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef ARACHNE_SLEEPLOCK_H_
#define ARACHNE_SLEEPLOCK_H_

#include <atomic>
#include <deque>

#include "Common.h"
#include "SpinLock.h"
#include "ThreadId.h"


namespace Arachne {

struct ThreadContext;

/**
 * A resource which blocks the current thread until it is available.
 * This resources should not be acquired from non-Arachne threads.
 */
class SleepLock {
  public:
    /** Constructor and destructor for sleepLock. */
    SleepLock()
        : blockedThreads(),
          blockedThreadsLock("blockedthreadslock", false),
          owner(NULL) {}
    ~SleepLock() {}
    void lock();
    bool try_lock();
    void unlock();
    bool owned();

  private:
    // Ordered collection of threads that are waiting on this lock. Threads
    // are processed from this list in FIFO order when a notifyOne() is called.
    std::deque<ThreadId> blockedThreads;

    // A SpinLock to protect the blockedThreads data structure.
    SpinLock blockedThreadsLock;

    // Used to identify the owning context for this lock. The lock is held iff
    // owner != NULL.
    ThreadContext* owner;
};

/**
 * A resource which blocks the current thread until it is available.
 * This resources should not be acquired from non-Arachne threads.
 */
class SleepLockSX {
  public:
    /** Constructor and destructor for sleepLock. */
    SleepLockSX()
        : blockedSThreads(),
          blockedXThreads(),
          shared(0),
          blockedThreadsLock("blockedthreadslock", false),
          owner(NULL) {}
    ~SleepLockSX() {}
    void slock();
    bool try_slock();
    void sunlock();
    void xlock();
    bool try_xlock();
    void xunlock();
    bool owned();
    uint32_t get_num_waiters();

  private:
    // Ordered collection of threads that are waiting on this lock. Threads
    // are processed from this list in FIFO order when a notifyOne() is called.
    std::deque<ThreadId> blockedSThreads;

    // Ordered collection of threads that are waiting on this lock. Threads
    // are processed from this list in FIFO order when a notifyOne() is called.
    std::deque<ThreadId> blockedXThreads;

    volatile int16_t shared;

    // A SpinLock to protect the blockedThreads data structure.
    SpinLock blockedThreadsLock;

    // Used to identify the owning context for this lock. The lock is held iff
    // owner != NULL.
    ThreadContext* owner;
};

}
#endif
