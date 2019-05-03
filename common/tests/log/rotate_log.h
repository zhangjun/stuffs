//
// Copyright(c) 2015 Gabi Melman.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)
//

#pragma once

#ifndef SPDLOG_H
#include "spdlog/spdlog.h"
#endif

#include "spdlog/details/file_helper.h"
#include "spdlog/details/null_mutex.h"
#include "spdlog/fmt/fmt.h"
#include "spdlog/sinks/base_sink.h"

#include <cerrno>
#include <chrono>
#include <ctime>
#include <mutex>
#include <string>
#include <tuple>

namespace logcommon {
namespace sinks {

//
// Rotating file sink based on size
//
template<typename Mutex>
class rotating_file_sink final : public base_sink<Mutex>
{
public:
    rotating_file_sink(filename_t base_filename, std::size_t max_size, std::size_t max_files, bool rotate_on_open=false)
        : base_filename_(std::move(base_filename))
        , max_size_(max_size)
        , max_files_(max_files)
    {
        file_helper_.open(calc_filename(base_filename_, 0));
        current_size_ = file_helper_.size(); // expensive. called only once
        if (rotate_on_open && current_size_ > 0)
        {
            rotate_();
        }
    }

    // calc filename according to index and file extension if exists.
    // e.g. calc_filename("logs/mylog.txt, 3) => "logs/mylog.3.txt".
    static filename_t calc_filename(const filename_t &filename, std::size_t index)
    {
        typename std::conditional<std::is_same<filename_t::value_type, char>::value, fmt::memory_buffer, fmt::wmemory_buffer>::type w;
        if (index != 0u)
        {
            filename_t basename, ext;
            std::tie(basename, ext) = details::file_helper::split_by_extension(filename);
            fmt::format_to(w, SPDLOG_FILENAME_T("{}.{}{}"), basename, index, ext);
        }
        else
        {
            fmt::format_to(w, SPDLOG_FILENAME_T("{}"), filename);
        }
        return fmt::to_string(w);
    }

    const filename_t &filename() const
    {
        return file_helper_.filename();
    }

protected:
    void sink_it_(const details::log_msg &msg) override
    {
        fmt::memory_buffer formatted;
        sink::formatter_->format(msg, formatted);
        current_size_ += formatted.size();
        if (current_size_ > max_size_)
        {
            rotate_();
            current_size_ = formatted.size();
        }
        file_helper_.write(formatted);
    }

    void flush_() override
    {
        file_helper_.flush();
    }

private:
    // Rotate files:
    // log.txt -> log.1.txt
    // log.1.txt -> log.2.txt
    // log.2.txt -> log.3.txt
    // log.3.txt -> delete
    void rotate_()
    {
        using details::os::filename_to_str;
        file_helper_.close();
        for (auto i = max_files_; i > 0; --i)
        {
            filename_t src = calc_filename(base_filename_, i - 1);
            if (!details::file_helper::file_exists(src))
            {
                continue;
            }
            filename_t target = calc_filename(base_filename_, i);

            if (!rename_file(src, target))
            {
                // if failed try again after a small delay.
                // this is a workaround to a windows issue, where very high rotation
                // rates can cause the rename to fail with permission denied (because of antivirus?).
                details::os::sleep_for_millis(100);
                if (!rename_file(src, target))
                {
                    file_helper_.reopen(true); // truncate the log file anyway to prevent it to grow beyond its limit!
                    current_size_ = 0;
                    throw spdlog_ex(
                        "rotating_file_sink: failed renaming " + filename_to_str(src) + " to " + filename_to_str(target), errno);
                }
            }
        }
        file_helper_.reopen(true);
    }

    // delete the target if exists, and rename the src file  to target
    // return true on success, false otherwise.
    bool rename_file(const filename_t &src_filename, const filename_t &target_filename)
    {
        // try to delete the target file in case it already exists.
        (void)details::os::remove(target_filename);
        return details::os::rename(src_filename, target_filename) == 0;
    }

    filename_t base_filename_;
    std::size_t max_size_;
    std::size_t max_files_;
    std::size_t current_size_;
    details::file_helper file_helper_;
};

using rotating_file_sink_mt = rotating_file_sink<std::mutex>;
using rotating_file_sink_st = rotating_file_sink<details::null_mutex>;



//  *****************
//   daily file sink
//  *****************

/*
 * Generator of daily log file names in format basename.YYYY-MM-DD.ext
 */
struct daily_filename_calculator
{
    // Create filename for the form basename.YYYY-MM-DD
    static filename_t calc_filename(const filename_t &filename, const tm &now_tm)
    {
        filename_t basename, ext;
        std::tie(basename, ext) = details::file_helper::split_by_extension(filename);
        std::conditional<std::is_same<filename_t::value_type, char>::value, fmt::memory_buffer, fmt::wmemory_buffer>::type w;
        fmt::format_to(
            w, SPDLOG_FILENAME_T("{}_{:04d}-{:02d}-{:02d}_{:02d}{}"), basename, now_tm.tm_year + 1900, now_tm.tm_mon + 1, now_tm.tm_mday, ext);
        return fmt::to_string(w);
    }
};

/*
 * Rotating file sink based on date. rotates at midnight
 */
template<typename Mutex, typename FileNameCalc = daily_filename_calculator>
class daily_file_sink final : public base_sink<Mutex>
{
public:
    // create daily file sink which rotates on given time
    daily_file_sink(filename_t base_filename, int rotation_hour, int rotation_minute, bool truncate = false)
        : base_filename_(std::move(base_filename))
        , rotation_h_(rotation_hour)
        , rotation_m_(rotation_minute)
        , truncate_(truncate)
    {
        if (rotation_hour < 0 || rotation_hour > 23 || rotation_minute < 0 || rotation_minute > 59)
        {
            throw spdlog_ex("daily_file_sink: Invalid rotation time in ctor");
        }
        auto now = log_clock::now();
        file_helper_.open(FileNameCalc::calc_filename(base_filename_, now_tm(now)), truncate_);
        rotation_tp_ = next_rotation_tp_();
    }

    const filename_t &filename() const
    {
        return file_helper_.filename();
    }

protected:
    void sink_it_(const details::log_msg &msg) override
    {

        if (msg.time >= rotation_tp_)
        {
            file_helper_.open(FileNameCalc::calc_filename(base_filename_, now_tm(msg.time)), truncate_);
            rotation_tp_ = next_rotation_tp_();
        }
        fmt::memory_buffer formatted;
        sink::formatter_->format(msg, formatted);
        file_helper_.write(formatted);
    }

    void flush_() override
    {
        file_helper_.flush();
    }

private:
    tm now_tm(log_clock::time_point tp)
    {
        time_t tnow = log_clock::to_time_t(tp);
        return spdlog::details::os::localtime(tnow);
    }

    log_clock::time_point next_rotation_tp_()
    {
        auto now = log_clock::now();
        tm date = now_tm(now);
        date.tm_hour = rotation_h_;
        date.tm_min = rotation_m_;
        date.tm_sec = 0;
        auto rotation_time = log_clock::from_time_t(std::mktime(&date));
        if (rotation_time > now)
        {
            return rotation_time;
        }
        return {rotation_time + std::chrono::hours(24)};
    }

    filename_t base_filename_;
    int rotation_h_;
    int rotation_m_;
    log_clock::time_point rotation_tp_;
    details::file_helper file_helper_;
    bool truncate_;
};

using daily_file_sink_mt = daily_file_sink<std::mutex>;
using daily_file_sink_st = daily_file_sink<details::null_mutex>;


// ************************
//   hour rotate log
// ************************
	
/*
 * Generator of daily log file names in format basename.YYYY-MM-DD_hh.ext
 */
struct hour_filename_calculator
{
    // Create filename for the form basename.YYYY-MM-DD
    static filename_t calc_filename(const filename_t &filename, const tm &now_tm)
    {
        filename_t basename, ext;
        std::tie(basename, ext) = details::file_helper::split_by_extension(filename);
        std::conditional<std::is_same<filename_t::value_type, char>::value, fmt::memory_buffer, fmt::wmemory_buffer>::type w;
        fmt::format_to(
            w, SPDLOG_FILENAME_T("{}_{:04d}-{:02d}-{:02d}_{:02d}{}"), basename, now_tm.tm_year + 1900, now_tm.tm_mon + 1, now_tm.tm_mday, now_tm.tm_hour, ext);
        return fmt::to_string(w);
    }
};

/*
 * Rotating file sink based on date. rotates at midnight
 */
template<typename Mutex, typename FileNameCalc = hour_filename_calculator>
class hour_file_sink final : public base_sink<Mutex>
{
public:
    // create daily file sink which rotates on given time
    hour_file_sink(filename_t base_filename, int rotation_minute, bool truncate = false)
        : base_filename_(std::move(base_filename))
        , rotation_m_(rotation_minute)
        , truncate_(truncate)
    {
        if (rotation_minute < 0 || rotation_minute > 59)
        {
            throw spdlog_ex("daily_file_sink: Invalid rotation time in ctor");
        }
        auto now = log_clock::now();
        file_helper_.open(FileNameCalc::calc_filename(base_filename_, now_tm(now)), truncate_);
        rotation_tp_ = next_rotation_tp_();
    }

    const filename_t &filename() const
    {
        return file_helper_.filename();
    }

protected:
    void sink_it_(const details::log_msg &msg) override
    {

        if (msg.time >= rotation_tp_)
        {
            file_helper_.open(FileNameCalc::calc_filename(base_filename_, now_tm(msg.time)), truncate_);
            rotation_tp_ = next_rotation_tp_();
        }
        fmt::memory_buffer formatted;
        sink::formatter_->format(msg, formatted);
        file_helper_.write(formatted);
    }

    void flush_() override
    {
        file_helper_.flush();
    }

private:
    tm now_tm(log_clock::time_point tp)
    {
        time_t tnow = log_clock::to_time_t(tp);
        return spdlog::details::os::localtime(tnow);
    }

    log_clock::time_point next_rotation_tp_()
    {
        auto now = log_clock::now();
        tm date = now_tm(now);
        date.tm_min = rotation_m_;
        date.tm_sec = 0;
        auto rotation_time = log_clock::from_time_t(std::mktime(&date));
        if (rotation_time > now)
        {
            return rotation_time;
        }
        return {rotation_time + std::chrono::hours(1)};
    }


    filename_t base_filename_;
    int rotation_m_;
    log_clock::time_point rotation_tp_;
    details::file_helper file_helper_;
    bool truncate_;
};

using hour_file_sink_mt = hour_file_sink<std::mutex>;
using hour_file_sink_st = hour_file_sink<details::null_mutex>;

} // namespace sinks

//
// factory functions
//

template<typename Factory = default_factory>
inline std::shared_ptr<logger> rotating_logger_mt(
    const std::string &logger_name, const filename_t &filename, size_t max_file_size, size_t max_files, bool rotate_on_open=false)
{
    return Factory::template create<sinks::rotating_file_sink_mt>(logger_name, filename, max_file_size, max_files, rotate_on_open);
}

template<typename Factory = default_factory>
inline std::shared_ptr<logger> rotating_logger_st(
    const std::string &logger_name, const filename_t &filename, size_t max_file_size, size_t max_files, bool rotate_on_open = false)
{
    return Factory::template create<sinks::rotating_file_sink_st>(logger_name, filename, max_file_size, max_files, rotate_on_open);
}


//
// factory functions
//
template<typename Factory = default_factory>
inline std::shared_ptr<logger> daily_logger_mt(
    const std::string &logger_name, const filename_t &filename, int hour = 0, int minute = 0, bool truncate = false)
{
    return Factory::template create<sinks::daily_file_sink_mt>(logger_name, filename, hour, minute, truncate);
}

template<typename Factory = default_factory>
inline std::shared_ptr<logger> daily_logger_st(
    const std::string &logger_name, const filename_t &filename, int hour = 0, int minute = 0, bool truncate = false)
{
    return Factory::template create<sinks::daily_file_sink_st>(logger_name, filename, hour, minute, truncate);
}


//
// factory functions
//


template<typename Factory = default_factory>
inline std::shared_ptr<logger> hour_logger_mt(
    const std::string &logger_name, const filename_t &filename, int minute = 0, bool truncate = false)
{
    return Factory::template create<sinks::hour_file_sink_mt>(logger_name, filename, minute, truncate);
}

template<typename Factory = default_factory>
inline std::shared_ptr<logger> hour_logger_st(
    const std::string &logger_name, const filename_t &filename, int minute = 0, bool truncate = false)
{
    return Factory::template create<sinks::hour_file_sink_st>(logger_name, filename, minute, truncate);
}


} // namespace spdlog
