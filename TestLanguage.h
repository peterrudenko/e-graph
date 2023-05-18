#pragma once

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
    struct Term : Symbol {};
    struct PatternVariable : seq<one<'$'>, Symbol> {};
    struct Operation : sor<one<'-'>, one<'+'>, one<'*'>, one<'/'>> {};
    struct Arrow : string<'=', '>'> {};
    struct Spacing : star<space> {};

    struct Expression;
    struct BracketExpression : if_must<one<'('>, Spacing, Expression, Spacing, one<')'>> {};
    struct Value : sor<PatternVariable, Term, BracketExpression> {};

    template <typename S, typename O>
    struct LeftAssociative : seq<S, Spacing, star_must<O, Spacing, S, Spacing>> {};

    struct Expression : LeftAssociative<Value, Operation> {};
    struct RewriteRuleOrExpression : LeftAssociative<Expression, Arrow> {};

    struct Grammar : must<Spacing, RewriteRuleOrExpression, eof> {};

    template <typename Rule>
    struct Selector : selector<Rule,
        store_content::on<Term, PatternVariable, Operation>,
        fold_one::on<Value, Expression>,
        discard_empty::on<Arrow>> {};

    struct Node : basic_node<Node> {};
} // namespace Ast

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
} // namespace TestLanguage
