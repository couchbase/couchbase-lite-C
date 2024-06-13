//
// VectorSearchTest.hh
//
// Copyright © 2024 Couchbase. All rights reserved.
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

#pragma once
#include "CBLTest.hh"
#include "CBLPrivate.h"
#include "fleece/Fleece.hh"
#include "fleece/Mutable.hh"
#include <optional>
#include <unordered_map>
#include <sstream>
#include <vector>

using namespace fleece;
using namespace std;

#ifdef COUCHBASE_ENTERPRISE

#if defined(__APPLE__) || defined(__linux__)
#define VECTOR_SEARCH_TEST_ENABLED 1
#endif

#if defined(WIN32)
    #if defined(__x86_64__) || defined(__x86_64) || defined(__amd64__) || defined(_M_X64)
        #define VECTOR_SEARCH_TEST_ENABLED 1
    #endif
#endif

class VectorSearchTest : public CBLTest {
public:
    constexpr static const fleece::slice kWordsDatabaseName = "words_db";
    
    constexpr static const fleece::slice kWordsCollectionName = "words";
    
    constexpr static const fleece::slice kExtWordsCollectionName = "extwords";
    
    constexpr static const fleece::slice kWordsIndexName = "words_index";
    
    constexpr static const fleece::slice kWordsPredictiveModelName = "WordEmbedding";
    
    constexpr static const fleece::slice kDinnerVector = "[0.03193166106939316, 0.032055653631687164, 0.07188114523887634, -0.09893740713596344, -0.07693558186292648, 0.07570040225982666, 0.42786234617233276, -0.11442682892084122, -0.7863243818283081, -0.47983086109161377, -0.10168658196926117, 0.10985997319221497, -0.15261511504650116, -0.08458329737186432, -0.16363860666751862, -0.20225222408771515, -0.2593214809894562, -0.032738097012043, -0.16649988293647766, -0.059701453894376755, 0.17472036182880402, -0.007310086861252785, -0.13918264210224152, -0.07260780036449432, -0.02461239881813526, -0.04195880889892578, -0.15714778006076813, 0.48038315773010254, 0.7536261677742004, 0.41809454560279846, -0.17144775390625, 0.18296195566654205, -0.10611499845981598, 0.11669538915157318, 0.07423929125070572, -0.3105475902557373, -0.045081984251737595, -0.18190748989582062, 0.22430984675884247, 0.05735112354159355, -0.017394868656992912, -0.148889422416687, -0.20618586242198944, -0.1446581482887268, 0.061972495168447495, 0.07787969708442688, 0.14225411415100098, 0.20560632646083832, 0.1786964386701584, -0.380594402551651, -0.18301603198051453, -0.19542981684207916, 0.3879885971546173, -0.2219538390636444, 0.11549852043390274, -0.0021717497147619724, -0.10556972026824951, 0.030264658853411674, 0.16252967715263367, 0.06010117009282112, -0.045007310807704926, 0.02435707487165928, 0.12623260915279388, -0.12688252329826355, -0.3306281864643097, 0.06452160328626633, 0.0707000121474266, -0.04959108680486679, -0.2567063570022583, -0.01878536120057106, -0.10857286304235458, -0.01754194125533104, -0.0713721290230751, 0.05946013703942299, -0.1821729987859726, -0.07293688505887985, -0.2778160572052002, 0.17880073189735413, -0.04669278487563133, 0.05351974070072174, -0.23292849957942963, 0.05746332183480263, 0.15462779998779297, -0.04772235080599785, -0.003306782804429531, 0.058290787041187286, 0.05908169597387314, 0.00504430802538991, -0.1262340396642685, 0.11612161248922348, 0.25303348898887634, 0.18580256402492523, 0.09704313427209854, -0.06087183952331543, 0.19697663187980652, -0.27528849244117737, -0.0837797075510025, -0.09988483041524887, -0.20565757155418396, 0.020984146744012833, 0.031014855951070786, 0.03521743416786194, -0.05171370506286621, 0.009112107567489147, -0.19296088814735413, -0.19363830983638763, 0.1591167151927948, -0.02629968523979187, -0.1695055067539215, -0.35807400941848755, -0.1935291737318039, -0.17090126872062683, -0.35123637318611145, -0.20035606622695923, -0.03487539291381836, 0.2650701701641083, -0.1588021069765091, 0.32268261909484863, -0.024521857500076294, -0.11985184997320175, 0.14826008677482605, 0.194917231798172, 0.07971998304128647, 0.07594677060842514, 0.007186363451182842, -0.14641280472278595, 0.053229596465826035, 0.0619836151599884, 0.003207010915502906, -0.12729716300964355, 0.13496214151382446, 0.107656329870224, -0.16516226530075073, -0.033881571143865585, -0.11175122112035751, -0.005806141998618841, -0.4765360355377197, 0.11495379358530045, 0.1472187340259552, 0.3781401813030243, 0.10045770555734634, -0.1352398842573166, -0.17544329166412354, -0.13191302120685577, -0.10440415143966675, 0.34598618745803833, 0.09728766977787018, -0.25583627820014954, 0.035236816853284836, 0.16205145418643951, -0.06128586828708649, 0.13735555112361908, 0.11582338809967041, -0.10182418674230576, 0.1370954066514969, 0.15048766136169434, 0.06671152263879776, -0.1884871870279312, -0.11004580557346344, 0.24694739282131195, -0.008159132674336433, -0.11668405681848526, -0.01214478351175785, 0.10379738360643387, -0.1626262664794922, 0.09377897530794144, 0.11594484746456146, -0.19621512293815613, 0.26271334290504456, 0.04888357222080231, -0.10103251039981842, 0.33250945806503296, 0.13565145432949066, -0.23888370394706726, -0.13335271179676056, -0.0076894499361515045, 0.18256276845932007, 0.3276212215423584, -0.06567271053791046, -0.1853761374950409, 0.08945729583501816, 0.13876311480998993, 0.09976287186145782, 0.07869105041027069, -0.1346970647573471, 0.29857659339904785, 0.1329529583454132, 0.11350086331367493, 0.09112624824047089, -0.12515446543693542, -0.07917925715446472, 0.2881546914577484, -1.4532661225530319e-05, -0.07712751626968384, 0.21063975989818573, 0.10858846455812454, -0.009552721865475178, 0.1629313975572586, -0.39703384041786194, 0.1904662847518921, 0.18924959003925323, -0.09611514210700989, 0.001136621693149209, -0.1293390840291977, -0.019481558352708817, 0.09661063551902771, -0.17659670114517212, 0.11671938002109528, 0.15038564801216125, -0.020016824826598167, -0.20642194151878357, 0.09050136059522629, -0.1768183410167694, -0.2891409397125244, 0.04596589505672455, -0.004407480824738741, 0.15323616564273834, 0.16503025591373444, 0.17370983958244324, 0.02883041836321354, 0.1463884711265564, 0.14786243438720703, -0.026439940556883812, -0.03113352134823799, 0.10978181660175323, 0.008928884752094746, 0.24813824892044067, -0.06918247044086456, 0.06958142668008804, 0.17475970089435577, 0.04911438003182411, 0.17614248394966125, 0.19236832857131958, -0.1425514668226242, -0.056531358510255814, -0.03680772706866264, -0.028677923604846, -0.11353116482496262, 0.012293893843889236, -0.05192646384239197, 0.20331953465938568, 0.09290937334299088, 0.15373043715953827, 0.21684466302394867, 0.40546831488609314, -0.23753701150417328, 0.27929359674453735, -0.07277711480855942, 0.046813879162073135, 0.06883064657449722, -0.1033223420381546, 0.15769273042678833, 0.21685580909252167, -0.00971329677850008, 0.17375953495502472, 0.027193285524845123, -0.09943609684705734, 0.05770351365208626, 0.0868956446647644, -0.02671697922050953, -0.02979189157485962, 0.024517420679330826, -0.03931192681193352, -0.35641804337501526, -0.10590721666812897, -0.2118944674730301, -0.22070199251174927, 0.0941486731171608, 0.19881175458431244, 0.1815279871225357, -0.1256905049085617, -0.0683583989739418, 0.19080783426761627, -0.009482398629188538, -0.04374842345714569, 0.08184348791837692, 0.20070189237594604, 0.039221834391355515, -0.12251003831624985, -0.04325549304485321, 0.03840530663728714, -0.19840988516807556, -0.13591833412647247, 0.03073180839419365, 0.1059495136141777, -0.10656466335058212, 0.048937033861875534, -0.1362423598766327, -0.04138947278261185, 0.10234509408473969, 0.09793911874294281, 0.1391254961490631, -0.0906999260187149, 0.146945983171463, 0.14941848814487457, 0.23930180072784424, 0.36049938201904297, 0.0239607822149992, 0.08884347230195999, 0.061145078390836716]";
    
    constexpr static const fleece::slice kLunchVectorBase64 = "4OYevd8eyDxJGj69HCKOvoCJYTzQCJs9xhDbPp1Y6r2OTEm/ZKz1vtRbwL1Ik8I9+RQFPpyGBD69OEI9ul+evZD71L2nI4y8uTINPnVN+702+c4+8zToPEoGKj6xEqi93vPFvQDdK71Z6yC+yPT1PqXtQD99ENY+xnh+PpBEOD6aIUi+eVezvg24fj0YAJ++46c4vfVFOr57sWU+A+lqPdFq3T1ZJg6+Ok6yvs1/Cr5blju+ITa9vAFxlj1+8h4+c7UePe6fUL6OaDu+wR5IvnGmxj7eR2O+fYrsPf8kw73IOfq8YOJtvAxBMj0g99O8+toTPr0v8r2I4mK+Yxd1PTGxhbzu3aS9zeJEPqKy0Ty2cOy9YqgQPL7af703wFK9965hvOM0pz2VuAc+RIyTu4nxi73pigA9RCjpvVTOFj6zPIC+HTsrvrcpTz4vXzS6ArPxvM+VNL3hJgk+9pM7vtP1jL51sao8q4oJPonfBDxkAiC9XvJUPWiWTD1Kwbe+4KHOvUQmjjypsrS6i4MJPjRnWz0g8E4+Ad3IvVsKMT5O7Qw9X4tFPbpriT1TYme8uw5uvqBar72DLEa+vgAvvkHVs74kKk2+gNkOvZkV57zBfcC+/WM7PrKQQb4+adC9ftEXPmKYRz47RKM9+4mbPZZ76zs4LZq+0gIXPgNoxL26tT09rGFdvPdQqDwi/Y8939OLvYVTQr7J8hK+ljyeveMZsL5xeGi8sppcPfezjT11QuU9cvRpPSoby7yIZ3U9FUPXPd/y1z2xBhu9CfRyvbjXR72xLjk+9rkLvrdWJD2u+Iy9TtM/vlc0Ez4E1ju9XtcrPP+4Cr5ymDu+DfEAPswpP770tKm+3u07vsXxXb19zcC8MQ/APX507T2e7Ei+XYKGPiQ6SD0MORK+Lk4NP1zuHTzrAKW+Eu2WvSGPRj6fL7g9IdSgPkNyojxUSPi95uGqvJugrj0Bqbc9x1eVPk8qh74NlYk+07gZPVqt271XR2E+bMxmOyw0JD1Lg2Y+h+GDvRpuj70YCss890HtPdFwMz7oo7I+RpgXv4/lkz54b+Y8l6yOPdbWYj3H+4G+Q4wXvsXhyD0ayts9XIXBPndXLj34Q1I+0zfQu5pblj66UKa9dSWqvRl1xb04RQK9HsA6PrH2rD2r8wC+XQQPPlSirDwC3zU+K7Z4vUfVML4xHyY92TguPigvMj2emD8+q3AXPsSHWz4Cq5+9P/o7PveDcD095w++4fc9vvE81j17lt09AY7CvHD/Nz7FdCe+t7z4PDJPZD4Qsce9mdwZPtvzDj60sz6+ETvUPTLZ970Gauu83dW7PZZPCj51tCc+yMYtPYrmSjyUcpE+GCDgPf1tGr7aODg+ESYGPmu52T070vi9kW0vvaiwWj6JgQ6+hoehPVygk77JeOg8yCI+PtSnpD2I6w0+z3IFPRUoLD7boxM+XJYbviPzNrxBSBs+XO+WPpkuH74N9+m9tds9PiCinT6BaZ2+tGIfvhZSTj2ZP2k+cld+PHx1Kj4uOfK9bsXHPRx8Bz5OlMg96nYOPuLAub0CeRY+KQEZPogLdT5gk7g+Z0nEPJHztT1Dc3o9";
    
    CBLDatabase *wordDB {nullptr};
    
    CBLDatabase *wordEmbeddingDB {nullptr};
    
    CBLCollection *wordsCollection {nullptr};
    
    CBLCollection *extwordsCollection {nullptr};
    
    static vector<string> sVectorSearchTestLogs;
    
    VectorSearchTest() {
        // Eanble vector search and reinit test databases:
        SetVectorSearchEnabled(true);
        initTestDatabases(false);
        
        auto config = databaseConfig();
        
        CBLError error { };
        if (!CBL_DeleteDatabase(kWordsDatabaseName, config.directory, &error) && error.code != 0) {
            FAIL("Can't delete words database: " << error.domain << "/" << error.code);
        }
        
        auto wordsDBPath = GetAssetFilePath("words_db.cblite2");
        if (!CBL_CopyDatabase(slice(wordsDBPath), kWordsDatabaseName, &config, &error)) {
            FAIL("Can't copy words database: " << error.domain << "/" << error.code);
        }
        
        wordDB = CBLDatabase_Open(kWordsDatabaseName, &config, &error);
        REQUIRE(wordDB);
        
        wordEmbeddingDB = CBLDatabase_Open(kWordsDatabaseName, &config, &error);
        REQUIRE(wordEmbeddingDB);
        
        wordsCollection = CBLDatabase_Collection(wordDB, kWordsCollectionName, kFLSliceNull, &error);
        REQUIRE(wordsCollection);
        
        extwordsCollection = CBLDatabase_Collection(wordDB, kExtWordsCollectionName, kFLSliceNull, &error);
        REQUIRE(extwordsCollection);
        
        registerWordEmbeddingModel();
        
        sVectorSearchTestLogs.clear();
        CBLLog_SetCallback([](CBLLogDomain domain, CBLLogLevel level, FLString msg) {
            sVectorSearchTestLogs.push_back(string(msg));
        });
        CBLLog_SetCallbackLevel(kCBLLogInfo);
    }
    
    ~VectorSearchTest() {
        CBLCollection_Release(wordsCollection);
        CBLCollection_Release(extwordsCollection);
        
        CBLError error {};
        
        if (wordDB){
            if (!CBLDatabase_Close(wordDB, &error))
                WARN("Failed to close words database: " << error.domain << "/" << error.code);
            CBLDatabase_Release(wordDB);
        }
        
        if (wordEmbeddingDB)  {
            if (!CBLDatabase_Close(wordEmbeddingDB, &error))
                WARN("Failed to close words database: " << error.domain << "/" << error.code);
            CBLDatabase_Release(wordEmbeddingDB);
        }
        
        unregisterWordEmbeddingModel();
        
        // Reset log callback:
        CBLLog_SetCallback(nullptr);
        CBLLog_SetCallbackLevel(kCBLLogNone);
        sVectorSearchTestLogs.clear();
        
        // Disable vector search:
        SetVectorSearchEnabled(false);
    }
    
    FLMutableArray vectorArrayForWord(FLString word, FLString collection) {
        stringstream ss;
        ss << "SELECT vector FROM " << slice(collection).asString() << " WHERE word = '" << slice(word).asString() << "'";
        
        CBLError error {};
        auto query = CBLDatabase_CreateQuery(wordEmbeddingDB, kCBLN1QLLanguage, slice(ss.str()), nullptr, &error);
        if (!query) {
            FAIL("Can't create query: " << error.domain << "/" << error.code);
        }
        
        auto results = CBLQuery_Execute(query, &error);
        if (!results) {
            FAIL("Can't execute query: " << error.domain << "/" << error.code);
        }
        
        FLMutableArray vector = nullptr;
        if (CBLResultSet_Next(results)) {
            auto array = FLValue_AsArray(CBLResultSet_ValueAtIndex(results, 0));
            if (array) {
                vector = FLArray_MutableCopy(array, kFLDeepCopyImmutables);
            }
        }
        
        CBLResultSet_Release(results);
        CBLQuery_Release(query);
        
        return vector;
    }
    
    vector<float> vectorForWord(FLString word) {
        vector<float> result {};
        
        auto vectorArray = vectorArrayForWord(word, kWordsCollectionName);
        if (!vectorArray) {
            vectorArray = vectorArrayForWord(word, kExtWordsCollectionName);
        }
        
        if (vectorArray) {
            auto array = Array(vectorArray);
            for (Array::iterator i(array); i; ++i) {
                auto val = i.value().asFloat();
                result.push_back(val);
            }
            FLMutableArray_Release(vectorArray);
        }
        
        return result;
    }
    
    void registerWordEmbeddingModel() {
        auto callback = [](void* context, FLDict input) -> FLSliceResult {
            auto word = fleece::Dict(input)["word"].asString();
            if (!word) { return FLSliceResult_CreateWith(nullptr, 0); }
            
            auto vector = ((VectorSearchTest*) context)->vectorArrayForWord(word, kWordsCollectionName);
            if (!vector) {
                vector = ((VectorSearchTest*) context)->vectorArrayForWord(word, kExtWordsCollectionName);
            }
            
            if (!vector) {
                return FLSliceResult_CreateWith(nullptr, 0);
            }
            
            FLEncoder enc = FLEncoder_New();
            FLEncoder_BeginDict(enc, 1);
            FLEncoder_WriteKey(enc, "vector"_sl);
            FLEncoder_WriteValue(enc, (FLValue)vector);
            FLEncoder_EndDict(enc);
            auto result = FLEncoder_Finish(enc, nullptr);
            FLMutableArray_Release(vector);
            return result;
        };
        
        CBLPredictiveModel model {};
        model.context = this;
        model.prediction = callback;
        CBL_RegisterPredictiveModel(kWordsPredictiveModelName, model);
    }
    
    void unregisterWordEmbeddingModel() {
        CBL_UnregisterPredictiveModel(kWordsPredictiveModelName);
    }
    
    void createVectorIndex(CBLCollection* collection, const slice& name, const CBLVectorIndexConfiguration& config) {
        CBLError error {};
        CHECK(CBLCollection_CreateVectorIndex(collection, name, config, &error));
        CheckNoError(error);
        
        FLArray indexNames = CBLCollection_GetIndexNames(collection, &error);
        CHECK(containsString(indexNames, name.asString()));
    }
    
    void createWordsIndex(const CBLVectorIndexConfiguration& config) {
        createVectorIndex(wordsCollection, kWordsIndexName, config);
    }
    
    CBLIndex* getWordsIndex() {
        CBLError error {};
        auto index = CBLCollection_GetIndex(wordsCollection, kWordsIndexName, &error);
        CheckNoError(error);
        CHECK(index);
        CHECK(CBLIndex_Name(index) == kWordsIndexName);
        CHECK(CBLIndex_Collection(index) == wordsCollection);
        return index;
    }
    
    void deleteWordsIndex() {
        CBLError error {};
        CHECK(CBLCollection_DeleteIndex(wordsCollection, kWordsIndexName, &error));
    }
    
    // Return false to skip update:
    typedef bool (*UpdateOrSkipCallback)(int);
    
    void updateWordsIndexWithUpdater(CBLIndexUpdater* updater,
                                     bool finish = true,
                                     vector<string>* outUpdatedWords = nullptr,
                                     vector<string>* outSkippedWords = nullptr,
                                     UpdateOrSkipCallback callback = nullptr)
    {
        CBLError error {};
        for (auto i = 0; i < CBLIndexUpdater_Count(updater); i++) {
            auto value = CBLIndexUpdater_Value(updater, i);
            auto word = FLValue_AsString(value);
            CHECK(word);
            
            bool update = !callback || callback(i);
            
            if (update) {
                auto vector = vectorForWord(word);
                CHECK(!vector.empty());
                CHECK(CBLIndexUpdater_SetVector(updater, i, vector.data(), vector.size(), &error));
                CheckNoError(error);
                
                if (outUpdatedWords) {
                    outUpdatedWords->push_back(slice(word).asString());
                }
            } else {
                CHECK(CBLIndexUpdater_SkipVector(updater, i, &error));
                CheckNoError(error);
                
                if (outSkippedWords) {
                    outSkippedWords->push_back(slice(word).asString());
                }
            }
        }
        
        if (finish) {
            CHECK(CBLIndexUpdater_Finish(updater, &error));
            CheckNoError(error);
        }
    }
    
    void setDinnerParameter(CBLQuery* query) {
        FLError error;
        FLMutableArray dinner = FLMutableArray_NewFromJSON(kDinnerVector, &error);
        REQUIRE(dinner);
        
        auto params = MutableDict::newDict();
        params["vector"_sl] = MutableArray(dinner);
        CBLQuery_SetParameters(query, params);
    }
    
    string wordQueryString(optional<int> limit={}, bool queryDistance=false, string addClause="") {
        auto indexName = slice(kWordsIndexName).asString();
        stringstream ss;
        ss << "SELECT meta().id, word";
        if (queryDistance) { ss << ", VECTOR_DISTANCE(" << indexName << ") "; } else { ss << " "; }
        ss << "FROM _default.words ";
        ss << "WHERE vector_match(" << indexName << ", $vector";
        if (limit) { ss << ", " << limit.value(); }
        ss << ")";
        if (!addClause.empty()) { ss << " " << addClause; }
        return ss.str();
    }
    
    CBLResultSet* executeWordsQuery(optional<int> limit={}, bool queryDistance=false, string addClause="") {
        auto query = CreateQuery(wordDB, wordQueryString(limit, queryDistance, addClause));
        setDinnerParameter(query);
        
        alloc_slice explanation(CBLQuery_Explain(query));
        CHECK(vectorIndexUsedInExplain(explanation, "words_index"));
        
        CBLError error {};
        auto rs = CBLQuery_Execute(query, &error);
        CHECK(rs);
        
        CBLQuery_Release(query);
        return rs;
    }
    
    void resetLog() {
        sVectorSearchTestLogs.clear();
    }

    bool isIndexTrained() {
        for (auto& str : sVectorSearchTestLogs) {
            if (str.find("Untrained index; queries may be slow") != string::npos) {
                return false;
            }
        }
        return true;
    }
    
    bool containsString(FLArray array, string str) {
        auto theArray = Array(array);
        for (Array::iterator i(theArray); i; ++i) {
            if (i->asstring().find(str) != string::npos) {
                return true;
            }
        }
        return false;
    }
    
    bool vectorIndexUsedInExplain(alloc_slice& explain, string indexName) {
        auto str = "SCAN kv_.words:vector:" + indexName;
        return explain.find(slice(str)).buf != nullptr;
    }
    
    void copyDocument(CBLCollection *collection, string docID, const CBLDocument* originalDoc) {
        CBLError error {};
        CBLDocument* doc = docID.empty() ? CBLDocument_Create() : CBLDocument_CreateWithID(slice(docID));
        CBLDocument_SetProperties(doc, FLDict_MutableCopy(CBLDocument_Properties(originalDoc), kFLDefaultCopy));
        REQUIRE(CBLCollection_SaveDocument(collection, doc, &error));
        CBLDocument_Release(doc);
    }
    
    unordered_map<string, string> mapWordResults(CBLResultSet *results) {
        unordered_map<string, string> map {};
        while (CBLResultSet_Next(results)) {
            FLString docID = FLValue_AsString(CBLResultSet_ValueAtIndex(results, 0));
            FLString word = FLValue_AsString(CBLResultSet_ValueAtIndex(results, 1));
            map[slice(docID).asString()] = slice(word).asString();
        }
        return map;
    }
    
    vector<string> wordResults(CBLResultSet *results) {
        vector<string> words {};
        while (CBLResultSet_Next(results)) {
            FLString word = FLValue_AsString(CBLResultSet_ValueAtIndex(results, 1));
            words.push_back(slice(word).asString());
        }
        return words;
    }
    
    vector<string> docIDResults(CBLResultSet *results) {
        vector<string> words {};
        while (CBLResultSet_Next(results)) {
            FLString word = FLValue_AsString(CBLResultSet_ValueAtIndex(results, 0));
            words.push_back(slice(word).asString());
        }
        return words;
    }
};

#endif
