#pragma once

#include "mongo/db/pipeline/value_comparator2.h"

namespace mongo {
class UnorderedFieldsValueComparator : public Value2Comparator {
public:
    UnorderedFieldsValueComparator() = default;

    int compare(const Value2& lhs, const Value2& rhs) const;
    bool evaluate(Value2::DeferredComparison deferredComparison) const;

    class LessThan {
    public:
        explicit LessThan(const UnorderedFieldsValueComparator* comparator)
            : _comparator(comparator) {}

        bool operator()(const Value2& lhs, const Value2& rhs) const {
            return _comparator->compare(lhs, rhs) < 0;
        }

    private:
        const UnorderedFieldsValueComparator* _comparator;
    };

    std::set<Value2, LessThan> makeSet() const {
        return std::set<Value2, LessThan>(LessThan(this));
    }
};
}