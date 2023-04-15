
#include "EGraph.h"

void congruenceTest()
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
    assert(eGraph.classes.size() == 3);
    assert(eGraph.find(x) == eGraph.find(y));
    assert(eGraph.find(ax) == eGraph.find(ay));
    assert(eGraph.find(ax) != eGraph.find(a));
}

void rewriteAssociativityRuleTest()
{
    e::Graph eGraph;

    // given
    const auto x = "x";
    const auto y = "y";
    const auto z = "z";

    e::RewriteRule associativityRule;
    associativityRule.leftHand = e::PatternTerm("+", {e::PatternTerm("+", {{x}, {y}}), {z}});
    associativityRule.rightHand = e::PatternTerm("+", {{x}, e::PatternTerm("+", {{y}, {z}})});

    // when
    const auto a = eGraph.addTerm("a");
    const auto b = eGraph.addTerm("b");
    const auto c = eGraph.addTerm("c");
    const auto d = eGraph.addTerm("d");

    const auto ab = eGraph.addOperation("+", {a, b});
    const auto bc = eGraph.addOperation("+", {b, c});
    const auto cd = eGraph.addOperation("+", {c, d});

    const auto ab_c = eGraph.addOperation("+", {ab, c});
    const auto a_bc = eGraph.addOperation("+", {a, bc});

    // then
    eGraph.restoreInvariants();
    assert(eGraph.find(ab_c) != eGraph.find(a_bc));

    eGraph.rewrite(associativityRule);
    assert(eGraph.find(ab_c) == eGraph.find(a_bc));

    // and when
    const auto b_cd = eGraph.addOperation("+", {b, cd});
    const auto a_b_cd = eGraph.addOperation("+", {a, b_cd});
    const auto ab_c_d = eGraph.addOperation("+", {ab_c, d});

    // then
    eGraph.rewrite(associativityRule);
    eGraph.rewrite(associativityRule); // needs one more iteration
    assert(eGraph.find(ab_c_d) == eGraph.find(a_b_cd));
}

void rewriteIdentityRuleTest()
{
    e::Graph eGraph;

    // given
    e::RewriteRule identityRule;
    identityRule.leftHand = e::PatternTerm("*", {{"x"}, {e::PatternTerm("1", {})}});
    identityRule.rightHand = "x";

    // when
    const auto a = eGraph.addTerm("a");
    const auto b = eGraph.addTerm("b");
    const auto c = eGraph.addTerm("c");
    const auto one = eGraph.addTerm("1");

    const auto ab = eGraph.addOperation("*", {a, b});
    const auto bc = eGraph.addOperation("+", {b, c});
    const auto abbc = eGraph.addOperation("*", {ab, bc});
    const auto full = eGraph.addOperation("*", {abbc, one});

    // then
    eGraph.restoreInvariants();
    assert(eGraph.find(full) != eGraph.find(abbc));

    eGraph.rewrite(identityRule);
    assert(eGraph.find(ab) != eGraph.find(a));
    assert(eGraph.find(full) != eGraph.find(one));
    assert(eGraph.find(full) == eGraph.find(abbc));
}

void rewriteZeroRuleTest()
{
    e::Graph eGraph;

    // given
    e::RewriteRule zeroRule;
    zeroRule.leftHand = e::PatternTerm("*", {{"x"}, e::PatternTerm("0", {})});
    zeroRule.rightHand = e::PatternTerm("0", {});

    // when
    const auto a = eGraph.addTerm("a");
    const auto b = eGraph.addTerm("b");
    const auto c = eGraph.addTerm("c");
    const auto zero = eGraph.addTerm("0");

    const auto ab = eGraph.addOperation("*", {a, b});
    const auto bc = eGraph.addOperation("+", {b, c});
    const auto abbc = eGraph.addOperation("*", {ab, bc});
    const auto full = eGraph.addOperation("*", {abbc, zero});

    // then
    eGraph.restoreInvariants();
    assert(eGraph.find(full) != eGraph.find(zero));

    eGraph.rewrite(zeroRule);
    assert(eGraph.find(ab) != eGraph.find(b));
    assert(eGraph.find(abbc) != eGraph.find(zero));
    assert(eGraph.find(full) == eGraph.find(zero));
}

int main(int argc, char **argv)
{
    congruenceTest();
    rewriteAssociativityRuleTest();
    rewriteIdentityRuleTest();
    rewriteZeroRuleTest();
}
