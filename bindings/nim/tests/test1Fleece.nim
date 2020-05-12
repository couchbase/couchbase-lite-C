# Fleece unit tests
#
# Copyright (c) 2020 Couchbase, Inc All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#


import CouchbaseLite/fleece
import unittest



suite "Fleece Accessors":
    let JSON = """{"integer": 1234, "string": "Hello, World!", "float": 1234.56,
                   "true":true, "false": false, "null": null,
                   "array": [null, false, true, 10, [], {}],
                   "dict": {"foo": "bar", "frob": -9}}"""
    var doc: Fleece

    setup:
        doc = parseJSON(JSON)

    test "Parse JSON":
        check doc.root.toJSON == """{"array":[null,false,true,10,[],{}],"dict":{"foo":"bar","frob":-9},"false":false,"float":1234.56,"integer":1234,"null":null,"string":"Hello, World!","true":true}"""

    test "Null":
        let n = doc["null"]
        check n != nil
        check n.type == Type.null
        check n.asBool == false

    test "Bool":
        let t = doc["true"]
        check t != nil
        check t.type == Type.bool
        check not t.isNumber
        check t.asBool == true
        check t != 1

        let f = doc["false"]
        check f != nil
        check f.type == Type.bool
        check not f.isNumber
        check f.asBool == false
        check f != 0

    test "Int":
        check doc.root["integer"].asInt == 1234
        let i = doc["integer"]
        check i != nil
        check i.type == Type.number
        check i.isInt
        check not i.isFloat
        check i == doc.root["integer"]
        check i.asInt == 1234
        check i.asInt(0'i8) == 127'i8
        check i.asFloat == 1234.0
        check i.asString == ""
        check i.asString("nope") == "nope"
        check i == 1234
        check i == 1234.0f64
        check i != 4321

    test "Float":
        let f = doc["float"]
        check f != nil
        check f.type == Type.number
        check f.isFloat
        check not f.isInt
        check f.asString == ""
        check f.asInt == 1234
        check f.asFloat == 1234.56
        check f.asBool == true
        check f == 1234.56
        check f != 1234

    test "String":
        let s = doc["string"]
        check s != nil
        check s.type == Type.string
        check s.asString == "Hello, World!"
        check s == "Hello, World!"
        check s != 0
        check s != 3.14

    test "Undefined":
        let m = doc["MISSING"]
        check m == nil
        check m.type == Type.undefined
        check m != 0

    test "Array":
        let v = doc["array"]
        check v != nil
        check v.type == Type.array
        let a = v.asArray
        check a != nil
        check a.len == 6
        check not a.isEmpty
        check a[0].type == Type.null
        check a[1].type == Type.bool
        check a[2].type == Type.bool
        check a[3].type == Type.number
        check a[4].type == Type.array
        check a[5].type == Type.dict

    test "Array iterator":
        var i = 0
        let expected = @["null", "false", "true", "10", "[]", "{}"]
        for item in doc["array"].asArray:
            check $item == expected[i]
            i += 1
        check i == 6

    test "Dict iterator":
        var i = 0
        let expectedKeys = @["foo", "frob"]
        let expectedVals = @["\"bar\"", "-9"]
        for k, v in doc["dict"].asDict.items:
            check k == expectedKeys[i]
            check $v == expectedVals[i]
            i += 1
        check i == 2

    test "Empty array iterator":
        let a = doc["array"][4].asArray
        check a != nil
        for item in a:
            check false

    test "Empty dict iterator":
        let d = doc["array"][5].asDict
        check d != nil
        for item in d:
            check false

    test "KeyPath":
        var path = keyPath("dict.frob")
        check $(path.eval(doc.root)) == "-9"
        path = keyPath("array[2]")
        check $(path.eval(doc.root)) == "true"



suite "Mutable Fleece":
    test "mutable array":
        var a = newMutableArray()
        check a.len == 0
        check a.isEmpty
        check not a.isChanged
        a.add(17)
        a.add(true)
        a.add("howdy")
        check a.len == 3
        check $a == "[17,true,\"howdy\"]"
        a[1] = false
        check $a == "[17,false,\"howdy\"]"
        a.delete(0)
        check $a == "[false,\"howdy\"]"
        a.add(-3.14)
        check $a == "[false,\"howdy\",-3.14]"
        check a.source == nil
        check a.isChanged

    test "mutable dict":
        var d = newMutableDict()
        check d.len == 0
        check d.isEmpty
        check not d.isChanged
        d["x"] = 17
        d["y"] = true
        d["z"] = "howdy"
        check d.len == 3
        check $d == """{"x":17,"y":true,"z":"howdy"}"""
        d["y"] = false
        check $d == """{"x":17,"y":false,"z":"howdy"}"""
        d.delete("x")
        check $d == """{"y":false,"z":"howdy"}"""
        d["zz"] = -3.14
        check $d == """{"y":false,"z":"howdy","zz":-3.14}"""
        check d.source == nil
        check d.isChanged

    test "Assign mutable array":
        var a = newMutableArray()
        a.add(17)
        a.add(true)
        a.add("howdy")

        var b = newMutableArray()
        b.add(false)

        a = b
        check $a == "[false]"
        check $b == "[false]"
