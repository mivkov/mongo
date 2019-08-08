#pragma once

#include "mongo/bson/bsonobj.h"
#include "mongo/bson/bsonobjbuilder.h"
#include "mongo/db/pipeline/value.h"

namespace mongo {
class BSONObj;
class Document2;
class Value2 {
    BSONElement _elem;

    BSONObj _obj;
    Value _val;
    bool _iselem;

public:
    struct DeferredComparison {
        enum class Type {
            kLT,
            kLTE,
            kEQ,
            kGT,
            kGTE,
            kNE,
        };

        DeferredComparison(Type type, const Value2& lhs, const Value2& rhs)
            : type(type), lhs(lhs), rhs(rhs) {}

        Type type;
        const Value2& lhs;
        const Value2& rhs;
    };

    Value2() : _elem(BSONElement()), _iselem(true) {}

    explicit Value2(std::vector<Value2> vec);

    explicit Value2(BSONElement elem) : _elem(elem), _iselem(true) {}

    Value2(const Value& val);

    bool missing() const {
        return _elem.type() == EOO;
    }

    bool nullish() const {
        return missing() || _elem.type() == jstNULL || _elem.type() == Undefined;
    }

    bool numeric() const {
        return _elem.type() == NumberDouble || _elem.type() == NumberLong ||
            _elem.type() == NumberInt || _elem.type() == NumberDecimal;
    }

    BSONType getType() const {
        return _elem.type();
    }

    double getDouble() const {
        return _elem.Double();
    }

    StringData getStringData() const {
        return _elem.valueStringData();
    }
    StringData getStringDataSafe() const {
        return getType() == String ? getStringData() : StringData();
    }
    bool getBool() const {
        return _elem.Bool();
    }

    const char* getRegex() const {
        return _elem.regex();
    }

    const char* getRegexFlags() const {
        return _elem.regexFlags();
    }

    int getInt() const {
        return _elem.Int();
    }
    long long getLong() const {
        return _elem.Long();
    }
    Document2 getDocument() const;
    BSONBinData getBinData() const;
    const std::vector<Value2> getArray() const;


    friend BSONObjBuilder& operator<<(BSONObjBuilderValueStream& builder, const Value2& val);

    bool coerceToBool() const;
    std::string coerceToString() const;

    long long coerceToLong() const;
    double coerceToDouble() const;
    Decimal128 coerceToDecimal() const;

    static int compare(const Value2& lhs,
                       const Value2& rhs,
                       const StringData::ComparatorInterface* stringComparator);

    friend DeferredComparison operator==(const Value2& lhs, const Value2& rhs) {
        return DeferredComparison(DeferredComparison::Type::kEQ, lhs, rhs);
    }

    friend DeferredComparison operator!=(const Value2& lhs, const Value2& rhs) {
        return DeferredComparison(DeferredComparison::Type::kNE, lhs, rhs);
    }

    friend DeferredComparison operator<(const Value2& lhs, const Value2& rhs) {
        return DeferredComparison(DeferredComparison::Type::kLT, lhs, rhs);
    }

    friend DeferredComparison operator<=(const Value2& lhs, const Value2& rhs) {
        return DeferredComparison(DeferredComparison::Type::kLTE, lhs, rhs);
    }

    friend DeferredComparison operator>(const Value2& lhs, const Value2& rhs) {
        return DeferredComparison(DeferredComparison::Type::kGT, lhs, rhs);
    }

    friend DeferredComparison operator>=(const Value2& lhs, const Value2& rhs) {
        return DeferredComparison(DeferredComparison::Type::kGTE, lhs, rhs);
    }

    std::string toString() const;
    friend std::ostream& operator<<(std::ostream& out, const Value2& v);

    void hash_combine(size_t& seed, const StringData::ComparatorInterface* stringComparator) const;
};
}