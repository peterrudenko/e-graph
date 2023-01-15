
#include <cassert>
#include "EGraph.h"


int main(int argc, char **argv)
{
    e::Graph eGraph;

    // given
    const auto a = eGraph.addTerm("a");
    const auto x = eGraph.addTerm("x");
    const auto y = eGraph.addTerm("y");
    const auto ax = eGraph.addOperation("*", {a, x});
    const auto ay = eGraph.addOperation("*", {a, y});

    // when
    eGraph.unite(x, y);
    eGraph.restoreInvariants();

    // then
    assert(eGraph.getNumClasses() == 3);
    assert(eGraph.find(x) == eGraph.find(y));
    assert(eGraph.find(ax) == eGraph.find(ay));
    assert(eGraph.find(ax) != eGraph.find(a));
}

