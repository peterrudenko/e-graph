/*
 * A simple e-graph implementation for educational purposes
 *
 * Copyright waived by Peter Rudenko <peter.rudenko@gmail.com>, 2023
 *
 * The contents of this file is free and unencumbered software released into the
 * public domain. For more information, please refer to <http://unlicense.org/>
 */

#pragma once

#include "EGraph.h"

// This e-graph serialization example uses Cereal,
// but plugging any other lib that works with plain data objects should be similar

#include "cereal/cereal.hpp"
#include "cereal/archives/portable_binary.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/string.hpp"

#include <sstream>

namespace e
{
struct GraphDTO final
{
    using Id = ClassId;

    struct Term final
    {
        Id leafId;
        Symbol name;
        Vector<Id> childrenIds;

        template <typename Archive>
        void serialize(Archive &archive)
        {
            archive(this->leafId, this->name, this->childrenIds);
        }
    };

    struct Class final
    {
        Id classId;
        Vector<Id> termIds;
        Vector<Id> parentIds;

        template <typename Archive>
        void serialize(Archive &archive)
        {
            archive(this->classId, this->termIds, this->parentIds);
        }
    };

    Vector<Id> unionFind;
    Vector<Term> terms;
    Vector<Class> classes;

    template <typename Archive>
    void serialize(Archive &archive)
    {
        archive(this->unionFind, this->terms, this->classes);
    }
};

static std::string serialize(const Graph &eGraph)
{
    GraphDTO dto;
    dto.unionFind = eGraph.unionFind.parents;

    for (const auto &[termPtr, leafId] : eGraph.termsLookup)
    {
        dto.terms.push_back({leafId, termPtr->name, termPtr->childrenIds});
    }

    for (const auto &[classId, classPtr] : eGraph.classes)
    {
        GraphDTO::Class c;
        c.classId = classId;

        for (const auto &term : classPtr->terms)
        {
            c.termIds.push_back(eGraph.termsLookup.at(term));
        }

        for (const auto &parent : classPtr->parents)
        {
            c.parentIds.push_back(eGraph.termsLookup.at(parent.term));
        }

        dto.classes.push_back(std::move(c));
    }

    std::stringstream stream;

    {
        cereal::PortableBinaryOutputArchive serializer(stream);
        serializer(dto);
    }

    return stream.str();
}

static Graph deserialize(const std::string &data)
{
    GraphDTO dto;

    {
        std::stringstream stream(data);
        cereal::PortableBinaryInputArchive deserializer(stream);
        deserializer(dto);
    }

    Graph eGraph;
    eGraph.unionFind.parents = move(dto.unionFind);

    HashMap<ClassId, Term::Ptr> termsLookup;

    for (const auto &term : dto.terms)
    {
        Term::Ptr termPtr = make<Term>(term.name, term.childrenIds);
        eGraph.termsLookup[termPtr] = term.leafId;
        termsLookup[term.leafId] = termPtr;
    }

    for (const auto &cls : dto.classes)
    {
        auto eClass = make<Class>(cls.classId);

        for (const auto &leafId : cls.termIds)
        {
            assert(termsLookup.find(leafId) != termsLookup.end());
            eClass->terms.push_back(termsLookup[leafId]);
        }

        for (const auto &leafId : cls.parentIds)
        {
            assert(termsLookup.find(leafId) != termsLookup.end());
            eClass->parents.push_back({termsLookup[leafId], leafId});
        }

        eGraph.classes[cls.classId] = move(eClass);
    }

    return eGraph;
}
} // namespace e
