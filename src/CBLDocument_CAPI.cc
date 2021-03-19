//
// CBLDocument_CAPI.cc
//
// Copyright (C) 2020 Jens Alfke. All Rights Reserved.
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

#include "CBLDocument_Internal.hh"

using namespace fleece;


CBLDocument* CBLDocument_Create() CBLAPI {
    return CBLDocument_CreateWithID(nullslice);
}

CBLDocument* CBLDocument_CreateWithID(FLString docID) CBLAPI {
    return make_retained<CBLDocument>(docID, true).detach();
}

CBLDocument* CBLDocument_MutableCopy(const CBLDocument* doc) CBLAPI {
    return make_retained<CBLDocument>(doc).detach();
}

FLSlice CBLDocument_ID(const CBLDocument* doc) CBLAPI              {return doc->docID();}
FLSlice CBLDocument_RevisionID(const CBLDocument* doc) CBLAPI      {return doc->revisionID();}
uint64_t CBLDocument_Sequence(const CBLDocument* doc) CBLAPI           {return doc->sequence();}
FLDict CBLDocument_Properties(const CBLDocument* doc) CBLAPI           {return doc->properties();}

FLSliceResult CBLDocument_CanonicalRevisionID(const CBLDocument* doc) CBLAPI {
    return FLSliceResult(doc->canonicalRevisionID());
}

FLMutableDict CBLDocument_MutableProperties(CBLDocument* doc) CBLAPI {
    return doc->mutableProperties();
}

FLSliceResult CBLDocument_CreateJSON(const CBLDocument* doc) CBLAPI {return FLSliceResult(doc->propertiesAsJSON());}

void CBLDocument_SetProperties(CBLDocument* doc, FLMutableDict properties _cbl_nonnull) CBLAPI {
    doc->setProperties(properties);
}

bool CBLDocument_SetJSON(CBLDocument* doc, FLSlice json, CBLError* outError) CBLAPI {
    doc->setPropertiesAsJSON(json);//FIXME: Catch
    return true;
}
