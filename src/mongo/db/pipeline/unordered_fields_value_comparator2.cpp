#include "mongo/db/pipeline/unordered_fields_value_comparator2.h"
#include "mongo/db/pipeline/document2.h"

namespace mongo {

// Stolen from value.cpp
template <typename T>
inline static int cmp(const T& left, const T& right) {
    if (left < right) {
        return -1;
    } else if (left == right) {
        return 0;
    } else {
        dassert(left > right);
        return 1;
    }
}

int UnorderedFieldsValueComparator::compare(const Value2& lhs, const Value2& rhs) const {
    BSONType lType = lhs.getType();
    BSONType rType = rhs.getType();

    int ret = lType == rType ? 0  // fast-path common case
                             : cmp(canonicalizeBSONType(lType), canonicalizeBSONType(rType));

    if (ret)
        return ret;

    switch (lType) {
        case Object: {
            const Document2 lDoc = lhs.getDocument();
            const Document2 rDoc = rhs.getDocument();
            Field2Iterator lIt(lDoc);
            Field2Iterator rIt(rDoc);
            std::vector<std::pair<StringData, Value2>> lArr, rArr;
            while (lIt.more() && rIt.more()) {
                lArr.push_back(lIt.next());
                rArr.push_back(rIt.next());
            }
            if (lIt.more() != rIt.more())
                return lIt.more() - rIt.more();  // unequal number of elems

            std::sort(lArr.begin(), lArr.end(), [&](auto a, auto b) { return a.first < b.first; });
            std::sort(rArr.begin(), rArr.end(), [&](auto a, auto b) { return a.first < b.first; });

            std::stringstream ss1;
            std::stringstream ss2;
            std::for_each(
                lArr.begin(), lArr.end(), [&](auto a) { ss1 << a.first << " " << a.second; });
            std::for_each(
                rArr.begin(), rArr.end(), [&](auto a) { ss2 << a.first << " " << a.second; });

            auto leftIt = lArr.begin();
            auto rightIt = rArr.begin();
            std::pair<StringData, Value2> lElem, rElem;
            for (; leftIt != lArr.end() && rightIt != rArr.end(); ++leftIt, ++rightIt) {
                lElem = *leftIt;
                rElem = *rightIt;
                ret = lElem.first.compare(rElem.first);
                if (ret)
                    return ret;
                ret = compare(lElem.second, rElem.second);
                if (ret)
                    return ret;
            }
            return 0;
        }
        case Array: {  // also stolen from value.cpp
            const std::vector<Value2>& lArr = lhs.getArray();
            const std::vector<Value2>& rArr = rhs.getArray();

            const size_t elems = std::min(lArr.size(), rArr.size());
            for (size_t i = 0; i < elems; i++) {
                // compare the two corresponding elements
                ret = compare(lArr[i], rArr[i]);
                if (ret)
                    return ret;  // values are unequal
            }

            // if we get here we are either equal or one is prefix of the other
            return cmp(lArr.size(), rArr.size());
        }
        default:
            return Value2::compare(lhs, rhs, nullptr);
    }
    verify(false);
}

bool UnorderedFieldsValueComparator::evaluate(Value2::DeferredComparison deferredComparison) const {
    int cmp = compare(deferredComparison.lhs, deferredComparison.rhs);
    switch (deferredComparison.type) {
        case Value2::DeferredComparison::Type::kLT:
            return cmp < 0;
        case Value2::DeferredComparison::Type::kLTE:
            return cmp <= 0;
        case Value2::DeferredComparison::Type::kEQ:
            return cmp == 0;
        case Value2::DeferredComparison::Type::kGTE:
            return cmp >= 0;
        case Value2::DeferredComparison::Type::kGT:
            return cmp > 0;
        case Value2::DeferredComparison::Type::kNE:
            return cmp != 0;
    }

    MONGO_UNREACHABLE;
}
}