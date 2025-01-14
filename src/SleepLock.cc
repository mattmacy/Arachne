#include "SleepLock.h"
#include "Arachne.h"

namespace Arachne {

/**
 * Attempt to acquire this resource and block if it is not available.
 */
void
SleepLock::lock() {
    std::unique_lock<SpinLock> guard(blockedThreadsLock);
    if (owner == NULL) {
        owner = core.loadedContext;
        return;
    }
    blockedThreads.push_back(getThreadId());
    guard.unlock();
    while (true) {
        // Spurious wake-ups can happen due to signalers of past inhabitants of
        // this core.loadedContext.
        dispatch();
        blockedThreadsLock.lock();
        if (owner == core.loadedContext) {
            blockedThreadsLock.unlock();
            break;
        }
        blockedThreadsLock.unlock();
    }
}

/**
 * Attempt to acquire this resource once.
 * \return
 *    Whether or not the acquisition succeeded.
 */
bool
SleepLock::try_lock() {
    std::lock_guard<SpinLock> guard(blockedThreadsLock);
    if (owner == NULL) {
        owner = core.loadedContext;
        return true;
    }
    return false;
}

/** Release resource. */
void
SleepLock::unlock() {
    std::lock_guard<SpinLock> guard(blockedThreadsLock);
    if (blockedThreads.empty()) {
        owner = NULL;
        return;
    }
    owner = blockedThreads.front().context;
    schedule(blockedThreads.front());
    blockedThreads.pop_front();
}

bool
SleepLock::owned() {
    return owner != nullptr;
}

/**
 * Attempt to acquire this resource and block if it is not available.
 */
void
SleepLockSX::xlock() {
    std::unique_lock<SpinLock> guard(blockedThreadsLock);

    if (owner == NULL && shared == 0) {
        owner = core.loadedContext;
        return;
    }
    blockedXThreads.push_back(getThreadId());
    guard.unlock();
    while (true) {
        // Spurious wake-ups can happen due to signalers of past inhabitants of
        // this core.loadedContext.
        dispatch();
        blockedThreadsLock.lock();
        if (owner == core.loadedContext) {
            blockedThreadsLock.unlock();
            break;
        }
        blockedThreadsLock.unlock();
    }
}

/**
 * Attempt to acquire this resource once.
 * \return
 *    Whether or not the acquisition succeeded.
 */
bool
SleepLockSX::try_xlock() {
    std::lock_guard<SpinLock> guard(blockedThreadsLock);

    if (owner == NULL && shared == 0) {
        owner = core.loadedContext;
        return true;
    }
    return false;
}

/** Release resource. */
void
SleepLockSX::xunlock() {
    std::lock_guard<SpinLock> guard(blockedThreadsLock);
    if (!blockedSThreads.empty()) {
        owner = NULL;
        while (!blockedSThreads.empty()) {
            schedule(blockedSThreads.front());
            blockedSThreads.pop_front();
        }
        return;
    }
    if (blockedXThreads.empty()) {
        owner = nullptr;
        return;
    }
    owner = blockedXThreads.front().context;
    schedule(blockedXThreads.front());
    blockedXThreads.pop_front();
}

void
SleepLockSX::slock() {
    std::unique_lock<SpinLock> guard(blockedThreadsLock);

    if (owner == nullptr && blockedXThreads.empty()) {
        shared++;
        return;
    }

    blockedSThreads.push_back(getThreadId());
    guard.unlock();
    while (true) {
        // Spurious wake-ups can happen due to signalers of past inhabitants of
        // this core.loadedContext.
        dispatch();
        blockedThreadsLock.lock();
        if (owner == nullptr) {
            shared++;
            blockedThreadsLock.unlock();
            break;
        }
        blockedThreadsLock.unlock();
    }
}

/**
 * Attempt to acquire this resource once.
 * \return
 *    Whether or not the acquisition succeeded.
 */
bool
SleepLockSX::try_slock() {
    std::lock_guard<SpinLock> guard(blockedThreadsLock);
    if (owner == NULL && blockedXThreads.empty()) {
        shared++;
        return true;
    }
    return false;
}


/** Release resource. */
void
SleepLockSX::sunlock() {
    std::lock_guard<SpinLock> guard(blockedThreadsLock);
    assert(shared > 0);
    shared--;
    if (shared || blockedXThreads.empty()) {
        return;
    }
    owner = blockedXThreads.front().context;
    schedule(blockedXThreads.front());
    blockedXThreads.pop_front();
}

bool
SleepLockSX::owned() {
    return (owner != nullptr) || (shared != 0);
}

uint32_t
SleepLockSX::get_num_waiters() {
    return blockedSThreads.size() + blockedXThreads.size();
}

}
