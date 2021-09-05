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
    blockedThreadsLock.lock();
    if (blockedThreads.empty()) {
        owner = NULL;
        blockedThreadsLock.unlock();
        return;
    }
    owner = blockedThreads.front().context;
    signal(blockedThreads.front());
    blockedThreads.pop_front();
    blockedThreadsLock.unlock();
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
    blockedThreadsLock.lock();
    if (!blockedSThreads.empty()) {
        owner = NULL;
        while (!blockedSThreads.empty()) {
            signal(blockedSThreads.front());
            blockedSThreads.pop_front();
            blockedThreadsLock.unlock();
            shared++;
        }
        blockedThreadsLock.unlock();
        return;
    }
    if (blockedXThreads.empty()) {
        owner = nullptr;
        blockedThreadsLock.unlock();
        return;
    }
    owner = blockedXThreads.front().context;
    signal(blockedXThreads.front());
    blockedXThreads.pop_front();
    blockedThreadsLock.unlock();
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
    blockedThreadsLock.lock();
    shared--;
    if (shared || blockedXThreads.empty()) {
        blockedThreadsLock.unlock();
        return;
    }
    owner = blockedXThreads.front().context;
    signal(blockedXThreads.front());
    blockedXThreads.pop_front();
    blockedThreadsLock.unlock();
}

bool
SleepLockSX::owned() {
    return (owner != nullptr) || (shared != 0);
}


}
