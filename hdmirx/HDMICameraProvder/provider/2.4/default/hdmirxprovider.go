package Libhdmirxprovider

import (
	"fmt"
    "android/soong/android"
    "android/soong/cc"
)

func init() {
	android.RegisterModuleType("hdmirxprovider_sharelib_defaults", hdmirxprovidersharelibFactory)
	android.RegisterModuleType("hdmirxproviderImpl_sharelib_defaults", hdmirxproviderImplsharelibFactory)
    android.RegisterModuleType("hdmirxprovider_binary_defaults", hdmirxproviderDefaultsFactory)
	
}

func hdmirxproviderDefaultsFactory() android.Module {
    module := cc.DefaultsFactory()
    android.AddLoadHook(module, hdmirxproviderDefaults)
    return module
}

func hdmirxproviderDefaults(ctx android.LoadHookContext) {
    type props struct {
        Srcs []string
        Init_rc []string
		Shared_libs []string
		Header_libs []string
    }
    p := &props{}
    vars := ctx.Config().VendorConfig("mtkPlugin")
    if vars.Bool("MTK_HDMI_RXVC_SUPPORT") {
        p.Srcs = append(p.Srcs, "hdmirx-service.cpp")
		fmt.Printf("MTK_HDMI_RXVC_SUPPORT is yes")
		p.Init_rc = append(p.Init_rc, "android.hardware.camera.provider@2.4-hdmirx-service.rc")
		p.Shared_libs = append(p.Shared_libs, "android.hardware.camera.common@1.0")
		p.Shared_libs = append(p.Shared_libs, "android.hardware.camera.device@1.0")
		p.Shared_libs = append(p.Shared_libs, "android.hardware.camera.device@3.2")
		p.Shared_libs = append(p.Shared_libs, "android.hardware.camera.device@3.3")
		p.Shared_libs = append(p.Shared_libs, "android.hardware.camera.device@3.4")
		p.Shared_libs = append(p.Shared_libs, "android.hardware.camera.device@3.5")
		p.Shared_libs = append(p.Shared_libs, "android.hardware.camera.provider@2.4")
		p.Shared_libs = append(p.Shared_libs, "libbinder")
		p.Shared_libs = append(p.Shared_libs, "libhidlbase")
		p.Shared_libs = append(p.Shared_libs, "liblog")
		p.Shared_libs = append(p.Shared_libs, "libtinyxml2")
		p.Shared_libs = append(p.Shared_libs, "libutils")
		p.Shared_libs = append(p.Shared_libs, "libcutils")

		p.Header_libs = append(p.Header_libs, "camera.device@3.4-hdmirx-impl_headers")
		p.Header_libs = append(p.Header_libs, "camera.device@3.4-impl_headers")
		p.Header_libs = append(p.Header_libs, "camera.device@3.5-hdmirx-impl_headers")
		p.Header_libs = append(p.Header_libs, "camera.device@3.5-impl_headers")

        ctx.AppendProperties(p)
    } else {
		fmt.Printf("MTK_HDMI_RXVC_SUPPORT is no")
	}
}

func hdmirxprovidersharelibFactory() android.Module {
    module := cc.DefaultsFactory()
    android.AddLoadHook(module, hdmirxprovidersharelibDefaults)
    return module
}

func hdmirxprovidersharelibDefaults(ctx android.LoadHookContext) {
    type props struct {
        Srcs []string
		Shared_libs []string
		Static_libs []string
		Header_libs []string
		Export_include_dirs []string
    }
    p := &props{}
    vars := ctx.Config().VendorConfig("mtkPlugin")
    if vars.Bool("MTK_HDMI_RXVC_SUPPORT") {
		fmt.Printf("MTK_HDMI_RXVC_SUPPORT is yes")
        p.Srcs = append(p.Srcs, "HdmiRxCameraProviderImpl_2_4.cpp")
		p.Shared_libs = append(p.Shared_libs, "android.hardware.camera.common@1.0")
		p.Shared_libs = append(p.Shared_libs, "android.hardware.camera.device@1.0")
		p.Shared_libs = append(p.Shared_libs, "android.hardware.camera.device@3.2")
		p.Shared_libs = append(p.Shared_libs, "android.hardware.camera.device@3.3")
		p.Shared_libs = append(p.Shared_libs, "android.hardware.camera.device@3.4")
		p.Shared_libs = append(p.Shared_libs, "android.hardware.camera.device@3.5")
		p.Shared_libs = append(p.Shared_libs, "android.hardware.camera.device@3.6")
		p.Shared_libs = append(p.Shared_libs, "android.hardware.camera.provider@2.4")
		p.Shared_libs = append(p.Shared_libs, "android.hardware.graphics.mapper@2.0")
		p.Shared_libs = append(p.Shared_libs, "android.hardware.graphics.mapper@3.0")
		p.Shared_libs = append(p.Shared_libs, "android.hardware.graphics.mapper@4.0")
		p.Shared_libs = append(p.Shared_libs, "android.hidl.allocator@1.0")
		p.Shared_libs = append(p.Shared_libs, "android.hidl.memory@1.0")
		p.Shared_libs = append(p.Shared_libs, "camera.device@3.3-impl")
		p.Shared_libs = append(p.Shared_libs, "camera.device@3.4-hdmirx-impl")
		p.Shared_libs = append(p.Shared_libs, "camera.device@3.4-impl")
		p.Shared_libs = append(p.Shared_libs, "camera.device@3.5-hdmirx-impl")
		p.Shared_libs = append(p.Shared_libs, "camera.device@3.5-impl")
		p.Shared_libs = append(p.Shared_libs, "camera.device@3.6-hdmirx-impl")
		p.Shared_libs = append(p.Shared_libs, "libcamera_metadata")
		p.Shared_libs = append(p.Shared_libs, "libcutils")
		p.Shared_libs = append(p.Shared_libs, "libhardware")
		p.Shared_libs = append(p.Shared_libs, "libhidlbase")
		p.Shared_libs = append(p.Shared_libs, "liblog")
		p.Shared_libs = append(p.Shared_libs, "libtinyxml2")
		p.Shared_libs = append(p.Shared_libs, "libutils")

		p.Shared_libs = append(p.Shared_libs, "libdpframework")
		p.Shared_libs = append(p.Shared_libs, "libnativewindow")

		p.Static_libs = append(p.Static_libs, "android.hardware.camera.common@1.0-helper")

		p.Header_libs = append(p.Header_libs, "camera.device@3.4-hdmirx-impl_headers")
		p.Header_libs = append(p.Header_libs, "camera.device@3.5-hdmirx-impl_headers")
		p.Header_libs = append(p.Header_libs, "camera.device@3.6-hdmirx-impl_headers")

		p.Export_include_dirs = append(p.Export_include_dirs, ".")

        ctx.AppendProperties(p)
    } else {
		fmt.Printf("MTK_HDMI_RXVC_SUPPORT is no")
	}
	
}

func hdmirxproviderImplsharelibFactory() android.Module {
    module := cc.DefaultsFactory()
    android.AddLoadHook(module, hdmirxproviderImplDefaults)
    return module
}

func hdmirxproviderImplDefaults(ctx android.LoadHookContext) {
    type props struct {
        Srcs []string
		Shared_libs []string
		Static_libs []string
		Header_libs []string
		Export_include_dirs []string
    }
    p := &props{}
    vars := ctx.Config().VendorConfig("mtkPlugin")
    if vars.Bool("MTK_HDMI_RXVC_SUPPORT") {
		fmt.Printf("MTK_HDMI_RXVC_SUPPORT is yes")
        p.Srcs = append(p.Srcs, "CameraProvider_2_4.cpp")
		p.Shared_libs = append(p.Shared_libs, "android.hardware.camera.common@1.0")
		p.Shared_libs = append(p.Shared_libs, "android.hardware.camera.device@1.0")
		p.Shared_libs = append(p.Shared_libs, "android.hardware.camera.device@3.2")
		p.Shared_libs = append(p.Shared_libs, "android.hardware.camera.device@3.3")
		p.Shared_libs = append(p.Shared_libs, "android.hardware.camera.device@3.4")
		p.Shared_libs = append(p.Shared_libs, "android.hardware.camera.device@3.5")
		p.Shared_libs = append(p.Shared_libs, "android.hardware.camera.provider@2.4")
		p.Shared_libs = append(p.Shared_libs, "android.hardware.camera.provider@2.4-hdmirx")
		p.Shared_libs = append(p.Shared_libs, "android.hardware.graphics.mapper@2.0")
		p.Shared_libs = append(p.Shared_libs, "android.hardware.graphics.mapper@3.0")
		p.Shared_libs = append(p.Shared_libs, "android.hardware.graphics.mapper@4.0")
		p.Shared_libs = append(p.Shared_libs, "android.hidl.allocator@1.0")
		p.Shared_libs = append(p.Shared_libs, "android.hidl.memory@1.0")
		p.Shared_libs = append(p.Shared_libs, "camera.device@1.0-impl")
		p.Shared_libs = append(p.Shared_libs, "camera.device@3.2-impl")
		p.Shared_libs = append(p.Shared_libs, "camera.device@3.3-impl")
		p.Shared_libs = append(p.Shared_libs, "camera.device@3.4-hdmirx-impl")
		p.Shared_libs = append(p.Shared_libs, "camera.device@3.4-impl")
		p.Shared_libs = append(p.Shared_libs, "camera.device@3.5-hdmirx-impl")
		p.Shared_libs = append(p.Shared_libs, "camera.device@3.5-impl")
		p.Shared_libs = append(p.Shared_libs, "libcamera_metadata")
		p.Shared_libs = append(p.Shared_libs, "libcutils")
		p.Shared_libs = append(p.Shared_libs, "libhardware")
		p.Shared_libs = append(p.Shared_libs, "libhidlbase")
		p.Shared_libs = append(p.Shared_libs, "liblog")
		p.Shared_libs = append(p.Shared_libs, "libtinyxml2")
		p.Shared_libs = append(p.Shared_libs, "libutils")
		
		p.Static_libs = append(p.Static_libs, "android.hardware.camera.common@1.0-helper")

		p.Header_libs = append(p.Header_libs, "camera.device@3.4-hdmirx-impl_headers")
		p.Header_libs = append(p.Header_libs, "camera.device@3.4-impl_headers")
		p.Header_libs = append(p.Header_libs, "camera.device@3.5-hdmirx-impl_headers")
		p.Header_libs = append(p.Header_libs, "camera.device@3.5-impl_headers")

		p.Export_include_dirs = append(p.Export_include_dirs, ".")		

        ctx.AppendProperties(p)
    } else {
		fmt.Printf("MTK_HDMI_RXVC_SUPPORT is no")
	}
	
}




