#pragma once

#include <string_view>
#include <variant>

#include <boost/spirit/home/x3.hpp>

namespace x3 = boost::spirit::x3;

namespace Candy {
    template <typename T>
    constexpr auto to(T& arg) {
        return [&](auto& ctx) { arg = x3::_attr(ctx); };
    }

    template <typename T, typename Parser>
    constexpr auto as(Parser&& p) {
        return x3::rule<struct _, T>{} = std::forward<Parser>(p);
    }

    template <typename Tag, typename Symbols>
    constexpr auto select_parser(Symbols&& sym) {
        auto action = [](auto& ctx) { x3::get<Tag>(ctx) = x3::_attr(ctx); };
        return x3::omit[sym[action]];
    }

    template <typename Tag>
    struct lazy_type : x3::parser<lazy_type<Tag>> {
        using attribute_type = typename Tag::attribute_type;

        template<typename It, typename Ctx, typename RCtx, typename Attr>
        bool parse(It& first, It last, Ctx& ctx, RCtx& rctx, Attr& attr) const {
            auto& subject = x3::get<Tag>(ctx);

            It saved = first;
            x3::skip_over(first, last, ctx);
            bool rv = x3::as_parser(subject).parse(
                first, last,
                std::forward<Ctx>(ctx), std::forward<RCtx>(rctx), attr
            );
            if (rv) return true;
            first = saved;
            return false;
        }
    };

    template <typename T>
    constexpr auto lazy = lazy_type<T>{};

    constexpr auto skipper_ = x3::lexeme[
        x3::blank |
            "//" >> *(x3::char_ - x3::eol) |
            ("/*" >> *(x3::char_ - "*/")) >> "*/"
    ];

    constexpr auto end_cmd_ = x3::omit[*x3::eol];

    constexpr auto od(char c) {
        return x3::omit[x3::char_(c)];
    }

    const auto name_ = as<std::string>(
        x3::lexeme[x3::char_("a-zA-Z_") >> *x3::char_("a-zA-Z_0-9")]
    );

    const auto quoted_name_ = as<std::string>(
        x3::lexeme['"' >> *(('\\' >> x3::char_("\\\"")) | ~x3::char_('"')) >> '"']
    );

    static std::string_view skip_blines(std::string_view rng) {
        constexpr auto eols_ = x3::omit[+x3::eol];
        auto iter = rng.begin();
        phrase_parse(iter, rng.end(), eols_, skipper_);
        return { iter, rng.end() };
    }

    using ParseResult = std::pair<std::string_view, bool>;

    using nodes_t = x3::symbols<size_t>;

    using attr_val_t = std::variant<int32_t, double, std::string>;
    using attr_val_rule = x3::any_parser<std::string_view::const_iterator, attr_val_t>;
    using attr_types_t = x3::symbols<attr_val_rule>;

    static auto make_rv(std::string_view::iterator b, std::string_view::iterator e, bool v) {
        return std::make_pair(std::string_view{ b, e }, v);
    }

    static auto make_rv(std::string_view s, bool v) {
        return std::make_pair(s, v);
    }

    bool syntax_error(std::string_view where, std::string_view what = "");

}