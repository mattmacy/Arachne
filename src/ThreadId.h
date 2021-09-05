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

#ifndef ARACHNE_THREADID_H_
#define ARACHNE_THREADID_H_


namespace Arachne {

struct ThreadContext;

/**
 * This structure is used to identify an Arachne thread to methods of the
 * Arachne API.
 */
struct ThreadId {
    /// The storage where this thread's state is held.
    ThreadContext* context;
    /// Differentiates this Arachne thread from previous threads (now defunct)
    /// that used the same context.
    uint32_t generation;

    /// Construct a ThreadId.
    /// \param context
    ///    The location where the thread's metadata currently lives.
    /// \param generation
    ///    Used to differentiate this thread from others that lived at this
    ///    context in the past and future.
    ThreadId(ThreadContext* context, uint32_t generation)
        : context(context), generation(generation) {}

    ThreadId() : context(NULL), generation(0) {}

    /// The equality operator is generally used for comparing against
    /// Arachne::NullThread.
    bool operator==(const ThreadId& other) const {
        return context == other.context && generation == other.generation;
    }

    /// Negation of the function above.
    bool operator!=(const ThreadId& other) const { return !(*this == other); }

    bool operator!() const { return *this == ThreadId(); }
};

}

#endif
