#pragma once
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#ifdef __cplusplus
extern "C" {
#endif

// 跨平台统一接口
VkSurfaceKHR CreatePlatformVulkanSurface(VkInstance instance, GLFWwindow* window);

#ifdef __cplusplus
}
#endif

