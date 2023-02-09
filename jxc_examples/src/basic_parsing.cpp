#include <iostream>
#include <string>
#include "jxc/jxc.h"

int main(int argc, const char** argv)
{
    std::string jxc_string = "[1, 2, true, null, 'string', dt'1996-06-07']";
    jxc::JumpParser parser(jxc_string);
    while (parser.next())
    {
        const jxc::Element& value = parser.value();
        std::cout << value.to_repr() << '\n';
    }
    if (parser.has_error())
    {
        std::cerr << "Parse error: " << parser.get_error().to_string() << '\n';
    }
    return 0;
}

