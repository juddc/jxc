# JXC Python Library

The JXC Python library has two APIs - a simple one similar to `json.loads`/`json.dumps`, and a more complex but more flexible one.

## Simple API

### Parsing
```python
import jxc
print(jxc.loads("[1, 2, true, null, {}]"))
```
```python
[1, 2, True, None, {}]
```

### Serializing
```python
import jxc
print(jxc.dumps([1, 2, True, None, {}]))
```
```jxc
[1,2,true,null,{}]
```

### Handling Annotations

Take this data normalization example from the home page. Lets say we wanted to convert any value with a `vec3` annotation to a Python tuple containing three floats.

```jxc
[
    vec3{x: 1, y: 2, z: 3}
    vec3[-3, -2, -1]
    vec3 0.2
    vec3 null
]
```

```python
import jxc

def read_vec3(value):
    if isinstance(value, dict) and len(value) == 3:
        return (float(value['x']), float(value['y']), float(value['z']))
    elif isinstance(value, list) and len(value) == 3:
        return tuple(float(v) for v in value)
    elif isinstance(value, (int, float)):
        return (float(value), float(value), float(value))
    elif value is None:
        return (0.0, 0.0, 0.0)
    else:
        raise TypeError(f'Invalid value for vec3: {value!r}')

vecs = jxc.loads("""
    [
        vec3{ x: 1, y: 2, z: 3 }
        vec3[ -3, -2, -1 ]
        vec3 0.2
        vec3 null
    ]
    """,
    annotation_hooks=[('vec3', read_vec3)],
)

assert vecs == [
    (1.0, 2.0, 3.0),
    (-3.0, -2.0, -1.0),
    (0.2, 0.2, 0.2),
    (0.0, 0.0, 0.0),
]
```

### Dataclasses and Enums

Lets say you want to use more complex objects. You could write your own serializers for types, but if your types are simple, you could just use dataclasses and enums, which are supported out of the box.

```python
import jxc
from enum import Enum
from dataclasses import dataclass

class DataType(Enum):
    A = 'A'
    B = 'B'

@dataclass
class Data:
    val: tuple[float, float]
    name: str
    ty: DataType
    meta: dict[str, str]

print(jxc.dumps(
    Data(val=(2.5, -5.6), name='abc', ty=DataType.B, meta={ 'key': 'value' }),
    indent=4,
    encode_dataclass=True,
    encode_enum=True))
```
```jxc
Data{
    val: [
        2.5,
        -5.6,
    ],
    name: "abc",
    ty: DataType "B",
    meta: {
        key: "value",
    },
}
```

You can use the same system for loading values into those dataclasses and enums.
```python
# This assumes that the DataType enum and Data dataclass are already defined

print(jxc.loads("""
    Data{
        name: 'jxc'
        ty: DataType 'A'
        val: [ -4.1, 9.8 ]
        meta: { key.a: 'b', key.b: '_' }
    }
    """,
    decode_dataclass=True,
    decode_enum=True))
```
```
Data(val=[-4.1, 9.8], name='jxc', ty=<DataType.A: 'A'>, meta={'key.a': 'b', 'key.b': '_'})
```
