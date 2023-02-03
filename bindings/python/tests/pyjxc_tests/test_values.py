import unittest
import typing
import jxc
import _pyjxc
import enum
import base64


def parse_annotations_to_source_tuple(val: str):
    """
    Simple parser that converts all values to 2-tuples in the form (annotation_as_string, annotated_value)
    """
    parser = jxc.Parser(val)

    def anno_extractor(ele: jxc.Element):
        if anno := ele.annotation.source():
            return lambda parsed_value: (anno, parsed_value)
        return None

    parser.set_find_construct_from_annotation_callback(anno_extractor)
    return parser.parse()


def parse_annotations_to_token_tuple(val: str):
    """
    Simple parser that converts all values to 2-tuples in the form (annotation_as_string, annotated_value)
    """
    parser = jxc.Parser(val)

    def anno_extractor(ele: jxc.Element):
        anno: list[str] = [ele.annotation[i].value for i in range(len(ele.annotation))]
        if len(anno) > 0:
            return lambda parsed_value: (anno, parsed_value)
        else:
            return None

    parser.set_find_construct_from_annotation_callback(anno_extractor)
    return parser.parse()


def parse_number_suffixes_to_tuple(val: str):
    parser = jxc.Parser(val)

    def find_suffix_callback(suffix: str):
        return lambda value: (value, suffix)

    parser.set_find_construct_from_number_suffix_callback(find_suffix_callback)
    return parser.parse()


class SimpleValueTests(unittest.TestCase):
    def test_parse_constants(self):
        self.assertEqual(jxc.loads("null"), None)
        self.assertEqual(jxc.loads("true"), True)
        self.assertEqual(jxc.loads("false"), False)

    def test_parse_ints(self):
        self.assertEqual(type(jxc.loads("0")), int)
        self.assertEqual(jxc.loads("0"), 0)
        self.assertEqual(jxc.loads("123"), 123)
        self.assertEqual(jxc.loads("-123"), -123)
        for i in range(999):
            self.assertEqual(jxc.loads(str(i)), i)
        offset = 10000000
        for i in range(999):
            val = i + offset
            self.assertEqual(jxc.loads(str(val)), val)
        offset = -10000000
        for i in range(999):
            val = i + offset
            self.assertEqual(jxc.loads(str(val)), val)

    def test_parse_int_suffixes(self):
        self.assertEqual(parse_number_suffixes_to_tuple("25%"), (25, '%'))
        self.assertEqual(parse_number_suffixes_to_tuple("50px"), (50, 'px'))

    def test_parse_floats(self):
        self.assertEqual(type(jxc.loads("0.0")), float)
        self.assertEqual(jxc.loads("0.0"), 0.0)
        self.assertAlmostEqual(jxc.loads("123.456"), 123.456)
        self.assertAlmostEqual(jxc.loads("-123.456"), -123.456)
        for i in range(999):
            self.assertAlmostEqual(jxc.loads(f"{i}.5"), i + 0.5)
        offset = -100053.25
        for i in range(999):
            val = i + offset
            self.assertAlmostEqual(jxc.loads(str(val)), val)
        offset = 13200053.25355
        for i in range(999):
            val = i + offset
            self.assertAlmostEqual(jxc.loads(str(val)), val)

    def test_parse_float_suffixes(self):
        self.assertEqual(parse_number_suffixes_to_tuple("25.25%"), (25.25, '%'))
        self.assertEqual(parse_number_suffixes_to_tuple("50.123px"), (50.123, 'px'))
        self.assertEqual(parse_number_suffixes_to_tuple("-50.123px"), (-50.123, 'px'))
        with self.assertRaises(ValueError):
            jxc.loads("2%", ignore_unknown_number_suffixes=False)

    def test_parse_strings(self):
        self.assertEqual(jxc.loads('""'), "")
        self.assertEqual(jxc.loads('"abc"'), "abc")
        self.assertEqual(jxc.loads("'abc'"), "abc")
        self.assertEqual(jxc.loads(r'"this is a longer\nstring with some\nline breaks and other\t\tchars"'),
            "this is a longer\nstring with some\nline breaks and other\t\tchars")

    def test_parse_raw_strings(self):
        self.assertEqual(jxc.loads(r'r"()"'), "")
        self.assertEqual(jxc.loads(r'r"(")"'), "\"")
        self.assertEqual(jxc.loads(r'r"(abc\t\)"'), "abc\\t\\")
        self.assertEqual(jxc.loads(r'''r"HEREDOC(this is a\n raw string\t\)HEREDOC"'''), "this is a\\n raw string\\t\\")

    def test_parse_bytes(self):
        self.assertEqual(jxc.loads("b64\'\'"), b"")
        self.assertEqual(jxc.loads("b64\'()\'"), b"")
        self.assertEqual(jxc.loads("b64\'( anhj )\'"), b"jxc")
        self.assertEqual(jxc.loads("b64\'anhjIGZvcm1hdA==\'"), b"jxc format")
        self.assertEqual(jxc.loads("b64\'( a n h j I G Z v c m 1 h d A = = )\'"), b"jxc format")
        byte_data = b'\xFF\x2A\x00JXC language\x00\x00\x00\x2a'
        byte_data_b64_str = base64.b64encode(byte_data).decode()
        self.assertEqual(jxc.loads(f'b64"{byte_data_b64_str}"'), byte_data)
        self.assertEqual(jxc.dumps(byte_data), f'b64"{byte_data_b64_str}"')

    def test_parse_arrays(self):
        self.assertEqual(jxc.loads("[]"), [])
        self.assertEqual(jxc.loads("[] # comment"), [])
        self.assertEqual(jxc.loads("[1,2,3]"), [1, 2, 3])
        self.assertEqual(jxc.loads("vec3[1,2,3]"), [1, 2, 3])
        with self.assertRaises(ValueError):
            jxc.loads("vec3[1,2,3]", ignore_unknown_annotations=False)

    def test_parse_dicts(self):
        self.assertEqual(jxc.loads(r"{}"),
            {})
        self.assertEqual(jxc.loads(r"{x: 1, y: 2, z: 3}"),
            {'x': 1, 'y': 2, 'z': 3})
        self.assertEqual(jxc.loads(r"{a.b: true, a.b.c: false, a.*.c: null, *: [], $$$***$$$: 'neat'}"),
            {'a.b': True, 'a.b.c': False, 'a.*.c': None, '*': [], '$$$***$$$': 'neat'})
        self.assertEqual(jxc.loads(r"a.b.c.d<a.b.c.d, a.b.c.d>{ value: 12345 }"),
            {'value': 12345})
        self.assertEqual(jxc.loads(r"{border.style: solid(2)}"),
            {'border.style': [2]})
        self.assertEqual(jxc.loads(r"p.attr<name='second'>{ height: auto(), font.size: 1.5em, background.color: rgb[20,50,150] }"),
            {'height': [], 'font.size': 1.5, 'background.color': [20, 50, 150]})
        self.assertEqual(jxc.loads(r'{"x": 1, "y": 2, "z": 3}'),
            {'x': 1, 'y': 2, 'z': 3})
        self.assertEqual(jxc.loads(r"vec3{ x: 1, y: 2, z: 3 }"),
            {'x': 1, 'y': 2, 'z': 3})
        self.assertEqual(jxc.loads(r"quat<double>{ x: 0.123, y: -0.733, z: 1.233, w: 1.523 }"),
            {'x': 0.123, 'y': -0.733, 'z': 1.233, 'w': 1.523})

    def test_annotation_source(self):
        self.assertEqual(parse_annotations_to_source_tuple("annotation null"), ("annotation", None))
        self.assertEqual(parse_annotations_to_source_tuple("abc 5"), ("abc", 5))
        self.assertEqual(parse_annotations_to_source_tuple("abc [5]"), ("abc", [5]))
        self.assertEqual(parse_annotations_to_source_tuple(
            r"Array<Math.Vector3<int32_t>> [ [0,1,2], [4,5,6] ]"),
            ("Array<Math.Vector3<int32_t>>", [ [0,1,2], [4,5,6] ]))
        self.assertEqual(parse_annotations_to_source_tuple(
            r"Array<Math.Vector3<int32_t>> [ {x:0,y:1,z:2}, {x:4,y:5,z:6} ]"),
            ("Array<Math.Vector3<int32_t>>", [ {'x': 0, 'y': 1, 'z': 2}, {'x': 4, 'y': 5, 'z': 6} ]))
        self.assertEqual(parse_annotations_to_source_tuple(
            r"Array<Vector3> [ Vector3{ x: 0, y: 1, z: 2 }, Vector3{ x: 4, y: 5, z: 6 } ]"),
            ("Array<Vector3>", [ ("Vector3", { "x": 0, "y": 1, "z": 2 }), ("Vector3", { "x": 4, "y": 5, "z": 6 }) ]))
        self.assertEqual(parse_annotations_to_source_tuple(
            r"std.vector<int32_t> [ 1, 2, 3, 4, 5 ]"),
            ("std.vector<int32_t>", [ 1, 2, 3, 4, 5 ]))
        self.assertEqual(parse_annotations_to_source_tuple(
            r"std.vector<double> [ 3.141, 0.0, -5.2, 11.22, 33.44, 55.66, 7.8, 9.0 ]"),
            ("std.vector<double>", [ 3.141, 0.0, -5.2, 11.22, 33.44, 55.66, 7.8, 9.0 ]))
        self.assertEqual(parse_annotations_to_source_tuple(
            r"std.unordered_map<std.string, int64_t> { 'abc': 123, 'def': 456, }"),
            ("std.unordered_map<std.string, int64_t>", { 'abc': 123, 'def': 456, }))

    def test_annotation_tokens(self):
        self.assertEqual(parse_annotations_to_token_tuple("annotation null"), (["annotation"], None))
        self.assertEqual(parse_annotations_to_token_tuple("abc 5"), (["abc"], 5))
        self.assertEqual(parse_annotations_to_token_tuple("abc [5]"), (["abc"], [5]))
        self.assertEqual(parse_annotations_to_token_tuple(
            r"Array<Math.Vector3<int32_t>> [ [0,1,2], [4,5,6] ]"),
            (["Array", "<", "Math", ".", "Vector3", "<", "int32_t", ">", ">"], [ [0,1,2], [4,5,6] ]))
        self.assertEqual(parse_annotations_to_token_tuple(
            r"Array<Math.Vector3<int32_t>> [ {x:0,y:1,z:2}, {x:4,y:5,z:6} ]"),
            (["Array", "<", "Math", ".", "Vector3", "<", "int32_t", ">", ">"], [ {'x': 0, 'y': 1, 'z': 2}, {'x': 4, 'y': 5, 'z': 6} ]))
        self.assertEqual(parse_annotations_to_token_tuple(
            r"Array<Vector3> [ Vector3{ x: 0, y: 1, z: 2 }, Vector3{ x: 4, y: 5, z: 6 } ]"),
            (["Array", "<", "Vector3", ">"], [ (["Vector3"], { "x": 0, "y": 1, "z": 2 }), (["Vector3"], { "x": 4, "y": 5, "z": 6 }) ]))
        self.assertEqual(parse_annotations_to_token_tuple(
            r"std.vector<int32_t> [ 1, 2, 3, 4, 5 ]"),
            (["std", ".", "vector", "<", "int32_t", ">"], [ 1, 2, 3, 4, 5 ]))
        self.assertEqual(parse_annotations_to_token_tuple(
            r"std.vector<double> [ 3.141, 0.0, -5.2, 11.22, 33.44, 55.66, 7.8, 9.0 ]"),
            (["std", ".", "vector", "<", "double", ">"], [ 3.141, 0.0, -5.2, 11.22, 33.44, 55.66, 7.8, 9.0 ]))
        self.assertEqual(parse_annotations_to_token_tuple(
            r"std.unordered_map<std.string, int64_t> { 'abc': 123, 'def': 456, }"),
            (["std", ".", "unordered_map", "<", "std", ".", "string", ",", "int64_t", ">"], { 'abc': 123, 'def': 456, }))

    def test_expressions(self):
        self.assertEqual(jxc.loads("()"), [])
        self.assertEqual(jxc.loads("(())"), ['(', ')'])
        self.assertEqual(jxc.loads("(1)"), [1])
        self.assertEqual(jxc.loads("(1 + 2 - 3)"), [1, '+', 2, '-', 3])
        self.assertEqual(jxc.loads("(true)"), [True])
        self.assertEqual(jxc.loads("(true || false)"), [True, '|', '|', False])
        self.assertEqual(jxc.loads("('abc')"), ['abc'])
        self.assertEqual(jxc.loads("(b64'/xEiu/8=')"), [b'\xff\x11"\xbb\xff'])
        self.assertEqual(jxc.loads("(b64'( anhj  I G x hbm  d1Y  W dl )')"), [b'jxc language'])

    def test_enum_unhandled_exception_bug(self):
        class TestEnum(enum.Enum):
            A = enum.auto()
            B = enum.auto()

        with self.assertRaises(TypeError):
            jxc.dumps([TestEnum.A, TestEnum.B], encode_enum=False)

        # FIXME: this triggers 'pure virtual method called'
        # with self.assertRaises(TypeError):
        #     jxc.dumps({'a': TestEnum.A}, encode_enum=False)

    def test_token_span_segfault_bug(self):
        list(_pyjxc.OwnedTokenSpan())



if __name__ == '__main__':
    unittest.main()
