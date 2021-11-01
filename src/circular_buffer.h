/* Copyright (c) 2021 Matthew Macy
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

#include <vector>

#pragma once
template <typename T, int size>
class circular_buffer {
    volatile uint32_t	br_prod;
    volatile uint32_t	br_cons alignas(CACHE_LINE_SIZE);
    std::vector<T *>    br_ring alignas(CACHE_LINE_SIZE);

public:
    circular_buffer() : br_prod(0), br_cons(0), br_ring(size)
    {
        static_assert((size & (size-1)) == 0, "size must be power-of-2");
    }
    DISALLOW_COPY_AND_ASSIGN(circular_buffer);

    bool enqueue(T *value) {
        uint32_t prod_next = (br_prod + 1) & (size-1);

        if (unlikely(prod_next == br_cons)) {
            return false;
        }
        br_ring[br_prod] = value;
        br_prod = prod_next;
        return true;
    }

    T *dequeue() {

        if (br_prod == br_cons) {
            return nullptr;
        }
        T *value = br_ring[br_cons];
        br_cons = (br_cons + 1) & (size-1);
        return value;
    }
};
