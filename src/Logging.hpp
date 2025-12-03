#pragma once

#include <SDL3/SDL_log.h>

#include <chrono>
#include <sstream>

// clang-format off

namespace Logging {
    enum class LogLevel {
        DEBUG,
        INFO,
        WARNING,
        ERROR,
        CRITICAL
    };

    namespace Colors {
        constexpr const char* RESET  = "\033[0m";
        constexpr const char* GRAY   = "\033[90m";
        constexpr const char* GREEN  = "\033[32m";
        constexpr const char* YELLOW = "\033[33m";
        constexpr const char* PINK   = "\033[35m";
        constexpr const char* RED    = "\033[31m";
    }

    // parse filename from path
    constexpr const char* strip_path(const char* file) {
        const char* last_separator = file;
        while (*file) {
            // Unix (/) and Windows (\) separators
            if (*file == '/' || *file == '\\') {
                last_separator = file + 1;
            }
            file++;
        }
        return last_separator;
    }

    inline std::string get_timestamp() {
        using namespace std::chrono;

        const auto now = system_clock::now();
        const auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

        const auto timer = system_clock::to_time_t(now);
        const std::tm bt = *std::localtime(&timer);

        std::ostringstream oss;
        oss << std::put_time(&bt, "%H:%M:%S");
        // Append milliseconds, padded with zeros, width 3
        oss << "." << std::setfill('0') << std::setw(3) << ms.count();
        return oss.str();
    }

    constexpr const char* get_prefix_color(LogLevel level) {
        switch (level) {
            case LogLevel::DEBUG:    return Colors::GRAY;
            case LogLevel::INFO:     return Colors::GREEN;
            case LogLevel::WARNING:  return Colors::YELLOW;
            case LogLevel::ERROR:    return Colors::PINK;
            case LogLevel::CRITICAL: return Colors::RED;
        }
        return Colors::RESET;
    }

    constexpr const char* get_level_string(LogLevel level) {
        switch (level) {
            case LogLevel::DEBUG:    return "[DEBUG]";
            case LogLevel::INFO:     return "[INFO] ";
            case LogLevel::WARNING:  return "[WARN] ";
            case LogLevel::ERROR:    return "[ERROR]";
            case LogLevel::CRITICAL: return "[CRIT] ";
        }
        return "[?LEVEL]";
    }


    // Base recursion
    inline void log_print_parts(std::ostream& os) {
        os << std::endl;
    }

    template<typename T, typename... Args>
    inline void log_print_parts(std::ostream& os, const T& first, const Args&... rest) {
        os << first;
        log_print_parts(os, rest...);
    }

    template<typename... Args>
    void log_message(LogLevel level, const char* file, int line, const Args&... args) {
        const char* color = get_prefix_color(level);
        const char* level_str = get_level_string(level);

        std::ostringstream oss;
        oss << color << get_timestamp() << " " << level_str << " (" << file << ":" << line << ") " << Colors::RESET;

        log_print_parts(oss, args...);

        const std::string msg = oss.str();

        switch (level) {
            case LogLevel::DEBUG:
                SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "%s", msg.c_str());
                break;
            case LogLevel::INFO:
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "%s", msg.c_str());
                break;
            case LogLevel::WARNING:
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "%s", msg.c_str());
                break;
            case LogLevel::ERROR:
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", msg.c_str());
                break;
            case LogLevel::CRITICAL:
                SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "%s", msg.c_str());
                break;
        }
    }
}

#define log_debug(...) log_message(Logging::LogLevel::DEBUG, Logging::strip_path(__FILE__), __LINE__, __VA_ARGS__)
#define log_info(...) log_message(Logging::LogLevel::INFO, Logging::strip_path(__FILE__), __LINE__, __VA_ARGS__)
#define log_warning(...) log_message(Logging::LogLevel::WARNING, Logging::strip_path(__FILE__), __LINE__, __VA_ARGS__)
#define log_error(...) log_message(Logging::LogLevel::ERROR, Logging::strip_path(__FILE__), __LINE__, __VA_ARGS__)
#define log_critical(...) log_message(Logging::LogLevel::CRITICAL, Logging::strip_path(__FILE__), __LINE__, __VA_ARGS__)

