// mod document

use super::*;
use super::base::*;
use super::c_api::*;
// use super::database::*;


//////// DATABASE'S DOCUMENT API:


pub type SaveConflictHandler = fn(&mut Document, &Document) -> bool;

pub struct Timestamp(i64);

pub type ChangeListener = fn(&Database, &str);


impl Database {
    pub fn get_document(&self, id: &str) -> Document {
        unsafe {
            Document{_ref: CBLDatabase_GetMutableDocument_s(self._ref, as_slice(id))}
        }
    }
    
    pub fn save_document(&self, doc: 
                         &mut Document, 
                         concurrency: ConcurrencyControl)
                         -> Result<Document, Error> 
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
                                   -> Result<Document, Error> 
    {
        todo!()
    }
    
    pub fn purge_document_by_id(&self, id: &str) -> Result<(), Error> {
        unsafe {
            return check_bool(|error| CBLDatabase_PurgeDocumentByID_s(self._ref, as_slice(id), error));
        }
    }

    pub fn document_expiration(&self, doc_id: &str) -> Result<Option<Timestamp>, Error> {
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

    pub fn set_document_expiration(&self, doc_id: &str, when: Option<Timestamp>) -> Result<(), Error> {
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
        unsafe {
            return Document{_ref: CBLDocument_New_s(as_slice(id))};
        }
    }
    
    pub fn delete(self) -> Result<(), Error> {
        todo!()
    }
    
    pub fn purge(self) -> Result<(), Error> {
        todo!()
    }
    
    pub fn id(&self) -> String {
        unsafe {
            return to_string(CBLDocument_ID(self._ref));
        }
    }
    
    pub fn revision_id(&self) -> String {
        unsafe {
            return to_string(CBLDocument_RevisionID(self._ref));
        }
    }
    
    pub fn sequence(&self) -> u64 {
        unsafe {
            return CBLDocument_Sequence(self._ref);
        }
    }
    
    pub fn properties_as_json(&self) -> String {
        unsafe {
            return to_string(CBLDocument_PropertiesAsJSON(self._ref))
        }
    }
    
    pub fn set_properties_as_json(&self, json: &str) -> Result<(), Error> {
        unsafe {
            let mut err = CBLError::default();
            let ok = CBLDocument_SetPropertiesAsJSON_s(self._ref, as_slice(json), &mut err);
            return check_failure(ok, &err);
        }
    }
}


impl Drop for Document {
    fn drop(&mut self) {
        unsafe {
            release(self._ref);
        }
    }
}


impl Clone for Document {
    fn clone(&self) -> Self {
        unsafe {
            return Document{_ref: retain(self._ref)}
        }
    }
}
