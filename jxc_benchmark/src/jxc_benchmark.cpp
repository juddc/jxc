#include <vector>
#include <string>
#include <string_view>
#include "jxc/jxc.h"
#include "jxc/jxc_format.h"
#include "jxc_cpp/jxc_value.h"
#include "jxc_cpp/jxc_document.h"


#if !defined(COMPARE_AGAINST_NLOHMANN_JSON) && __has_include("nlohmann/json.hpp")
#define COMPARE_AGAINST_NLOHMANN_JSON 1
#else
#define COMPARE_AGAINST_NLOHMANN_JSON 0
#endif

#if COMPARE_AGAINST_NLOHMANN_JSON
#include <nlohmann/json.hpp>
#endif


struct Args
{
    int32_t num_iters = 64;
	std::vector<std::string> files;

	static Args parse(int argc, const char** argv)
	{
        Args result;
        bool default_num_iters = true;

        if (argc > 1)
        {
            std::vector<std::string_view> args;
            for (int i = 1; i < argc; i++)
            {
                args.push_back(std::string_view(argv[i]));
            }

            for (size_t i = 0; i < args.size(); i++)
            {
                if (args[i] == "/?" || args[i] == "-h" || args[i] == "-help" || args[i] == "--help")
                {
                    jxc::print("Usage: {} [--num-iters 64] [--input path/to/file.jxc]\n", argv[0]);
                    exit(0);
                }
            }

            size_t idx = 0;
            while (idx < args.size() - 1)
            {
                if (args[idx] == "-n" || args[idx] == "--num-iters")
                {
                    ++idx;
                    result.num_iters = static_cast<int32_t>(strtol(args[idx].data(), nullptr, 10));
                    default_num_iters = false;

                    if (result.num_iters <= 0)
                    {
                        jxc::print(stderr, "Invalid value for num-iters: {}\n", args[idx]);
                        exit(1);
                    }

                    ++idx;
                }
                else if (args[idx] == "-i" || args[idx] == "--input")
                {
                    ++idx;
                    while (idx < args.size() && !args[idx].starts_with('-'))
                    {
                        result.files.push_back(std::string(args[idx]));
                        ++idx;
                    }
                }
                else
                {
                    jxc::print(stderr, "Invalid argument {}\n", args[idx]);
                    exit(1);
                }
            }
        }

#if JXC_DEBUG
        // much smaller default for debug builds because they take longer
        if (default_num_iters)
        {
            result.num_iters = 16;
        }
#endif

        return result;
	}
};


template<typename Lambda>
int64_t run_benchmark_and_get_average_runtime_ns(int64_t num_iters, Lambda&& callback)
{
    if (num_iters <= 0)
    {
        return 0;
    }

    std::vector<int64_t> benchmark_times_ns;
    benchmark_times_ns.reserve((size_t)num_iters);

    jxc::detail::Timer timer;

    for (int64_t i = 0; i < num_iters; i++)
    {
        timer.reset();
        callback();
        const int64_t elapsed_ns = timer.elapsed().count();
        benchmark_times_ns.push_back(elapsed_ns);
    }

    int64_t total_elapsed_ns = 0;
    for (int64_t val : benchmark_times_ns)
    {
        total_elapsed_ns += val;
    }

    return total_elapsed_ns / num_iters;
}


void jump_parser_benchmark(std::string_view buf)
{
    jxc::JumpParser parser(buf);
    while (parser.next())
    {
        const jxc::Element& ele = parser.value();
        if (ele.type == jxc::ElementType::Invalid)
        {
            break;
        }
    }

    if (parser.has_error())
    {
        jxc::ErrorInfo err = parser.get_error();
        err.get_line_and_col_from_buffer(buf);
        JXC_ASSERTF(!parser.has_error(), "ParseError: {}", err.to_string(buf));
    }
}


int main(int argc, const char** argv)
{
    auto args = Args::parse(argc, argv);
    if (args.files.size() == 0)
    {
        jxc::print(stderr, "No files to benchmark. See --help for usage.\n");
        return 1;
    }

    for (const auto& path : args.files)
    {
        jxc::print("Input = {}\n", path);
    }
    jxc::print("Num iters = {}\n", args.num_iters);

    std::vector<std::string> file_data;
    size_t file_data_size = 0;

    for (const auto& path : args.files)
    {
        std::string err;
        if (auto data = jxc::detail::read_file_to_string(path, &err))
        {
            file_data_size += data->size();
            file_data.push_back(data.value());
        }
        else
        {
            jxc::print(stderr, "Failed reading input file {}: {}\n", path, err);
            return 1;
        }
    }

    const double file_data_size_mb = (double)file_data_size / 1024.0 / 1024.0;
    jxc::print("Total File Size = {} bytes ({:.2f} MB)\n", file_data_size, file_data_size_mb);

    jxc::print("\n");

    auto benchmark_result_to_string = [](int64_t runtime_ns, int32_t num_iters) -> std::string
    {
        return jxc::format("Average {}ns ({:.4f} ms) over {} iterations\n",
            runtime_ns, jxc::detail::Timer::ns_to_ms(runtime_ns), num_iters);
    };

    const int64_t parser_avg_runtime_ns = run_benchmark_and_get_average_runtime_ns(args.num_iters, [&]()
    {
        for (const auto& data : file_data)
        {
            jump_parser_benchmark(data);
        }
    });

    jxc::print("Parser-only benchmark: {}\n", benchmark_result_to_string(parser_avg_runtime_ns, args.num_iters));

    {
        const int64_t doc_value_avg_runtime_ns = run_benchmark_and_get_average_runtime_ns(args.num_iters, [&]()
        {
            jxc::ErrorInfo err;
            const bool try_return_view = false;
            for (const std::string& data : file_data)
            {
                jxc::Value result = jxc::parse(data, err, try_return_view);
                if (err)
                {
                    err.get_line_and_col_from_buffer(data);
                    JXC_ASSERTF(!err.is_err, "Parse error: {}", err.to_string(data));
                }
            }
        });

        jxc::print("Value parser benchmark: {}\n", benchmark_result_to_string(doc_value_avg_runtime_ns, args.num_iters));
    }

#if COMPARE_AGAINST_NLOHMANN_JSON
    {
        bool all_json_files = true;
        for (const auto& path : args.files)
        {
            if (!path.ends_with(".json"))
            {
                all_json_files = false;
                break;
            }
        }

        if (all_json_files)
        {
            const int64_t nlohmann_json_avg_runtime_ns = run_benchmark_and_get_average_runtime_ns(args.num_iters, [&]()
            {
                for (const std::string& data : file_data)
                {
                    nlohmann::json result = nlohmann::json::parse(data);
                }
            });

            jxc::print("nlohmann_json parser benchmark: {}\n", benchmark_result_to_string(nlohmann_json_avg_runtime_ns, args.num_iters));
        }
    }
#endif

	return 0;
}
