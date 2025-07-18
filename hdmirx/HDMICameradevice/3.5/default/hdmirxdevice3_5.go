package Libhdmirxdevice3_5

import (
	"fmt"
    "android/soong/android"
    "android/soong/cc"
)

func init() {
	android.RegisterModuleType("hdmirxdevice_3_5_sharelib_defaults", hdmirxdevices3_5sharelibFactory)
}

func hdmirxdevices3_5sharelibFactory() android.Module {
    module := cc.DefaultsFactory()
    android.AddLoadHook(module, hdmirxdevice3_5sharelibDefaults)
    return module
}

func hdmirxdevice3_5sharelibDefaults(ctx android.LoadHookContext) {
    type props struct {
        Srcs []string
		Shared_libs []string
		Static_libs []string
		Export_shared_lib_headers []string
    }
    p := &props{}
    vars := ctx.Config().VendorConfig("mtkPlugin")
    if vars.Bool("MTK_HDMI_RXVC_SUPPORT") {
		fmt.Printf("HDMIRX device 3_5 MTK_HDMI_RXVC_SUPPORT is yes")
        p.Srcs = append(p.Srcs, "HdmirxCameraDevice.cpp")
		p.Srcs = append(p.Srcs, "HdmirxCameraDeviceSession.cpp")

		p.Shared_libs = append(p.Shared_libs, "android.hardware.camera.device@3.2")
		p.Shared_libs = append(p.Shared_libs, "android.hardware.camera.device@3.3")
		p.Shared_libs = append(p.Shared_libs, "android.hardware.camera.device@3.4")
		p.Shared_libs = append(p.Shared_libs, "android.hardware.camera.device@3.5")
		p.Shared_libs = append(p.Shared_libs, "android.hardware.camera.provider@2.4")
		p.Shared_libs = append(p.Shared_libs, "android.hardware.graphics.mapper@2.0")
		p.Shared_libs = append(p.Shared_libs, "android.hardware.graphics.mapper@3.0")
		p.Shared_libs = append(p.Shared_libs, "android.hardware.graphics.mapper@4.0")
		p.Shared_libs = append(p.Shared_libs, "camera.device@3.2-impl")
		p.Shared_libs = append(p.Shared_libs, "camera.device@3.3-impl")
		p.Shared_libs = append(p.Shared_libs, "camera.device@3.4-hdmirx-impl")
		p.Shared_libs = append(p.Shared_libs, "libcamera_metadata")
		p.Shared_libs = append(p.Shared_libs, "libcutils")
		p.Shared_libs = append(p.Shared_libs, "libhardware")
		p.Shared_libs = append(p.Shared_libs, "libhidlbase")
		p.Shared_libs = append(p.Shared_libs, "liblog")
		p.Shared_libs = append(p.Shared_libs, "libtinyxml2")
		p.Shared_libs = append(p.Shared_libs, "libutils")
		p.Shared_libs = append(p.Shared_libs, "libfmq")
		p.Shared_libs = append(p.Shared_libs, "libgralloctypes")
		p.Shared_libs = append(p.Shared_libs, "libsync")
		p.Shared_libs = append(p.Shared_libs, "libyuv")
		p.Shared_libs = append(p.Shared_libs, "libjpeg")
		p.Shared_libs = append(p.Shared_libs, "libexif")
		p.Shared_libs = append(p.Shared_libs, "libdpframework")
		p.Shared_libs = append(p.Shared_libs, "libnativewindow")

		p.Static_libs = append(p.Static_libs, "android.hardware.camera.common@1.0-helper")

		p.Export_shared_lib_headers = append(p.Export_shared_lib_headers, "libfmq")
        ctx.AppendProperties(p)
    } else {
		fmt.Printf("HDMIRX device 3_5 MTK_HDMI_RXVC_SUPPORT is no")
	}

}
