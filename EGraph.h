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

template <typename T>
using UniquePointer = std::unique_ptr<T>;

template <typename T>
using SharedPointer = std::shared_ptr<T>;

template <typename T, typename... Args>
inline UniquePointer<T> make(Args &&...args)
{
    return UniquePointer<T>(new T(std::forward<Args>(args)...));
}

template <typename T>
using Vector = std::vector<T>;

template <typename T>
void appendVector(Vector<T> &v1, const Vector<T> &v2)
{
    v1.insert(v1.begin(), v2.begin(), v2.end());
}

template <typename T1, typename T2>
using Variant = std::variant<T1, T2>;

template <typename T>
using Optional = std::optional<T>;
using std::nullopt;

// In future this should probably be replaced by some more performant
// or cache-friendly unordered_map-compatible hash map implementation
template <typename K, typename V, typename H = std::hash<K>, typename E = std::equal_to<K>>
using HashMap = std::unordered_map<K, V, H, E>;

using ClassId = int32_t;

// Symbols are used to name terms, and for now they are just strings,
// but in future it could be refactored to use a symbol pool instead,
// e.g. Identifier class in JUCE, where the construction is slower,
// but all comparison operators are as simple as comparing the pointers
using Symbol = std::string;

//------------------------------------------------------------------------------
// Disjoint-set forest a.k.a. union-find

template <typename Id>
class UnionFind final
{
public:

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

private:

    Vector<Id> parents;
};

//------------------------------------------------------------------------------
// E-node, how they call it the paper

class Term final
{
public:

    explicit Term(const Symbol &name, const Vector<ClassId> &children = {}) :
        name(name), childrenIds(children) {}

    using Ptr = SharedPointer<Term>;

    struct Hash final
    {
        auto operator()(const Term::Ptr &x) const
        {
            return std::hash<Symbol>()(x->getName());
        }
    };

    friend bool operator ==(const Term::Ptr &l, const Term::Ptr &r)
    {
        return l.get() == r.get() ||
               (l->name == r->name && l->childrenIds == r->childrenIds);
    }

    const Symbol &getName() const noexcept
    {
        return this->name;
    }

    const auto &getChildrenIds() const
    {
        return this->childrenIds;
    }

    template <typename UF>
    void restoreInvariants(UF &unionFind)
    {
        for (auto &id : this->childrenIds)
        {
            id = unionFind.find(id);
        }
    }

private:

    const Symbol name;

    // One of the key tricks here is that terms, a.k.a. e-nodes,
    // are connected to equivalence classes, not other terms:
    Vector<ClassId> childrenIds;
};

// The term id here is the leaf id, while the canonical class id is the root id:
struct TermWithLeafId final
{
    Term::Ptr term;
    ClassId termId;
};

//------------------------------------------------------------------------------
// Equivalence class

class Class final
{
public:

    Class(ClassId id, Term::Ptr term) :
        id(id), terms({term}) {}

    ClassId getId() const noexcept
    {
        return this->id;
    }

    const auto &getTerms() const noexcept
    {
        return this->terms;
    }

    const auto &getParents() const noexcept
    {
        return this->parents;
    }

    void addParent(Term::Ptr term, ClassId parentClassId)
    {
        this->parents.push_back({term, parentClassId});
    }

    void uniteWith(const Class *other)
    {
        assert(other != this);
        appendVector(this->terms, other->terms);
        appendVector(this->parents, other->parents);
    }

    template <typename UF>
    void restoreInvariants(UF &unionFind)
    {
        for (auto &term : this->terms)
        {
            term->restoreInvariants(unionFind);
        }

        // deduplicate
        std::sort(this->terms.begin(), this->terms.end());
        this->terms.erase(std::unique(this->terms.begin(), this->terms.end()), this->terms.end());

        // todo test:
        // using the shared pointer's pointer-wise equality and comparison
        // operators here for sorting and dedup, but it should be fine,
        // since terms are checked when adding (see the Graph's termsCache),
        // so we shouldn't have duplicate terms, only duplicate shared pointers

        // fixme do we also need to deduplicate parents here?
    }

private:

    const ClassId id;

    Vector<Term::Ptr> terms;

    Vector<TermWithLeafId> parents;
};

//------------------------------------------------------------------------------
// E-matching and rewriting stuff

struct PatternTerm;

// match against symbols (a.k.a. pattern variables) for algebraic rewriting,
// match against pattern terms for rewriting concrete named operations/terms,
// e.g. the identity rule for a specific operation would look like:
// <Symbol x> <PatternTerm op> <PatternTerm identity> -> <Symbol x>
// and the zero rule would look like:
// <Symbol x> <PatternTerm op> <PatternTerm zero> -> <PatternTerm zero>
using Pattern = Variant<Symbol, PatternTerm>;

struct PatternTerm final
{
    Symbol name;
    Vector<Pattern> arguments;
};

struct SymbolBindings final
{
    using Ptr = SharedPointer<SymbolBindings>;

    Optional<ClassId> find(const Symbol &symbol)
    {
        const auto result = this->bindings.find(symbol);
        if (result == this->bindings.end())
        {
            return nullopt;
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

class Graph final
{
public:

    ClassId find(ClassId classId) const noexcept
    {
        return this->unionFind.find(classId);
    }

    const auto &getTerms() const noexcept
    {
        return this->termsCache;
    }

    const auto &getClasses() const noexcept
    {
        return this->classes;
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
        auto rootId1 = this->unionFind.find(termId1);
        auto rootId2 = this->unionFind.find(termId2);
        if (rootId1 == rootId2)
        {
            return false;
        }

        // make sure the new root has more parents
        if (this->classes[rootId1]->getParents().size() <
            this->classes[rootId2]->getParents().size())
        {
            std::swap(rootId1, rootId2);
        }

        this->unionFind.unite(rootId1, rootId2);

        auto *class1 = this->classes[rootId1].get();
        const auto *class2 = this->classes[rootId2].get();

        assert(rootId1 == class1->getId());
        assert(rootId2 == class2->getId());

        appendVector(this->dirtyTerms, class2->getParents());

        class1->uniteWith(class2);

        this->classes.erase(rootId2);

        return true;
    }

    void restoreInvariants()
    {
        // rebuild unions

        while (!this->dirtyTerms.empty())
        {
            const auto updated = this->dirtyTerms.back();
            this->dirtyTerms.pop_back();

            updated.term->restoreInvariants(this->unionFind);

            const auto cachedTerm = this->termsCache.find(updated.term);
            if (cachedTerm != this->termsCache.end())
            {
                const auto cachedTermId = cachedTerm->second;
                this->unite(cachedTermId, updated.termId);
                this->termsCache[updated.term] = updated.termId;
            }
            else
            {
                this->termsCache.insert({updated.term, updated.termId});
            }
        }

        // rebuild equivalence classes

        for (auto &[classId, classPtr] : this->classes)
        {
            // update root class ids for all terms, sort them and deduplicate
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
    }

    Vector<SymbolBindings::Ptr> matchPattern(const Pattern &pattern,
        ClassId classId, SymbolBindings::Ptr bindings)
    {
        if (const auto *patternVariable = std::get_if<Symbol>(&pattern))
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

private:

    Vector<SymbolBindings::Ptr> matchVariable(const Symbol &symbol,
        ClassId classId, SymbolBindings::Ptr bindings)
    {
        Vector<SymbolBindings::Ptr> result;
        const auto rootId = this->unionFind.find(classId);
        if (const auto matchedClassId = bindings->find(symbol))
        {
            if (this->unionFind.find(matchedClassId.value()) == rootId)
            {
                result.push_back(bindings);
            }
        }
        else
        {
            bindings->add(symbol, rootId);
            result.push_back(bindings);
        }

        return result;
    }

    Vector<SymbolBindings::Ptr> matchTerm(const PatternTerm &patternTerm,
        ClassId classId, SymbolBindings::Ptr bindings)
    {
        const auto rootId = this->unionFind.find(classId);
        assert(this->classes.find(rootId) != this->classes.end());

        Vector<SymbolBindings::Ptr> result;
        for (const auto &term : this->classes.at(rootId)->getTerms())
        {
            if (term->getName() != patternTerm.name ||
                term->getChildrenIds().size() != patternTerm.arguments.size())
            {
                continue;
            }

            for (const auto &subBinding :
                this->matchMany(patternTerm.arguments, term->getChildrenIds(), bindings))
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
        if (const auto *subVariable = std::get_if<Symbol>(&pattern))
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

    ClassId instantiateVariable(const Symbol &symbol, SymbolBindings::Ptr bindings)
    {
        const auto result = bindings->find(symbol);
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

private:

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

            for (const auto &childClassId : term->getChildrenIds())
            {
                const auto rootChildClassId = this->unionFind.find(childClassId);
                assert(this->classes.find(rootChildClassId) != this->classes.end());
                this->classes[rootChildClassId]->addParent(term, newId);
            }

            this->classes[newId] = std::move(newClass);
            this->termsCache.insert({term, newId});
            this->dirtyTerms.push_back({term, newId});

            return newId;
        }
    }

    Optional<ClassId> lookup(Term::Ptr term) const
    {
        const auto existingTerm = this->termsCache.find(term);
        if (existingTerm != this->termsCache.end())
        {
            return existingTerm->second;
        }

        return nullopt;
    }

private:

    UnionFind<ClassId> unionFind;

    HashMap<ClassId, UniquePointer<Class>> classes;

    HashMap<Term::Ptr, ClassId, Term::Hash> termsCache;

    Vector<TermWithLeafId> dirtyTerms;
};
} // namespace e
