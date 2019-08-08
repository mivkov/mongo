#include "mongo/db/pipeline/value2.h"
#include "mongo/bson/bsonelement.h"
#include "mongo/db/pipeline/document2.h"

namespace mongo {
Value2::Value2(std::vector<Value2> vec) : _iselem(true) {  // hack
    // invariant(false);
    BSONArrayBuilder bab;
    for (size_t i = 0; i < vec.size(); i++) {
        bab << vec[i]._elem;
    }
    BSONObjBuilder bob;
    bob << "" << bab.arr();
    _obj = bob.obj();
    _elem = _obj.firstElement();
}

Value2::Value2(const Value& val) : _val(val), _iselem(true) {
    _obj = BSON("" << val);
    _elem = _obj.firstElement();
    invariant(false);
}

Document2 Value2::getDocument() const {
    return _iselem ? Document2(_elem.Obj()) : _val.getDocument();
}

BSONBinData Value2::getBinData() const {
    if (_iselem) {
        const char* data;
        int len;
        data = _elem.binData(len);
        return BSONBinData(data, len, _elem.binDataType());
    }
    return _val.getBinData();
}

const std::vector<Value2> Value2::getArray() const {
    std::vector<BSONElement> elemarr = _elem.Array();
    std::vector<Value2> array;
    for (BSONElement e : elemarr) {
        Value2 val(e);
        array.push_back(val);
    }
    return array;
}

BSONObjBuilder& operator<<(BSONObjBuilderValueStream& builder, const Value2& val) {
    return val._iselem ? builder << val._elem : builder << val._val;
}

bool Value2::coerceToBool() const {
    return _iselem ? _elem.trueValue() : _val.coerceToBool();
}

std::string Value2::coerceToString() const {
    return _iselem ? std::string(_elem.valuestr(), _elem.valuestrsize() - 1)
                   : _val.coerceToString();
}

long long Value2::coerceToLong() const {
    return _iselem ? _elem.numberLong() : _val.coerceToLong();
}
double Value2::coerceToDouble() const {
    return _iselem ? _elem.numberDouble() : _val.coerceToDouble();
}
Decimal128 Value2::coerceToDecimal() const {
    return _iselem ? _elem.numberDecimal() : _val.coerceToDecimal();
}

int Value2::compare(const Value2& lhs,
                    const Value2& rhs,
                    const StringData::ComparatorInterface* stringComparator) {
    Value lhsVal = Value(lhs._elem);
    Value rhsVal = Value(rhs._elem);
    return Value::compare(lhsVal, rhsVal, stringComparator);
}

std::string Value2::toString() const {
    return _iselem ? _elem.toString() : _val.toString();
}

std::ostream& operator<<(std::ostream& out, const Value2& v) {
    out << v.toString();
    return out;
}

void Value2::hash_combine(size_t& seed,
                          const StringData::ComparatorInterface* stringComparator) const {
    if (_iselem) {
        Value(_elem).hash_combine(seed, stringComparator);
    } else {
        _val.hash_combine(seed, stringComparator);
    }
}
}