// mod blob

use super::*;
use crate::slice::*;
use crate::c_api::*;

use std::ffi::c_void;
use std::marker::PhantomData;


/** A binary attachment to a Document. */
pub struct Blob {
    pub(crate) _ref: *const CBLBlob
}


impl Blob {

    //////// CREATION

    /** Creates a new blob, given its contents as a byte array. */
    pub fn new_from_data(data: &[u8], content_type: &str) -> Blob {
        unsafe {
            let blob = CBLBlob_CreateWithData_s(as_slice(content_type), bytes_as_slice(data));
            return Blob{_ref: blob};
        }
    }

    /** Creates a new blob from data that has has been written to a \ref Writeer.
        You should then add the blob to a document as a property, using \ref Slot::put_blob. */
    pub fn new_from_stream(mut stream: BlobWriter, content_type: &str) -> Blob {
        unsafe {
            let blob = CBLBlob_CreateWithStream_s(as_slice(content_type), stream._stream_ref);
            stream._stream_ref = std::ptr::null_mut();  // stop `drop` from closing the stream
            return Blob{_ref: blob};
        }
    }

    // called by FleeceReference::as_blob()
    fn from_value<V: FleeceReference>(value: &V) -> Option<Blob> {
        unsafe {
            let blob = CBLBlob_Get(FLValue_AsDict(value._fleece_ref()));
            return if blob.is_null() {None} else {Some(Blob{_ref: blob})};
        }
    }

    //////// ACCESSORS


    /** The length of the content data in bytes. */
    pub fn length(&self) -> u64 {
        unsafe { CBLBlob_Length(self._ref) }
    }

    /** The unique digest of the blob: A base64-encoded SHA-1 digest of its data. */
    pub fn digest(&self) -> String {
        unsafe { to_string(CBLBlob_Digest(self._ref)) }
    }

    /** The MIME type assigned to the blob, if any. */
    pub fn content_type(&self) -> Option<String> {
        unsafe {
            let cstr = CBLBlob_ContentType(self._ref);
            return if cstr.is_null() { None } else { Some(to_string(cstr)) };
        }
    }

    /** The blob's metadata properties as a dictionary. */
    pub fn properties(&self) -> Dict {
        unsafe { Dict{_ref: CBLBlob_Properties(self._ref), _owner: PhantomData} }
    }


    //////// READING:

    /** Reads the blob's contents into memory and returns them as a byte array.
        This can potentially allocate a lot of memory! */
    pub fn load_content(&self) -> Result<Vec<u8>> {
        unsafe {
            let mut err = CBLError::default();
            let content = CBLBlob_LoadContent(self._ref, &mut err).to_vec();
            return if let Some(c) = content { Ok(c) } else { failure(err) };
        }
    }

    /** Opens a stream for reading a blob's content from disk. */
    pub fn open_content(&self) -> Result<BlobReader> {
        check_ptr(|err|    unsafe{ CBLBlob_OpenContentStream(self._ref, err) },
                  |stream| BlobReader{blob: self, _stream_ref: stream})
    }
}

impl Drop for Blob {
    fn drop(&mut self) {
        unsafe { release(self._ref as *mut CBLBlob); }
    }
}

impl Clone for Blob {
    fn clone(&self) -> Self {
        unsafe { Blob{_ref: retain(self._ref as *mut CBLBlob)} }
    }
}


//////// BLOB ADDITIONS FOR ARRAY / DICT:


impl Slot<'_> {
    /** Stores a Blob reference in an Array or Dict. This is how you add a Blob to a Document. */
    pub fn put_blob(self, blob: &mut Blob) {
        unsafe { FLSlot_SetBlob(self._ref, blob._ref as *mut CBLBlob) }
    }
}


//////// BLOB READER


/** A stream for reading Blob conents. */
pub struct BlobReader<'r> {
    pub blob: &'r Blob,
    _stream_ref: *mut CBLBlobReadStream
}

impl<'r> std::io::Read for BlobReader<'r> {
    fn read(&mut self, buf: &mut [u8]) -> std::io::Result<usize> {
        unsafe {
            check_io(|err| CBLBlobReader_Read(self._stream_ref,
                                              buf.as_mut_ptr() as *mut c_void,
                                              buf.len() as u64,
                                              err))
        }
    }
}

impl<'r> Drop for BlobReader<'r> {
    fn drop(&mut self) {
        unsafe { CBLBlobReader_Close(self._stream_ref); }
    }
}


//////// BLOB WRITER


/** A stream for writing data that will become a Blob's contents.
    After you're done writing the data, call \ref Blob::new_from_stream,
    then add the Blob to a document property via \ref Slot::put_blob. */
pub struct BlobWriter<'d> {
    _stream_ref: *mut CBLBlobWriteStream,
    db: PhantomData<&'d mut Database>
}

impl<'d> BlobWriter<'d> {
    pub fn new(db: &'d mut Database) -> Result<BlobWriter<'d>> {
        unsafe {
            let db_ref = db._ref;
            check_ptr(|err| CBLBlobWriter_New(db_ref, err),
                      move |stream| BlobWriter{_stream_ref: stream, db: PhantomData})
        }
    }
}

impl<'r> std::io::Write for BlobWriter<'r> {
    fn write(&mut self, data: &[u8]) -> std::result::Result<usize, std::io::Error> {
        unsafe {
            check_io(|err| {
                let ok = CBLBlobWriter_Write(self._stream_ref,
                                             data.as_ptr() as *const c_void,
                                             data.len() as u64,
                                             err);
                if ok {data.len() as i32} else {-1}
            })
        }
    }

    fn flush(&mut self) -> std::result::Result<(), std::io::Error> {
        Ok(())
    }
}

impl<'r> Drop for BlobWriter<'r> {
    fn drop(&mut self) {
        unsafe { CBLBlobWriter_Close(self._stream_ref) }
    }
}
