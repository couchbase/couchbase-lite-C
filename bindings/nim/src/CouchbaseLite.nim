# CouchbaseLite.nim

import CouchbaseLite/database
import CouchbaseLite/document
import CouchbaseLite/errors
import CouchbaseLite/query


type
    Error* = errors.Error
    Database* = database.Database
    Document* = document.Document
    MutableDocument* = document.MutableDocument
    Query = query.Query
