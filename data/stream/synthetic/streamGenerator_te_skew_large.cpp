#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
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

static std::string fraction_token(double value) {
    if (std::floor(value) == value)
        return std::to_string(static_cast<long long>(value));
    std::ostringstream out;
    out << value;
    std::string text = out.str();
    for (char& ch : text) {
        if (ch == '.')
            ch = 'p';
    }
    return text;
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

// Truncated Zipf on {1, ..., max_duration}: P(k) ∝ k^{-s}.
class TruncatedZipf {
public:
    TruncatedZipf(Time max_duration, double exponent) : max_duration_(max_duration) {
        if (max_duration == 0)
            throw std::invalid_argument("MAX_DURATION must be positive");
        if (exponent <= 1.0)
            throw std::invalid_argument("ZIPF_EXPONENT must be > 1");
        cdf_.resize(static_cast<std::size_t>(max_duration) + 1, 0.0);
        long double sum = 0;
        for (Time k = 1; k <= max_duration; k++) {
            sum += std::pow(static_cast<long double>(k), -exponent);
            cdf_[k] = static_cast<double>(sum);
        }
        const double total = cdf_[max_duration];
        for (Time k = 1; k <= max_duration; k++)
            cdf_[k] /= total;
    }

    template <typename RNG>
    Time operator()(RNG& rng) const {
        std::uniform_real_distribution<double> unit(0.0, 1.0);
        const double u = unit(rng);
        const auto it = std::lower_bound(cdf_.begin() + 1, cdf_.end(), u);
        return static_cast<Time>(it - cdf_.begin());
    }

private:
    Time max_duration_;
    std::vector<double> cdf_;
};

int main(int argc, char** argv) {
    if (argc != 11 && argc != 12) {
        std::cerr
            << "Usage: " << argv[0]
            << " OUTPUT RECORDS DOMAIN QUERIES SEED ZIPF_EXPONENT "
            << "MAX_DURATION_FRACTION HOTSPOT_WIDTH CLUSTER_FRACTION EXTENT_FRACTION "
            << "[QUERY_MODE]\n"
            << "  QUERY_MODE is 'uniform' (default) or 'hotspot'\n";
        return 1;
    }

    const fs::path output_base = fs::absolute(argv[1]);
    const std::uint64_t records = std::stoull(argv[2]);
    const Time domain = static_cast<Time>(std::stoul(argv[3]));
    const std::uint64_t query_count = std::stoull(argv[4]);
    const std::uint64_t seed = std::stoull(argv[5]);
    const double zipf_exponent = std::stod(argv[6]);
    const double max_duration_fraction = std::stod(argv[7]);
    const double hotspot_width_fraction = std::stod(argv[8]);
    const double cluster_fraction = std::stod(argv[9]);
    const double extent_fraction = std::stod(argv[10]);
    const std::string query_mode = argc == 12 ? argv[11] : "uniform";

    if (records == 0 || records > std::numeric_limits<Id>::max())
        throw std::invalid_argument("RECORDS must fit in a 32-bit record identifier");
    if (domain == 0 || query_count == 0)
        throw std::invalid_argument("DOMAIN and QUERIES must be positive");
    if (zipf_exponent <= 1.0)
        throw std::invalid_argument("ZIPF_EXPONENT must be > 1");
    if (max_duration_fraction <= 0 || max_duration_fraction >= 1)
        throw std::invalid_argument("MAX_DURATION_FRACTION must be between zero and one");
    if (hotspot_width_fraction <= 0 || hotspot_width_fraction >= 1)
        throw std::invalid_argument("HOTSPOT_WIDTH must be between zero and one");
    if (cluster_fraction < 0 || cluster_fraction > 1)
        throw std::invalid_argument("CLUSTER_FRACTION must be in [0, 1]");
    if (extent_fraction <= 0 || extent_fraction >= 1)
        throw std::invalid_argument("EXTENT_FRACTION must be between zero and one");
    if (query_mode != "uniform" && query_mode != "hotspot")
        throw std::invalid_argument("QUERY_MODE must be 'uniform' or 'hotspot'");
    if (output_base.filename().string().rfind("1b", 0) != 0 ||
        output_base.parent_path().filename() != "te_skew" ||
        output_base.parent_path().parent_path().filename() != "synthetic") {
        throw std::invalid_argument(
            "Output must be data/stream/synthetic/te_skew/1b*"
        );
    }

    const Time max_duration =
        std::max<Time>(1, static_cast<Time>(domain * max_duration_fraction));
    const Time hotspot_width =
        std::max<Time>(1, static_cast<Time>(domain * hotspot_width_fraction));
    const Time hotspot_center = domain / 2;
    Time hotspot_lo =
        hotspot_center > hotspot_width / 2 ? hotspot_center - hotspot_width / 2 : 0;
    Time hotspot_hi = std::min(domain, hotspot_lo + hotspot_width);

    const std::string dataset_name = "te_skew_" + fraction_token(cluster_fraction);
    const fs::path output_dir = output_base / dataset_name;
    fs::create_directories(output_dir);
    const fs::path stream_path =
        output_dir / (dataset_name + "_stream_dom0p001.stream");

    std::mt19937_64 rng(
        seed + static_cast<std::uint64_t>(cluster_fraction * 10000.0 + 0.5)
    );
    TruncatedZipf duration_distribution(max_duration, zipf_exponent);
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    std::vector<Time> starts(records);
    std::vector<Time> ends(records);

    long double duration_sum = 0;
    std::uint64_t clustered_count = 0;
    Time minimum_duration = std::numeric_limits<Time>::max();
    Time maximum_duration = 0;
    Time maximum_time = 0;

    for (std::uint64_t id = 0; id < records; id++) {
        const Time duration = duration_distribution(rng);
        Time end = 0;
        if (unit(rng) < cluster_fraction) {
            clustered_count++;
            std::uniform_int_distribution<Time> hotspot_end(hotspot_lo, hotspot_hi);
            end = hotspot_end(rng);
            end = std::max(end, duration);
            end = std::min(end, domain);
        } else {
            std::uniform_int_distribution<Time> start_distribution(0, domain - duration);
            const Time start = start_distribution(rng);
            end = start + duration;
        }
        const Time start = end - duration;
        starts[id] = start;
        ends[id] = end;
        maximum_time = std::max(maximum_time, end);
        minimum_duration = std::min(minimum_duration, duration);
        maximum_duration = std::max(maximum_duration, duration);
        duration_sum += duration;
        if ((id + 1) % 100000000 == 0)
            std::cerr << dataset_name << ": sampled " << (id + 1) << " intervals\n";
    }

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

    // Adversarial placement for Q2: every query ends immediately *before* the
    // hotspot, so its range covers only the sparse region while the scan window
    // [q.t_e, q.t_e+delta] runs straight into the burst of end times. No record
    // of the hotspot can end inside the query range itself.
    // The window is deliberately independent of the hotspot width: it must stay
    // narrow relative to the duration threshold delta, otherwise the scan window
    // of a query drawn from its far end never reaches the hotspot at all. Fixing
    // it in absolute terms also keeps hotspot widths comparable to each other.
    const Time query_jitter = std::max<Time>(1, query_extent / 10);
    const Time hotspot_query_hi =
        std::max(query_extent, hotspot_lo > 0 ? hotspot_lo - 1 : 0);
    const Time hotspot_query_lo = std::max(
        query_extent, hotspot_lo > query_jitter ? hotspot_lo - query_jitter : 0
    );

    auto count_not_after = [](const std::vector<Time>& values,
                              const std::vector<Id>& order, Time bound) {
        std::uint64_t low = 0;
        std::uint64_t high = order.size();
        while (low < high) {
            const std::uint64_t middle = low + (high - low) / 2;
            if (values[order[middle]] <= bound)
                low = middle + 1;
            else
                high = middle;
        }
        return low;
    };

    // Queries can only be emitted once ingestion has reached their end time,
    // so in hotspot mode we spread them over the suffix of the stream.
    const std::uint64_t first_query_event =
        query_mode == "hotspot"
            ? std::min(
                  total_events,
                  count_not_after(starts, start_order, hotspot_query_hi) +
                      count_not_after(ends, end_order, hotspot_query_hi)
              )
            : 0;
    const std::uint64_t stride = std::max<std::uint64_t>(
        1, (total_events - first_query_event) / query_count
    );

    std::uint64_t starts_written = 0;
    std::uint64_t ends_written = 0;
    std::uint64_t events_written = 0;
    std::uint64_t queries_written = 0;
    Time maximum_seen_time = 0;

    auto emit_query = [&]() {
        Time query_end;
        if (query_mode == "hotspot") {
            std::uniform_int_distribution<Time> hotspot_query_distribution(
                hotspot_query_lo, hotspot_query_hi
            );
            query_end = hotspot_query_distribution(rng);
        } else {
            if (maximum_seen_time < query_extent)
                return false;
            std::uniform_int_distribution<Time> query_end_distribution(
                query_extent, maximum_seen_time
            );
            query_end = query_end_distribution(rng);
        }
        if (query_end < query_extent || query_end > maximum_seen_time)
            return false;
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
        if (queries_written < query_count && events_written > first_query_event &&
            (events_written - first_query_event) % stride == 0)
            emit_query();
        if (events_written % 100000000 == 0)
            std::cerr << dataset_name << ": wrote " << events_written << " events\n";
    }
    if (queries_written < query_count && query_mode == "hotspot" &&
        maximum_seen_time < hotspot_query_lo) {
        throw std::runtime_error(
            "stream never reached the hotspot query window; cannot place queries"
        );
    }
    while (queries_written < query_count)
        emit_query();
    output.close();
    // metadata.json marks a complete stream, so never write it after a failed write.
    if (!output)
        throw std::runtime_error("Failed writing output stream: " + stream_path.string());

    const long double mean = duration_sum / records;
    std::ofstream metadata(output_dir / "metadata.json", std::ios::trunc);
    metadata << "{\n"
             << "  \"dataset\": \"" << dataset_name << "\",\n"
             << "  \"records\": " << records << ",\n"
             << "  \"cluster_fraction\": " << cluster_fraction << ",\n"
             << "  \"hotspot_width_fraction\": " << hotspot_width_fraction << ",\n"
             << "  \"hotspot\": [" << hotspot_lo << ", " << hotspot_hi << "],\n"
             << "  \"clustered_count\": " << clustered_count << ",\n"
             << "  \"query_mode\": \"" << query_mode << "\",\n"
             << "  \"query_end_window\": "
             << (query_mode == "hotspot"
                     ? "[" + std::to_string(hotspot_query_lo) + ", " +
                           std::to_string(hotspot_query_hi) + "]"
                     : "null")
             << ",\n"
             << "  \"query_extent\": " << query_extent << ",\n"
             << "  \"duration_distribution\": \"zipf\",\n"
             << "  \"zipf_exponent\": " << zipf_exponent << ",\n"
             << "  \"max_duration_fraction\": " << max_duration_fraction << ",\n"
             << "  \"max_duration\": " << max_duration << ",\n"
             << "  \"duration_mean\": " << static_cast<double>(mean) << ",\n"
             << "  \"duration_min\": " << minimum_duration << ",\n"
             << "  \"duration_max\": " << maximum_duration << ",\n"
             << "  \"configured_domain\": " << domain << ",\n"
             << "  \"observed_domain\": " << maximum_time << ",\n"
             << "  \"mean_duration_fraction\": "
             << static_cast<double>(mean / domain) << ",\n"
             << "  \"queries\": " << queries_written << "\n"
             << "}\n";

    std::cerr << dataset_name << ": complete; records=" << records
              << ", clustered=" << clustered_count
              << ", duration_mean=" << static_cast<double>(mean)
              << ", queries=" << queries_written << "\n";
    return 0;
}
