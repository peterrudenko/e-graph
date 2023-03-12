/*
 * A simple e-graph implementation for educational purposes
 *
 * Copyright waived by Peter Rudenko <peter.rudenko@gmail.com>, 2023
 *
 * The contents of this file is free and unencumbered software released into the
 * public domain. For more information, please refer to <http://unlicense.org/>
 */

#pragma once

#include <cassert>
#include <memory>
#include <vector>
#include <optional>
#include <variant>
#include <unordered_map>
#include <algorithm>

namespace e
{

//------------------------------------------------------------------------------
// Shortcuts and helpers

using ClassId = int32_t;

// Symbols are used to name terms, and for now they are just strings,
// but in future it could be refactored to use a symbol pool instead,
// so that comparing them would be as fast as comparing pointers
using Symbol = std::string;

// In future this should probably be replaced by some more performant
// or cache-friendly unordered_map-compatible hash map implementation
template <typename K, typename V, typename H = std::hash<K>>
using HashMap = std::unordered_map<K, V, H>;

template <typename T>
using SharedPointer = std::shared_ptr<T>;

template <typename T>
using UniquePointer = std::unique_ptr<T>;

template <typename T, typename... Args>
inline auto make(Args &&...args)
{
    return UniquePointer<T>(new T(std::forward<Args>(args)...));
}

template <typename T>
using Optional = std::optional<T>;

template <typename T1, typename T2>
using Variant = std::variant<T1, T2>;

template <typename T>
using Vector = std::vector<T>;

template <typename T>
inline void append(Vector<T> &v1, const Vector<T> &v2)
{
    v1.insert(v1.end(), v2.begin(), v2.end());
}

//------------------------------------------------------------------------------
// Disjoint-set forest a.k.a. union-find

template <typename Id>
struct UnionFind final
{
    Id addSet()
    {
        const auto id = this->parents.size();
        this->parents.push_back(id);
        return id;
    }

    Id find(Id id) const
    {
        while (id != this->parents[id])
        {
            id = this->parents[id];
        }

        return id;
    }

    Id find(Id id)
    {
        // The non-const find method also does path compression
        while (id != this->parents[id])
        {
            const auto grandparent = this->parents[this->parents[id]];
            this->parents[id] = grandparent;
            id = grandparent;
        }

        return id;
    }

    Id unite(Id root1, Id root2)
    {
        this->parents[root2] = root1;
        return root1;
    }

    Vector<Id> parents;
};

//------------------------------------------------------------------------------
// E-node, a term of some language

struct Term final
{
    explicit Term(const Symbol &name, const Vector<ClassId> &children = {}) :
        name(name), childrenIds(children) {}

    using Ptr = SharedPointer<Term>;

    struct Hash final
    {
        auto operator()(const Term::Ptr &x) const
        {
            return std::hash<Symbol>()(x->name);
        }
    };

    friend bool operator==(const Term::Ptr &l, const Term::Ptr &r)
    {
        return l.get() == r.get() ||
               (l->name == r->name && l->childrenIds == r->childrenIds);
    }

    template <typename UF>
    void restoreInvariants(UF &unionFind)
    {
        for (auto &id : this->childrenIds)
        {
            id = unionFind.find(id);
        }
    }

    const Symbol name;

    // One of the key tricks here is that terms, a.k.a. e-nodes,
    // are connected to equivalence classes, not other terms:
    Vector<ClassId> childrenIds;
};

//------------------------------------------------------------------------------
// Equivalence class

struct Class final
{
    explicit Class(ClassId id) :
        id(id) {}

    Class(ClassId id, Term::Ptr term) :
        id(id), terms({term}) {}

    void addParent(Term::Ptr term)
    {
        this->parents.push_back(term);
    }

    void uniteWith(const Class *other)
    {
        assert(other != this);
        append(this->terms, other->terms);
        append(this->parents, other->parents);
    }

    template <typename UF>
    void restoreInvariants(UF &unionFind)
    {
        for (auto &term : this->terms)
        {
            term->restoreInvariants(unionFind);
        }

        // Deduplicate using the shared pointer's pointer-wise equality and comparison operators
        // for sorting, but it should be fine, since duplicate terms are checked on adding,
        // so we shouldn't have duplicate terms here at all, only duplicate shared pointers

        std::sort(this->terms.begin(), this->terms.end());
        this->terms.erase(std::unique(this->terms.begin(), this->terms.end()), this->terms.end());

        std::sort(this->parents.begin(), this->parents.end());
        this->parents.erase(std::unique(this->parents.begin(), this->parents.end()), this->parents.end());
    }

    const ClassId id;

    Vector<Term::Ptr> terms;

    Vector<Term::Ptr> parents;
};

//------------------------------------------------------------------------------
// E-matching and rewriting stuff

struct PatternTerm;
using PatternVariable = Symbol;

// Match against pattern variables (or just symbols) for algebraic rewriting,
// match against pattern terms for rewriting concrete named operations/terms,
// e.g. the identity rule for a specific operation would look like:
// <Symbol x> <PatternTerm op> <PatternTerm identity> -> <Symbol x>
// and the zero rule would look like:
// <Symbol x> <PatternTerm op> <PatternTerm zero> -> <PatternTerm zero>
using Pattern = Variant<PatternVariable, PatternTerm>;

struct PatternTerm final
{
    Symbol name;
    Vector<Pattern> arguments;
};

struct SymbolBindings final
{
    using Ptr = SharedPointer<SymbolBindings>;

    SymbolBindings() = default;
    explicit SymbolBindings(SymbolBindings::Ptr &other) :
        bindings(other->bindings) {}

    Optional<ClassId> find(const Symbol &symbol)
    {
        const auto result = this->bindings.find(symbol);
        if (result == this->bindings.end())
        {
            return {};
        }

        return result->second;
    }

    void add(const Symbol &symbol, ClassId classId)
    {
        this->bindings[symbol] = classId;
    }

    HashMap<Symbol, ClassId> bindings;
};

struct RewriteRule final
{
    Pattern leftHand;
    Pattern rightHand;
};

struct Match final
{
    ClassId id1;
    ClassId id2;
};

Pattern makePatternTerm(const Symbol &name, const Vector<Pattern> &arguments = {})
{
    PatternTerm term;
    term.name = name;
    term.arguments = arguments;
    return Pattern(term);
};

//------------------------------------------------------------------------------
// E-graph

struct Graph final
{
    ClassId find(ClassId classId) const noexcept
    {
        return this->unionFind.find(classId);
    }

    ClassId addTerm(const Symbol &name)
    {
        return this->add(make<Term>(name));
    }

    ClassId addOperation(const Symbol &name, const Vector<ClassId> &children)
    {
        return this->add(make<Term>(name, children));
    }

    bool unite(ClassId termId1, ClassId termId2)
    {
        const auto rootId1 = this->unionFind.find(termId1);
        const auto rootId2 = this->unionFind.find(termId2);
        if (rootId1 == rootId2)
        {
            return false;
        }

        this->unionFind.unite(rootId1, rootId2);

        auto *class1 = this->classes[rootId1].get();
        const auto *class2 = this->classes[rootId2].get();

        assert(rootId1 == class1->id);
        assert(rootId2 == class2->id);

        append(this->dirtyTerms, class2->parents);

        class1->uniteWith(class2);

        this->classes.erase(rootId2);

        return true;
    }

    void restoreInvariants()
    {
        // Rebuild unions

        while (!this->dirtyTerms.empty())
        {
            const auto updatedTerm = this->dirtyTerms.back();
            this->dirtyTerms.pop_back();

            const auto foundCachedTerm = this->termsLookup.find(updatedTerm);
            assert(foundCachedTerm != this->termsLookup.end());

            const auto updatedLeafId = foundCachedTerm->second;
            this->termsLookup.erase(foundCachedTerm);

            updatedTerm->restoreInvariants(this->unionFind);

            // Here the updatedTerm's children ids might have changed,
            // so cache may contain a duplicate term, but with different
            // leaf id, and if it does, make sure to unite those leaf ids,
            // and if it doesn't, just put the term back in the cache

            const auto foundOtherTerm = this->termsLookup.find(updatedTerm);
            if (foundOtherTerm != this->termsLookup.end())
            {
                const auto otherLeafId = foundOtherTerm->second;
                this->unite(otherLeafId, updatedLeafId);
            }
            else
            {
                this->termsLookup.insert({updatedTerm, updatedLeafId});
            }
        }

        // Rebuild equivalence classes

        for (auto &[classId, classPtr] : this->classes)
        {
            classPtr->restoreInvariants(this->unionFind);
        }
    }

    void rewrite(const RewriteRule &rewriteRule)
    {
        Vector<Match> matches;

        for (const auto &[classId, classPtr] : this->classes)
        {
            SymbolBindings::Ptr emptyBindings = make<SymbolBindings>();
            const auto matchResult = this->matchPattern(rewriteRule.leftHand, classId, emptyBindings);
            for (const auto &bindings : matchResult)
            {
                matches.push_back({this->instantiatePattern(rewriteRule.leftHand, bindings),
                    this->instantiatePattern(rewriteRule.rightHand, bindings)});
            }
        }

        for (const auto &match : matches)
        {
            this->unite(match.id1, match.id2);
        }

        this->restoreInvariants();
    }

    Vector<SymbolBindings::Ptr> matchPattern(const Pattern &pattern,
        ClassId classId, SymbolBindings::Ptr bindings)
    {
        if (const auto *patternVariable = std::get_if<PatternVariable>(&pattern))
        {
            return this->matchVariable(*patternVariable, classId, bindings);
        }
        else if (const auto *patternTerm = std::get_if<PatternTerm>(&pattern))
        {
            return this->matchTerm(*patternTerm, classId, bindings);
        }

        assert(false);
        return {};
    }

    Vector<SymbolBindings::Ptr> matchVariable(const PatternVariable &variable,
        ClassId classId, SymbolBindings::Ptr bindings)
    {
        Vector<SymbolBindings::Ptr> result;
        const auto rootId = this->unionFind.find(classId);
        if (const auto matchedClassId = bindings->find(variable))
        {
            if (this->unionFind.find(matchedClassId.value()) == rootId)
            {
                result.push_back(bindings);
            }
        }
        else
        {
            SymbolBindings::Ptr newBindings = make<SymbolBindings>(bindings);
            newBindings->add(variable, rootId);
            result.push_back(newBindings);
        }

        return result;
    }

    Vector<SymbolBindings::Ptr> matchTerm(const PatternTerm &patternTerm,
        ClassId classId, SymbolBindings::Ptr bindings)
    {
        const auto rootId = this->unionFind.find(classId);
        assert(this->classes.find(rootId) != this->classes.end());

        Vector<SymbolBindings::Ptr> result;
        for (const auto &term : this->classes.at(rootId)->terms)
        {
            if (term->name != patternTerm.name ||
                term->childrenIds.size() != patternTerm.arguments.size())
            {
                continue;
            }

            for (const auto &subBinding :
                this->matchMany(patternTerm.arguments, term->childrenIds, bindings))
            {
                result.push_back(subBinding);
            }
        }

        return result;
    }

    Vector<SymbolBindings::Ptr> matchMany(const Vector<Pattern> &patterns,
        const Vector<ClassId> &classIds, SymbolBindings::Ptr bindings)
    {
        if (patterns.empty())
        {
            return {bindings};
        }

        Vector<SymbolBindings::Ptr> result;
        for (const auto &subBinding1 : this->matchPattern(patterns.front(), classIds.front(), bindings))
        {
            const Vector<Pattern> subPatterns(patterns.begin() + 1, patterns.end());
            const Vector<ClassId> subClasses(classIds.begin() + 1, classIds.end());
            for (const auto &subBinding2 : this->matchMany(subPatterns, subClasses, subBinding1))
            {
                result.push_back(subBinding2);
            }
        }

        return result;
    }

    ClassId instantiatePattern(const Pattern &pattern, SymbolBindings::Ptr bindings)
    {
        if (const auto *subVariable = std::get_if<PatternVariable>(&pattern))
        {
            return this->instantiateVariable(*subVariable, bindings);
        }
        else if (const auto *subTerm = std::get_if<PatternTerm>(&pattern))
        {
            return this->instantiateOperation(*subTerm, bindings);
        }

        assert(false);
        return -1;
    }

    ClassId instantiateVariable(const PatternVariable &variable, SymbolBindings::Ptr bindings)
    {
        const auto result = bindings->find(variable);
        assert(result);
        return result.value();
    }

    ClassId instantiateOperation(const PatternTerm &patternTerm, SymbolBindings::Ptr bindings)
    {
        Vector<ClassId> children;

        for (const auto &pattern : patternTerm.arguments)
        {
            children.push_back(this->instantiatePattern(pattern, bindings));
        }

        return this->add(make<Term>(patternTerm.name, children));
    }

    ClassId add(Term::Ptr term)
    {
        if (auto existingClassId = this->lookup(term))
        {
            return existingClassId.value();
        }
        else
        {
            const auto newId = this->unionFind.addSet();
            auto newClass = make<Class>(newId, term);

            for (const auto &childClassId : term->childrenIds)
            {
                const auto rootChildClassId = this->unionFind.find(childClassId);
                assert(this->classes.find(rootChildClassId) != this->classes.end());
                this->classes[rootChildClassId]->addParent(term);
            }

            this->classes[newId] = std::move(newClass);
            this->termsLookup.insert({term, newId});
            this->dirtyTerms.push_back(term);

            return newId;
        }
    }

    Optional<ClassId> lookup(Term::Ptr term) const
    {
        const auto existingTerm = this->termsLookup.find(term);
        if (existingTerm != this->termsLookup.end())
        {
            return existingTerm->second;
        }

        return {};
    }

    UnionFind<ClassId> unionFind;

    // Classes are identified by canonical class ids (root ids)
    HashMap<ClassId, UniquePointer<Class>> classes;

    // Contains terms and their own uncanonicalized ids (leaf ids)
    HashMap<Term::Ptr, ClassId, Term::Hash> termsLookup;

    Vector<Term::Ptr> dirtyTerms;
};
} // namespace e
