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

#include "mongo/db/matcher/path.h"
#include "mongo/db/jsobj.h"
#include "mongo/db/matcher/path_internal.h"
#include "mongo/platform/basic.h"

namespace mongo {

void ElementPath::init(StringData path) {
    _nonLeafArrayBehavior = NonLeafArrayBehavior::kTraverse;
    _leafArrayBehavior = LeafArrayBehavior::kTraverse;
    _fieldRef.parse(path);
}

// -----

ElementIterator::~ElementIterator() {}

void ElementIterator::Context::reset() {
    _element = Value2();
}

void ElementIterator::Context::reset(BSONElement element, BSONElement arrayOffset) {
    _element = Value2(element);
    _arrayOffset = Value2(arrayOffset);
    _fieldName = arrayOffset.fieldNameStringData();
}

void ElementIterator::Context::reset(Value2 element, BSONElement arrayOffset) {
    _element = element;
    _arrayOffset = Value2(arrayOffset);
    _fieldName = arrayOffset.fieldNameStringData();
}

void ElementIterator::Context::reset(Value2 element, Value2 arrayOffset, StringData fieldName) {
    _element = element;
    _arrayOffset = arrayOffset;
    _fieldName = fieldName;
}

// ------

SimpleArrayElementIterator::SimpleArrayElementIterator(const Value2& theArray, bool returnArrayLast)
    : _theArray(theArray), _returnArrayLast(returnArrayLast) {
    _array = theArray.getArray();
    _iterator = _array.begin();
    _end = _array.end();
}

bool SimpleArrayElementIterator::more() {
    return _iterator != _end || _returnArrayLast;
}

ElementIterator::Context SimpleArrayElementIterator::next() {
    if (_iterator != _end) {
        Context e;
        e.reset(*(_iterator++), BSONElement());
        return e;
    }
    _returnArrayLast = false;
    Context e;
    e.reset(_theArray, BSONElement());
    return e;
}


// ------
BSONElementIterator::BSONElementIterator() {
    _path = nullptr;
}

BSONElementIterator::BSONElementIterator(const ElementPath* path,
                                         size_t suffixIndex,
                                         BSONElement elementToIterate)
    : _path(path), _state(BEGIN) {
    _setTraversalStart(suffixIndex, elementToIterate);
}

BSONElementIterator::BSONElementIterator(const ElementPath* path, const BSONObj& objectToIterate)
    : _path(path), _state(BEGIN) {
    _traversalStart =
        getFieldDottedOrArray(objectToIterate, _path->fieldRef(), &_traversalStartIndex);
}

BSONElementIterator::~BSONElementIterator() {}

void BSONElementIterator::reset(const ElementPath* path,
                                size_t suffixIndex,
                                BSONElement elementToIterate) {
    _path = path;
    _traversalStartIndex = 0;
    _traversalStart = BSONElement();
    _setTraversalStart(suffixIndex, elementToIterate);
    _state = BEGIN;
    _next.reset();

    _subCursor.reset();
    _subCursorPath.reset();
}

void BSONElementIterator::reset(const ElementPath* path, const BSONObj& objectToIterate) {
    _path = path;
    _traversalStartIndex = 0;
    _traversalStart =
        getFieldDottedOrArray(objectToIterate, _path->fieldRef(), &_traversalStartIndex);
    _state = BEGIN;
    _next.reset();

    _subCursor.reset();
    _subCursorPath.reset();
}

void BSONElementIterator::_setTraversalStart(size_t suffixIndex, BSONElement elementToIterate) {
    invariant(_path->fieldRef().numParts() >= suffixIndex);

    if (suffixIndex == _path->fieldRef().numParts()) {
        _traversalStart = elementToIterate;
    } else {
        if (elementToIterate.type() == BSONType::Object) {
            _traversalStart = getFieldDottedOrArray(
                elementToIterate.Obj(), _path->fieldRef(), &_traversalStartIndex, suffixIndex);
        } else if (elementToIterate.type() == BSONType::Array) {
            _traversalStart = elementToIterate;
        }
    }
}

void BSONElementIterator::ArrayIterationState::reset(const FieldRef& ref, int start) {
    restOfPath = ref.dottedField(start).toString();
    hasMore = restOfPath.size() > 0;
    if (hasMore) {
        nextPieceOfPath = ref.getPart(start);
        nextPieceOfPathIsNumber = isAllDigits(nextPieceOfPath);
    } else {
        nextPieceOfPathIsNumber = false;
    }
}

bool BSONElementIterator::ArrayIterationState::isArrayOffsetMatch(StringData fieldName) const {
    if (!nextPieceOfPathIsNumber)
        return false;
    return nextPieceOfPath == fieldName;
}


void BSONElementIterator::ArrayIterationState::startIterator(BSONElement e) {
    _theArray = e;
    _iterator.reset(new BSONObjIterator(_theArray.Obj()));
}

bool BSONElementIterator::ArrayIterationState::more() {
    return _iterator && _iterator->more();
}

BSONElement BSONElementIterator::ArrayIterationState::next() {
    _current = _iterator->next();
    return _current;
}


bool BSONElementIterator::subCursorHasMore() {
    // While we still are still finding arrays along the path, keep traversing deeper.
    while (_subCursor) {
        if (_subCursor->more()) {
            return true;
        }
        _subCursor.reset();

        // If the subcursor doesn't have more, see if the current element is an array offset
        // match (see comment in BSONElementIterator::more() for an example).  If it is indeed
        // an array offset match, create a new subcursor and examine it.
        if (_arrayIterationState.isArrayOffsetMatch(_arrayIterationState._current.fieldName())) {
            if (_arrayIterationState.nextEntireRest()) {
                // Our path terminates at the array offset.  _next should point at the current
                // array element. _next._arrayOffset should be EOO, since this is not an implicit
                // array traversal.
                _next.reset(_arrayIterationState._current, BSONElement());
                _arrayIterationState._current = BSONElement();
                return true;
            }

            _subCursorPath.reset(new ElementPath());
            _subCursorPath->init(_arrayIterationState.restOfPath.substr(
                _arrayIterationState.nextPieceOfPath.size() + 1));
            _subCursorPath->setLeafArrayBehavior(_path->leafArrayBehavior());

            // If we're here, we must be able to traverse nonleaf arrays.
            dassert(_path->nonLeafArrayBehavior() == ElementPath::NonLeafArrayBehavior::kTraverse);
            dassert(_subCursorPath->nonLeafArrayBehavior() ==
                    ElementPath::NonLeafArrayBehavior::kTraverse);

            _subCursor.reset(
                new BSONElementIterator(_subCursorPath.get(), _arrayIterationState._current.Obj()));

            // Set _arrayIterationState._current to EOO. This is not an implicit array traversal, so
            // we should not override the array offset of the subcursor with the current array
            // offset.
            _arrayIterationState._current = BSONElement();
        }
    }

    return false;
}

bool BSONElementIterator::more() {
    if (subCursorHasMore()) {
        return true;
    }

    if (!_next.element().missing()) {
        return true;
    }

    if (_state == DONE) {
        return false;
    }

    if (_state == BEGIN) {
        if (_traversalStart.type() != Array) {
            _next.reset(_traversalStart, BSONElement());
            _state = DONE;
            return true;
        }

        // It's an array.

        _arrayIterationState.reset(_path->fieldRef(), _traversalStartIndex + 1);

        if (_arrayIterationState.hasMore &&
            _path->nonLeafArrayBehavior() != ElementPath::NonLeafArrayBehavior::kTraverse) {
            // Don't allow traversing the array.
            if (_path->nonLeafArrayBehavior() == ElementPath::NonLeafArrayBehavior::kMatchSubpath) {
                _next.reset(_traversalStart, BSONElement());
                _state = DONE;
                return true;
            }

            _state = DONE;
            return false;
        } else if (!_arrayIterationState.hasMore &&
                   _path->leafArrayBehavior() == ElementPath::LeafArrayBehavior::kNoTraversal) {
            // Return the leaf array.
            _next.reset(_traversalStart, BSONElement());
            _state = DONE;
            return true;
        }

        _arrayIterationState.startIterator(_traversalStart);
        _state = IN_ARRAY;

        invariant(_next.element().missing());
    }

    if (_state == IN_ARRAY) {
        // We're traversing an array.  Look at each array element.

        while (_arrayIterationState.more()) {
            BSONElement eltInArray = _arrayIterationState.next();
            if (!_arrayIterationState.hasMore) {
                // Our path terminates at this array.  _next should point at the current array
                // element.
                _next.reset(eltInArray, eltInArray);
                return true;
            }

            // Our path does not terminate at this array; there's a subpath left over.  Inspect
            // the current array element to see if it could match the subpath.

            if (eltInArray.type() == Object) {
                // The current array element is a subdocument.  See if the subdocument generates
                // any elements matching the remaining subpath.
                _subCursorPath.reset(new ElementPath());
                _subCursorPath->init(_arrayIterationState.restOfPath);
                _subCursorPath->setLeafArrayBehavior(_path->leafArrayBehavior());

                _subCursor.reset(new BSONElementIterator(_subCursorPath.get(), eltInArray.Obj()));
                if (subCursorHasMore()) {
                    return true;
                }
            } else if (_arrayIterationState.isArrayOffsetMatch(eltInArray.fieldName())) {
                // The path we're traversing has an array offset component, and the current
                // array element corresponds to the offset we're looking for (for example: our
                // path has a ".0" component, and we're looking at the first element of the
                // array, so we should look inside this element).

                if (_arrayIterationState.nextEntireRest()) {
                    // Our path terminates at the array offset.  _next should point at the
                    // current array element. _next._arrayOffset should be EOO, since this is not an
                    // implicit array traversal.
                    _next.reset(eltInArray, BSONElement());
                    return true;
                }

                invariant(eltInArray.type() != Object);  // Handled above.
                if (eltInArray.type() == Array) {
                    // The current array element is itself an array.  See if the nested array
                    // has any elements matching the remainihng.
                    _subCursorPath.reset(new ElementPath());
                    _subCursorPath->init(_arrayIterationState.restOfPath.substr(
                        _arrayIterationState.nextPieceOfPath.size() + 1));
                    _subCursorPath->setLeafArrayBehavior(_path->leafArrayBehavior());
                    BSONElementIterator* real = new BSONElementIterator(
                        _subCursorPath.get(), _arrayIterationState._current.Obj());
                    _subCursor.reset(real);
                    real->_arrayIterationState.reset(_subCursorPath->fieldRef(), 0);
                    real->_arrayIterationState.startIterator(eltInArray);
                    real->_state = IN_ARRAY;

                    // Set _arrayIterationState._current to EOO. This is not an implicit array
                    // traversal, so we should not override the array offset of the subcursor with
                    // the current array offset.
                    _arrayIterationState._current = BSONElement();

                    if (subCursorHasMore()) {
                        return true;
                    }
                }
            }
        }

        if (_arrayIterationState.hasMore) {
            return false;
        }

        _next.reset(_arrayIterationState._theArray, BSONElement());
        _state = DONE;
        return true;
    }

    return false;
}

ElementIterator::Context BSONElementIterator::next() {
    if (_subCursor) {
        Context e = _subCursor->next();
        // Use our array offset if we have one, otherwise copy our subcursor's.  This has the
        // effect of preferring the outermost array offset, in the case where we are implicitly
        // traversing nested arrays and have multiple candidate array offsets.  For example,
        // when we use the path "a.b" to generate elements from the document {a: [{b: [1, 2]}]},
        // the element with a value of 2 should be returned with an array offset of 0.
        if (!_arrayIterationState._current.eoo()) {
            e.setArrayOffset(_arrayIterationState._current);
        }
        return e;
    }
    Context x = _next;
    _next.reset();
    return x;
}

//-----
DocumentValueIterator::DocumentValueIterator() {
    _path = NULL;
}

DocumentValueIterator::DocumentValueIterator(const ElementPath* path,
                                             size_t suffixIndex,
                                             Value2 elementToIterate)
    : _path(path), _state(BEGIN) {
    _setTraversalStart(suffixIndex, elementToIterate);
}

DocumentValueIterator::DocumentValueIterator(const ElementPath* path,
                                             const Document2& objectToIterate)
    : _path(path), _state(BEGIN) {
    _traversalStart =
        getFieldDottedOrArrayDV(objectToIterate, _path->fieldRef(), &_traversalStartIndex);
}

DocumentValueIterator::~DocumentValueIterator() {}

void DocumentValueIterator::reset(const ElementPath* path,
                                  size_t suffixIndex,
                                  Value2 elementToIterate) {
    _path = path;
    _traversalStartIndex = 0;
    _traversalStart = Value2();
    _setTraversalStart(suffixIndex, elementToIterate);
    _state = BEGIN;
    _next.reset();

    _subCursor.reset();
    _subCursorPath.reset();
}

void DocumentValueIterator::reset(const ElementPath* path, const Document2& objectToIterate) {
    _path = path;
    _traversalStartIndex = 0;
    _traversalStart =
        getFieldDottedOrArrayDV(objectToIterate, _path->fieldRef(), &_traversalStartIndex);
    _state = BEGIN;
    _next.reset();

    _subCursor.reset();
    _subCursorPath.reset();
}

void DocumentValueIterator::_setTraversalStart(size_t suffixIndex, Value2 elementToIterate) {
    invariant(_path->fieldRef().numParts() >= suffixIndex);

    if (suffixIndex == _path->fieldRef().numParts()) {
        _traversalStart = elementToIterate;
    } else {
        if (elementToIterate.getType() == BSONType::Object) {
            _traversalStart = getFieldDottedOrArrayDV(elementToIterate.getDocument(),
                                                      _path->fieldRef(),
                                                      &_traversalStartIndex,
                                                      suffixIndex);
        } else if (elementToIterate.getType() == BSONType::Array) {
            _traversalStart = elementToIterate;
        }
    }
}

void DocumentValueIterator::ArrayIterationState::reset(const FieldRef& ref, int start) {
    restOfPath = ref.dottedField(start).toString();
    hasMore = restOfPath.size() > 0;
    if (hasMore) {
        nextPieceOfPath = ref.getPart(start);
        nextPieceOfPathIsNumber = isAllDigits(nextPieceOfPath);
    } else {
        nextPieceOfPathIsNumber = false;
    }
}

bool DocumentValueIterator::ArrayIterationState::isArrayOffsetMatch(StringData fieldName) const {
    if (!nextPieceOfPathIsNumber)
        return false;
    return nextPieceOfPath == fieldName;
}


void DocumentValueIterator::ArrayIterationState::startIterator(Value2 e) {
    _theArray = e;
    _array = _theArray.getArray();
    _iterator = _array.begin();
    _index = -1;
    _end = _array.end();
}

bool DocumentValueIterator::ArrayIterationState::more() {
    return _iterator != _end;
}

Value2 DocumentValueIterator::ArrayIterationState::next() {
    _current = *_iterator;
    ++_iterator;
    ++_index;
    return _current;
}


bool DocumentValueIterator::subCursorHasMore() {
    // While we still are still finding arrays along the path, keep traversing deeper.
    while (_subCursor) {
        if (_subCursor->more()) {
            return true;
        }

        _subCursor.reset();

        // If the subcursor doesn't have more, see if the current element is an array offset
        // match (see comment in BSONElementIterator::more() for an example).  If it is indeed
        // an array offset match, create a new subcursor and examine it.
        if (_arrayIterationState.isArrayOffsetMatch(ItoA(_arrayIterationState._index))) {
            if (_arrayIterationState.nextEntireRest()) {
                // Our path terminates at the array offset.  _next should point at the current
                // array element. _next._arrayOffset should be EOO, since this is not an implicit
                // array traversal.
                _next.reset(_arrayIterationState._current, BSONElement());
                _arrayIterationState._current = Value2();
                return true;
            }

            _subCursorPath.reset(new ElementPath());
            _subCursorPath->init(_arrayIterationState.restOfPath.substr(
                _arrayIterationState.nextPieceOfPath.size() + 1));
            _subCursorPath->setLeafArrayBehavior(_path->leafArrayBehavior());

            // If we're here, we must be able to traverse nonleaf arrays.
            dassert(_path->nonLeafArrayBehavior() == ElementPath::NonLeafArrayBehavior::kTraverse);
            dassert(_subCursorPath->nonLeafArrayBehavior() ==
                    ElementPath::NonLeafArrayBehavior::kTraverse);

            if (_arrayIterationState._current.getType() == Array) {
                _subCursor.reset(new DocumentValueIterator(
                    _subCursorPath.get(), 0, Value2(_arrayIterationState._current.getArray())));
            } else if (_arrayIterationState._current.getType() == Object) {
                _subCursor.reset(new DocumentValueIterator(
                    _subCursorPath.get(), _arrayIterationState._current.getDocument()));
            }

            // Set _arrayIterationState._current to EOO. This is not an implicit array traversal, so
            // we should not override the array offset of the subcursor with the current array
            // offset.
            _arrayIterationState._current = Value2();
        }
    }

    return false;
}

bool DocumentValueIterator::more() {
    if (subCursorHasMore()) {
        return true;
    }

    if (!_next.element().missing()) {
        return true;
    }

    if (_state == DONE) {
        return false;
    }

    if (_state == BEGIN) {
        if (_traversalStart.getType() != Array) {
            _next.reset(_traversalStart, BSONElement());
            _state = DONE;
            return true;
        }

        // It's an array.

        _arrayIterationState.reset(_path->fieldRef(), _traversalStartIndex + 1);

        if (_arrayIterationState.hasMore &&
            _path->nonLeafArrayBehavior() != ElementPath::NonLeafArrayBehavior::kTraverse) {
            // Don't allow traversing the array.
            if (_path->nonLeafArrayBehavior() == ElementPath::NonLeafArrayBehavior::kMatchSubpath) {
                _next.reset(_traversalStart, BSONElement());
                _state = DONE;
                return true;
            }

            _state = DONE;
            return false;
        } else if (!_arrayIterationState.hasMore &&
                   _path->leafArrayBehavior() == ElementPath::LeafArrayBehavior::kNoTraversal) {
            // Return the leaf array.
            _next.reset(_traversalStart, BSONElement());
            _state = DONE;
            return true;
        }

        _arrayIterationState.startIterator(_traversalStart);
        _state = IN_ARRAY;

        invariant(_next.element().missing());
    }

    if (_state == IN_ARRAY) {

        while (_arrayIterationState.more()) {
            Value2 eltInArray = _arrayIterationState.next();
            if (!_arrayIterationState.hasMore) {
                // Our path terminates at this array.  _next should point at the current array
                // element.
                _next.reset(eltInArray, eltInArray, ItoA(_arrayIterationState._index));
                return true;
            }

            // Our path does not terminate at this array; there's a subpath left over.  Inspect
            // the current array element to see if it could match the subpath.

            if (eltInArray.getType() == Object) {
                // The current array element is a subdocument.  See if the subdocument generates
                // any elements matching the remaining subpath.
                _subCursorPath.reset(new ElementPath());
                _subCursorPath->init(_arrayIterationState.restOfPath);
                _subCursorPath->setLeafArrayBehavior(_path->leafArrayBehavior());

                _subCursor.reset(
                    new DocumentValueIterator(_subCursorPath.get(), eltInArray.getDocument()));
                if (subCursorHasMore()) {
                    return true;
                }
            } else if (_arrayIterationState.isArrayOffsetMatch(ItoA(_arrayIterationState._index))) {
                // The path we're traversing has an array offset component, and the current
                // array element corresponds to the offset we're looking for (for example: our
                // path has a ".0" component, and we're looking at the first element of the
                // array, so we should look inside this element).

                if (_arrayIterationState.nextEntireRest()) {
                    // Our path terminates at the array offset.  _next should point at the
                    // current array element. _next._arrayOffset should be EOO, since this is not an
                    // implicit array traversal.
                    _next.reset(eltInArray, BSONElement());
                    return true;
                }

                invariant(eltInArray.getType() != Object);  // Handled above.
                if (eltInArray.getType() == Array) {
                    // The current array element is itself an array.  See if the nested array
                    // has any elements matching the remainihng.
                    _subCursorPath.reset(new ElementPath());
                    _subCursorPath->init(_arrayIterationState.restOfPath.substr(
                        _arrayIterationState.nextPieceOfPath.size() + 1));
                    _subCursorPath->setLeafArrayBehavior(_path->leafArrayBehavior());
                    DocumentValueIterator* real;
                    if (_arrayIterationState._current.getType() == Array) {
                        real = new DocumentValueIterator(
                            _subCursorPath.get(),
                            0,
                            Value2(_arrayIterationState._current.getArray()));
                    } else {
                        real = new DocumentValueIterator(
                            _subCursorPath.get(), _arrayIterationState._current.getDocument());
                    }
                    _subCursor.reset(real);
                    real->_arrayIterationState.reset(_subCursorPath->fieldRef(), 0);
                    real->_arrayIterationState.startIterator(eltInArray);
                    real->_state = IN_ARRAY;

                    // Set _arrayIterationState._current to EOO. This is not an implicit array
                    // traversal, so we should not override the array offset of the subcursor with
                    // the current array offset.
                    _arrayIterationState._current = Value2();

                    if (subCursorHasMore()) {
                        return true;
                    }
                }
            }
        }

        if (_arrayIterationState.hasMore) {
            return false;
        }

        _next.reset(_arrayIterationState._theArray, BSONElement());
        _state = DONE;
        return true;
    }

    return false;
}

ElementIterator::Context DocumentValueIterator::next() {
    if (_subCursor) {
        Context e = _subCursor->next();
        // Use our array offset if we have one, otherwise copy our subcursor's.  This has the
        // effect of preferring the outermost array offset, in the case where we are implicitly
        // traversing nested arrays and have multiple candidate array offsets.  For example,
        // when we use the path "a.b" to generate elements from the document {a: [{b: [1, 2]}]},
        // the element with a value of 2 should be returned with an array offset of 0.
        if (!_arrayIterationState._current.missing()) {
            e.setArrayOffset(_arrayIterationState._current, ItoA(_arrayIterationState._index));
        }
        return e;
    }
    Context x = _next;
    _next.reset();
    return x;
}
}
