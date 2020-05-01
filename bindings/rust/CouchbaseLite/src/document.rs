// mod document

use super::*;
use super::base::*;
use super::error::*;
use super::c_api::*;
use super::fleece::*;
use super::fleece_mutable::*;


//////// DATABASE'S DOCUMENT API:


pub type SaveConflictHandler = fn(&mut Document, &Document) -> bool;

pub type ChangeListener = fn(&Database, &str);


impl Database {
    pub fn get_document(&self, id: &str) -> Result<Document> {
        unsafe {
            // we always get a mutable CBLDocument,
            // since Rust doesn't let us have MutableDocument subclass.
            let doc = CBLDatabase_GetMutableDocument_s(self._ref, as_slice(id));
            if doc.is_null() {
                return Err(Error::CouchbaseLite(CouchbaseLiteError::NotFound));
            }
            return Ok(Document{_ref: doc});
        }
    }

    pub fn save_document(&self,
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

    pub fn save_document_resolving(&self,
                                   _doc: &mut Document,
                                   _conflict_handler: SaveConflictHandler)
                                   -> Result<Document>
    {
        todo!()
    }

    pub fn purge_document_by_id(&self, id: &str) -> Result<()> {
        unsafe {
            return check_bool(|error| CBLDatabase_PurgeDocumentByID_s(self._ref, as_slice(id), error));
        }
    }

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

    pub fn set_document_expiration(&self, doc_id: &str, when: Option<Timestamp>) -> Result<()> {
        let exp :i64 = match when {
            Some(Timestamp(n)) => n,
            _ => 0,
        };
        unsafe {
            return check_bool(|error| CBLDatabase_SetDocumentExpiration_s(self._ref, as_slice(doc_id), exp, error));
        }
    }

    pub fn add_document_change_listener(&self, _doc_id: &str, _listener: ChangeListener) -> ListenerToken {
        todo!()
    }

}


//////// DOCUMENT API:


impl Document {

    pub fn new(id: &str) -> Self {
        unsafe { Document{_ref: CBLDocument_New_s(as_slice(id))} }
    }

    pub fn delete(self) -> Result<()> {
        todo!()
    }

    pub fn purge(self) -> Result<()> {
        todo!()
    }

    pub fn id(&self) -> String {
        unsafe { to_string(CBLDocument_ID(self._ref)) }
    }

    pub fn revision_id(&self) -> String {
        unsafe { to_string(CBLDocument_RevisionID(self._ref)) }
    }

    pub fn sequence(&self) -> u64 {
        unsafe { CBLDocument_Sequence(self._ref) }
    }

    pub fn properties<'a>(&'a self) -> Dict {
        unsafe { Dict::wrap(CBLDocument_Properties(self._ref), self) }
    }

    pub fn mutable_properties(&mut self) -> MutableDict {
        unsafe { MutableDict::adopt(CBLDocument_MutableProperties(self._ref)) }
    }

    pub fn properties_as_json(&self) -> String {
        unsafe { to_string(CBLDocument_PropertiesAsJSON(self._ref)) }
    }

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
