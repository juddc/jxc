#!/usr/bin/env python3
import argparse
import typing
import datetime
import dataclasses
import enum
import json
import re
import jxc


def pretty_repr(obj: typing.Any, indent=0) -> str:
    result = []
    prefix_first = '    ' * indent
    prefix = '    ' * (indent + 1)
    if dataclasses.is_dataclass(obj) and not isinstance(obj, type):
        result.append(f'{prefix_first}{obj.__class__.__name__}:')
        for attr_name in obj.__class__.__annotations__.keys():
            result.append(f'{prefix}{attr_name} = {pretty_repr(getattr(obj, attr_name), indent=indent + 1).lstrip()}')
    elif isinstance(obj, (list, tuple)):
        result.append(f'{prefix_first}list:')
        for i, val in enumerate(obj):
            result.append(f'{prefix}[{i}] {pretty_repr(val, indent=indent + 1).lstrip()}')
    elif isinstance(obj, dict):
        result.append(f'{prefix_first}dict:')
        for key, val in obj.items():
            result.append(f'{prefix}[{key!r}] {pretty_repr(val, indent=indent + 1).lstrip()}')
    else:
        result.append(f'{prefix}{obj!r}')
    return '\n'.join(result)



def _tzinfo_from_offset(hours, minutes=0) -> datetime.timezone:
    return datetime.timezone(offset=datetime.timedelta(seconds=(hours * 60 * 60) + (minutes * 60)))


# Example value: 'Sun Aug 31 00:29:09 +0000 2014'
DATETIME_REGEX = re.compile(r'([A-Z][a-z]{2}) ([A-Z][a-z]{2}) ([0-9]{2}) ([0-9]{2}):([0-9]{2}):([0-9]{2}) ([+-])([0-9]{2})([0-9]{2}) ([0-9]{4})')
DATETIME_MONTHS = {'Jan': 1, 'Feb': 2, 'Mar': 3, 'Apr': 4, 'May': 5, 'Jun': 6, 'Jul': 7, 'Aug': 8, 'Sep': 9, 'Oct': 10, 'Nov': 11, 'Dec': 12}

def parse_datetime(dt: str) -> datetime.datetime:
    vals = DATETIME_REGEX.match(dt).groups()
    return datetime.datetime(
        year=int(vals[9]),
        month=DATETIME_MONTHS[vals[1]],
        day=int(vals[2]),
        hour=int(vals[3]),
        minute=int(vals[4]),
        second=int(vals[5]),
        tzinfo=_tzinfo_from_offset(
            hours=int(vals[7]) if vals[6] == '+' else -int(vals[7]),
            minutes=int(vals[8])))


assert(parse_datetime('Sun Aug 31 00:29:09 -0830 2014') == datetime.datetime(2014, 8, 31, 0, 29, 9, tzinfo=_tzinfo_from_offset(-8, 30)))


class IdNumber:
    def __init__(self, value: int, value_str: str):
        assert value >= 0
        self.value: int = value
        self.value_str: str = value_str
    
    def __repr__(self):
        return f'IdNumber({self.value:#x}, {self.value_str!r})'
    
    def _jxc_encode(self, doc: jxc.Serializer, enc: jxc.Encoder):
        doc.annotation('id')
        (doc.array_begin(separator=', ')
            .value_int_hex(self.value)
            .value_string(self.value_str)
            .array_end())

    @classmethod
    def _jxc_decode(cls, value: list):
        if isinstance(value, list) and len(value) == 2 and isinstance(value[0], int) and isinstance(value[1], str):
            return cls(value[0], value[1])
        else:
            raise ValueError(f'Invalid expression value for id {value!r}')


class IndexSpan:
    def __init__(self, start: int, end: int):
        self.start: int = start
        self.end: int = end
    
    def __repr__(self):
        return f'IndexSpan({self.start}, {self.end})'
    
    def _jxc_encode(self, doc: jxc.Serializer, enc: jxc.Encoder):
        doc.annotation('span')
        (doc.expression_begin()
            .value_int(self.start)
            .write(':')  # write() direct-writes to the expression - no auto-whitespace handling
            .value_int(self.end)
            .expression_end())

    @classmethod
    def _jxc_decode(cls, value: list):
        if isinstance(value, list) and len(value) == 3 and isinstance(value[0], int) and value[1] == ':' and isinstance(value[2], int):
            return cls(value[0], value[2])
        else:
            raise ValueError(f'Invalid value for IndexSpan {value!r}')


def convert_id(data: dict, key: str) -> typing.Optional[IdNumber]:
    key_str = f'{key}_str'
    assert key in data and key_str in data
    if data[key] is None:
        assert data[key_str] is None
        del data[key_str]
        return None
    result = IdNumber(data[key], data[key_str])
    del data[key_str]
    return result


@dataclasses.dataclass
class Entity:
    urls: typing.Optional[list['URLEntity']] = None

    @classmethod
    def from_json(cls, ent: dict):
        result = cls(urls=[ URLEntity.from_json(url) for url in ent.pop('urls') ])
        if len(ent) > 0:
            raise ValueError(f'Failed to parse all keys (missed {list(ent.keys())})')
        return result


class MediaResize(enum.Enum):
    Fit = 'fit'
    Crop = 'crop'


@dataclasses.dataclass
class Media:
    w: int
    h: int
    resize: MediaResize

    def __post_init__(self):
        if isinstance(self.resize, str):
            self.resize = MediaResize(self.resize)

    def _jxc_encode(self, doc: jxc.Serializer, enc: jxc.Encoder):
        doc.annotation(self.__class__.__name__)
        doc.object_begin(separator=', ')
        doc.identifier('w').sep().value_int(self.w)
        doc.identifier('h').sep().value_int(self.h)
        doc.identifier('resize').sep()
        enc.encode_value(self.resize)
        doc.object_end()


@dataclasses.dataclass
class MediaSet:
    large: Media
    medium: Media
    small: Media
    thumb: Media

    @classmethod
    def from_json(cls, ent: dict):
        return cls(**{ key: Media(**media) for key, media in ent.items() })


@dataclasses.dataclass
class MediaEntity:
    id: IdNumber
    indices: IndexSpan
    media_url: str
    media_url_https: str
    url: str
    display_url: str
    expanded_url: str
    type: str
    sizes: MediaSet
    source_status_id: typing.Optional[IdNumber] = None

    @classmethod
    def from_json(cls, ent: dict):
        ent['id'] = convert_id(ent, 'id')
        ent['indices'] = IndexSpan(*ent['indices'])
        ent['sizes'] = MediaSet.from_json(ent['sizes'])
        if 'source_status_id' in ent:
            ent['source_status_id'] = convert_id(ent, 'source_status_id')
        return cls(**ent)


@dataclasses.dataclass
class HashtagEntity:
    text: str
    indices: IndexSpan

    @classmethod
    def from_json(cls, ent: dict):
        result = cls(text=ent.pop('text'), indices=IndexSpan(*ent['indices']))
        ent.pop('indices')
        if len(ent) > 0:
            raise ValueError(f'Failed to parse all keys (missed {list(ent.keys())})')
        return result


@dataclasses.dataclass
class UserMentionEntity:
    id: IdNumber
    screen_name: str
    name: str
    indices: tuple[int, int]

    @classmethod
    def from_json(cls, value: dict):
        value['id'] = convert_id(value, 'id')
        value['indices'] = IndexSpan(*value['indices'])
        return cls(**value)


@dataclasses.dataclass
class URLEntity:
    url: str
    expanded_url: str
    display_url: str
    indices: IndexSpan

    @classmethod
    def from_json(cls, value: dict):
        value['indices'] = IndexSpan(*value['indices'])
        return cls(**value)


@dataclasses.dataclass
class Entities:
    hashtags: typing.Optional[list[HashtagEntity]] = None
    symbols: typing.Optional[list[Entity]] = None
    user_mentions: typing.Optional[list[UserMentionEntity]] = None
    media: typing.Optional[list[MediaEntity]] = None
    urls: typing.Optional[list[URLEntity]] = None
    description: typing.Optional[Entity] = None
    url: typing.Optional[Entity] = None

    @classmethod
    def from_json(cls, entities: dict):
        result = cls(
            hashtags=[ HashtagEntity.from_json(ent) for ent in entities.pop('hashtags') ] if 'hashtags' in entities else None,
            symbols=[ Entity.from_json(ent) for ent in entities.pop('symbols') ] if 'symbols' in entities else None,
            user_mentions=[ UserMentionEntity.from_json(ent) for ent in entities.pop('user_mentions') ] if 'user_mentions' in entities else None,
            media=[ MediaEntity.from_json(ent) for ent in entities.pop('media') ] if 'media' in entities else None,
            urls=[ URLEntity.from_json(ent) for ent in entities.pop('urls') ] if 'urls' in entities else None,
            description=Entity.from_json(entities.pop('description')) if 'description' in entities else None,
            url=Entity.from_json(entities.pop('url')) if 'url' in entities else None,
        )
        if len(entities) > 0:
            raise ValueError(f'Failed to parse all keys (missed {list(entities.keys())})')
        return result
        # for key in entities.keys():
        #     if key == "user_mentions":
        #         entities[key] = [ UserMentionEntity.from_json(mention) for mention in entities[key] ]
        #     elif key == 'urls':
        #         entities[key] = [ URLEntity.from_json(url_ent) for url_ent in entities[key] ]

        #     if 'urls' in entities[key]:
        #         entities[key]['urls'] = [ URLEntity.from_json(url_ent) for url_ent in entities[key]['urls'] ]


@dataclasses.dataclass
class TwitterUser:
    id: IdNumber
    name: str
    screen_name: str
    location: str
    description: str
    entities: dict
    protected: bool
    followers_count: int
    friends_count: int
    listed_count: int
    created_at: datetime.datetime
    favourites_count: int
    geo_enabled: bool
    verified: bool
    statuses_count: int
    lang: str
    contributors_enabled: bool
    is_translator: bool
    is_translation_enabled: bool
    profile_background_color: str
    profile_background_image_url: str
    profile_background_image_url_https: str
    profile_background_tile: bool
    profile_image_url: str
    profile_image_url_https: str
    profile_link_color: str
    profile_sidebar_border_color: str
    profile_sidebar_fill_color: str
    profile_text_color: str
    profile_use_background_image: bool
    default_profile: bool
    default_profile_image: bool
    following: bool
    follow_request_sent: bool
    notifications: bool
    profile_banner_url: typing.Optional[str] = None
    url: typing.Optional[str] = None
    utc_offset: typing.Optional[int] = None
    time_zone: typing.Optional[str] = None

    @classmethod
    def from_json(cls, value: dict):
        value['id'] = convert_id(value, 'id')
        value['created_at'] = parse_datetime(value['created_at'])
        #convert_json_entities(value['entities'])
        value['entities'] = Entities.from_json(value['entities'])
        return cls(**value)


@dataclasses.dataclass
class TwitterStatus:
    id: IdNumber
    created_at: datetime.datetime
    text: str
    source: str
    truncated: bool
    in_reply_to_status_id: typing.Optional[IdNumber]
    in_reply_to_user_id: typing.Optional[IdNumber]
    in_reply_to_screen_name: str
    user: TwitterUser
    retweet_count: int
    favorite_count: int
    entities: dict
    favorited: bool
    retweeted: bool
    lang: str
    metadata: dict[str, str]
    geo: typing.Optional[dict] = None
    coordinates: typing.Optional[str] = None
    place: typing.Optional[str] = None
    contributors: typing.Optional[list[str]] = None
    possibly_sensitive: typing.Optional[bool] = False
    retweeted_status: typing.Optional['TwitterStatus'] = None

    @classmethod
    def from_json(cls, value: dict):
        value['id'] = convert_id(value, 'id')
        value['in_reply_to_status_id'] = convert_id(value, 'in_reply_to_status_id')
        value['in_reply_to_user_id'] = convert_id(value, 'in_reply_to_user_id')
        value['created_at'] = parse_datetime(value['created_at'])
        value['user'] = TwitterUser.from_json(value['user'])
        if 'retweeted_status' in value and value['retweeted_status']:
            value['retweeted_status'] = TwitterStatus.from_json(value['retweeted_status'])
        value['entities'] = Entities.from_json(value['entities'])
        #convert_json_entities(value['entities'])

        return cls(**value)



if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('--input', '-i', type=str)
    parser.add_argument('--output', '-o', type=str)
    parser.add_argument('--debug', '-d', action='store_true')
    args = parser.parse_args()

    with open(args.input) as fp:
        json_data = fp.read()
    src_data = json.loads(json_data)

    assert isinstance(src_data, dict)
    assert len(src_data) == 2
    assert 'statuses' in src_data and isinstance(src_data['statuses'], list)
    assert 'search_metadata' in src_data and isinstance(src_data['search_metadata'], dict)

    # convert data to dataclasses
    src_data['statuses'] = [ TwitterStatus.from_json(status) for status in src_data['statuses'] ]

    if args.debug:
        # pretty print all objects
        for status in src_data['statuses']:
            print('-------------------------')
            assert isinstance(status, TwitterStatus)
            print(pretty_repr(status, indent=1))

    with open(args.output, 'w') as fp:
        fp.write(jxc.dumps(src_data,
            indent=2,
            separators=('\n', ': '),
            encode_dataclass=True,
            encode_enum=True,
            quote=jxc.StringQuoteMode.Single))

