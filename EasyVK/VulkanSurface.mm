#include "VulkanSurface.h"

#define GLFW_EXPOSE_NATIVE_COCOA
#include <vulkan/vulkan_macos.h>
#include <GLFW/glfw3native.h>

#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>
#include <iostream>

VkSurfaceKHR CreatePlatformVulkanSurface(VkInstance instance, GLFWwindow* window)
{
    if (!instance || !window) return VK_NULL_HANDLE;

    // 获取 GLFW 对应的 NSWindow 和 NSView
    NSWindow* nswindow = glfwGetCocoaWindow(window);
    if (!nswindow) return VK_NULL_HANDLE;

    NSView* nsview = [nswindow contentView];

    // 设置 Layer 为 CAMetalLayer
    [nsview setWantsLayer:YES];
    [nsview setLayer:[CAMetalLayer layer]];

    VkMacOSSurfaceCreateInfoMVK surfaceInfo{};
    surfaceInfo.sType = VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK;
    surfaceInfo.pView = nsview;

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkResult res = vkCreateMacOSSurfaceMVK(instance, &surfaceInfo, nullptr, &surface);
    if (res != VK_SUCCESS) {
        std::cerr << "vkCreateMacOSSurfaceMVK failed: " << res << std::endl;
        return VK_NULL_HANDLE;
    }
    return surface;
}

