# rs-tracking-and-depth Sample

## Overview

This sample demonstrates an example use of
the tracking camera T265 
together with a depth camera D4xx to
display a 3D pointcloud as generated by the D4xx
with respect to a static reference frame.
The pose of the depth camera is tracked by the tracking camera which is rigidly attached to it.

## Expected Output
The application should open a window with a pointcloud.
A static scene should appear static on the screen despite the camera can be moving.
This allows "scanning" the scene (frame-by-frame) by moving the camera. (Currently, no depth data is accumulated or filtered over time.)
Using your mouse, you should be able to interact with the pointcloud rotating and zooming using the mouse.

<img class="animated-gif" src="https://user-images.githubusercontent.com/28366639/61068360-b0647c80-a3be-11e9-96de-d48823800973.gif" width="800">

## Mechanical Setup
The provided [STL](./bracket_t265nd435_external.stl) can be 3D-printed
and used to attach T265 and D435
using **M3 screws, 10mm & 18mm**, respectively.
![snapshot00](https://user-images.githubusercontent.com/28366639/60457143-2b27dd80-9bf0-11e9-8286-757953d9013e.png)
If the mount is used in the depicted orientation with T265 below and D435 mounted above,
the provided [calibration](./H_t265_d400.cfg) can be used to transform between point cloud and pose reference frame.

In the depicted orientation a **¼-20 nut** can be inserted into the 3D-print in order to mount the assembly on a tripod.

![IMG_1900_3](https://user-images.githubusercontent.com/28366639/61068929-1271b180-a3c0-11e9-9502-d30092c7506a.png)

Please note that the homogeneous transformation is expected to be provided as row-major 3x4 matrix of the form `H = [R, t]` where `R` is the rotation matrix and `t` the translation vector of the depth, i.e. infrared 1, frame, with respect to the T265 body frame (see above).


## Code Overview

This example is based on the [pointcloud example](../pointcloud/). Please also refer the respective documentation.

Similar to the [first tutorial](../capture/) we include the Cross-Platform API:
```cpp
#include <any_librealsense2/rs.hpp> // Include RealSense Cross Platform API
```

Next, we prepared a [very short helper library](../example.hpp) encapsulating basic OpenGL rendering and window management:
```cpp
#include "example.hpp"          // Include short list of convenience functions for rendering
```

We also include the STL `<algorithm>` header for `std::min` and `std::max`,
and `<fstream>` for `std::ifstream`
to parse the homogeneous extrinsic matrix from a text file.

Next, we define a `state` struct and two helper functions. `state` and `register_glfw_callbacks` handle the pointcloud's rotation in the application, and `draw_pointcloud_wrt_world` makes all the OpenGL calls necessary to display the pointcloud.
```cpp
// Struct for managing rotation of pointcloud view
struct state { double yaw, pitch, last_x, last_y; bool ml; float offset_x, offset_y; texture tex; };

// Helper functions
void register_glfw_callbacks(window& app, state& app_state);
draw_pointcloud_wrt_world(float width, float height, glfw_state& app_state, rs2::points& points, rs2_pose& pose, float H_t265_d400[16]);
```

The `example.hpp` header lets us easily open a new window and prepare textures for rendering. The `state` class (declared above) is used for interacting with the mouse, with the help of some callbacks registered through glfw.
```cpp
// Create a simple OpenGL window for rendering:
window app(1280, 720, "RealSense Tracking and Depth Example");
// Construct an object to manage view state
state app_state = { 0, 0, 0, 0, false, 0, 0, 0 };
// register callbacks to allow manipulation of the pointcloud
register_glfw_callbacks(app, app_state);
```

We are going to use classes within the `rs2` namespace:
```cpp
using namespace rs2;
```

As part of the API we offer the `pointcloud` class which calculates a pointcloud and corresponding texture mapping from depth and color frames. To make sure we always have something to display, we also make a `rs2::points` object to store the results of the pointcloud calculation.
```cpp
// Declare pointcloud object, for calculating pointclouds and texture mappings
pointcloud pc = rs2::context().create_pointcloud();
// We want the points object to be persistent so we can display the last cloud when a frame drops
rs2::points points;
```

We declare a `rs2_pose` object to store the latest pose as reported by T265.
```cpp
rs2_pose pose;
```


To stream from multiple device please also refer to the [multicam example](../multicam/).
First a common context is created and a (separate) pipeline is started for each of the queried devices.
```cpp
// Start a streaming pipe per each connected device
for (auto&& dev : ctx.query_devices())
{
    rs2::pipeline pipe(ctx);
    rs2::config cfg;
    cfg.enable_device(dev.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER));
    pipe.start(cfg);
    pipelines.emplace_back(pipe);
}
```

The extrinsics between the streams, namely depth and pose, are loaded from a configuration file
that has to be provided in form of a *row-major* homogeneous 4-by-4 matrix.

While the app is running,
we loop over the pipelines, wait for frames, before we collect the respective color, depth and pose frames:
```cpp
for (auto &&pipe : pipelines) // loop over pipelines
        {
            auto frames = pipe.wait_for_frames();  // Wait for the next set of frames from the camera
```

Using helper functions on the `frameset` object we check for new depth and color frames. We pass it to the `pointcloud` object to use as the texture, and also give it to OpenGL with the help of the `texture` class. We generate a new pointcloud.
```cpp
auto depth = frames.get_depth_frame();

// Generate the pointcloud and texture mappings
points = pc.calculate(depth);

auto color = frames.get_color_frame();

// Tell pointcloud object to map to this color frame
pc.map_to(color);

// Upload the color frame to OpenGL
app_state.tex.upload(color);

```

In a similar way we get the pose data from the pose frame
```cpp
auto pose_frame = frames.get_pose_frame();
if (pose_frame) {
    pose = pose_frame.get_pose_data();
}
```

Finally we call `draw_pointcloud_wrt_world` to draw the pointcloud with respect to a common (fixed) world frame.
This is done by moving the observing camera according to the transformation reported by T265 and extrinsics to the depth stream (instead of transforming the scene by the inverse which results in the same relative motion).
```cpp
draw_pointcloud_wrt_world(app.width(), app.height(), app_state, points, pose, H_t265_d400);
```

`draw_pointcloud_wrt_world` primarily calls OpenGL, but the critical portion iterates over all the points in the pointcloud, and where we have depth data, we upload the point's coordinates and texture mapping coordinates to OpenGL.
```cpp
/* this segment actually prints the pointcloud */
auto vertices = points.get_vertices();              // get vertices
auto tex_coords = points.get_texture_coordinates(); // and texture coordinates
for (int i = 0; i < points.size(); i++)
{
    if (vertices[i].z)
    {
        // upload the point and texture coordinates only for points we have depth data for
        glVertex3fv(vertices[i]);
        glTexCoord2fv(tex_coords[i]);
    }
}
```

The second critical portion changes the viewport according to the provided transformation.
```cpp
// viewing matrix
glMatrixMode(GL_MODELVIEW);
glPushMatrix();

GLfloat H_world_t265[16];
quat2mat(pose.rotation, H_world_t265);
H_world_t265[12] = pose.translation.x;
H_world_t265[13] = pose.translation.y;
H_world_t265[14] = pose.translation.z;

glMultMatrixf(H_world_t265);
glMultMatrixf(H_t265_d400);
```
