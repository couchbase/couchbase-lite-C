//
// CBLBlob+FILE.cc
//
// Copyright © 2021 Couchbase. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "CBLBlob+FILE.h"
#include "CBLLog.h"

using namespace std;
using namespace fleece;


/*
    There are two nonstandard APIs in <stdio.h> for opening a `FILE*` with custom read/write/seek
    behavior:
    - Apple platforms and BSD have `funopen`.
    - GNU's libc (Linux) has a similar API called `fopencookie`.
    - Sadly, Windows does not support this :(

    The two functions, and the callbacks they use, have slightly different parameter types and
    semantics. Since `fopencookie`s callbacks have more sensible types, we've implemented those
    and then added some `funopen`-compatible wrapper functions.

    `funopen` callback error reporting is consistent:
    > All user I/O functions can report an error by returning -1.  Additionally,
    > all of the functions should set the external variable errno appropriately
    > if an error occurs.

    `fopencookie`s man page doesn't mention setting `errno`, but presumably it's allowed.
    The return values are somewhat inconsistent in that the write callback is supposed to return
    0, not -1, on error. (Even though the man page itself shows an example that returns -1...)

    References:
    <https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/funopen.3.html>
    <https://www.freebsd.org/cgi/man.cgi?query=funopen>
    <https://man7.org/linux/man-pages/man3/fopencookie.3.html>
 */


#ifndef _MSC_VER

#ifdef __APPLE__ // ...or BSD...
    #define USE_FUNOPEN
#endif


static inline int withErrno(int err) {
    errno = err;
    return -1;
}


#pragma mark - STDIO READ CALLBACKS:


static ssize_t readFn_cookie(void *cookie, char *dst, size_t len) noexcept {
    int result = CBLBlobReader_Read((CBLBlobReadStream*)cookie, dst, len, nullptr);
    if (result < 0)
        return withErrno(EIO);
    return result;
}


_cbl_unused
static int readFn_fun(void *cookie, char *dst, int len) {
    if (len < 0)
        return withErrno(EINVAL);
    return int(readFn_cookie(cookie, dst, len));
}


static int seekFn_cookie(void *cookie, int64_t *offset, int mode) noexcept {
    CBLSeekBase base;
    switch (mode) {
        case SEEK_SET:  base = kCBLSeekModeFromStart; break;
        case SEEK_CUR:  base = kCBLSeekModeRelative; break;
        case SEEK_END:  base = kCBLSeekModeFromEnd; break;
        default:        return withErrno(EINVAL);
    }
    int64_t newOffset = CBLBlobReader_Seek((CBLBlobReadStream*)cookie, *offset, base, nullptr);
    if (newOffset < 0)
        return withErrno(EINVAL);
    *offset = newOffset;
    return 0;
}


_cbl_unused
static fpos_t seekFn_fun(void *cookie, fpos_t pos, int mode) noexcept {
    return (seekFn_cookie(cookie, &pos, mode) == 0) ? pos : -1;
}


static int closeReaderFn(void *cookie) noexcept {
    CBLBlobReader_Close((CBLBlobReadStream*)cookie);
    return 0;
}


#pragma mark - STDIO WRITE CALLBACKS:


static ssize_t writeFn_cookie(void *cookie, const char *src, size_t len) noexcept {
    // "the write function should return the number of bytes copied from buf, or 0 on error.
    //  (The function must not return a negative value.)" --Linux man page
    if (len == 0) {
        errno = EINVAL;
        return 0;
    }
    if (!CBLBlobWriter_Write((CBLBlobWriteStream*)cookie, src, len, nullptr)) {
        errno = EIO;
        return 0;
    }
    return len;
}


_cbl_unused
static int writeFn_fun(void *cookie, const char *src, int len) noexcept {
    if (len < 0)
        return withErrno(EINVAL);
    if (len == 0)
        return 0;
    auto bytesWritten = writeFn_cookie(cookie, src, len);
    return bytesWritten > 0 ? int(bytesWritten) : -1;
}


// Coordinator between `closeWriterFn` and `CBLBlobWriter_CreateFILE`.
// (It's thread-local to avoid race conditions if multiple threads create blobs at once.)
__thread static CBLBlobWriteStream** sPutStreamHereOnClose = nullptr;


static int closeWriterFn(void *cookie) noexcept {
    if (sPutStreamHereOnClose) {
        // Instead of actually closing, copy the pointer to the blob write stream where the
        // `CBLBlob_CreateWithFILE` function can retrieve it.
        *sPutStreamHereOnClose = (CBLBlobWriteStream*)cookie;
    } else {
        // If our secret pointer is NULL, then `CBLBlobWriter_CreateFILE` isn't being called,
        // so the app must just be calling `fclose` itself to cancel creating a blob.
        CBLBlobWriter_Close((CBLBlobWriteStream*)cookie);
    }
    return 0;
}


static CBLBlobWriteStream* closeFILEAndRecoverStream(FILE *f) {
    // There's no stdio API to recover the "cookie" value from a custom `FILE*`, so how are we
    // going to get the `CBLBlobWriteStream*` back?
    // Kludgy solution: the "close" callback (`closeWriterFn`) stores the cookie into a variable
    // pointed to by the static pointer `sPutStreamHereOnClose`, so after calling `fclose`
    // -- which we need to do anyway to flush the buffer -- our variable will be set.
    // If it wasn't, it means the caller passed in a `FILE*` we didn't open, which is an error.
    CBLBlobWriteStream *stream = nullptr;
    sPutStreamHereOnClose = &stream;
    fclose(f);
    sPutStreamHereOnClose = nullptr;
    return stream;
}


#pragma mark - API FUNCTIONS:


FILE* CBLBlob_OpenAsFILE(CBLBlob* blob, CBLError* outError) noexcept {
    auto stream = CBLBlob_OpenContentStream(blob, outError);
    if (!stream)
        return nullptr;
#ifdef USE_FUNOPEN
    return funopen(stream, &readFn_fun, nullptr, &seekFn_fun, &closeReaderFn);
#else
    return fopencookie(stream, "r", {&readfn_cookie, nullptr, &seekfn_cookie, &closeReaderFn});
#endif
}


FILE* _cbl_nullable CBLBlobWriter_CreateFILE(CBLDatabase* db, CBLError* outError) noexcept {
    CBLBlobWriteStream *stream = CBLBlobWriter_Create(db, outError);
    if (!stream)
        return nullptr;
#ifdef USE_FUNOPEN
    FILE *f = funopen(stream, nullptr, writeFn_fun, nullptr, closeWriterFn);
#else
    FILE *f = fopencookie(stream, "w", {nullptr, &writefn_cookie, nullptr, closeWriterFn});
#endif
    if (!f)
        CBLBlobWriter_Close(stream);
    return f;
}


CBLBlob* CBLBlob_CreateWithFILE(FLString contentType, FILE* file) noexcept {
    CBLBlobWriteStream *stream = closeFILEAndRecoverStream(file);
    if (!stream) {
        CBL_LogMessage(kCBLLogDomainDatabase, kCBLLogError,
                       FLSTR("CBLBlob_CreateWithFILE was called with a FILE* not opened by"
                             " CBLBlobWriter_CreateFILE"));
        return nullptr;
    }
    return CBLBlob_CreateWithStream(contentType, stream);
}

#endif // _MSC_VER
