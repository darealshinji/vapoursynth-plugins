/*****************************************************************************
 * AlignedMemory.cpp
 *****************************************************************************
 * Copyright (C) 2015
 *
 * Authors: Yusuke Nakamura <muken.the.vfrmaniac@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *****************************************************************************/

#include <cstddef>
#include <cassert>
#include <cstdlib>

namespace AlignedMemory
{
    void *alloc( size_t size, size_t alignment )
    {
        assert( alignment <= 0x80 );
        void *ptr = std::malloc( size + alignment );
        if( ptr == nullptr )
            return nullptr;
        ptrdiff_t diff = ((~(reinterpret_cast<ptrdiff_t>(ptr))) & (alignment - 1)) + 1;
        ptr = static_cast<void *>(static_cast<char *>(ptr) + diff);
        (static_cast<char *>(ptr))[-1] = diff;
        return ptr;
    }

    void free( void *ptr )
    {
        if( ptr )
            std::free( static_cast<char *>(ptr) - static_cast<ptrdiff_t>(((char *)ptr)[-1]) );
    }
}
