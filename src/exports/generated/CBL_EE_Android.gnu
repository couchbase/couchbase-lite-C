CBL_C {
	global:
		CBL_Init;
		CBLEncryptionKey_FromPassword;
		CBLEncryptionKey_FromPasswordOld;
		CBLDatabase_ChangeEncryptionKey;
		CBLEndpoint_CreateWithLocalDB;
		kCBLEncryptableType;
		kCBLEncryptableValueProperty;
		CBLEncryptable_CreateWithNull;
		CBLEncryptable_CreateWithBool;
		CBLEncryptable_CreateWithInt;
		CBLEncryptable_CreateWithUInt;
		CBLEncryptable_CreateWithFloat;
		CBLEncryptable_CreateWithDouble;
		CBLEncryptable_CreateWithString;
		CBLEncryptable_CreateWithValue;
		CBLEncryptable_CreateWithArray;
		CBLEncryptable_CreateWithDict;
		CBLEncryptable_Properties;
		CBLEncryptable_Value;
		FLDict_IsEncryptableValue;
		FLDict_GetEncryptableValue;
		FLSlot_SetEncryptableValue;
		CBL_RegisterPredictiveModel;
		CBL_UnregisterPredictiveModel;
		CBLAuth_CreateCertificate;
		CBL_EnableVectorSearch;
		CBLCollection_CreateVectorIndex;
		CBLVectorEncoding_CreateNone;
		CBLVectorEncoding_CreateProductQuantizer;
		CBLVectorEncoding_CreateScalarQuantizer;
		CBLVectorEncoding_Free;
		CBLQueryIndex_BeginUpdate;
		CBLIndexUpdater_Count;
		CBLIndexUpdater_SetVector;
		CBLIndexUpdater_SkipVector;
		CBLIndexUpdater_Finish;
		CBLIndexUpdater_Value;
		CBLCollection_IsIndexTrained;
		CBLKeyPair_GenerateRSAKeyPair;
		CBLKeyPair_PublicKeyFromData;
		CBLListenerAuth_CreatePassword;
		CBLListenerAuth_CreateCertificate;
		CBLListenerAuth_Free;
		CBLURLEndpointListener_Create;
		CBLURLEndpointListener_Config;
		CBLURLEndpointListener_Port;
		CBLURLEndpointListener_Urls;
		CBLURLEndpointListener_Status;
		CBLURLEndpointListener_Start;
		CBLURLEndpointListener_Stop;
		kCBLCertAttrKeyCommonName;
		kCBLCertAttrKeyPseudonym;
		kCBLCertAttrKeyGivenName;
		kCBLCertAttrKeySurname;
		kCBLCertAttrKeyOrganization;
		kCBLCertAttrKeyOrganizationUnit;
		kCBLCertAttrKeyPostalAddress;
		kCBLCertAttrKeyLocality;
		kCBLCertAttrKeyPostalCode;
		kCBLCertAttrKeyStateOrProvince;
		kCBLCertAttrKeyCountry;
		kCBLCertAttrKeyEmailAddress;
		kCBLCertAttrKeyHostname;
		kCBLCertAttrKeyURL;
		kCBLCertAttrKeyIPAddress;
		kCBLCertAttrKeyRegisteredID;
		CBLKeyPair_CreateWithCallbacks;
		CBLKeyPair_CreateWithPrivateKeyData;
		CBLKeyPair_PublicKeyDigest;
		CBLKeyPair_PublicKeyData;
		CBLKeyPair_PrivateKeyData;
		CBLCert_CreateWithData;
		CBLCert_CertNextInChain;
		CBLCert_Data;
		CBLCert_SubjectName;
		CBLCert_SubjectNameComponent;
		CBLCert_getValidTimespan;
		CBLCert_PublicKey;
		CBLTLSIdentity_Certificates;
		CBLTLSIdentity_Expiration;
		CBLTLSIdentity_CreateIdentity;
		CBLTLSIdentity_CreateIdentityWithKeyPair;
		CBLTLSIdentity_DeleteIdentityWithLabel;
		CBLTLSIdentity_IdentityWithCerts;
		CBLTLSIdentity_IdentityWithKeyPairAndCerts;
		CBLTLSIdentity_IdentityWithLabel;
		kCBLDefaultVectorIndexLazy;
		kCBLDefaultVectorIndexDistanceMetric;
		kCBLDefaultVectorIndexMinTrainingSize;
		kCBLDefaultVectorIndexMaxTrainingSize;
		kCBLDefaultVectorIndexNumProbes;
		CBL_Retain;
		CBL_Release;
		CBL_InstanceCount;
		CBL_DumpInstances;
		CBL_Now;
		CBLError_Message;
		CBLListener_Remove;
		kCBLTypeProperty;
		kCBLBlobType;
		kCBLBlobDigestProperty;
		kCBLBlobLengthProperty;
		kCBLBlobContentTypeProperty;
		FLDict_IsBlob;
		FLDict_GetBlob;
		FLSlot_SetBlob;
		CBLBlob_ContentType;
		CBLBlob_Length;
		CBLBlob_Digest;
		CBLBlob_Equals;
		CBLBlob_Properties;
		CBLBlob_Content;
		CBLBlob_OpenContentStream;
		CBLBlob_CreateJSON;
		CBLBlob_CreateWithData;
		CBLBlob_CreateWithStream;
		CBLBlobReader_Read;
		CBLBlobReader_Position;
		CBLBlobReader_Seek;
		CBLBlobReader_Close;
		CBLBlobWriter_Create;
		CBLBlobWriter_Close;
		CBLBlobWriter_Write;
		CBLDatabase_GetBlob;
		CBLDatabase_SaveBlob;
		CBLDatabaseConfiguration_Default;
		CBL_DatabaseExists;
		CBL_CopyDatabase;
		CBL_DeleteDatabase;
		CBLDatabase_Delete;
		CBLDatabase_Open;
		CBLDatabase_Close;
		CBLDatabase_Name;
		CBLDatabase_Path;
		CBLDatabase_Config;
		CBLDatabase_Count;
		CBLDatabase_Delete;
		CBLDatabase_BeginTransaction;
		CBLDatabase_EndTransaction;
		CBLDatabase_PerformMaintenance;
		CBLDatabase_AddChangeListener;
		CBLDatabase_AddDocumentChangeListener;
		CBLDatabase_BufferNotifications;
		CBLDatabase_SendNotifications;
		CBLDatabase_CreateValueIndex;
		CBLDatabase_CreateFullTextIndex;
		CBLDatabase_DeleteIndex;
		CBLDatabase_GetIndexNames;
		CBLDatabase_Collection;
		CBLDatabase_CollectionNames;
		CBLDatabase_CreateCollection;
		CBLDatabase_DeleteCollection;
		CBLDatabase_DefaultCollection;
		CBLDatabase_DefaultScope;
		CBLDatabase_Scope;
		CBLDatabase_ScopeNames;
		kCBLDefaultCollectionName;
		CBLCollection_Scope;
		CBLCollection_Name;
		CBLCollection_FullName;
		CBLCollection_Database;
		CBLCollection_Count;
		CBLCollection_GetDocument;
		CBLCollection_SaveDocument;
		CBLCollection_SaveDocumentWithConcurrencyControl;
		CBLCollection_SaveDocumentWithConflictHandler;
		CBLCollection_DeleteDocument;
		CBLCollection_DeleteDocumentWithConcurrencyControl;
		CBLCollection_PurgeDocument;
		CBLCollection_PurgeDocumentByID;
		CBLCollection_GetDocumentExpiration;
		CBLCollection_SetDocumentExpiration;
		CBLCollection_GetMutableDocument;
		CBLCollection_AddChangeListener;
		CBLCollection_AddDocumentChangeListener;
		CBLCollection_CreateArrayIndex;
		CBLCollection_CreateValueIndex;
		CBLCollection_CreateFullTextIndex;
		CBLCollection_DeleteIndex;
		CBLCollection_GetIndex;
		CBLCollection_GetIndexNames;
		kCBLDefaultScopeName;
		CBLScope_Name;
		CBLScope_Database;
		CBLScope_Collection;
		CBLScope_CollectionNames;
		CBLDocument_ID;
		CBLDocument_RevisionID;
		CBLDocument_Sequence;
		CBLDocument_Collection;
		CBLDocument_Create;
		CBLDocument_CreateWithID;
		CBLDocument_MutableCopy;
		CBLDocument_Properties;
		CBLDocument_MutableProperties;
		CBLDocument_SetProperties;
		CBLDocument_CreateJSON;
		CBLDocument_SetJSON;
		CBLDatabase_GetDocument;
		CBLDatabase_GetMutableDocument;
		CBLDatabase_SaveDocument;
		CBLDatabase_SaveDocumentWithConcurrencyControl;
		CBLDatabase_SaveDocumentWithConflictHandler;
		CBLDatabase_DeleteDocument;
		CBLDatabase_DeleteDocumentWithConcurrencyControl;
		CBLDatabase_PurgeDocument;
		CBLDatabase_PurgeDocumentByID;
		CBLDatabase_GetDocumentExpiration;
		CBLDatabase_SetDocumentExpiration;
		CBL_Log;
		CBL_LogMessage;
		CBLLog_Callback;
		CBLLog_SetCallback;
		CBLLog_CallbackLevel;
		CBLLog_SetCallbackLevel;
		CBLLog_ConsoleLevel;
		CBLLog_SetConsoleLevel;
		CBLLog_FileConfig;
		CBLLog_SetFileConfig;
		CBLLogSinks_SetConsole;
		CBLLogSinks_Console;
		CBLLogSinks_SetCustom;
		CBLLogSinks_CustomSink;
		CBLLogSinks_SetFile;
		CBLLogSinks_File;
		CBLDatabase_CreateQuery;
		CBLQuery_Parameters;
		CBLQuery_SetParameters;
		CBLQuery_Execute;
		CBLQuery_Explain;
		CBLQuery_ColumnCount;
		CBLQuery_ColumnName;
		CBLQuery_AddChangeListener;
		CBLQuery_CopyCurrentResults;
		CBLResultSet_Next;
		CBLResultSet_ValueAtIndex;
		CBLResultSet_ValueForKey;
		CBLResultSet_ResultArray;
		CBLResultSet_ResultDict;
		CBLResultSet_GetQuery;
		CBLQueryIndex_Collection;
		CBLQueryIndex_Name;
		kCBLAuthDefaultCookieName;
		CBLEndpoint_CreateWithURL;
		CBLEndpoint_Free;
		CBLAuth_CreatePassword;
		CBLAuth_CreateSession;
		CBLAuth_Free;
		CBLReplicator_Create;
		CBLReplicator_Config;
		CBLReplicator_Start;
		CBLReplicator_Stop;
		CBLReplicator_SetHostReachable;
		CBLReplicator_SetSuspended;
		CBLReplicator_Status;
		CBLReplicator_PendingDocumentIDs;
		CBLReplicator_PendingDocumentIDs2;
		CBLReplicator_IsDocumentPending;
		CBLReplicator_IsDocumentPending2;
		CBLReplicator_AddChangeListener;
		CBLReplicator_AddDocumentReplicationListener;
		CBLReplicator_UserAgent;
		CBLDefaultConflictResolver;
		CBLCollection_DeleteDocumentByID;
		CBLCollection_GetIndexesInfo;
		CBLCollection_LastSequence;
		CBLDatabase_DeleteDocumentByID;
		CBLDatabase_LastSequence;
		CBLDatabase_PublicUUID;
		CBLDocument_CanonicalRevisionID;
		CBLDocument_Generation;
		CBLDocument_GetRevisionHistory;
		CBLError_GetCaptureBacktraces;
		CBLError_SetCaptureBacktraces;
		CBLQuery_SetListenerCallbackDelay;
		CBLLog_BeginExpectingExceptions;
		CBLLog_EndExpectingExceptions;
		CBLLog_Reset;
		CBLLog_LogWithC4Log;
		CBL_DeleteDirectoryRecursive;
		kCBLDefaultDatabaseFullSync;
		kCBLDefaultDatabaseMmapDisabled;
		kCBLDefaultLogFileUsePlaintext;
		kCBLDefaultLogFileUsePlainText;
		kCBLDefaultLogFileMaxSize;
		kCBLDefaultLogFileMaxRotateCount;
		kCBLDefaultFileLogSinkUsePlaintext;
		kCBLDefaultFileLogSinkMaxSize;
		kCBLDefaultFileLogSinkMaxKeptFiles;
		kCBLDefaultFullTextIndexIgnoreAccents;
		kCBLDefaultReplicatorType;
		kCBLDefaultReplicatorContinuous;
		kCBLDefaultReplicatorHeartbeat;
		kCBLDefaultReplicatorMaxAttemptsSingleShot;
		kCBLDefaultReplicatorMaxAttemptsContinuous;
		kCBLDefaultReplicatorMaxAttemptWaitTime;
		kCBLDefaultReplicatorMaxAttemptsWaitTime;
		kCBLDefaultReplicatorDisableAutoPurge;
		kCBLDefaultReplicatorAcceptParentCookies;
		kFLNullValue;
		kFLUndefinedValue;
		kFLEmptyArray;
		kFLEmptyDict;
		FLSlice_Equal;
		FLSlice_Compare;
		FLSlice_Hash;
		FLSlice_Copy;
		FLSlice_ToCString;
		FLSliceResult_New;
		_FLBuf_Retain;
		_FLBuf_Release;
		FLDoc_FromResultData;
		FLDoc_FromJSON;
		FLDoc_Release;
		FLDoc_Retain;
		FLDoc_GetAllocedData;
		FLDoc_GetData;
		FLDoc_GetRoot;
		FLDoc_GetSharedKeys;
		FLDoc_SetAssociated;
		FLDoc_GetAssociated;
		FLValue_GetType;
		FLValue_IsInteger;
		FLValue_IsUnsigned;
		FLValue_IsDouble;
		FLValue_IsEqual;
		FLValue_IsMutable;
		FLValue_AsBool;
		FLValue_AsData;
		FLValue_AsInt;
		FLValue_AsUnsigned;
		FLValue_AsFloat;
		FLValue_AsDouble;
		FLValue_AsString;
		FLValue_AsArray;
		FLValue_AsDict;
		FLValue_AsTimestamp;
		FLValue_ToString;
		FLValue_ToJSON;
		FLValue_ToJSONX;
		FLValue_ToJSON5;
		FLValue_FindDoc;
		FLValue_Retain;
		FLValue_Release;
		FLValue_NewString;
		FLValue_NewData;
		FLArray_Count;
		FLArray_IsEmpty;
		FLArray_Get;
		FLArray_AsMutable;
		FLArray_MutableCopy;
		FLArrayIterator_Begin;
		FLArrayIterator_GetCount;
		FLArrayIterator_GetValue;
		FLArrayIterator_GetValueAt;
		FLArrayIterator_Next;
		FLMutableArray_New;
		FLMutableArray_NewFromJSON;
		FLMutableArray_GetSource;
		FLMutableArray_IsChanged;
		FLMutableArray_SetChanged;
		FLMutableArray_Append;
		FLMutableArray_Set;
		FLMutableArray_Insert;
		FLMutableArray_Remove;
		FLMutableArray_Resize;
		FLMutableArray_GetMutableArray;
		FLMutableArray_GetMutableDict;
		FLDict_Count;
		FLDict_IsEmpty;
		FLDict_Get;
		FLDict_GetWithKey;
		FLDict_AsMutable;
		FLDict_MutableCopy;
		FLDictIterator_Begin;
		FLDictIterator_GetCount;
		FLDictIterator_GetKey;
		FLDictIterator_GetKeyString;
		FLDictIterator_GetValue;
		FLDictIterator_Next;
		FLDictIterator_End;
		FLDictKey_Init;
		FLDictKey_GetString;
		FLEncoder_New;
		FLEncoder_NewWithOptions;
		FLEncoder_NewWritingToFile;
		FLEncoder_Free;
		FLEncoder_SetSharedKeys;
		FLEncoder_SetExtraInfo;
		FLEncoder_GetExtraInfo;
		FLEncoder_WriteRaw;
		FLEncoder_WriteNull;
		FLEncoder_WriteUndefined;
		FLEncoder_WriteBool;
		FLEncoder_WriteInt;
		FLEncoder_WriteUInt;
		FLEncoder_WriteFloat;
		FLEncoder_WriteDouble;
		FLEncoder_WriteString;
		FLEncoder_WriteDateString;
		FLEncoder_WriteData;
		FLEncoder_WriteValue;
		FLEncoder_ConvertJSON;
		FLEncoder_BeginArray;
		FLEncoder_EndArray;
		FLEncoder_BeginDict;
		FLEncoder_WriteKey;
		FLEncoder_WriteKeyValue;
		FLEncoder_EndDict;
		FLEncoder_BytesWritten;
		FLEncoder_FinishDoc;
		FLEncoder_Finish;
		FLEncoder_Reset;
		FLEncoder_GetError;
		FLEncoder_GetErrorMessage;
		FLMutableDict_New;
		FLMutableDict_NewFromJSON;
		FLMutableDict_GetSource;
		FLMutableDict_IsChanged;
		FLMutableDict_SetChanged;
		FLMutableDict_Set;
		FLMutableDict_Remove;
		FLMutableDict_RemoveAll;
		FLMutableDict_GetMutableArray;
		FLMutableDict_GetMutableDict;
		FLSlot_SetNull;
		FLSlot_SetBool;
		FLSlot_SetInt;
		FLSlot_SetUInt;
		FLSlot_SetFloat;
		FLSlot_SetDouble;
		FLSlot_SetString;
		FLSlot_SetData;
		FLSlot_SetValue;
		FLKeyPath_New;
		FLKeyPath_Free;
		FLKeyPath_Eval;
		FLKeyPath_EvalOnce;
		FLKeyPath_ToString;
		FLKeyPath_Equals;
		FLKeyPath_GetElement;
		FLDeepIterator_New;
		FLDeepIterator_Free;
		FLDeepIterator_GetValue;
		FLDeepIterator_GetKey;
		FLDeepIterator_GetParent;
		FLDeepIterator_GetIndex;
		FLDeepIterator_GetDepth;
		FLDeepIterator_SkipChildren;
		FLDeepIterator_Next;
		FLDeepIterator_GetPath;
		FLDeepIterator_GetPathString;
		FLDeepIterator_GetJSONPointer;
		FLTimestamp_Now;
		FLTimestamp_FromString;
		FLTimestamp_ToString;
		FL_WipeMemory;
		FLApplyJSONDelta;
		FLCreateJSONDelta;
		FLData_ConvertJSON;
		FLData_Dump;
		FLDump;
		FLDumpData;
		FLEncodeApplyingJSONDelta;
		FLEncodeJSONDelta;
		FLEncoder_Amend;
		FLEncoder_FinishItem;
		FLEncoder_GetBase;
		FLEncoder_GetNextWritePos;
		FLEncoder_LastValueWritten;
		FLEncoder_Snip;
		FLEncoder_SuppressTrailer;
		FLEncoder_WriteValueAgain;
		FLJSON5_ToJSON;
		FLSharedKeyScope_Free;
		FLSharedKeyScope_WithRange;
		FLSharedKeys_Count;
		FLSharedKeys_Decode;
		FLSharedKeys_Encode;
		FLSharedKeys_GetStateData;
		FLSharedKeys_LoadState;
		FLSharedKeys_LoadStateData;
		FLSharedKeys_New;
		FLSharedKeys_NewWithRead;
		FLSharedKeys_Release;
		FLSharedKeys_Retain;
		FLSharedKeys_RevertToCount;
		FLSharedKeys_WriteState;
		FLValue_FromData;
	local:
		*;
};
