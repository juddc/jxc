#include <iostream>
#include "jxc/jxc.h"

int main(int argc, const char** argv)
{
    jxc::StringOutputBuffer buffer;
    jxc::Serializer doc(&buffer);
    doc.array_begin()
        .value_int(1)
        .value_int(2)
        .value_bool(true)
        .value_null()
        .value_string("string")
        .value_date(jxc::Date(1996, 6, 7))
        .array_end();
    doc.flush();
    std::cout << buffer.to_string() << '\n';
    return 0;
}
