#ifndef EASYVULKAN_H
#define EASYVULKAN_H

#include "VKBase+.h"

static const VkExtent2D& windowSize = vulkan::graphicsBase::Base().SwapchainCreateInfo().imageExtent;

namespace easyVulkan {

    struct renderPassWithFramebuffers {
        vulkan::renderPass renderPass;
        std::vector<vulkan::framebuffer> framebuffers;
    };

    //创建一个最简单的渲染通道
    extern const renderPassWithFramebuffers& CreateRpwf_Screen();

    //创建一个启动动画
    extern void BootScreen(const char* imagePath,VkFormat imageFormat);
}
#endif // EASYVULKAN_H
