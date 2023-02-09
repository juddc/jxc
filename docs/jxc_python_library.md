# JXC Python Library

## Installing

The fastest way to start using JXC from Python is with pip. If you're already comfortable installing other Python packages, you can simply run `pip install jxc` and you're good to go.

The recommended approach is to set up a virtualenv first, for example:
```bash
$ virtualenv venv
$ source venv/bin/activate
(venv) $ pip install jxc
(venv) $ python -c "import jxc; print(jxc.loads('[1, 2, 3]'))"
[1, 2, 3]
```

## Basic Usage

The JXC Python library has two APIs - a simple one similar to `json.loads`/`json.dumps`, and a more complex but more flexible one.

### Simple API: Parsing
```python
import jxc
print(jxc.loads("[1, 2, true, null, {}, dt'1999-07-18']"))
```
```
[1, 2, True, None, {}, datetime.date(1999, 7, 18)]
```

### Simple API: Serializing
```python
import jxc, datetime
print(jxc.dumps([1, 2, True, None, {}, datetime.date(1999, 7, 18)]))
```
```jxc
[1,2,true,null,{},dt"1999-07-18"]
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

To start with, lets define some types we want to use:

```python
from enum import Enum
from dataclasses import dataclass
import jxc

class DataType(Enum):
    A = 'A'
    B = 'B'

@dataclass
class Data:
    val: tuple[float, float]
    name: str
    ty: DataType
    meta: dict[str, str]
```

Serializing these is trivial:

```python
print(jxc.dumps(
    Data(val=(2.5, -5.6), name='abc', ty=DataType.B, meta={ 'key': 'value' }),
    indent=4,
    encode_dataclass=True,
    encode_enum=True))
```

Output:

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

Parsing these values is also trivial:

```python
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

Output:

```
Data(val=[-4.1, 9.8], name='jxc', ty=<DataType.A: 'A'>, meta={'key.a': 'b', 'key.b': '_'})
```
