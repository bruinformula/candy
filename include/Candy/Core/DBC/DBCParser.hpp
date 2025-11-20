#pragma once

#include <string_view>

namespace Candy {

    template<typename T>
    concept HasParseDBC = requires(T t, std::string_view dbc_src) {
        { t.parse_dbc(dbc_src) } -> std::same_as<bool>;
    };

    template <typename Derived>
    struct DBCParser {
        bool parse_dbc_vrtl(std::string_view dbc_src) {
            static_cast<Derived&>(*this).parse_dbc(dbc_src);
        }

        DBCParser() {
            static_assert(HasParseDBC<Derived>, "Derived must satisfy HasParseDBC concept");
        }
    };
    
}

