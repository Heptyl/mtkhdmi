package Libhdmirxdevice34

import (
	"fmt"
	"android/soong/android"
	"android/soong/cc"
)

func init() {
	android.RegisterModuleType("hdmirxdevice_3_4_sharelib_defaults", hdmirxdevices3_4sharelibFactory)

	
}

func hdmirxdevices3_4sharelibFactory() android.Module {
	module := cc.DefaultsFactory()
	android.AddLoadHook(module, hdmirxdevice3_4sharelibDefaults)
	return module
}

func hdmirxdevice3_4sharelibDefaults(ctx android.LoadHookContext) {
	type props struct {
		Srcs []string
		Shared_libs []string
		Static_libs []string
		Export_shared_lib_headers []string
		Cflags []string
		Cppflags []string
	}
	p := &props{}
	vars := ctx.Config().VendorConfig("mtkPlugin")
	if vars.Bool("MTK_HDMI_RXVC_SUPPORT") {
		fmt.Printf("HDMIRX device 3_4 MTK_HDMI_RXVC_SUPPORT is yes")
		p.Srcs = append(p.Srcs, "HdmirxCameraDevice.cpp")
		p.Srcs = append(p.Srcs, "HdmirxCameraDeviceSession.cpp")
		p.Srcs = append(p.Srcs, "HdmirxCameraUtils.cpp")
		p.Srcs = append(p.Srcs, "grallocdev.cpp")
		p.Srcs = append(p.Srcs, "hdmi_ctl.cpp")
		p.Srcs = append(p.Srcs, "mdp_ctl.cpp")
		p.Srcs = append(p.Srcs, "IONDevice.cpp")
		p.Srcs = append(p.Srcs, "optee_ctl.cpp")
		p.Srcs = append(p.Srcs, "VendorTagDescriptor.cpp")
		p.Srcs = append(p.Srcs, "DMABUFDevice.cpp")

		p.Shared_libs = append(p.Shared_libs, "android.hardware.camera.device@3.2")
		p.Shared_libs = append(p.Shared_libs, "android.hardware.camera.device@3.3")
		p.Shared_libs = append(p.Shared_libs, "android.hardware.camera.device@3.4")
		p.Shared_libs = append(p.Shared_libs, "android.hardware.camera.provider@2.4")
		p.Shared_libs = append(p.Shared_libs, "android.hardware.graphics.mapper@2.0")
		p.Shared_libs = append(p.Shared_libs, "android.hardware.graphics.mapper@3.0")
		p.Shared_libs = append(p.Shared_libs, "android.hardware.graphics.mapper@4.0")
		p.Shared_libs = append(p.Shared_libs, "camera.device@3.2-impl")
		p.Shared_libs = append(p.Shared_libs, "camera.device@3.3-impl")
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
		p.Shared_libs = append(p.Shared_libs, "libion")
		p.Shared_libs = append(p.Shared_libs, "libdpframework")
		p.Shared_libs = append(p.Shared_libs, "libgralloc_extra")
		p.Shared_libs = append(p.Shared_libs, "libnativewindow")
		p.Shared_libs = append(p.Shared_libs, "libTEECommon")
		p.Shared_libs = append(p.Shared_libs, "libopenteec")

		p.Static_libs = append(p.Static_libs, "android.hardware.camera.common@1.0-helper")

		p.Export_shared_lib_headers = append(p.Export_shared_lib_headers, "libfmq")

		p.Cflags = append(p.Cflags, "-Wno-unused-parameter")
		p.Cflags = append(p.Cflags, "-Wno-macro-redefined")

 		if vars.Bool("MTK_HDMI_RXVC_DS_SUPPORT") {
			p.Cppflags = append(p.Cppflags, "-DMTK_HDMI_RXVC_DS_SUPPORT=1")
		}
		if vars.Bool("MTK_HDMI_RXVC_HDR_SUPPORT") {
			p.Cppflags = append(p.Cppflags, "-DMTK_HDMI_RXVC_HDR_SUPPORT=1")
		}
		ctx.AppendProperties(p)
	} else {
		fmt.Printf("HDMIRX device 3_5 MTK_HDMI_RXVC_SUPPORT is no")
	}
}

