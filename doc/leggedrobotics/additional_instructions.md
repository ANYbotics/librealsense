# Additional instructions
## Build
For building the package, run:

    catkin build any_librealsense2

## Patching of UVC kernel module
Running RealSense Depth Cameras on Linux requires patching and inserting modified kernel drivers, specially when it comes to devices with the latest firmware versions (> v5.09.02).

Furthermore, applying the kernel is essential to have access to the following features:
* More accurate timestamping.
* Enabling hardware synchronization.
* Fetching frame metadata.
* Improved depth frame alignment.
* Fetching motion module data (applicable to IMU-enabled devices, such as the T265).

In order to patch the Linux UVC kernel module, the steps outlined below must be followed in order:
1. Navigate to *librealsense* root directory to run the following scripts.<br />
    Unplug any connected Intel RealSense camera.<br />  

2. Install the core packages required to build *librealsense* binaries and the affected kernel modules:  
    `sudo apt-get install git libssl-dev libusb-1.0-0-dev pkg-config libgtk-3-dev`  <br /><br />
    Distribution-specific packages:  <br />

    * Ubuntu 18:<br />
        `sudo apt-get install libglfw3-dev libgl1-mesa-dev libglu1-mesa-dev`  <br /><br />
    
3. Install Intel Realsense udev rules:<br />
        `sudo cp config/99-realsense-libusb.rules /etc/udev/rules.d/`  <br />
        `sudo udevadm control --reload-rules && udevadm trigger`
        <br />

4. Build and apply patched kernel modules: <br />
    The librealsense package ships with scripts that will download, patch and build the Linux UVC kernel module. After the build process is finished, these scripts will attempt to insert the patched module instead of the active one into the kernel. If failed the original uvc modules will be restored.

    * **Ubuntu 18 with LTS kernel**
    
        `./scripts/patch-realsense-ubuntu-lts.sh`<br />

    * **Ubuntu 18 with Low Latency kernel**

        `./scripts/patch-realsense-ubuntu-lowlatency.sh`<br />

### FAQ / Troubleshooting
#### I get the following error message: 'Unsupported kernel version xx-xx . The patches are maintained for Ubuntu XX.XX LTS with kernel y.yy only'
Currently, the kernel patch has only been released for a few versions of the Linux kernel. We recommend that users stick to one of the supported kernels to retain full compatibility with the UVC kernel patch.

The official list of kernels supported can be found [here](https://github.com/IntelRealSense/librealsense/blob/master/doc/distribution_linux.md#using-pre-build-packages).

If you explicitly need to use an unsupported kernel version, we recommend to follow the steps outlined in the Intel wiki for experimental [LibUVC-backend installation](https://github.com/IntelRealSense/librealsense/blob/master/doc/libuvc_installation.md)

#### When the script tries to mount the patched kernel module I get a message saying 'Permission denied'
Check that you have installed the udev rules, as indicated in step 3.

#### After the kernel build is finished I get a message saying 'Failed to unload module videobuf2_core'.
`Videobuf2_core` is a kernel module that depends on `uvcvideo`. In some systems removing the `uvcvideo` module can fail if another dependent module is already inserted into the kernel. In order to solve this, we recommend to run the kernel patching script again after executing the following commands:

```
sudo modprobe -r videobuf2_core
sudo modprobe -r videodev
sudo modprobe -r uvcvideo
```