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

#include "mongo/platform/basic.h"

#include "mongo/db/matcher/schema/expression_internal_schema_allowed_properties.h"

namespace mongo {
constexpr StringData InternalSchemaAllowedPropertiesMatchExpression::kName;

InternalSchemaAllowedPropertiesMatchExpression::InternalSchemaAllowedPropertiesMatchExpression(
    StringDataSet properties,
    StringData namePlaceholder,
    std::vector<PatternSchema> patternProperties,
    std::unique_ptr<ExpressionWithPlaceholder> otherwise)
    : MatchExpression(MatchExpression::INTERNAL_SCHEMA_ALLOWED_PROPERTIES),
      _properties(std::move(properties)),
      _namePlaceholder(namePlaceholder),
      _patternProperties(std::move(patternProperties)),
      _otherwise(std::move(otherwise)) {

    for (auto&& constraint : _patternProperties) {
        const auto& errorStr = constraint.first.regex->error();
        uassert(ErrorCodes::BadValue,
                str::stream() << "Invalid regular expression: " << errorStr,
                errorStr.empty());
    }
}

void InternalSchemaAllowedPropertiesMatchExpression::debugString(StringBuilder& debug,
                                                                 int indentationLevel) const {
    _debugAddSpace(debug, indentationLevel);

    BSONObjBuilder builder;
    serialize(&builder);
    debug << builder.obj().toString() << "\n";

    const auto* tag = getTag();
    if (tag) {
        debug << " ";
        tag->debugString(&debug);
    }

    debug << "\n";
}

bool InternalSchemaAllowedPropertiesMatchExpression::equivalent(const MatchExpression* expr) const {
    if (matchType() != expr->matchType()) {
        return false;
    }

    const auto* other = static_cast<const InternalSchemaAllowedPropertiesMatchExpression*>(expr);
    return _properties == other->_properties && _namePlaceholder == other->_namePlaceholder &&
        _otherwise->equivalent(other->_otherwise.get()) &&
        std::is_permutation(_patternProperties.begin(),
                            _patternProperties.end(),
                            other->_patternProperties.begin(),
                            other->_patternProperties.end(),
                            [](const auto& expr1, const auto& expr2) {
                                return expr1.first.rawRegex == expr2.first.rawRegex &&
                                    expr1.second->equivalent(expr2.second.get());
                            });
}

bool InternalSchemaAllowedPropertiesMatchExpression::matches(const MatchableDocument* doc,
                                                             MatchDetails* details) const {
    return _matchesDocument(doc->toDocument());
}

bool InternalSchemaAllowedPropertiesMatchExpression::matchesSingleValue(const Value2& elem,
                                                                        MatchDetails*) const {
    if (elem.getType() != BSONType::Object) {
        return false;
    }

    return _matchesDocument(elem.getDocument());
}

bool InternalSchemaAllowedPropertiesMatchExpression::_matchesDocument(const Document2& obj) const {
    Field2Iterator i(obj);
    while (i.more()) {
        auto pair = i.next();
        std::string fst(pair.first);
        pcrecpp::StringPiece data(fst);
        bool checkOtherwise = true;
        for (auto&& constraint : _patternProperties) {
            if (constraint.first.regex->PartialMatch(data)) {
                checkOtherwise = false;
                if (!constraint.second->matchesValue(pair.second)) {
                    return false;
                }
            }
        }

        if (checkOtherwise && _properties.find(pair.first) != _properties.end()) {
            checkOtherwise = false;
        }

        if (checkOtherwise && !_otherwise->matchesValue(pair.second)) {
            return false;
        }
    }
    return true;
}

void InternalSchemaAllowedPropertiesMatchExpression::serialize(BSONObjBuilder* builder) const {
    BSONObjBuilder expressionBuilder(
        builder->subobjStart(InternalSchemaAllowedPropertiesMatchExpression::kName));

    std::vector<StringData> sortedProperties(_properties.begin(), _properties.end());
    std::sort(sortedProperties.begin(), sortedProperties.end());
    expressionBuilder.append("properties", sortedProperties);

    expressionBuilder.append("namePlaceholder", _namePlaceholder);

    BSONArrayBuilder patternPropertiesBuilder(expressionBuilder.subarrayStart("patternProperties"));
    for (auto&& item : _patternProperties) {
        BSONObjBuilder itemBuilder(patternPropertiesBuilder.subobjStart());
        itemBuilder.appendRegex("regex", item.first.rawRegex);

        BSONObjBuilder subexpressionBuilder(itemBuilder.subobjStart("expression"));
        item.second->getFilter()->serialize(&subexpressionBuilder);
        subexpressionBuilder.doneFast();
    }
    patternPropertiesBuilder.doneFast();

    BSONObjBuilder otherwiseBuilder(expressionBuilder.subobjStart("otherwise"));
    _otherwise->getFilter()->serialize(&otherwiseBuilder);
    otherwiseBuilder.doneFast();
    expressionBuilder.doneFast();
}

std::unique_ptr<MatchExpression> InternalSchemaAllowedPropertiesMatchExpression::shallowClone()
    const {
    std::vector<PatternSchema> clonedPatternProperties;
    clonedPatternProperties.reserve(_patternProperties.size());
    for (auto&& constraint : _patternProperties) {
        clonedPatternProperties.emplace_back(Pattern(constraint.first.rawRegex),
                                             constraint.second->shallowClone());
    }

    auto clone = std::make_unique<InternalSchemaAllowedPropertiesMatchExpression>(
        _properties,
        _namePlaceholder,
        std::move(clonedPatternProperties),
        _otherwise->shallowClone());
    return {std::move(clone)};
}

MatchExpression::ExpressionOptimizerFunc
InternalSchemaAllowedPropertiesMatchExpression::getOptimizer() const {
    return [](std::unique_ptr<MatchExpression> expression) {
        auto& allowedPropertiesExpr =
            static_cast<InternalSchemaAllowedPropertiesMatchExpression&>(*expression);

        for (auto& property : allowedPropertiesExpr._patternProperties) {
            property.second->optimizeFilter();
        }
        allowedPropertiesExpr._otherwise->optimizeFilter();

        return expression;
    };
}
}  // namespace mongo
