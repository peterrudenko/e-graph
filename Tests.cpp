
#include "EGraph.h"
#include "TestLanguage.h"
#include "Serialization.h"

using namespace TestLanguage;

void rewriteIdentityRuleTest()
{
    // given
    e::Graph eGraph;

    const auto abbc = makeExpression("(a * b) * (b + c)", eGraph);
    const auto id1 = makeExpression("(a * b) * ((b + c) * 1)", eGraph);
    const auto id2 = makeExpression("((a * 1) * b) * (b + (c * 1))", eGraph);
    const auto id3 = makeExpression("((a * b) * (b + c)) * 1", eGraph);
    const auto id4 = makeExpression("(((a * b) * (b + c)) * 1) * 1", eGraph);

    const auto identityRule = makeRewriteRule("$x * 1 => $x");

    // when
    eGraph.restoreInvariants();

    // then
    assert(eGraph.find(id1) != eGraph.find(abbc));

    // and when
    eGraph.rewrite(identityRule);

    // then
    assert(eGraph.find(id1) == eGraph.find(abbc));
    assert(eGraph.find(id2) == eGraph.find(abbc));
    assert(eGraph.find(id3) == eGraph.find(abbc));
    assert(eGraph.find(id4) == eGraph.find(abbc));
}

void rewriteZeroRuleTest()
{
    // given
    e::Graph eGraph;

    const auto zeroTerm = eGraph.addTerm("0");
    const auto zero1 = makeExpression("((a - b) + c) * ((b - c) * 0)", eGraph);
    const auto zero2 = makeExpression("((a * (b + c)) * d) * 0", eGraph);
    const auto zero3 = makeExpression("((a - b) * 0) * ((b + c) * 0)", eGraph);

    const auto zeroRule = makeRewriteRule("$x * 0 => 0");

    // when
    eGraph.restoreInvariants();

    // then
    assert(eGraph.find(zero1) != eGraph.find(zeroTerm));

    // and when
    eGraph.rewrite(zeroRule);
    eGraph.rewrite(zeroRule); // needs one more iteration to know that 0 * 0 == 0

    // then
    assert(eGraph.find(zero1) == eGraph.find(zeroTerm));
    assert(eGraph.find(zero2) == eGraph.find(zeroTerm));
    assert(eGraph.find(zero3) == eGraph.find(zeroTerm));
}

void rewriteAssociativityRuleTest()
{
    // given
    e::Graph eGraph;

    const auto abc1 = makeExpression("(a + b) + c", eGraph);
    const auto abc2 = makeExpression("a + (b + c)", eGraph);
    const auto abcd1 = makeExpression("a + (b + (c + d))", eGraph);
    const auto abcd2 = makeExpression("((a + b) + c) + d", eGraph);

    const auto associativityRule = makeRewriteRule("($x + $y) + $z => $x + ($y + $z)");

    // when
    eGraph.rewrite(associativityRule);

    // then
    assert(eGraph.find(abc1) == eGraph.find(abc2));
    assert(eGraph.find(abcd1) != eGraph.find(abcd2));

    // and when
    eGraph.rewrite(associativityRule); // needs one more iteration

    // then
    assert(eGraph.find(abcd1) == eGraph.find(abcd2));
    assert(eGraph.find(abc1) != eGraph.find(abcd1));
}

void rewriteDistributivityRuleTest()
{
    // given
    e::Graph eGraph;

    const auto expr1 = makeExpression("(10 + ((20 + 20) * 30)) * 40", eGraph);
    const auto expr2 = makeExpression("(10 * 40) + (((20 * 30) + (20 * 30)) * 40)", eGraph);
    const auto expr3 = makeExpression("(10 * 40) + (((20 + 20) * 30) * 40)", eGraph);

    // when
    eGraph.rewrite(makeRewriteRule("($x + $y) * $z => ($x * $z) + ($y * $z)"));

    // then
    assert(eGraph.find(expr1) == eGraph.find(expr2));
    assert(eGraph.find(expr2) == eGraph.find(expr3));
}

void serializationTest()
{
    // given
    e::Graph eGraph;

    const auto expr1 = makeExpression("(10 + ((20 + 30) + 40)) + 50", eGraph);
    const auto expr2 = makeExpression("50 + ((40 + (30 + 20)) + 10)", eGraph);

    // when
    eGraph.rewrite(makeRewriteRule("$x + $y => $y + $x"));

    const auto serializedData = e::serialize(eGraph);
    const auto otherGraph = e::deserialize(serializedData);

    // then
    assert(otherGraph.find(expr1) == otherGraph.find(expr2));
    assert(otherGraph.find(expr1) == eGraph.find(expr1));
}

int main(int argc, char **argv)
{
    rewriteIdentityRuleTest();
    rewriteZeroRuleTest();
    rewriteAssociativityRuleTest();
    rewriteDistributivityRuleTest();
    serializationTest();
}
