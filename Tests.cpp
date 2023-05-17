
#include "EGraph.h"
#include "Serialization.h"

#include "tao/pegtl.hpp"
#include "tao/pegtl/contrib/parse_tree.hpp"

// A simple language for tests which supports expressions like "(a + b) + c"
// and rewrite rules like "$x * ($y * $z) => ($x * $y) * $z" or "$x * 0 => 0"
namespace TestLanguage
{
    namespace Ast
    {
        using namespace tao::pegtl;
        using namespace tao::pegtl::parse_tree;

        using Symbol = plus<ascii::ranges<'a', 'z', 'A', 'Z', '0', '9'>>;
        struct Term: Symbol {};
        struct PatternVariable: seq<one<'$'>, Symbol> {};
        struct Operation: sor<one<'-'>, one<'+'>, one<'*'>, one<'/'>> {};
        struct Arrow: string<'=', '>'> {};
        struct Spacing: star<space> {};

        struct Expression;
        struct BracketExpression: if_must<one<'('>, Spacing, Expression, Spacing, one<')'>> {};
        struct Value: sor<PatternVariable, Term, BracketExpression> {};

        template <typename S, typename O>
        struct LeftAssociative: seq<S, Spacing, star_must<O, Spacing, S, Spacing>> {};

        struct Expression: LeftAssociative<Value, Operation> {};
        struct RewriteRuleOrExpression: LeftAssociative<Expression, Arrow> {};

        struct Grammar: must<Spacing, RewriteRuleOrExpression, eof> {};

        template <typename Rule>
        struct Selector: selector<Rule,
            store_content::on<Term, PatternVariable, Operation>,
            fold_one::on<Value, Expression>,
            discard_empty::on<Arrow>> {};

        struct Node: basic_node<Node> {};
    }

    using namespace e;

    Pattern makePattern(const Ast::Node &astNode)
    {
        if (astNode.is_root())
        {
            assert(!astNode.children.empty());
            return makePattern(*astNode.children.front());
        }
        else if (astNode.is_type<Ast::Expression>())
        {
            PatternTerm term;

            assert(astNode.children.size() == 3);
            for (auto &childNode : astNode.children)
            {
                if (childNode->is_type<Ast::Operation>())
                {
                    term.name = childNode->string();
                }
                else
                {
                    term.arguments.push_back(makePattern(*childNode));
                }
            }

            return term;
        }
        else if (astNode.is_type<Ast::Term>())
        {
            PatternTerm term;
            term.name = astNode.string();
            return term;
        }
        else if (astNode.is_type<Ast::PatternVariable>())
        {
            PatternVariable variable = astNode.string();
            return variable;
        }

        assert(false);
    }

    RewriteRule makeRewriteRule(const Ast::Node &astNode)
    {
        assert(astNode.is_root());
        assert(astNode.children.size() == 2);
        RewriteRule rule;
        rule.leftHand = makePattern(*astNode.children.front());
        rule.rightHand = makePattern(*astNode.children.back());
        return rule;
    }

    ClassId makeExpression(const Ast::Node &astNode, Graph &eGraph)
    {
        if (astNode.is_root())
        {
            assert(!astNode.children.empty());
            return makeExpression(*astNode.children.front(), eGraph);
        }
        else if (astNode.is_type<Ast::Expression>())
        {
            Symbol name;
            Vector<ClassId> childrenIds;

            assert(astNode.children.size() == 3); // only binary operations are supported
            for (auto &childNode : astNode.children)
            {
                if (childNode->is_type<Ast::Operation>())
                {
                    name = childNode->string();
                }
                else
                {
                    childrenIds.push_back(makeExpression(*childNode, eGraph));
                }
            }

            return eGraph.addOperation(name, childrenIds);
        }
        else if (astNode.is_type<Ast::Term>())
        {
            return eGraph.addTerm(astNode.string());
        }

        assert(false);
    }

    ClassId makeExpression(const std::string &expression, Graph &eGraph)
    {
        using namespace tao::pegtl;
        string_input input(expression, "");
        const auto node = parse_tree::parse<Ast::Grammar, Ast::Node, Ast::Selector>(input);
        return makeExpression(*node, eGraph);
    }

    RewriteRule makeRewriteRule(const std::string &expression)
    {
        using namespace tao::pegtl;
        string_input input(expression, "");
        const auto node = parse_tree::parse<Ast::Grammar, Ast::Node, Ast::Selector>(input);
        return makeRewriteRule(*node);
    }
}

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
