#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <future>
// #include <print>  // not until GCC 14
#include <iostream>
#include <stdexcept>
#include <string>

namespace asio  = boost::asio;
namespace ssl   = asio::ssl;
namespace beast = boost::beast;
namespace http  = beast::http;
using tcp = asio::ip::tcp;

static std::string env_or(const char* k, const char* def) {
    if (const char* v = std::getenv(k)) return v;
    return def;
}

static std::string today_yyyy_mm_dd_local() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[11];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tm);
    return buf;
}

static std::string datetime_local() {
  std::time_t t = std::time(nullptr);
  std::tm tm{};
  localtime_r(&t, &tm);
  char buf[30];
  std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
  return buf;
}

// Extremely small "JSON" extractor for demo purposes.
// Looks for:  "key": "value"   or  "key": 0.123
static std::string extract_json_value(const std::string& s, const char* key) {
    const std::string needle = std::string("\"") + key + "\"";
    auto pos = s.find(needle);
    if (pos == std::string::npos) throw std::runtime_error("Key not found: " + std::string(key));

    pos = s.find(':', pos);
    if (pos == std::string::npos) throw std::runtime_error("Malformed JSON near: " + std::string(key));
    ++pos;

    while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos]))) ++pos;

    if (pos < s.size() && s[pos] == '"') {
        ++pos;
        auto end = s.find('"', pos);
        if (end == std::string::npos) throw std::runtime_error("Unterminated string for: " + std::string(key));
        return s.substr(pos, end - pos);
    } else {
        // number / bool / null — read until comma or end brace
        auto end = pos;
        while (end < s.size() && s[end] != ',' && s[end] != '}' && !std::isspace(static_cast<unsigned char>(s[end])))
            ++end;
        return s.substr(pos, end - pos);
    }
}

static std::future<std::string> https_get_body_promise(std::string host, std::string target) {
    std::promise<std::string> p;
    auto fut = p.get_future();

    std::thread([host = std::move(host), target = std::move(target), p = std::move(p)]() mutable {
        try {
            asio::io_context ioc;
            ssl::context ctx{ssl::context::tls_client};

            // NOTE: For production, validate certs properly:
            // ctx.set_default_verify_paths();
            // stream.set_verify_mode(ssl::verify_peer);
            // and consider hostname verification.
            beast::ssl_stream<beast::tcp_stream> stream{ioc, ctx};

            tcp::resolver resolver{ioc};
            auto const results = resolver.resolve(host, "443");
            beast::get_lowest_layer(stream).connect(results);

            stream.handshake(ssl::stream_base::client);

            http::request<http::empty_body> req{http::verb::get, target, 11};
            req.set(http::field::host, host);
            req.set(http::field::user_agent, "moonphase-demo/1.0");

            http::write(stream, req);

            beast::flat_buffer buffer;
            http::response<http::string_body> res;
            http::read(stream, buffer, res);

            beast::error_code ec;
            stream.shutdown(ec); // ignore shutdown errors

            if (res.result() != http::status::ok) {
                throw std::runtime_error("HTTP error: " + std::to_string(res.result_int()));
            }

            p.set_value(std::move(res.body()));
        } catch (...) {
            p.set_exception(std::current_exception());
        }
    }).detach();

    return fut;
}

std::string ansi_reset_code() {
  return std::string("\e[0m");
}

std::string ansi_gray_code() {
  return std::string("\e[38;5;240m");
}

int main() {
    const std::string date = env_or("USNO_DATE", today_yyyy_mm_dd_local().c_str());
    const std::string lat  = env_or("USNO_LAT", "40.7128");   // default: NYC-ish
    const std::string lon  = env_or("USNO_LON", "-74.0060");
    const std::string tz   = env_or("USNO_TZ",  "-5");        // Eastern Standard

    const std::string host   = "aa.usno.navy.mil";
    const std::string target =
        "/api/rstt/oneday?date=" + date + "&coords=" + lat + "," + lon + "&tz=" + tz;

    std::cout << ansi_gray_code() 
      << datetime_local().c_str() << " - Fetching data..." 
      << ansi_reset_code() << '\n';

    try {
        auto body_fut = https_get_body_promise(host, target);
        const std::string json = body_fut.get();

        const std::string phase = extract_json_value(json, "curphase");
        const std::string frac  = extract_json_value(json, "fracillum"); // e.g. 0.734

        double f = std::stod(frac);
        // std::println("USNO Moon phase on {}: {} (illumination {:.1f}%)", date, phase, f * 100.0);
        std::cout << phase << ' '  << '(' << f << "%)" << '\n';
    } catch (const std::exception& e) {
        // std::println("Error: {}", e.what());
      std::cerr << "Error: " << e.what();
        return 1;
    }
}

