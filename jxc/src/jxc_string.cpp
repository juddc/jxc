#include "jxc/jxc_string.h"


JXC_BEGIN_NAMESPACE(jxc)


void FlexString::free_owned()
{
    JXC_DEBUG_ASSERT(type == FlexOwned);
    JXC_DEBUG_ASSERT(str_data != nullptr);
    JXC_FREE((void*)str_data, flex_len);
    str_data = nullptr;
    flex_len = 0;
}


void FlexString::set_inline_internal(const char* in_str, size_t in_str_len)
{
    JXC_DEBUG_ASSERT(in_str != nullptr);
    JXC_DEBUG_ASSERT(in_str_len > 0);
    JXC_DEBUG_ASSERT(in_str_len <= max_inline_len);
    type = FlexInline;
    flex_len = in_str_len;
    JXC_STRNCPY(&str_buf[0], str_buf_size, in_str, flex_len);
    str_buf[flex_len] = '\0';
}


void FlexString::set_view_internal(const char* in_str, size_t in_str_len)
{
    type = FlexView;
    flex_len = in_str_len;
    str_data = in_str;
}


void FlexString::set_view_static_internal(const char* in_str, size_t in_str_len)
{
    type = FlexViewStatic;
    flex_len = in_str_len;
    str_data = in_str;
}


void FlexString::set_owned_internal(const char* in_str, size_t in_str_len)
{
    type = FlexOwned;
    flex_len = in_str_len;
    str_data = JXC_STRNDUP(in_str, in_str_len);
}


char* FlexString::set_owned_empty_buffer_internal(size_t in_buffer_len, char fill_char)
{
    JXC_DEBUG_ASSERT(in_buffer_len > 0);
    type = FlexOwned;
    flex_len = in_buffer_len;
    char* new_buf = (char*)JXC_MALLOC(in_buffer_len + 1); // one extra byte for a null terminator
    memset(new_buf, (int)fill_char, in_buffer_len);

    // memset doesn't handle this for us because we don't know if fill_char is 0, and I don't
    // see a reason to add a branch to this low-level function.
    new_buf[in_buffer_len] = '\0';

    str_data = new_buf;
    return new_buf;
}


void FlexString::copy_internal(const FlexString& rhs)
{
    if (rhs.flex_len == 0)
    {
        set_inline_empty_internal();
        return;
    }

    type = rhs.type;
    flex_len = rhs.flex_len;
    switch (type)
    {
    case FlexInline:
        // copy the inline string buffer
        JXC_STRNCPY(&str_buf[0], str_buf_size, &rhs.str_buf[0], flex_len);
        str_buf[flex_len] = '\0';
        break;
    case FlexView:
        // fallthrough
    case FlexViewStatic:
        str_data = rhs.str_data;
        break;
    case FlexOwned:
        // duplicate the owned string into this one
        str_data = JXC_STRNDUP(rhs.str_data, flex_len);
        break;
    default:
        break;
    }
}


void FlexString::move_internal(FlexString&& rhs)
{
    if (rhs.flex_len == 0)
    {
        set_inline_empty_internal();
        return;
    }

    type = rhs.type;
    flex_len = rhs.flex_len;
    switch (type)
    {
    case FlexInline:
        // copy the inline string buffer
        JXC_STRNCPY(&str_buf[0], str_buf_size, &rhs.str_buf[0], flex_len);
        str_buf[flex_len] = '\0';
        break;
    case FlexView:
        // fallthrough
    case FlexViewStatic:
        str_data = rhs.str_data;
        break;
    case FlexOwned:
        // transfer ownership of the existing owned string into this one
        str_data = rhs.str_data;
        // clear the old instance
        rhs.set_inline_empty_internal();
        break;
    default:
        break;
    }
}


char* FlexString::data()
{
    switch (type)
    {
    case FlexView:
    case FlexViewStatic:
    case FlexOwned:
        return const_cast<char*>(str_data);
    case FlexInline:
        return &str_buf[0];
    default:
        break;
    }
    return nullptr;
}


void FlexString::resize(size_t new_size, char fill_char)
{
    if (new_size == flex_len)
    {
        if (type == FlexView || type == FlexViewStatic)
        {
            // no size change, but we're a view so switch to owned
            to_owned_inplace();
        }
        return;
    }
    else if (new_size == 0)
    {
        reset();
        return;
    }
    else if (flex_len == 0)
    {
        // we're currently an empty string, so there's no data to migrate to a new allocation
        JXC_DEBUG_ASSERT(type != FlexOwned);
        if (new_size <= max_inline_len)
        {
            // currently empty, and the new size is small enough to keep using the inline buffer
            type = FlexInline;
            flex_len = new_size;
            memset(&str_buf[0], (int)fill_char, flex_len);
            str_buf[flex_len] = '\0';
        }
        else
        {
            set_owned_empty_buffer_internal(new_size, fill_char);
        }
        return;
    }

    // number of characters we're going to copy from the old string into the new one
    const size_t orig_size = flex_len;
    const size_t num_chars_to_copy = std::min(new_size, orig_size);

    auto set_inline_sized = [this, orig_size, new_size, num_chars_to_copy, fill_char](const char* orig_str)
    {
        JXC_DEBUG_ASSERT(new_size <= max_inline_len);

        set_inline_internal(orig_str, num_chars_to_copy);
        if (new_size > orig_size)
        {
            // string is larger than before, so fill the extra chars in the inline buffer
            memset(&str_buf[orig_size], (int)fill_char, new_size - orig_size);
        }
        flex_len = new_size;

        // str_buf is exactly one larger than max_inline_len, so this is always safe
        str_buf[new_size] = '\0';
    };

    auto set_owned_sized = [this, orig_size, new_size, num_chars_to_copy, fill_char](const char* orig_str)
    {
        char* new_buf = set_owned_empty_buffer_internal(new_size, fill_char);
        JXC_STRNCPY(new_buf, new_size, orig_str, num_chars_to_copy);
        // set_owned_empty_buffer_internal allocates and zeros an extra null terminator char, so we're done
    };

    switch (type)
    {
    case FlexView:
        // fallthrough
    case FlexViewStatic:
    {
        // existing string is not owned, so we'll switch to either inline or owned
        const char* view_str = str_data;
        if (new_size <= max_inline_len)
        {
            set_inline_sized(view_str);
        }
        else
        {
            set_owned_sized(view_str);
        }
        break;
    }
    case FlexOwned:
    {
        const char* orig_owned_str = str_data;
        if (new_size <= max_inline_len)
        {
            set_inline_sized(orig_owned_str);
        }
        else
        {
            set_owned_sized(orig_owned_str);
        }
        JXC_FREE((void*)orig_owned_str, orig_size);
        break;
    }
    case FlexInline:
    {
        if (new_size <= max_inline_len)
        {
            // currently inline and new size is inline, so there's no need to reallocate anything
            if (new_size > orig_size)
            {
                // new size is larger, fill extra chars
                JXC_DEBUG_ASSERT(num_chars_to_copy <= max_inline_len);
                memset(&str_buf[num_chars_to_copy], (int)fill_char, new_size - orig_size);
                // new null terminator
                str_buf[new_size] = '\0';
                flex_len = new_size;
            }
            else
            {
                // resizing an inline string to a smaller inline string. super easy.
                flex_len = new_size;
                // zero out the rest of the buffer just to be safe
                memset(&str_buf[new_size], 0, str_buf_size - new_size);
            }
        }
        else
        {
            // Calling set_owned_empty_buffer_internal will overwrite the inline string before we can copy it,
            // so we'll make a copy of the inline buffer first.
            // In theory it would be more efficient to allocate a new buffer here, copy the inline string
            // into it, and then set the buffer to be our new string, but until I see perf issues with this
            // approach, I'm sticking with this simpler, more testable version.
            char str_buf_copy[str_buf_size];
            JXC_MEMCPY(&str_buf_copy[0], str_buf_size, &str_buf[0], num_chars_to_copy);
            set_owned_sized(&str_buf_copy[0]);
        }
        break;
    }
    default:
        JXC_ASSERTF(false, "Invalid FlexString type");
        break;
    }
}


void FlexString::to_owned_inplace()
{
    if (type != FlexView && type != FlexViewStatic)
    {
        return;
    }

    const char* view_str = str_data;
    const size_t view_len = flex_len;
    if (view_len == 0)
    {
        set_inline_empty_internal();
    }
    else if (view_len <= max_inline_len)
    {
        set_inline_internal(view_str, view_len);
    }
    else
    {
        set_owned_internal(view_str, view_len);
    }
}


JXC_END_NAMESPACE(jxc)
