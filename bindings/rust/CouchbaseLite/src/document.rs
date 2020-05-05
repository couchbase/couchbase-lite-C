// mod document

use super::*;
use super::slice::*;
use super::c_api::*;


/** An in-memory copy of a document. */
pub struct Document {
    _ref: *mut CBLDocument
}


//////// DATABASE'S DOCUMENT API:


/** Conflict-handling options when saving or deleting a document. */
pub enum ConcurrencyControl {
    LastWriteWins  = kCBLConcurrencyControlLastWriteWins as isize,
    FailOnConflict = kCBLConcurrencyControlFailOnConflict as isize
}


pub type SaveConflictHandler = fn(&mut Document, &Document) -> bool;

pub type ChangeListener = fn(&Database, &str);


impl Database {
    /** Reads a document from the database. Each call to this function returns a new object
        containing the document's current state. */
    pub fn get_document(&self, id: &str) -> Result<Document> {
        unsafe {
            // we always get a mutable CBLDocument,
            // since Rust doesn't let us have MutableDocument subclass.
            let doc = CBLDatabase_GetMutableDocument_s(self._ref, as_slice(id));
            if doc.is_null() {
                return Err(Error::cbl_error(CouchbaseLiteError::NotFound));
            }
            return Ok(Document{_ref: doc});
        }
    }

    /** Saves a new or modified document to the database.
        If a conflicting revision has been saved since `doc` was loaded, the `concurrency`
        parameter specifies whether the save should fail, or the conflicting revision should
        be overwritten with the revision being saved.
        If you need finer-grained control, call `save_document_resolving` instead. */
    pub fn save_document(&mut self,
                         doc: &mut Document,
                         concurrency: ConcurrencyControl)
                         -> Result<Document>
    {
        let mut error = CBLError::default();
        unsafe {
            let doc2 = CBLDatabase_SaveDocument(self._ref, doc._ref, concurrency as u8, &mut error);
            if doc2.is_null() {
                return failure(error);
            }
            return Ok(Document{_ref: doc2 as *mut CBLDocument});
        }
    }

    /** Saves a new or modified document to the database. This function is the same as
        `save_document`, except that it allows for custom conflict handling in the event
        that the document has been updated since `doc` was loaded. */
    pub fn save_document_resolving(&mut self,
                                   _doc: &mut Document,
                                   _conflict_handler: SaveConflictHandler)
                                   -> Result<Document>
    {
        todo!()
    }

    pub fn purge_document_by_id(&mut self, id: &str) -> Result<()> {
        unsafe {
            return check_bool(|error| CBLDatabase_PurgeDocumentByID_s(self._ref, as_slice(id), error));
        }
    }

    /** Returns the time, if any, at which a given document will expire and be purged.
        Documents don't normally expire; you have to call `set_document_expiration`
        to set a document's expiration time. */
    pub fn document_expiration(&self, doc_id: &str) -> Result<Option<Timestamp>> {
        unsafe {
            let mut error = CBLError::default();
            let exp = CBLDatabase_GetDocumentExpiration_s(self._ref, as_slice(doc_id), &mut error);
            if exp < 0 {
                return failure(error);
            } else if exp == 0 {
                return Ok(None);
            } else {
                return Ok(Some(Timestamp(exp)));
            }
        }
    }

    /** Sets or clears the expiration time of a document. */
    pub fn set_document_expiration(&mut self, doc_id: &str, when: Option<Timestamp>) -> Result<()> {
        let exp :i64 = match when {
            Some(Timestamp(n)) => n,
            _ => 0,
        };
        unsafe {
            return check_bool(|error| CBLDatabase_SetDocumentExpiration_s(self._ref, as_slice(doc_id), exp, error));
        }
    }

    /** Registers a document change listener callback. It will be called after a specific document
        is changed on disk. */
    pub fn add_document_change_listener(&self, _doc_id: &str, _listener: ChangeListener) -> ListenerToken {
        todo!()
    }

}


//////// DOCUMENT API:


impl Document {

    /** Creates a new, empty document in memory, with an automatically generated unique ID.
        It will not be added to a database until saved. */
    pub fn new() -> Self {
        unsafe { Document{_ref: CBLDocument_New_s(NULL_SLICE)} }
    }

    /** Creates a new, empty document in memory, with the given ID.
        It will not be added to a database until saved. */
    pub fn new_with_id(id: &str) -> Self {
        unsafe { Document{_ref: CBLDocument_New_s(as_slice(id))} }
    }

    /** Deletes a document from the database. (Deletions are replicated, unlike purges.) */
    pub fn delete(self) -> Result<()> {
        todo!()
    }

    /** Purges a document. This removes all traces of the document from the database.
        Purges are _not_ replicated. If the document is changed on a server, it will be re-created
        when pulled. */
    pub fn purge(self) -> Result<()> {
        todo!()
    }

    /** Returns the document's ID. */
    pub fn id(&self) -> String {
        unsafe { to_string(CBLDocument_ID(self._ref)) }
    }

    /** Returns a document's revision ID, which is a short opaque string that's guaranteed to be
        unique to every change made to the document.
        If the document doesn't exist yet, this method returns None. */
    pub fn revision_id(&self) -> Option<String> {
        unsafe {
            let revid = CBLDocument_RevisionID(self._ref);
            return if revid.is_null() {None} else {Some(to_string(revid))}
        }
    }

    /** Returns a document's current sequence in the local database.
        This number increases every time the document is saved, and a more recently saved document
        will have a greater sequence number than one saved earlier, so sequences may be used as an
        abstract 'clock' to tell relative modification times. */
    pub fn sequence(&self) -> u64 {
        unsafe { CBLDocument_Sequence(self._ref) }
    }

    /** Returns a document's properties as a dictionary.
        This dictionary cannot be mutated; call `mutable_properties` if you want to make
        changes to the document's properties. */
    pub fn properties<'a>(&'a self) -> Dict {
        unsafe { Dict::wrap(CBLDocument_Properties(self._ref), self) }
    }

    /** Returns a document's properties as an mutable dictionary. Any changes made to this
        dictionary will be saved to the database when this Document instance is saved. */
    pub fn mutable_properties(&mut self) -> MutableDict {
        unsafe { MutableDict::adopt(CBLDocument_MutableProperties(self._ref)) }
    }

    /** Replaces a document's properties with the contents of the dictionary.
        The dictionary is retained, not copied, so further changes _will_ affect the document. */
    pub fn set_properties(&mut self, properties: MutableDict) {
        unsafe { CBLDocument_SetProperties(self._ref, properties._ref) }
    }

    /** Returns a document's properties as a JSON string. */
    pub fn properties_as_json(&self) -> String {
        unsafe { to_string(CBLDocument_PropertiesAsJSON(self._ref)) }
    }

    /** Sets a mutable document's properties from a JSON string. */
    pub fn set_properties_as_json(&mut self, json: &str) -> Result<()> {
        unsafe {
            let mut err = CBLError::default();
            let ok = CBLDocument_SetPropertiesAsJSON_s(self._ref, as_slice(json), &mut err);
            return check_failure(ok, &err);
        }
    }
}


impl Drop for Document {
    fn drop(&mut self) {
        unsafe { release(self._ref); }
    }
}


impl Clone for Document {
    fn clone(&self) -> Self {
        unsafe { Document{_ref: retain(self._ref)} }
    }
}
