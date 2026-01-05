#ifndef SAMPLES2DCOMPUTE_H
#define SAMPLES2DCOMPUTE_H

#include "EasyVulkan.h"

class Samples2DCmp{
public:
    Samples2DCmp();
    ~Samples2DCmp();

    void initResource(const QString&);

    void runDispatch(const vulkan::commandBuffer& command_buffer);

private:
    /*管线资源*/
    vulkan::descriptorSetLayout descriptorSetLayout_compute; //描述符布局
    vulkan::pipeline pipeline_compute;             //管线
    vulkan::pipelineLayout pipelineLayout_compute; //管线布局
    std::unique_ptr<vulkan::descriptorPool> descriptor_pool;
    vulkan::descriptorSet descriptorSet_compute;

    /*图像资源*/
    VkExtent2D imageExtent;
    vulkan::imageMemory image_memory_src;
    vulkan::imageView image_view_src;
    vulkan::imageMemory image_memory_dst;
    vulkan::imageView image_view_dst;
};

#endif // SAMPLES2DCOMPUTE_H
