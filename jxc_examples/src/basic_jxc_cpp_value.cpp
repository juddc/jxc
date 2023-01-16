#include <iostream>
#include <cassert>
#include "jxc_cpp/jxc_cpp.h"

void example_parse()
{
    std::string jxc_value = "[true, 'abc', 12px, annotated null]";
    jxc::Value result = jxc::parse(jxc_value);
    assert(result.get_type() == jxc::ValueType::Array);
    assert(result.size() == 4);
    assert(result[0].is_bool() && result[0].as_bool() == true);
    assert(result[1].is_string() && result[1].as_string() == "abc");
    assert(result[2].is_integer() && result[2].as_integer() == 12 && result[2].get_number_suffix() == "px");
    assert(result[3].is_null() && result[3].get_annotation_source() == "annotated");
}

void example_serialize()
{
    jxc::Value val = jxc::default_object;
    val["parent"] = nullptr;
    val["location"] = jxc::annotated_array("vec3", { 3.141, -4.2, 12.02 });
    val["rotation"] = jxc::annotated_array("rot", { 0.0, -45.0, -2.0 });
    val["scale"] = 1.0;
    std::cout << val.to_string() << '\n';
}

int main(int argc, const char** argv)
{
    example_parse();
    example_serialize();
    return 0;
}

