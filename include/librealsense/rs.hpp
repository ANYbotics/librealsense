// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 Intel Corporation. All Rights Reserved.

#ifndef LIBREALSENSE_RS_HPP
#define LIBREALSENSE_RS_HPP

#include "rs.h"
#include "rscore.hpp"

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <exception>
#include <ostream>

namespace rs
{
    class error : public std::runtime_error
    {
        std::string function, args;
    public:
        explicit error(rs_error * err) : std::runtime_error(rs_get_error_message(err))
        {
            function = (nullptr != rs_get_failed_function(err)) ? rs_get_failed_function(err) : std::string();
            args = (nullptr != rs_get_failed_args(err)) ? rs_get_failed_args(err) : std::string();
            rs_free_error(err);
        }
        const std::string & get_failed_function() const { return function; }
        const std::string & get_failed_args() const { return args; }
        static void handle(rs_error * e) { if (e) throw error(e); }
    };

    class context;
    class device;
    class subdevice;

    class device_info
    {
    public:

    private:
        friend context;
        explicit device_info(std::shared_ptr<rs_device_info> info) : _info(info) {}
        
        std::shared_ptr<rs_device_info> _info;
    };

    struct stream_profile
    {
        rs_stream stream;
        int width;
        int height;
        int fps;
        rs_format format;
    };

    class frame
    {
        const rs_stream_lock * lock;
        rs_frame_ref * frame_ref;

        frame(const frame &) = delete;

    public:
        frame() : lock(nullptr), frame_ref(nullptr) {}
        frame(const rs_stream_lock * lock, rs_frame_ref * frame_ref) : lock(lock), frame_ref(frame_ref) {}
        frame(frame&& other) : lock(other.lock), frame_ref(other.frame_ref) { other.frame_ref = nullptr; }
        frame& operator=(frame other)
        {
            swap(other);
            return *this;
        }
        void swap(frame& other)
        {
            std::swap(lock, other.lock);
            std::swap(frame_ref, other.frame_ref);
        }

        ~frame()
        {
            if (lock && frame_ref)
            {
                rs_release_frame(lock, frame_ref);
            }
        }

        /// retrieve the time at which the frame was captured
        /// \return            the timestamp of the frame, in milliseconds since the device was started
        double get_timestamp() const
        {
            rs_error * e = nullptr;
            auto r = rs_get_frame_timestamp(frame_ref, &e);
            error::handle(e);
            return r;
        }

        /// retrieve the timestamp domain 
        /// \return            timestamp domain (clock name) for timestamp values
        rs_timestamp_domain get_frame_timestamp_domain() const
        {
            rs_error * e = nullptr;
            auto r = rs_get_frame_timestamp_domain(frame_ref, &e);
            error::handle(e);
            return r;
        }

        /// retrieve the current value of a single frame_metadata
        /// \param[in] frame_metadata  the frame_metadata whose value should be retrieved
        /// \return            the value of the frame_metadata
        double get_frame_metadata(rs_frame_metadata frame_metadata) const
        {
            rs_error * e = nullptr;
            auto r = rs_get_frame_metadata(frame_ref, frame_metadata, &e);
            error::handle(e);
            return r;
        }

        /// determine if the device allows a specific metadata to be queried
        /// \param[in] frame_metadata  the frame_metadata to check for support
        /// \return            true if the frame_metadata can be queried
        bool supports_frame_metadata(rs_frame_metadata frame_metadata) const
        {
            rs_error * e = nullptr;
            auto r = rs_supports_frame_metadata(frame_ref, frame_metadata, &e);
            error::handle(e);
            return r != 0;
        }

        unsigned long long get_frame_number() const
        {
            rs_error * e = nullptr;
            auto r = rs_get_frame_number(frame_ref, &e);
            error::handle(e);
            return r;
        }

        const void * get_data() const
        {
            rs_error * e = nullptr;
            auto r = rs_get_frame_data(frame_ref, &e);
            error::handle(e);
            return r;
        }

        // returns image width in pixels
        int get_width() const
        {
            rs_error * e = nullptr;
            auto r = rs_get_frame_width(frame_ref, &e);
            error::handle(e);
            return r;
        }

        int get_height() const
        {
            rs_error * e = nullptr;
            auto r = rs_get_frame_height(frame_ref, &e);
            error::handle(e);
            return r;
        }

        // retrive frame stride, meaning the actual line width in memory in bytes (not the logical image width)
        int get_stride_in_bytes() const
        {
            rs_error * e = nullptr;
            auto r = rs_get_frame_stride_in_bytes(frame_ref, &e);
            error::handle(e);
            return r;
        }

        /// retrieve bits per pixel
        /// \return            number of bits per one pixel
        int get_bits_per_pixel() const
        {
            rs_error * e = nullptr;
            auto r = rs_get_frame_bits_per_pixel(frame_ref, &e);
            error::handle(e);
            return r;
        }

        rs_format get_format() const
        {
            rs_error * e = nullptr;
            auto r = rs_get_frame_format(frame_ref, &e);
            error::handle(e);
            return r;;
        }

        rs_stream get_stream_type() const
        {
            rs_error * e = nullptr;
            auto s = rs_get_frame_stream_type(frame_ref, &e);
            error::handle(e);
            return s;
        }
    };

    class frame_callback : public rs_frame_callback
    {
        std::function<void(frame)> on_frame_function;
    public:
        explicit frame_callback(std::function<void(frame)> on_frame) : on_frame_function(on_frame) {}

        void on_frame(const rs_stream_lock *device, rs_frame_ref * fref) override
        {
            on_frame_function(std::move(frame(device, fref)));
        }

        void release() override { delete this; }
    };

    class streaming_lock
    {
    public:
        void play(std::function<void(frame)> callback) const
        {
            rs_error * e = nullptr;
            rs_play_cpp(_lock.get(), new frame_callback(callback), &e);
            error::handle(e);
        }

        void stop() const
        {
            rs_error * e = nullptr;
            rs_stop(_lock.get(), &e);
            error::handle(e);
        }

    private:
        friend subdevice;
        explicit streaming_lock(std::shared_ptr<rs_stream_lock> lock)
            : _lock(std::move(lock)) {}

        std::shared_ptr<rs_stream_lock> _lock;
    };

    struct option_range
    {
        float min;
        float max;
        float def;
        float step;
    };

    class subdevice
    {
    public:
        streaming_lock open(const stream_profile& profile) const
        {
            rs_error* e = nullptr;
            std::shared_ptr<rs_stream_lock> lock(
                rs_open_subdevice(_dev, _index, 
                    profile.stream, 
                    profile.width,
                    profile.height,
                    profile.fps,
                    profile.format,
                    &e),
                rs_release_streaming_lock);
            error::handle(e);

            return streaming_lock(lock);
        }

        float get_option(rs_option option) const
        {
            rs_error* e = nullptr;
            auto res = rs_get_subdevice_option(_dev, _index, option, &e);
            error::handle(e);
            return res;
        }

        option_range get_option_range(rs_option option) const
        {
            option_range result;
            rs_error* e = nullptr;
            rs_get_subdevice_option_range(_dev, _index, option,
                &result.min, &result.max, &result.step, &result.def, &e);
            error::handle(e);
            return result;
        }

        void set_option(rs_option option, float value) const
        {
            rs_error* e = nullptr;
            rs_set_subdevice_option(_dev, _index, option, value, &e);
            error::handle(e);
        }

        bool supports(rs_option option) const
        {
            rs_error* e = nullptr;
            auto res = rs_supports_subdevice_option(_dev, _index, option, &e);
            error::handle(e);
            return res > 0;
        }

        std::vector<stream_profile> get_stream_profiles() const
        {
            std::vector<stream_profile> results;

            rs_error* e = nullptr;
            std::shared_ptr<rs_stream_profile_list> list(
                rs_get_supported_profiles(_dev, _index, &e),
                rs_delete_profiles_list);
            error::handle(e);

            auto size = rs_get_profile_list_size(list.get(), &e);
            error::handle(e);
            
            for (auto i = 0; i < size; i++)
            {
                stream_profile profile;
                rs_get_profile(list.get(), i, 
                    &profile.stream,
                    &profile.width,
                    &profile.height,
                    &profile.fps,
                    &profile.format,
                    &e);
                error::handle(e);
                results.push_back(profile);
            }

            return results;
        }

    private:
        friend device;
        explicit subdevice(rs_device* dev, rs_subdevice index) 
            : _dev(dev), _index(index) {}

        rs_device* _dev;
        rs_subdevice _index;
    };

    class device
    {
    public:
        subdevice& get_subdevice(rs_subdevice sub)
        {
            if (sub < _subdevices.size() && _subdevices[sub].get()) 
                return *_subdevices[sub];
            throw std::runtime_error("Requested subdevice is not supported!");
        }

        bool supports(rs_subdevice sub) const
        {
            return sub < _subdevices.size() && _subdevices[sub].get();
        }

        subdevice& color() { return get_subdevice(RS_SUBDEVICE_COLOR); }
        subdevice& depth() { return get_subdevice(RS_SUBDEVICE_DEPTH); }

        bool supports(rs_camera_info info) const
        {
            rs_error* e = nullptr;
            auto is_supported = rs_supports_camera_info(_dev.get(), info, &e);
            error::handle(e);
            return is_supported > 0;
        }

        const char* get_camera_info(rs_camera_info info) const
        {
            rs_error* e = nullptr;
            auto result = rs_get_camera_info(_dev.get(), info, &e);
            error::handle(e);
            return result;
        }

    private:
        friend context;
        explicit device(std::shared_ptr<rs_device> dev) : _dev(dev)
        {
            _subdevices.resize(RS_SUBDEVICE_COUNT);
            for (auto i = 0; i < RS_SUBDEVICE_COUNT; i++)
            {
                auto s = static_cast<rs_subdevice>(i);
                rs_error* e = nullptr;
                auto is_supported = rs_is_subdevice_supported(_dev.get(), s, &e);
                error::handle(e);
                if (is_supported) _subdevices[s].reset(new subdevice(_dev.get(), s));
                else  _subdevices[s] = nullptr;
            }
        }

        std::shared_ptr<rs_device> _dev;
        std::vector<std::shared_ptr<subdevice>> _subdevices;
    };

    class context
    {
    public:
        context()
        {
            rs_error* e = nullptr;
            _context = std::shared_ptr<rs_context>(
                rs_create_context(RS_API_VERSION, &e),
                rs_delete_context);
            error::handle(e);
        }

        std::vector<device_info> query_devices() const
        {
            rs_error* e = nullptr;
            std::shared_ptr<rs_device_info_list> list(
                rs_query_devices(_context.get(), &e),
                rs_delete_device_info_list);
            error::handle(e);

            auto size = rs_get_device_list_size(list.get(), &e);
            error::handle(e);

            std::vector<device_info> results;
            for (auto i = 0; i < size; i++)
            {
                std::shared_ptr<rs_device_info> info(
                    rs_get_device_info(list.get(), i, &e),
                    rs_delete_device_info);
                error::handle(e);

                device_info rs_info(info);
                results.push_back(rs_info);
            }

            return results;
        }

        device create(const device_info& info) const
        {
            rs_error* e = nullptr;
            std::shared_ptr<rs_device> dev(
                rs_create_device(_context.get(), info._info.get(), &e),
                rs_delete_device);
            error::handle(e);
            device result(dev);
            return result;
        }

    private:
        std::shared_ptr<rs_context> _context;
    };

    inline std::ostream & operator << (std::ostream & o, rs_stream stream) { return o << rs_stream_to_string(stream); }
    inline std::ostream & operator << (std::ostream & o, rs_format format) { return o << rs_format_to_string(format); }
    inline std::ostream & operator << (std::ostream & o, rs_preset preset) { return o << rs_preset_to_string(preset); }
    inline std::ostream & operator << (std::ostream & o, rs_distortion distortion) { return o << rs_distortion_to_string(distortion); }
    inline std::ostream & operator << (std::ostream & o, rs_option option) { return o << rs_option_to_string(option); }
    inline std::ostream & operator << (std::ostream & o, rs_capabilities capability) { return o << rs_capabilities_to_string(capability); }
    inline std::ostream & operator << (std::ostream & o, rs_source src) { return o << rs_source_to_string(src); }
    inline std::ostream & operator << (std::ostream & o, rs_subdevice evt) { return o << rs_subdevice_to_string(evt); }


    class log_callback : public rs_log_callback
    {
        std::function<void(rs_log_severity, const char *)> on_event_function;
    public:
        explicit log_callback(std::function<void(rs_log_severity, const char *)> on_event) : on_event_function(on_event) {}

        void on_event(rs_log_severity severity, const char * message) override
        {
            on_event_function(static_cast<rs_log_severity>(severity), message);
        }

        void release() override { delete this; }
    };

    inline void log_to_console(rs_log_severity min_severity)
    {
        rs_error * e = nullptr;
        rs_log_to_console(min_severity, &e);
        error::handle(e);
    }

    inline void log_to_file(rs_log_severity min_severity, const char * file_path)
    {
        rs_error * e = nullptr;
        rs_log_to_file(min_severity, file_path, &e);
        error::handle(e);
    }

    inline void log_to_callback(rs_log_severity min_severity, std::function<void(rs_log_severity, const char *)> callback)
    {
        rs_error * e = nullptr;
        rs_log_to_callback_cpp(min_severity, new log_callback(callback), &e);
        error::handle(e);
    }
}

#endif // LIBREALSENSE_RS2_HPP