#include "jxc/jxc_core.h"
#include "jxc/jxc_util.h"
#include "jxc/jxc_memory.h"

#if JXC_DEBUG_MEM
#include <unordered_map>
#endif

#if JXC_DEBUG
// abort function that can auto-trigger a breakpoint when a debugger is attached
#if defined(_WIN32)
static inline void _jxc_abort() { __debugbreak(); std::abort(); }
#else
#include <signal.h>
static inline void _jxc_abort() { raise(SIGTRAP); std::abort(); }
#endif
#else
// release build verion - just call std::abort()
static inline void _jxc_abort() { std::abort(); }
#endif


JXC_BEGIN_NAMESPACE(jxc)

const char* log_level_to_string(LogLevel level)
{
    switch (level)
    {
    case JXC_ENUMSTR(LogLevel, Info);
    case JXC_ENUMSTR(LogLevel, Warning);
    case JXC_ENUMSTR(LogLevel, Error);
    case JXC_ENUMSTR(LogLevel, Fatal);
    default: break;
    }
    return "INVALID";
}


static LogHandlerFunc s_log_handler_func;
static AssertHandlerFunc s_assert_handler_callback;


void set_custom_log_handler(const LogHandlerFunc& handler_func)
{
    s_log_handler_func = handler_func;
}


bool have_custom_log_handler()
{
    return s_log_handler_func != nullptr;
}


void set_custom_assert_handler(const AssertHandlerFunc& handler_func)
{
    s_assert_handler_callback = handler_func;
}


bool have_custom_assert_handler()
{
    return s_assert_handler_callback != nullptr;
}


JXC_END_NAMESPACE(jxc)


void _jxc_log_message_string(jxc::LogLevel level, std::string&& msg)
{
    if (jxc::s_log_handler_func)
    {
        jxc::s_log_handler_func(level, std::move(msg));
    }
    else
    {
        const char* level_str = "";
        switch (level)
        {
        case jxc::LogLevel::Info: level_str = "Info"; break;
        case jxc::LogLevel::Warning: level_str = "Warning"; break;
        case jxc::LogLevel::Error: level_str = "Error"; break;
        case jxc::LogLevel::Fatal: level_str = "Fatal"; break;
        default: break;
        }
        jxc::print((level == jxc::LogLevel::Error || level == jxc::LogLevel::Fatal) ? stderr : stdout, "[{}] {}", level_str, std::move(msg));
    }
}


void _jxc_assert_failed_msg(const char* file, int line, const char* cond, std::string&& msg)
{
    if (jxc::s_assert_handler_callback)
    {
        jxc::s_assert_handler_callback(jxc::detail::get_base_filename(file), line, std::string_view{ cond }, std::move(msg));
    }
    else
    {
        JXC_ERROR("Assert failed [{}:{}]: {} ({})", jxc::detail::get_base_filename(file), line, cond, std::move(msg));
        _jxc_abort();
    }
}


void _jxc_assert_failed(const char* file, int line, const char* cond)
{
    if (jxc::s_assert_handler_callback)
    {
        jxc::s_assert_handler_callback(jxc::detail::get_base_filename(file), line, std::string_view{ cond }, std::string{});
    }
    else
    {
        JXC_ERROR("Assert failed [{}:{}]: {}", jxc::detail::get_base_filename(file), line, cond);
        _jxc_abort();
    }
}


#if JXC_DEBUG_MEM

JXC_BEGIN_NAMESPACE(jxc::detail)

DebugAllocationCheck::DebugAllocationCheck(const char* label, bool assert_on_fail, bool* out_has_leaks)
    : label(label)
    , assert_on_fail(assert_on_fail)
    , out_has_leaks(out_has_leaks)
    , start_allocator_count(_jxc_debug_num_allocator_allocations())
    , start_global_count(_jxc_debug_num_global_new_free_allocations())
{}


DebugAllocationCheck::~DebugAllocationCheck()
{
    const int64_t cur_allocator_count = _jxc_debug_num_allocator_allocations();
    const int64_t cur_global_count = _jxc_debug_num_global_new_free_allocations();
    if (cur_allocator_count == start_allocator_count && cur_global_count == start_global_count)
    {
        if (out_has_leaks)
        {
            *out_has_leaks = false;
        }
        return;
    }

    if (cur_allocator_count != start_allocator_count)
    {
        jxc::print(stderr, "[{}] MEMORY LEAK: Expected {} JXC allocations, got {}\n", label, start_allocator_count, cur_allocator_count);
    }
    if (cur_global_count != start_global_count)
    {
        jxc::print(stderr, "[{}] MEMORY LEAK: Expected {} global new/free, got {}\n", label, start_global_count, cur_global_count);
    }
    if (assert_on_fail)
    {
        JXC_ASSERTF(cur_allocator_count == start_allocator_count && cur_global_count == start_global_count,
            "cur_allocator_count({}) == start_allocator_count({}) && cur_global_count({}) == start_global_count({})",
            cur_allocator_count, start_allocator_count, cur_global_count, start_global_count);
    }
    if (out_has_leaks)
    {
        *out_has_leaks = true;
    }
}


int64_t DebugAllocationCheck::num_allocator_allocations() const
{
    return _jxc_debug_num_allocator_allocations() - start_allocator_count;
}


bool DebugAllocationCheck::no_leaks() const
{
    const int64_t cur_allocator_count = _jxc_debug_num_allocator_allocations();
    const int64_t cur_global_count = _jxc_debug_num_global_new_free_allocations();
    return cur_allocator_count == start_allocator_count && cur_global_count == start_global_count;
}


JXC_END_NAMESPACE(jxc::detail)


using jxc_mem_tracker_t = std::unordered_map<void*, size_t>;
static jxc_mem_tracker_t s_mem_tracker;
static int64_t num_global_new_free_allocations = 0;

void _jxc_debug_allocator_notify_allocation(void* ptr, size_t num_bytes)
{
    JXC_ASSERTF(!s_mem_tracker.contains(ptr),
        "[JXC DEBUG ALLOCATOR] allocate({:#x}, num_bytes={}) already has an allocation at that address", reinterpret_cast<uintptr_t>(ptr), num_bytes);
    s_mem_tracker.insert({ ptr, num_bytes });
}

void _jxc_debug_allocator_notify_deallocation(void* ptr, size_t num_bytes)
{
    auto iter = s_mem_tracker.find(ptr);
    JXC_ASSERTF(iter != s_mem_tracker.end(),
        "[JXC DEBUG ALLOCATOR] deallocate({:#x}, num_bytes={}) does not have a matching allocation", reinterpret_cast<uintptr_t>(ptr), num_bytes);
    JXC_ASSERTF(num_bytes == 0 || iter->second == num_bytes,
        "[JXC DEBUG ALLOCATOR] allocation size mismatch: deallocate({:#x}, num_bytes={}) found existing allocation for {} bytes", reinterpret_cast<uintptr_t>(ptr), num_bytes, iter->second);
    s_mem_tracker.erase(ptr);
}

void _jxc_debug_allocator_print_all_allocations(const char* prefix)
{
    std::vector<void*> keys;
    keys.reserve(s_mem_tracker.size());
    for (const auto& pair : s_mem_tracker)
    {
        keys.push_back(pair.first);
    }
    std::sort(keys.begin(), keys.end());
    for (void* ptr : keys)
    {
        jxc::print("[JXC DEBUG ALLOCATOR] {} {:#x} ({} bytes)\n", prefix, reinterpret_cast<uintptr_t>(ptr), s_mem_tracker[ptr]);
    }
}

int64_t _jxc_debug_num_allocator_allocations()
{
    return static_cast<int64_t>(s_mem_tracker.size());
}

int64_t _jxc_debug_num_global_new_free_allocations()
{
    return num_global_new_free_allocations;
}

//void* operator new(std::size_t size)
//{
//    // we are required to return non-null
//    void* mem = std::malloc(size == 0 ? 1 : size);
//    if (mem == nullptr)
//    {
//        throw std::bad_alloc();
//    }
//    ++num_global_new_free_allocations;
//    return mem;
//}
//
//void operator delete(void* mem)
//{
//    --num_global_new_free_allocations;
//    std::free(mem);
//}

#endif // #if JXC_DEBUG_MEM


JXC_BEGIN_NAMESPACE(jxc)


bool detail::find_line_and_col(std::string_view buf, size_t idx, size_t& out_line, size_t& out_col)
{
    if (idx == invalid_idx || idx >= buf.size())
    {
        return false;
    }

    size_t line = 1;
    size_t col = 1;

    const char* ptr = buf.data();
    for (size_t i = 0; i < buf.size(); i++)
    {
        if (i == idx)
        {
            break;
        }
        else if (ptr[i] == '\n')
        {
            ++line;
            col = 1;
        }
        else
        {
            ++col;
        }
    }

    out_line = line;
    out_col = col;
    return true;
}


void ErrorInfo::reset()
{
    is_err = false;
    message.clear();
    buffer_start_idx = 0;
    buffer_end_idx = 0;
    line = 0;
    col = 0;
}


std::string ErrorInfo::to_string(std::string_view buffer) const
{
    if (!is_err)
    {
        return "Success";
    }

    if (buffer_start_idx == invalid_idx && buffer_end_idx == invalid_idx && line == 0 && col == 0)
    {
        return message;
    }

    auto make_preview = [this, &buffer](const std::string& prefix) -> std::string
    {
        if (buffer_start_idx < buffer.size() && buffer_end_idx < buffer.size() && buffer_end_idx >= buffer_start_idx)
        {
            return prefix + detail::debug_string_repr(buffer.substr(buffer_start_idx, buffer_end_idx - buffer_start_idx), '`');
        }
        else if (buffer_start_idx < buffer.size())
        {
            return prefix + detail::debug_char_repr((uint32_t)buffer[buffer_start_idx], '`');
        }
        return std::string{};
    };

    const char* sep = (message.size() > 0) ? " " : "";

    if (line > 0)
    {
        return jxc::format("{}{}(line {}, col {}{})", message, sep, line, col, make_preview(", "));
    }
    else if (buffer_start_idx != invalid_idx && buffer_end_idx != invalid_idx)
    {
        return jxc::format("{}{}(index {}..{}{})", message, sep, buffer_start_idx, buffer_end_idx, make_preview(", "));
    }
    else if (buffer_start_idx != invalid_idx)
    {
        return jxc::format("{}{}(index {}{})", message, sep, buffer_start_idx, make_preview(", "));
    }

    return "Unknown error";
}


JXC_END_NAMESPACE(jxc)
