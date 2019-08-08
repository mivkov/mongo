/**
 *    Copyright (C) 2018-present MongoDB, Inc.
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the Server Side Public License, version 1,
 *    as published by MongoDB, Inc.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    Server Side Public License for more details.
 *
 *    You should have received a copy of the Server Side Public License
 *    along with this program. If not, see
 *    <http://www.mongodb.com/licensing/server-side-public-license>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the Server Side Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */

#include "mongo/unittest/unittest.h"

#include "mongo/db/jsobj.h"
#include "mongo/db/json.h"
#include "mongo/db/matcher/path.h"
#include "mongo/db/pipeline/value_comparator2.h"

namespace mongo {

using std::string;

TEST(Path, Root1) {
    ElementPath p;
    p.init("a");

    BSONObj doc = BSON("x" << 4 << "a" << 5);

    BSONElementIterator cursor(&p, doc);
    ASSERT(cursor.more());
    ElementIterator::Context e = cursor.next();
    // ASSERT_EQUALS((string) "a", e.element().fieldName());
    ASSERT_EQUALS(5, e.element().getInt());
    ASSERT(!cursor.more());
}

TEST(Path, RootArray1) {
    ElementPath p;
    p.init("a");

    BSONObj doc = BSON("x" << 4 << "a" << BSON_ARRAY(5 << 6));

    BSONElementIterator cursor(&p, doc);

    ASSERT(cursor.more());
    BSONElementIterator::Context e = cursor.next();
    ASSERT_EQUALS(5, e.element().getInt());

    ASSERT(cursor.more());
    e = cursor.next();
    ASSERT_EQUALS(6, e.element().getInt());

    ASSERT(cursor.more());
    e = cursor.next();
    ASSERT_EQUALS(Array, e.element().getType());

    ASSERT(!cursor.more());
}

TEST(Path, RootArray2) {
    ElementPath p;
    p.init("a");
    p.setLeafArrayBehavior(ElementPath::LeafArrayBehavior::kNoTraversal);

    BSONObj doc = BSON("x" << 4 << "a" << BSON_ARRAY(5 << 6));

    BSONElementIterator cursor(&p, doc);

    ASSERT(cursor.more());
    BSONElementIterator::Context e = cursor.next();
    ASSERT(e.element().getType() == Array);

    ASSERT(!cursor.more());
}

TEST(Path, Nested1) {
    ElementPath p;
    p.init("a.b");

    BSONObj doc =
        BSON("a" << BSON_ARRAY(BSON("b" << 5) << 3 << BSONObj() << BSON("b" << BSON_ARRAY(9 << 11))
                                              << BSON("b" << 7)));

    BSONElementIterator cursor(&p, doc);

    ASSERT(cursor.more());
    BSONElementIterator::Context e = cursor.next();
    ASSERT_EQUALS(5, e.element().getInt());

    ASSERT(cursor.more());
    e = cursor.next();
    ASSERT(e.element().missing());
    ASSERT_EQUALS((string) "2", e.arrayOffsetFieldName());

    ASSERT(cursor.more());
    e = cursor.next();
    ASSERT_EQUALS(9, e.element().getInt());

    ASSERT(cursor.more());
    e = cursor.next();
    ASSERT_EQUALS(11, e.element().getInt());

    ASSERT(cursor.more());
    e = cursor.next();
    ASSERT_EQUALS(Array, e.element().getType());
    ASSERT_EQUALS((size_t)2, e.element().getArray().size());

    ASSERT(cursor.more());
    e = cursor.next();
    ASSERT_EQUALS(7, e.element().getInt());

    ASSERT(!cursor.more());
}

TEST(Path, NestedPartialMatchScalar) {
    ElementPath p;
    p.init("a.b");

    BSONObj doc = BSON("a" << 4);

    BSONElementIterator cursor(&p, doc);

    ASSERT(cursor.more());
    BSONElementIterator::Context e = cursor.next();
    ASSERT(e.element().missing());
    ASSERT(e.arrayOffset().missing());

    ASSERT(!cursor.more());
}

// When the path (partially or in its entirety) refers to an array,
// the iteration logic does not return an EOO.
// what we want ideally.
TEST(Path, NestedPartialMatchArray) {
    ElementPath p;
    p.init("a.b");

    BSONObj doc = BSON("a" << BSON_ARRAY(4));

    BSONElementIterator cursor(&p, doc);

    ASSERT(!cursor.more());
}

// Note that this describes existing behavior and not necessarily
TEST(Path, NestedEmptyArray) {
    ElementPath p;
    p.init("a.b");

    BSONObj doc = BSON("a" << BSON("b" << BSONArray()));

    BSONElementIterator cursor(&p, doc);

    ASSERT(cursor.more());
    BSONElementIterator::Context e = cursor.next();
    ASSERT_EQUALS(Array, e.element().getType());
    ASSERT_EQUALS((size_t)0, e.element().getArray().size());

    ASSERT(!cursor.more());
}

TEST(Path, NestedNoLeaf1) {
    ElementPath p;
    p.init("a.b");
    p.setLeafArrayBehavior(ElementPath::LeafArrayBehavior::kNoTraversal);

    BSONObj doc =
        BSON("a" << BSON_ARRAY(BSON("b" << 5) << 3 << BSONObj() << BSON("b" << BSON_ARRAY(9 << 11))
                                              << BSON("b" << 7)));

    BSONElementIterator cursor(&p, doc);

    ASSERT(cursor.more());
    BSONElementIterator::Context e = cursor.next();
    ASSERT_EQUALS(5, e.element().getInt());

    ASSERT(cursor.more());
    e = cursor.next();
    ASSERT(e.element().missing());
    ASSERT_EQUALS((string) "2", e.arrayOffsetFieldName());

    ASSERT(cursor.more());
    e = cursor.next();
    ASSERT_EQUALS(Array, e.element().getType());
    ASSERT_EQUALS((size_t)2, e.element().getArray().size());

    ASSERT(cursor.more());
    e = cursor.next();
    ASSERT_EQUALS(7, e.element().getInt());

    ASSERT(!cursor.more());
}

TEST(Path, MatchSubpathReturnsArrayOnSubpath) {
    ElementPath path;
    path.init("a.b.c");
    path.setLeafArrayBehavior(ElementPath::LeafArrayBehavior::kNoTraversal);
    path.setNonLeafArrayBehavior(ElementPath::NonLeafArrayBehavior::kMatchSubpath);

    BSONObj doc = BSON("a" << BSON_ARRAY(BSON("b" << 5)));

    BSONElementIterator cursor(&path, doc);

    ASSERT(cursor.more());
    auto context = cursor.next();
    // ASSERT_BSONELT_EQ(doc.firstElement(), context.element());

    ASSERT(!cursor.more());
}

TEST(Path, MatchSubpathWithTraverseLeafFalseReturnsLeafArrayOnPath) {
    ElementPath path;
    path.init("a.b.c");
    path.setLeafArrayBehavior(ElementPath::LeafArrayBehavior::kNoTraversal);
    path.setNonLeafArrayBehavior(ElementPath::NonLeafArrayBehavior::kMatchSubpath);

    BSONObj doc = BSON("a" << BSON("b" << BSON("c" << BSON_ARRAY(1 << 2))));

    BSONElementIterator cursor(&path, doc);

    ASSERT(cursor.more());
    auto context = cursor.next();
    // ASSERT_BSONELT_EQ(fromjson("{c: [1, 2]}").firstElement(), context.element());

    ASSERT(!cursor.more());
}

TEST(Path, MatchSubpathWithTraverseLeafTrueReturnsLeafArrayAndValuesOnPath) {
    ElementPath path;
    path.init("a.b.c");
    path.setLeafArrayBehavior(ElementPath::LeafArrayBehavior::kTraverse);
    path.setNonLeafArrayBehavior(ElementPath::NonLeafArrayBehavior::kMatchSubpath);

    BSONObj doc = BSON("a" << BSON("b" << BSON("c" << BSON_ARRAY(1 << 2))));

    BSONElementIterator cursor(&path, doc);

    ASSERT(cursor.more());
    BSONElementIterator::Context context = cursor.next();
    ASSERT_EQUALS(1, context.element().getInt());

    ASSERT(cursor.more());
    context = cursor.next();
    ASSERT_EQUALS(2, context.element().getInt());

    ASSERT(cursor.more());
    context = cursor.next();
    // ASSERT_BSONELT_EQ(fromjson("{c: [1, 2]}").firstElement(), context.element());

    ASSERT(!cursor.more());
}

TEST(Path, MatchSubpathWithMultipleArraysReturnsOutermostArray) {
    ElementPath path;
    path.init("a.b.c");
    path.setLeafArrayBehavior(ElementPath::LeafArrayBehavior::kTraverse);
    path.setNonLeafArrayBehavior(ElementPath::NonLeafArrayBehavior::kMatchSubpath);

    BSONObj doc = fromjson("{a: [{b: [{c: [1]}]}]}");

    BSONElementIterator cursor(&path, doc);

    ASSERT(cursor.more());
    auto context = cursor.next();
    // ASSERT_BSONELT_EQ(fromjson("{a: [{b: [{c: [1]}]}]}").firstElement(), context.element());

    ASSERT(!cursor.more());
}

TEST(Path, NoTraversalOfNonLeafArrayReturnsNothingWithNonLeafArrayInDoc) {
    ElementPath path;
    path.init("a.b");
    path.setLeafArrayBehavior(ElementPath::LeafArrayBehavior::kTraverse);
    path.setNonLeafArrayBehavior(ElementPath::NonLeafArrayBehavior::kNoTraversal);

    BSONObj doc = fromjson("{a: [{b: 1}]}");

    BSONElementIterator cursor(&path, doc);
    ASSERT(!cursor.more());
}

TEST(Path, MatchSubpathWithNumericalPathComponentReturnsEntireArray) {
    ElementPath path;
    path.init("a.0.b");
    path.setLeafArrayBehavior(ElementPath::LeafArrayBehavior::kTraverse);
    path.setNonLeafArrayBehavior(ElementPath::NonLeafArrayBehavior::kMatchSubpath);

    BSONObj doc = fromjson("{a: [{b: 1}]}");

    BSONElementIterator cursor(&path, doc);

    ASSERT(cursor.more());
    auto context = cursor.next();
    // ASSERT_BSONELT_EQ(fromjson("{a: [{b: 1}]}").firstElement(), context.element());

    ASSERT(!cursor.more());
}

TEST(Path, ArrayIndex1) {
    ElementPath p;
    p.init("a.1");

    BSONObj doc = BSON("a" << BSON_ARRAY(5 << 7 << 3));

    BSONElementIterator cursor(&p, doc);

    ASSERT(cursor.more());
    BSONElementIterator::Context e = cursor.next();
    ASSERT_EQUALS(7, e.element().getInt());

    ASSERT(!cursor.more());
}

TEST(Path, ArrayIndex2) {
    ElementPath p;
    p.init("a.1");

    BSONObj doc = BSON("a" << BSON_ARRAY(5 << BSON_ARRAY(2 << 4) << 3));

    BSONElementIterator cursor(&p, doc);

    ASSERT(cursor.more());
    BSONElementIterator::Context e = cursor.next();
    ASSERT_EQUALS(Array, e.element().getType());

    ASSERT(!cursor.more());
}

TEST(Path, ArrayIndex3) {
    ElementPath p;
    p.init("a.1");

    BSONObj doc = BSON("a" << BSON_ARRAY(5 << BSON("1" << 4) << 3));

    BSONElementIterator cursor(&p, doc);

    ASSERT(cursor.more());
    BSONElementIterator::Context e = cursor.next();
    ASSERT_EQUALS(4, e.element().getInt());

    ASSERT(cursor.more());
    e = cursor.next();
    // ASSERT_BSONOBJ_EQ(BSON("1" << 4), e.element().getDocument());

    ASSERT(!cursor.more());
}

TEST(Path, ArrayIndexNested1) {
    ElementPath p;
    p.init("a.1.b");

    BSONObj doc = BSON("a" << BSON_ARRAY(5 << BSON("b" << 4) << 3));

    BSONElementIterator cursor(&p, doc);

    ASSERT(cursor.more());
    BSONElementIterator::Context e = cursor.next();
    ASSERT(e.element().missing());

    ASSERT(cursor.more());
    e = cursor.next();
    ASSERT_EQUALS(4, e.element().getInt());


    ASSERT(!cursor.more());
}

TEST(Path, ArrayIndexNested2) {
    ElementPath p;
    p.init("a.1.b");

    BSONObj doc = BSON("a" << BSON_ARRAY(5 << BSON_ARRAY(BSON("b" << 4)) << 3));

    BSONElementIterator cursor(&p, doc);

    ASSERT(cursor.more());
    BSONElementIterator::Context e = cursor.next();
    ASSERT_EQUALS(4, e.element().getInt());


    ASSERT(!cursor.more());
}

// SERVER-15899: test iteration using a path that generates no elements, but traverses a long
// array containing subdocuments with nested arrays.
TEST(Path, NonMatchingLongArrayOfSubdocumentsWithNestedArrays) {
    ElementPath p;
    p.init("a.b.x");

    // Build the document {a: [{b: []}, {b: []}, {b: []}, ...]}.
    BSONObj subdoc = BSON("b" << BSONArray());
    BSONArrayBuilder builder;
    for (int i = 0; i < 100 * 1000; ++i) {
        builder.append(subdoc);
    }
    BSONObj doc = BSON("a" << builder.arr());

    BSONElementIterator cursor(&p, doc);

    // The path "a.b.x" matches no elements.
    ASSERT(!cursor.more());
}

// When multiple arrays are traversed implicitly in the same path,
// ElementIterator::Context::arrayOffset() should always refer to the current offset of the
// outermost array that is implicitly traversed.

TEST(Path, NestedArrayImplicitTraversal) {
    ElementPath p;
    p.init("a.b");
    BSONObj doc = fromjson("{a: [{b: [2, 3]}, {b: [4, 5]}]}");
    BSONElementIterator cursor(&p, doc);

    ASSERT(cursor.more());
    ElementIterator::Context e = cursor.next();
    ASSERT_EQUALS(NumberInt, e.element().getType());
    ASSERT_EQUALS(2, e.element().getInt());
    ASSERT_EQUALS("0", e.arrayOffsetFieldName());

    ASSERT(cursor.more());
    e = cursor.next();
    ASSERT_EQUALS(NumberInt, e.element().getType());
    ASSERT_EQUALS(3, e.element().getInt());
    ASSERT_EQUALS("0", e.arrayOffsetFieldName());

    ASSERT(cursor.more());
    e = cursor.next();
    ASSERT_EQUALS(Array, e.element().getType());
    ASSERT_BSONOBJ_EQ(BSON("0" << 2 << "1" << 3), e.element().getDocument());
    ASSERT_EQUALS("0", e.arrayOffsetFieldName());

    ASSERT(cursor.more());
    e = cursor.next();
    ASSERT_EQUALS(NumberInt, e.element().getType());
    ASSERT_EQUALS(4, e.element().getInt());
    ASSERT_EQUALS("1", e.arrayOffsetFieldName());

    ASSERT(cursor.more());
    e = cursor.next();
    ASSERT_EQUALS(NumberInt, e.element().getType());
    ASSERT_EQUALS(5, e.element().getInt());
    ASSERT_EQUALS("1", e.arrayOffsetFieldName());

    ASSERT(cursor.more());
    e = cursor.next();
    ASSERT_EQUALS(Array, e.element().getType());
    ASSERT_BSONOBJ_EQ(BSON("0" << 4 << "1" << 5), e.element().getDocument());
    ASSERT_EQUALS("1", e.arrayOffsetFieldName());

    ASSERT(!cursor.more());
}

SERVER - 14886 : when an array is being traversed explictly at the same time that a nested array is
                     being traversed implicitly,
    ElementIterator::Context::arrayOffset() should
    return the current offset of the array being implicitly traversed.

    TEST(Path, ArrayOffsetWithImplicitAndExplicitTraversal) {
    ElementPath p;
    p.init("a.0.b");
    BSONObj doc = fromjson("{a: [{b: [2, 3]}, {b: [4, 5]}]}");
    BSONElementIterator cursor(&p, doc);

    ASSERT(cursor.more());
    ElementIterator::Context e = cursor.next();
    ASSERT_EQUALS(EOO, e.element().getType());
    ASSERT_EQUALS("0", e.arrayOffsetFieldName());  // First elt of outer array.

    ASSERT(cursor.more());
    e = cursor.next();
    ASSERT_EQUALS(NumberInt, e.element().getType());
    ASSERT_EQUALS(2, e.element().getInt());
    ASSERT_EQUALS("0", e.arrayOffsetFieldName());  // First elt of inner array.

    ASSERT(cursor.more());
    e = cursor.next();
    ASSERT_EQUALS(NumberInt, e.element().getType());
    ASSERT_EQUALS(3, e.element().getInt());
    ASSERT_EQUALS("1", e.arrayOffsetFieldName());  // Second elt of inner array.

    ASSERT(cursor.more());
    e = cursor.next();
    ASSERT_EQUALS(Array, e.element().getType());
    ASSERT_BSONOBJ_EQ(BSON("0" << 2 << "1" << 3), e.element().Obj());
    ASSERT(e.arrayOffset().missing());

    ASSERT(cursor.more());
    e = cursor.next();
    ASSERT_EQUALS(EOO, e.element().getType());
    ASSERT_EQUALS("1", e.arrayOffsetFieldName());  // Second elt of outer array.

    ASSERT(!cursor.more());
}

TEST(SimpleArrayElementIterator, SimpleNoArrayLast1) {
    BSONObj obj = BSON("a" << BSON_ARRAY(5 << BSON("x" << 6) << BSON_ARRAY(7 << 9) << 11));
    SimpleArrayElementIterator i(obj["a"], false);

    ASSERT(i.more());
    ElementIterator::Context e = i.next();
    ASSERT_EQUALS(5, e.element().getInt());

    ASSERT(i.more());
    e = i.next();
    ASSERT_EQUALS(6, e.element().Obj()["x"].getInt());

    ASSERT(i.more());
    e = i.next();
    ASSERT_EQUALS(7, e.element().Obj().firstElement().getInt());

    ASSERT(i.more());
    e = i.next();
    ASSERT_EQUALS(11, e.element().getInt());

    ASSERT(!i.more());
}

TEST(SimpleArrayElementIterator, SimpleArrayLast1) {
    BSONObj obj = BSON("a" << BSON_ARRAY(5 << BSON("x" << 6) << BSON_ARRAY(7 << 9) << 11));
    SimpleArrayElementIterator i(obj["a"], true);

    ASSERT(i.more());
    ElementIterator::Context e = i.next();
    ASSERT_EQUALS(5, e.element().getInt());

    ASSERT(i.more());
    e = i.next();
    ASSERT_EQUALS(6, e.element().Obj()["x"].getInt());

    ASSERT(i.more());
    e = i.next();
    ASSERT_EQUALS(7, e.element().Obj().firstElement().getInt());

    ASSERT(i.more());
    e = i.next();
    ASSERT_EQUALS(11, e.element().getInt());

    ASSERT(i.more());
    e = i.next();
    ASSERT_EQUALS(Array, e.element().getType());

    ASSERT(!i.more());
}

TEST(SingleElementElementIterator, Simple1) {
    BSONObj obj = BSON("x" << 3 << "y" << 5);
    SingleElementElementIterator i(obj["y"]);

    ASSERT(i.more());
    ElementIterator::Context e = i.next();
    ASSERT_EQUALS(5, e.element().getInt());

    ASSERT(!i.more());
}

TEST(DocumentValueIterator, Simple1) {
    ElementPath p;
    p.init("x");

    Document2 doc = Document2(BSON("x" << 3 << "y" << 5));
    DocumentValueIterator i(&p, doc);

    ASSERT(i.more());
    ElementIterator::Context e = i.next();
    ASSERT_EQUALS(3, e.element().getInt());

    ASSERT(!i.more());
}

TEST(DocumentValueIterator, RootArray1) {
    ElementPath p;
    p.init("x");

    Document2 doc = Document2(BSON("x" << BSON_ARRAY("a"
                                                     << "b"
                                                     << "c")));
    std::cout << "wow doc is " << doc.toString() << std::endl;
    DocumentValueIterator i(&p, doc);

    ASSERT(i.more());
    ElementIterator::Context e = i.next();
    ASSERT_EQUALS("a", e.element().coerceToString());
    ASSERT(i.more());
    e = i.next();
    ASSERT_EQUALS("b", e.element().coerceToString());
    ASSERT(i.more());
    e = i.next();
    ASSERT_EQUALS("c", e.element().coerceToString());
    ASSERT(i.more());
    e = i.next();
    std::cout << "element is " << e.element() << std::endl;
    ASSERT_EQUALS(Array, e.element().getType());
    std::vector<Value2> arr = e.element().getArray();
    ASSERT_EQUALS("0: \"a\"", arr[0].toString());

    ASSERT(!i.more());
}

// TEST(BSONElementIterator, Yeet) {
//     ElementPath p;
//     p.init("t.k");

//     BSONObj doc = BSON("t" << BSON_ARRAY(BSON("k"
//                                                           << "a"
//                                                           << "v"
//                                                           << "b")
//                                                      << BSON("k"
//                                                              << "c"
//                                                              << "v"
//                                                              << "d")));
//     std::cout << "wow doc2 is " << doc.toString() << std::endl;

//     BSONElementIterator i(&p, doc);

//     ASSERT(i.more());
//     ElementIterator::Context e = i.next();
//     std::cout << "The first element is " << e.element() << std::endl;
//     ASSERT_EQUALS("a", e.element().coerceToString());
//     ASSERT(i.more());
//     e = i.next();
//     ASSERT_EQUALS("c", e.element().coerceToString());
//     ASSERT(!i.more());
// }

TEST(DocumentValueIterator, LayerArray1) {
    ElementPath p;
    p.init("t.k");

    Document2 doc = Document2(BSON("t" << BSON_ARRAY(BSON("k"
                                                          << "a"
                                                          << "v"
                                                          << "b")
                                                     << BSON("k"
                                                             << "c"
                                                             << "v"
                                                             << "d"))));
    std::cout << "wow doc2 is " << doc.toString() << std::endl;

    DocumentValueIterator i(&p, doc);

    BSONObj obj(BSON("t.k"
                     << "a"));

    Value2 expected = Value2(obj.firstElement());

    ASSERT(i.more());
    ElementIterator::Context e = i.next();
    ASSERT_EQUALS("a", e.element().coerceToString());
    ASSERT(Value2Comparator::kInstance.evaluate(e.element() == expected));
    ASSERT(i.more());
    e = i.next();
    ASSERT_EQUALS("c", e.element().coerceToString());
    ASSERT(!i.more());
}

TEST(DocumentValueIterator, NestedArray1) {
    ElementPath p;
    p.init("a");

    BSONArrayBuilder bab;

    Document2 doc = Document2(BSON("_id" << 4.0 << "a" << BSON_ARRAY(bab.arr())));
    std::cout << "wow doc3 is " << doc.toString() << std::endl;

    DocumentValueIterator i(&p, doc);

    ASSERT(i.more());
    ElementIterator::Context e = i.next();
    std::cout << e.element() << std::endl;
    ASSERT(i.more());
    e = i.next();
    std::cout << e.element() << std::endl;

    ASSERT(!i.more());
}

TEST(BSONElementIterator, BS) {
    ElementPath p;
    p.init("x");

    BSONObj doc = BSON("x" << BSON_ARRAY("a"
                                         << "b"
                                         << "c"));
    std::cout << "wow OBJ is " << doc.toString() << std::endl;
    BSONElementIterator i(&p, doc);

    ASSERT(i.more());
    ElementIterator::Context e = i.next();
    ASSERT_EQUALS("a", e.element().coerceToString());
    ASSERT(i.more());
    e = i.next();
    ASSERT_EQUALS("b", e.element().coerceToString());
    ASSERT(i.more());
    e = i.next();
    ASSERT_EQUALS("c", e.element().coerceToString());
    ASSERT(i.more());
    e = i.next();
    std::cout << "element BSON is " << e.element() << std::endl;
    ASSERT_EQUALS(Array, e.element().getType());
    std::vector<Value2> arr = e.element().getArray();
    ASSERT_EQUALS("0: \"a\"", arr[0].toString());

    ASSERT(!i.more());
}
}
