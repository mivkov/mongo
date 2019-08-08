#pragma once


#include "mongo/base/string_data.h"
#include "mongo/base/string_data_comparator_interface.h"
#include "mongo/bson/bsonobj.h"
#include "mongo/db/pipeline/document.h"
#include "mongo/db/pipeline/value2.h"

namespace mongo {
class BSONObj;
class Field2Iterator;
class FieldPath;
class Value2;

class Document2 {
    BSONObj _obj;
    Document _doc;

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

        DeferredComparison(Type type, const Document2& lhs, const Document2& rhs)
            : type(type), lhs(lhs), rhs(rhs) {}

        Type type;
        const Document2& lhs;
        const Document2& rhs;
    };

    bool is_obj;
    Document2() : _obj(BSONObj()), is_obj(true) {}

    explicit Document2(const BSONObj& obj) : _obj(obj), is_obj(true) {}

    Document2(const Document& doc) : _doc(doc), is_obj(true) {
        _obj = doc.toBson();
    }

    void swap(Document2& rhs) {
        _obj.swap(rhs._obj);
    }

    const Value2 operator[](StringData key) const {
        return is_obj ? Value2(_obj.getField(key)) : _doc.getField(key);
    }

    const Value2 getField(StringData key) const {
        Value2 val(_obj.getField(key));
        return is_obj ? val : _doc.getField(key);
    }

    size_t size() const {
        return is_obj ? _obj.nFields() : _doc.size();
    }

    bool empty() const {
        return is_obj ? _obj.isEmpty() : _doc.empty();
    }

    Field2Iterator fieldIterator() const;

    typedef std::pair<StringData, Value2> FieldPair;

    // TODO
    static int compare(const Document2& lhs,
                       const Document2& rhs,
                       const StringData::ComparatorInterface* stringComparator);

    std::string toString() const {
        return is_obj ? _obj.toString() : _doc.toString();
    }

    friend std::ostream& operator<<(std::ostream& out, const Document2& doc) {
        return out << doc.toString();
    }

    // TODO
    void hash_combine(size_t& seed, const StringData::ComparatorInterface* stringComparator) const;

    BSONObj toBson() const {
        return is_obj ? _obj : _doc.toBson();
    }

    friend BSONObjBuilder& operator<<(BSONObjBuilderValueStream& builder, const Document2& d) {
        return builder << (d.toBson());
    }

    Document2 clone() const {
        return is_obj ? Document2(_obj.copy()) : _doc.clone();
    }

    // struct SorterDeserializeSettings {};  // unused
    // void serializeForSorter(BufBuilder& buf) const;
    // static Document2 deserializeForSorter(BufReader& buf, const SorterDeserializeSettings&);
    // int memUsageForSorter() const {
    //     return _obj.objsize();
    // }
    // Document2 getOwned() const {
    //     return *this;
    // }
};

inline Document2::DeferredComparison operator==(const Document2& lhs, const Document2& rhs) {
    return Document2::DeferredComparison(Document2::DeferredComparison::Type::kEQ, lhs, rhs);
}

inline Document2::DeferredComparison operator!=(const Document2& lhs, const Document2& rhs) {
    return Document2::DeferredComparison(Document2::DeferredComparison::Type::kNE, lhs, rhs);
}

inline Document2::DeferredComparison operator<(const Document2& lhs, const Document2& rhs) {
    return Document2::DeferredComparison(Document2::DeferredComparison::Type::kLT, lhs, rhs);
}

inline Document2::DeferredComparison operator<=(const Document2& lhs, const Document2& rhs) {
    return Document2::DeferredComparison(Document2::DeferredComparison::Type::kLTE, lhs, rhs);
}

inline Document2::DeferredComparison operator>(const Document2& lhs, const Document2& rhs) {
    return Document2::DeferredComparison(Document2::DeferredComparison::Type::kGT, lhs, rhs);
}

inline Document2::DeferredComparison operator>=(const Document2& lhs, const Document2& rhs) {
    return Document2::DeferredComparison(Document2::DeferredComparison::Type::kGTE, lhs, rhs);
}

class Field2Iterator {
public:
    explicit Field2Iterator(const Document2& doc) : _doc(doc) {
        _it = new BSONObjIterator(_doc.toBson());
    }

    bool more() const {
        return _it->more();
    }

    Document2::FieldPair next() {
        BSONElement nxt = _it->next();
        return Document2::FieldPair(nxt.fieldNameStringData(), nxt);
    }

    ~Field2Iterator() {
        delete _it;
    }

private:
    Document2 _doc;
    BSONObjIterator* _it;
};

inline Field2Iterator Document2::fieldIterator() const {
    return Field2Iterator(*this);
}
}