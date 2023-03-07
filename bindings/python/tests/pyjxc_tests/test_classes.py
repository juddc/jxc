import unittest
import jxc
import enum
import dataclasses


class TestEnumInt(enum.Enum):
    A = 0
    B = 1
    C = 2


class TestEnumStr(enum.Enum):
    A = 'A'
    B = 'B'
    C = 'C'


@dataclasses.dataclass
class IntVec3:
    x: int = 0
    y: int = 0
    z: int = 0


def construct_test_enum_int(val):
    if isinstance(val, int):
        return TestEnumInt(val)
    elif isinstance(val, str):
        return getattr(TestEnumInt, val)
    elif isinstance(val, list) and len(val) == 1 and type(val[0]) in (int, str):
        return construct_test_enum_int(val[0])
    raise ValueError(f'Value {val!r} not valid for TestEnumInt')


def encode_test_enum_int(doc: jxc.Serializer, val: TestEnumInt):
    doc.annotation(TestEnumInt.__name__)
    doc.expression_begin().identifier(val.name).expression_end()


class ClassTests(unittest.TestCase):
    def test_get_class_path(self):
        self.assertEqual(jxc.get_class_path(TestEnumInt), 'pyjxc_tests.test_classes.TestEnumInt')
        self.assertEqual(jxc.get_class_path(TestEnumStr), 'pyjxc_tests.test_classes.TestEnumStr')
        self.assertEqual(jxc.get_class_path(IntVec3), 'pyjxc_tests.test_classes.IntVec3')

    def test_find_class(self):
        self.assertIs(jxc.find_class('IntVec3'), IntVec3)
        self.assertIs(jxc.find_class('pyjxc_tests.test_classes.IntVec3'), IntVec3)
        self.assertIs(jxc.find_class('TestEnumInt'), TestEnumInt)
        self.assertIs(jxc.find_class('pyjxc_tests.test_classes.TestEnumInt'), TestEnumInt)
        self.assertIs(jxc.find_class('TestEnumStr'), TestEnumStr)
        self.assertIs(jxc.find_class('pyjxc_tests.test_classes.TestEnumStr'), TestEnumStr)

    def test_enum_parsing(self):
        self.assertEqual(jxc.loads("TestEnumInt 0", decode_enum=True), TestEnumInt.A)
        self.assertEqual(jxc.loads("TestEnumInt 1", decode_enum=True), TestEnumInt.B)
        self.assertEqual(jxc.loads("TestEnumInt 2", decode_enum=True), TestEnumInt.C)
        self.assertEqual(jxc.loads("TestEnumStr 'A'", decode_enum=True), TestEnumStr.A)
        self.assertEqual(jxc.loads("TestEnumStr 'B'", decode_enum=True), TestEnumStr.B)
        self.assertEqual(jxc.loads("TestEnumStr 'C'", decode_enum=True), TestEnumStr.C)

    def test_enum_serializing(self):
        self.assertEqual(jxc.dumps(TestEnumInt.A, encode_enum=True), 'pyjxc_tests.test_classes.TestEnumInt 0')
        self.assertEqual(jxc.dumps(TestEnumInt.B, encode_enum=True), 'pyjxc_tests.test_classes.TestEnumInt 1')
        self.assertEqual(jxc.dumps(TestEnumInt.C, encode_enum=True), 'pyjxc_tests.test_classes.TestEnumInt 2')
        self.assertEqual(jxc.dumps(TestEnumStr.A, encode_enum=True), 'pyjxc_tests.test_classes.TestEnumStr "A"')
        self.assertEqual(jxc.dumps(TestEnumStr.B, encode_enum=True), 'pyjxc_tests.test_classes.TestEnumStr "B"')
        self.assertEqual(jxc.dumps(TestEnumStr.C, encode_enum=True), 'pyjxc_tests.test_classes.TestEnumStr "C"')

    def test_enum_parsing_custom(self):
        anno_hooks = [(TestEnumInt.__name__, construct_test_enum_int)]
        self.assertEqual(jxc.loads("TestEnumInt 0", annotation_hooks=anno_hooks), TestEnumInt.A)
        self.assertEqual(jxc.loads("TestEnumInt 'A'", annotation_hooks=anno_hooks), TestEnumInt.A)
        self.assertEqual(jxc.loads("TestEnumInt(A)", annotation_hooks=anno_hooks), TestEnumInt.A)
        self.assertEqual(jxc.loads("TestEnumInt(C)", annotation_hooks=anno_hooks), TestEnumInt.C)
        self.assertEqual(jxc.loads("TestEnumInt['A']", annotation_hooks=anno_hooks), TestEnumInt.A)
        self.assertEqual(jxc.loads("TestEnumInt[0]", annotation_hooks=anno_hooks), TestEnumInt.A)

    def test_enum_serializing_custom(self):
        encoders = { TestEnumInt: encode_test_enum_int }
        self.assertEqual(jxc.dumps(TestEnumInt.A, encode_class=encoders), "TestEnumInt(A)")
        self.assertEqual(jxc.dumps(TestEnumInt.B, encode_class=encoders), "TestEnumInt(B)")
        self.assertEqual(jxc.dumps(TestEnumInt.C, encode_class=encoders), "TestEnumInt(C)")

    def test_dataclass_parsing(self):
        self.assertEqual(jxc.loads(r"IntVec3{x:1, y:2, z:3}", decode_dataclass=True), IntVec3(1, 2, 3))

    def test_dataclass_serializing(self):
        self.assertEqual(jxc.dumps(IntVec3(1, 2, 3), encode_dataclass=True), r'pyjxc_tests.test_classes.IntVec3{x:1,y:2,z:3}')


if __name__ == '__main__':
    unittest.main()
