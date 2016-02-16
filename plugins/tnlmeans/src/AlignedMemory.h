/*****************************************************************************
 * AlignedMemory.h
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

namespace AlignedMemory
{
    void *alloc( size_t size, size_t alignment );
    void free( void *ptr );
}

template < typename T, size_t alignment >
class AlignedArrayObject
{
private:
    T *x;
public:
    using bad_alloc = class : std::bad_alloc { using std::bad_alloc::bad_alloc; };
    AlignedArrayObject() { x = nullptr; }
    template < typename U >
    AlignedArrayObject( U n )
    {
        if( n < 0 )
            throw bad_alloc{};
        x = static_cast< T * >(AlignedMemory::alloc( n * sizeof(T), alignment ));
        if( x == nullptr )
            throw bad_alloc{};
    }
    ~AlignedArrayObject() { AlignedMemory::free( x ); }
    inline T * get() const { return x; }
};
