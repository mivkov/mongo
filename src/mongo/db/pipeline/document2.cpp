#include "mongo/db/pipeline/document2.h"

namespace mongo {
int Document2::compare(const Document2& lhs,
                       const Document2& rhs,
                       const StringData::ComparatorInterface* stringComparator) {
    if (lhs.is_obj && rhs.is_obj) {
        return lhs._obj.woCompare(rhs._obj,
                                  BSONObj(),
                                  BSONElement::ComparisonRules::kConsiderFieldName,
                                  stringComparator);
    } else if (lhs.is_obj) {
        return Document::compare(Document(lhs._obj), rhs._doc, stringComparator);
    } else if (rhs.is_obj) {
        return Document::compare(lhs._doc, Document(rhs._obj), stringComparator);
    }
    return Document::compare(lhs._doc, rhs._doc, stringComparator);
}

void Document2::hash_combine(size_t& seed,
                             const StringData::ComparatorInterface* stringComparator) const {
    if (is_obj) {
        Document(_obj).hash_combine(seed, stringComparator);
    } else {
        _doc.hash_combine(seed, stringComparator);
    }
}
}