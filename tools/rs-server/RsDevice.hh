// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2020 Intel Corporation. All Rights Reserved.

#pragma once

#include <any_librealsense2/rs.hpp>
#include "RsUsageEnvironment.h"
#include "RsSensor.hh"
#include <map>

class RsDevice
{
public:
    RsDevice(UsageEnvironment* t_env);
    ~RsDevice();
    std::vector<RsSensor>& getSensors()
    {
        return m_sensors;
    }

    static int getPhysicalSensorUniqueKey(rs2_stream stream_type, int sensors_index);

    // sensor index
    // map for stream pysical sensor
    // key is generated by rs2_stream+index: depth=1,color=2,irl=3,irr=4
    // todo: make smart_ptr
    std::map<std::pair<int, int>, rs2_extrinsics> minimal_extrinsics_map;

    rs2::device getDevice()
    {
        return m_device;
    }

private:
    rs2::device m_device;
    std::vector<RsSensor> m_sensors;

    UsageEnvironment* env;
};
