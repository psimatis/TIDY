#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

using Time = std::uint32_t;
using Id = std::uint32_t;

static void write_operation(std::ofstream& output, char operation, Time first, Time second) {
    char line[128];
    char* cursor = line;
    *cursor++ = operation;
    *cursor++ = ' ';
    cursor = std::to_chars(cursor, line + 32, first).ptr;
    *cursor++ = ' ';
    cursor = std::to_chars(cursor, line + 64, second).ptr;
    *cursor++ = ' ';
    *cursor++ = '0';
    *cursor++ = ' ';
    *cursor++ = '0';
    *cursor++ = '\n';
    output.write(line, cursor - line);
}

static std::vector<Id> counting_order(
    const std::vector<Time>& timestamps, Time domain
) {
    std::vector<std::uint64_t> offsets(static_cast<std::size_t>(domain) + 2, 0);
    for (Time timestamp : timestamps)
        offsets[static_cast<std::size_t>(timestamp) + 1]++;
    for (std::size_t i = 1; i < offsets.size(); i++)
        offsets[i] += offsets[i - 1];

    std::vector<std::uint64_t> cursors = offsets;
    std::vector<Id> order(timestamps.size());
    for (std::uint64_t id = 0; id < timestamps.size(); id++) {
        Time timestamp = timestamps[id];
        order[cursors[timestamp]++] = static_cast<Id>(id);
    }
    return order;
}

int main(int argc, char** argv) {
    if (argc != 8) {
        std::cerr
            << "Usage: " << argv[0]
            << " OUTPUT RECORDS DOMAIN QUERIES SEED LAMBDA EXTENT_FRACTION\n";
        return 1;
    }

    const fs::path output_base = fs::absolute(argv[1]);
    const std::uint64_t records = std::stoull(argv[2]);
    const Time domain = static_cast<Time>(std::stoul(argv[3]));
    const std::uint64_t query_count = std::stoull(argv[4]);
    const std::uint64_t seed = std::stoull(argv[5]);
    const double lambda = std::stod(argv[6]);
    const double extent_fraction = std::stod(argv[7]);

    if (records == 0 || records > std::numeric_limits<Id>::max())
        throw std::invalid_argument("RECORDS must fit in a 32-bit record identifier");
    if (domain == 0 || query_count == 0 || lambda <= 0)
        throw std::invalid_argument("DOMAIN, QUERIES, and LAMBDA must be positive");
    if (extent_fraction <= 0 || extent_fraction >= 1)
        throw std::invalid_argument("EXTENT_FRACTION must be between zero and one");
    if (output_base.filename() != "1b" ||
        output_base.parent_path().filename() != "poisson_duration" ||
        output_base.parent_path().parent_path().filename() != "synthetic") {
        throw std::invalid_argument(
            "Output must be data/stream/synthetic/poisson_duration/1b"
        );
    }

    const std::string lambda_name = std::to_string(static_cast<std::uint64_t>(lambda));
    const std::string dataset_name = "poisson_lambda_" + lambda_name;
    const fs::path output_dir = output_base / dataset_name;
    fs::create_directories(output_dir);
    const fs::path stream_path =
        output_dir / (dataset_name + "_stream_dom0p001.stream");

    std::mt19937_64 rng(seed + static_cast<std::uint64_t>(lambda));
    std::poisson_distribution<std::uint64_t> duration_distribution(lambda);
    std::vector<Time> starts(records);
    std::vector<Time> ends(records);

    long double duration_sum = 0;
    long double duration_square_sum = 0;
    long double duration_cube_sum = 0;
    Time minimum_duration = std::numeric_limits<Time>::max();
    Time maximum_duration = 0;
    Time minimum_time = domain;
    Time maximum_time = 0;

    for (std::uint64_t id = 0; id < records; id++) {
        const std::uint64_t sampled_duration = duration_distribution(rng);
        if (sampled_duration > domain)
            throw std::runtime_error("Generated duration exceeds configured domain");
        const Time duration = static_cast<Time>(sampled_duration);
        std::uniform_int_distribution<Time> start_distribution(0, domain - duration);
        const Time start = start_distribution(rng);
        const Time end = start + duration;
        starts[id] = start;
        ends[id] = end;
        minimum_time = std::min(minimum_time, start);
        maximum_time = std::max(maximum_time, end);
        minimum_duration = std::min(minimum_duration, duration);
        maximum_duration = std::max(maximum_duration, duration);
        duration_sum += duration;
        duration_square_sum += static_cast<long double>(duration) * duration;
        duration_cube_sum +=
            static_cast<long double>(duration) * duration * duration;
    }

    for (std::uint64_t id = 0; id < records; id++) {
        starts[id] -= minimum_time;
        ends[id] -= minimum_time;
    }
    maximum_time -= minimum_time;

    std::cerr << dataset_name << ": ordering start events\n";
    std::vector<Id> start_order = counting_order(starts, domain);
    std::cerr << dataset_name << ": ordering end events\n";
    std::vector<Id> end_order = counting_order(ends, domain);
    std::vector<Id> stream_ids(records);
    for (std::uint64_t position = 0; position < records; position++)
        stream_ids[start_order[position]] = static_cast<Id>(position);

    std::ofstream output(stream_path, std::ios::binary | std::ios::trunc);
    if (!output)
        throw std::runtime_error("Cannot open output stream: " + stream_path.string());
    std::vector<char> output_buffer(8 * 1024 * 1024);
    output.rdbuf()->pubsetbuf(output_buffer.data(), output_buffer.size());

    const Time query_extent =
        std::max<Time>(1, static_cast<Time>(domain * extent_fraction));
    const std::uint64_t total_events = records * 2;
    const std::uint64_t stride = std::max<std::uint64_t>(1, total_events / query_count);
    std::uint64_t starts_written = 0;
    std::uint64_t ends_written = 0;
    std::uint64_t events_written = 0;
    std::uint64_t queries_written = 0;
    Time maximum_seen_time = 0;

    auto emit_query = [&]() {
        if (maximum_seen_time < query_extent)
            return false;
        std::uniform_int_distribution<Time> query_end_distribution(
            query_extent, maximum_seen_time
        );
        const Time query_end = query_end_distribution(rng);
        write_operation(output, 'Q', query_end - query_extent, query_end);
        queries_written++;
        return true;
    };

    std::cerr << dataset_name << ": writing " << stream_path << "\n";
    while (starts_written < records || ends_written < records) {
        bool write_start =
            ends_written == records ||
            (starts_written < records &&
             starts[start_order[starts_written]] <= ends[end_order[ends_written]]);
        if (write_start) {
            const Id original_id = start_order[starts_written++];
            const Time timestamp = starts[original_id];
            write_operation(output, 'S', stream_ids[original_id], timestamp);
            maximum_seen_time = timestamp;
        } else {
            const Id original_id = end_order[ends_written++];
            const Time timestamp = ends[original_id];
            write_operation(output, 'E', stream_ids[original_id], timestamp);
            maximum_seen_time = timestamp;
        }
        events_written++;
        if (queries_written < query_count && events_written % stride == 0)
            emit_query();
        if (events_written % 100000000 == 0)
            std::cerr << dataset_name << ": wrote " << events_written << " events\n";
    }
    while (queries_written < query_count)
        emit_query();
    output.close();
    // metadata.json marks a complete stream, so never write it after a failed write.
    if (!output)
        throw std::runtime_error("Failed writing output stream: " + stream_path.string());

    const long double mean = duration_sum / records;
    const long double variance =
        duration_square_sum / records - mean * mean;
    const long double standard_deviation = std::sqrt(std::max<long double>(0, variance));
    const long double third_central_moment =
        duration_cube_sum / records -
        3 * mean * duration_square_sum / records +
        2 * mean * mean * mean;
    const long double skewness =
        standard_deviation == 0
            ? 0
            : third_central_moment /
                  (standard_deviation * standard_deviation * standard_deviation);

    std::ofstream metadata(output_dir / "metadata.json", std::ios::trunc);
    metadata << "{\n"
             << "  \"dataset\": \"" << dataset_name << "\",\n"
             << "  \"records\": " << records << ",\n"
             << "  \"lambda\": " << lambda << ",\n"
             << "  \"duration_mean\": " << static_cast<double>(mean) << ",\n"
             << "  \"duration_stddev\": "
             << static_cast<double>(standard_deviation) << ",\n"
             << "  \"duration_skewness\": " << static_cast<double>(skewness)
             << ",\n"
             << "  \"duration_min\": " << minimum_duration << ",\n"
             << "  \"duration_max\": " << maximum_duration << ",\n"
             << "  \"configured_domain\": " << domain << ",\n"
             << "  \"observed_domain\": " << maximum_time << ",\n"
             << "  \"mean_duration_fraction\": "
             << static_cast<double>(mean / domain) << ",\n"
             << "  \"queries\": " << queries_written << "\n"
             << "}\n";

    std::cerr << dataset_name << ": complete; records=" << records
              << ", queries=" << queries_written
              << ", mean_duration=" << static_cast<double>(mean) << "\n";
    return 0;
}
