#pragma once

#include <memory>
#include <utility>
#include <vector>
#include <functional>
#include <optional>

#pragma clang diagnostic push
#pragma ide diagnostic ignored "performance-unnecessary-value-param"
#pragma ide diagnostic ignored "modernize-use-nodiscard"

namespace e
{

//------------------------------------------------------------------------------
// Shortcuts for readability

template <typename T>
using UniquePointer = std::unique_ptr<T>;

template <typename T>
using SharedPointer = std::shared_ptr<T>;

template <typename T, typename... Args>
inline UniquePointer<T> make(Args &&...args)
{
    return UniquePointer<T>(new T(std::forward<Args>(args)...));
}

using std::move;
using std::sort;
using std::unique;

template <typename T>
using Vector = std::vector<T>;

template <typename T>
void appendVector(Vector<T> &v1, const Vector<T> &v2)
{
    v1.insert(v1.begin(), v2.begin(), v2.end());
}

template <typename T>
using Function = std::function<T>;

template <typename T>
using Optional = std::optional<T>;
using std::nullopt;

// In future this should probably be replaced by some more performant
// or cache-friendly unordered_map-compatible hash map implementation
template <typename K, typename V, typename H = std::hash<K>, typename E = std::equal_to<K>>
using HashMap = std::unordered_map<K, V, H, E>;

using String = std::string;

using ClassId = int32_t;

//------------------------------------------------------------------------------
// A simple disjoint-set forest implementation with optional path compression

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

    auto getSize() const noexcept
    {
        return this->parents.size();
    }

private:

    Vector<Id> parents;
};

//------------------------------------------------------------------------------
// E-node, how they call it the paper

class Term
{
public:

    Term() = default;
    virtual ~Term() = default;

    using Ptr = SharedPointer<Term>;

    struct Hash final
    {
        auto operator()(const Term::Ptr &x) const
        {
            return std::hash<String>()(x->getName());
        }
    };

    struct Equality final
    {
        auto operator()(const Term::Ptr &l, const Term::Ptr &r) const
        {
            return l.get() == r.get() ||
                   (l->getName() == r->getName() && l->getChildrenIds() == r->getChildrenIds());
        }
    };

    static Term::Ptr makeTerm(const String &name)
    {
        auto term = make<Term>();
        term->name = name;
        return term;
    }

    static Term::Ptr makeOperation(const String &name, const Vector<ClassId> &children)
    {
        auto term = make<Term>();
        term->name = name;
        term->childrenIds = children;
        return term;
    }

    const String &getName() const noexcept
    {
        return this->name;
    }

    virtual const Vector<ClassId> &getChildrenIds() const
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

    String name;

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

    ClassId getId() const noexcept { return this->id; }

    bool isEmpty() const noexcept { return this->terms.empty(); }

    auto getNumParents() const noexcept { return this->parents.size(); }

    const Vector<TermWithLeafId> &getParents() const noexcept { return this->parents; }

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
        sort(this->terms.begin(), this->terms.end());
        this->terms.erase(std::unique(this->terms.begin(), this->terms.end()), this->terms.end());

        // todo test:
        // using the shared pointer's pointer-wise equality and comparison
        // operators here for sorting and dedup, but it should be fine,
        // since terms are checked when adding (see the Graph's termsCache),
        // so we shouldn't have duplicate terms, only duplicate shared pointers

        // fixme do we also need to deduplicate parents here?
    }

private:

    ClassId id;

    Vector<Term::Ptr> terms;

    Vector<TermWithLeafId> parents;
};

//------------------------------------------------------------------------------
// E-graph

class Graph final
{
public:

    bool find(ClassId classId) const noexcept
    {
        return this->unionFind.find(classId);
    }

    auto getNumClasses() const noexcept
    {
        return this->classes.size();
    }

    ClassId addTerm(const String &name)
    {
        return this->add(Term::makeTerm(name));
    }

    ClassId addOperation(const String &name, const Vector<ClassId> &children)
    {
        return this->add(Term::makeOperation(name, children));
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
        if (this->classes[rootId1]->getNumParents() <
            this->classes[rootId2]->getNumParents())
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
                assert(this->classes.find(childClassId) != this->classes.end());
                this->classes[childClassId]->addParent(term, newId);
            }

            this->classes[newId] = move(newClass);
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

    HashMap<Term::Ptr, ClassId, Term::Hash, Term::Equality> termsCache;

    Vector<TermWithLeafId> dirtyTerms;
};
} // namespace e

#pragma clang diagnostic pop
