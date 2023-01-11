## JXC Syntax
    jxc
        element

    value_container
        object
        array
        expression

    value_single
        string
        string_raw
        string_hexbyte
        string_base64
        number
        "true"
        "false"
        "null"

    value_annotated
        annotation whitespace_required value_single
        annotation whitespace value_container
        value_single
        value_container

    annotation
        dotted_identifier
        dotted_identifier '<' annotation_generic '>'
        '!' dotted_identifier
        '!' dotted_identifier '<' annotation_generic '>'
    
    annotation_generic
        ""
        dotted_identifier
        value_single
        '!'
        '*'
        '?'
        '|'
        '&'
        '='
        ','
        '<' annotation_generic '>'
        '(' annotation_generic ')'

    dotted_identifier
        identifier
        identifier '.' identifier

    object
        '{' whitespace '}'
        '{' object_pairs '}'
    
    object_pairs
        object_pair
        object_pair container_separator
        object_pair container_separator object_pairs
    
    object_pair
        whitespace object_key whitespace ':' element

    object_key
        string
        string_hexbyte
        number_integer
        "null"
        "true"
        "false"
        identifier_object_key
        identifier_object_key '.' identifier_object_key

    identifier_object_key
        identifier_object_key_first_char
        identifier_object_key_first_char identifier_object_key_chars

    identifier_object_key_chars
        identifier_object_key_char
        identifier_object_key_char identifier_object_key_chars

    identifier_object_key_first_char
        identifier_first_char
        '*'

    identifier_object_key_char
        identifier_char
        '*'

    array
        '[' whitespace ']'
        '[' array_items ']'

    array_items
        element
        element container_separator
        element container_separator element

    element
        whitespace value_annotated whitespace

    container_separator
        ','
        linebreak

    expression
        '(' whitespace ')'
        '(' expression_items ')'

    expression_items
        whitespace expression_item whitespace
        whitespace expression_item whitespace expression_items

    expression_item
        string
        string_raw
        string_hexbyte
        string_base64
        number_unsigned
        "true"
        "false"
        "null"
        identifier
        expression_operator
        linebreak
        ','
        ':'
        '@'
        '[' whitespace ']'
        '[' expression_items ']'
        '{' whitespace '}'
        '{' expression_items '}'
        '(' whitespace ')'
        '(' expression_items ')'

    expression_operator
        '|'
        '&'
        '!'
        '='
        '+'
        '-'
        '*'
        '/'
        '\'
        '%'
        '^'
        '.'
        '?'
        '~'
        '<'
        '>'
        '`'
        ';'

    identifier
        identifier_first_char
        identifier_first_char identifier_chars

    identifier_chars
        identifer_char
        identifer_char identifer_chars

    identifier_first_char
        'a' . 'z'
        'A' . 'Z'
        '_'
        '$'
    
    identifier_char
        'a' . 'z'
        'A' . 'Z'
        '0' . '9'
        '_'
        '$'

    string
        '"' string_characters '"'
        '\'' string_characters '\''

    string_characters
        ""
        string_character string_characters

    string_character
        '0020' . '10FFFF' - '"' - '\'
        '\' escape

    escape
        '"'
        '\'
        '/'
        'b'
        'f'
        'n'
        'r'
        't'
        'x' hex hex
        'u' hex hex hex hex
        'U' hex hex hex hex hex hex hex hex

    string_raw
        "r\"(" string_raw_characters ")\""
        "r'(" string_raw_characters ")'"
        "r\"" heredoc '(' string_raw_characters ')' heredoc '"'
        "r'" heredoc '(' string_raw_characters ')' heredoc '\''

    string_raw_characters
        ""
        string_raw_character string_raw_characters

    string_raw_character
        '0020' . '10FFFF'

    heredoc
        heredoc_first_character
        heredoc_first_character heredoc_characters

    heredoc_first_character
        'a' . 'z'
        'A' . 'Z'
        '_'

    heredoc_characters
        heredoc_character
        heredoc_character heredoc_characters
    
    heredoc_character
        'a' . 'z'
        'A' . 'Z'
        '0' . '9'
        '_'

    string_hexbyte
        "bx\"" hexbyte_characters '"'
        "bx'" hexbyte_characters '\''
        "bx\"(" hexbyte_multi_characters ")\""
        "bx'(" hexbyte_multi_characters ")'"

    hexbyte_characters
        ""
        hexbyte_pair hexbyte_characters

    hexbyte_pair
        hex hex

    hexbyte_multi_characters
        ""
        whitespace hex whitespace hexbyte_multi_characters

    string_base64
        "b64\"" base64_single_characters '"'
        "b64'" base64_single_characters '\''
        "b64\"(" base64_multi_characters ")\""
        "b64'(" base64_multi_characters ")'"

    base64_single_characters
        ""
        base64_digit base64_single_characters

    base64_multi_characters
        ""
        whitespace base64_digit whitespace base64_multi_characters
    
    number
        sign number_unsigned_hex
        sign number_unsigned_bin
        sign number_unsigned_oct
        sign number_unsigned_dec exponent
        sign number_unsigned_dec '.' digits exponent
        sign number_unsigned_dec '.' digits
        sign number_unsigned_dec

    number_integer
        sign number_unsigned_hex
        sign number_unsigned_bin
        sign number_unsigned_oct
        sign number_unsigned_dec exponent_positive
        sign number_unsigned_dec

    number_unsigned_hex
        "0x" digits_hex '_' number_suffix
        "0x" digits_hex

    number_unsigned_bin
        "0b" digits_bin '_' number_suffix
        "0b" digits_bin number_suffix
        "0b" digits_bin

    number_unsigned_oct
        "0o" digits_oct '_' number_suffix
        "0o" digits_oct number_suffix
        "0o" digits_oct

    number_unsigned_dec
        digits_integer '_' number_suffix
        digits_integer number_suffix
        digits_integer

    number_suffix
        number_suffix_first_char
        number_suffix_first_char number_suffix_chars

    number_suffix_chars
        number_suffix_char
        number_suffix_char number_suffix_chars

    number_suffix_first_char
        'a' . 'z'
        'A' . 'Z'
        '%'

    number_suffix_char
        'a' . 'z'
        'A' . 'Z'
        '0' . '9'
        '_'
        '%'

    exponent
        exponent_positive
        exponent_negative
    
    exponent_positive
        "e+" digits
        "E+" digits
        'e' digits
        'E' digits
    
    exponent_negative
        "e-" digits
        "E-" digits

    sign
        ""
        '+'
        '-'

    digits_integer
        '0'
        digit
        digit_1_9 digits

    digits
        digit
        digit digits

    digits_hex
        hex
        hex digits_hex
    
    digits_oct
        oct
        oct digits_oct
    
    digits_bin
        bin
        bin digits_bin

    digit
        '0' . '9'

    digit_1_9
        '1' . '9'
    
    hex
        'a' . 'f'
        'A' . 'F'

    bin
        '0'
        '1'

    oct
        '0' . '7'

    base64
        '0' . '9'
        'A' . 'Z'
        'a' . 'z'
        '+'
        '/'
        '='

    whitespace
        ""
        space
        linebreak

    whitespace_required
        space
        linebreak

    space
        space_char
        space_char space

    space_char
        ' '
        '\t'

    linebreak
        linebreak_char
        linebreak_char linebreak
    
    linebreak_char
        '\n'
        '\r'
