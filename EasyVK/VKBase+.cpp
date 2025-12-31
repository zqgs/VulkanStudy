#include "VKBase+.h"

using namespace vulkan;

formatInfo FormatInfo(VkFormat format)
{
#ifndef NDEBUG
    if (uint32_t(format) >= uint32_t(sizeof(formatInfos_v1_0) / sizeof(formatInfos_v1_0[0]))){
        qDebug("[ FormatInfo ] ERROR\nThis function only supports definite formats provided by VK_VERSION_1_0.\n"),
        abort();
    }
#endif
    return formatInfos_v1_0[format];
}
VkFormat Corresponding16BitFloatFormat(VkFormat format_32BitFloat)
{
    switch (format_32BitFloat) {
    case VK_FORMAT_R32_SFLOAT:
        return VK_FORMAT_R16_SFLOAT;
    case VK_FORMAT_R32G32_SFLOAT:
        return VK_FORMAT_R16G16_SFLOAT;
    case VK_FORMAT_R32G32B32_SFLOAT:
        return VK_FORMAT_R16G16B16_SFLOAT;
    case VK_FORMAT_R32G32B32A32_SFLOAT:
        return VK_FORMAT_R16G16B16A16_SFLOAT;
    }
    return format_32BitFloat;
}

const VkFormatProperties &FormatProperties(VkFormat format)
{
    return graphicsBase::Plus().FormatProperties(format);
}


graphicsBasePlus graphicsBasePlus::singleton;

graphicsBasePlus::graphicsBasePlus():
    formatProperties(sizeof(formatInfos_v1_0) / sizeof(formatInfos_v1_0[0]))
{
    //在创建逻辑设备时执行Initialize()
    auto Initialize = [] {
        if (graphicsBase::Base().QueueFamilyIndex_Graphics() != VK_QUEUE_FAMILY_IGNORED){
            singleton.commandPool_graphics.Create(graphicsBase::Base().QueueFamilyIndex_Graphics(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT),
            singleton.commandPool_graphics.AllocateBuffers(singleton.commandBuffer_transfer);
        }
        if (graphicsBase::Base().QueueFamilyIndex_Compute() != VK_QUEUE_FAMILY_IGNORED){
            singleton.commandPool_compute.Create(graphicsBase::Base().QueueFamilyIndex_Compute(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
        }
        if (graphicsBase::Base().QueueFamilyIndex_Presentation() != VK_QUEUE_FAMILY_IGNORED &&
            graphicsBase::Base().QueueFamilyIndex_Presentation() != graphicsBase::Base().QueueFamilyIndex_Graphics() &&
            graphicsBase::Base().SwapchainCreateInfo().imageSharingMode == VK_SHARING_MODE_EXCLUSIVE){
            singleton.commandPool_presentation.Create(graphicsBase::Base().QueueFamilyIndex_Presentation(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT),
            singleton.commandPool_presentation.AllocateBuffers(singleton.commandBuffer_presentation);
        }

        for (size_t i = 0; i < singleton.formatProperties.size(); i++){
            //该接口通过vulkan查询物理设备支持哪些格式
            vkGetPhysicalDeviceFormatProperties(graphicsBase::Base().PhysicalDevice(),VkFormat(i),&singleton.formatProperties[i]);
        }
    };

    auto CleanUp = [] {
        singleton.commandPool_graphics.~commandPool();
        singleton.commandPool_presentation.~commandPool();
        singleton.commandPool_compute.~commandPool();
    };
    graphicsBase::Plus(singleton);
    graphicsBase::Base().AddCallback_CreateDevice(Initialize);
    graphicsBase::Base().AddCallback_DestroyDevice(CleanUp);
}

const VkFormatProperties &graphicsBasePlus::FormatProperties(VkFormat format) const
{
#ifndef NDEBUG
       if (uint32_t(format) >= formatProperties.size())
           qDebug("[ FormatProperties ] ERROR\nThis function only supports definite formats provided by VK_VERSION_1_0.\n"),
           abort();
#endif
        return formatProperties[format];
}

result_t graphicsBasePlus::ExecuteCommandBuffer_Graphics(VkCommandBuffer commandBuffer) const
{
    fence f;
    VkSubmitInfo submitInfo = {};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    VkResult result = graphicsBase::Base().SubmitCommandBuffer_Graphics(submitInfo, f);
    if (!result){
        f.Wait();
    }
    return result;
}

result_t graphicsBasePlus::AcquireImageOwnership_Presentation(VkSemaphore semaphore_renderingIsOver, VkSemaphore semaphore_ownershipIsTransfered, VkFence fence) const
{
    if (VkResult result = commandBuffer_presentation.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT)){
        return result;
    }
    graphicsBase::Base().CmdTransferImageOwnership(commandBuffer_presentation);
    if (VkResult result = commandBuffer_presentation.End()){
        return result;
    }
    return graphicsBase::Base().SubmitCommandBuffer_Presentation(commandBuffer_presentation, semaphore_renderingIsOver, semaphore_ownershipIsTransfered, fence);

}

stagingBuffer::stagingBuffer(VkDeviceSize size)
{
    Expand(size);
}

void stagingBuffer::RetrieveData(void *pData_src, VkDeviceSize size) const
{
    buffer_memory.RetrieveData(pData_src, size);
}

void stagingBuffer::Expand(VkDeviceSize size)
{
    if (size <= AllocationSize())
        return;
    Release();
    VkBufferCreateInfo bufferCreateInfo = {};
    bufferCreateInfo.size = size;
    bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    buffer_memory.Create(bufferCreateInfo, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
}

void stagingBuffer::Release()
{
    buffer_memory.~bufferMemory();
}

void *stagingBuffer::MapMemory(VkDeviceSize size)
{
    Expand(size);
    void* pData_dst = nullptr;
    buffer_memory.MapMemory(pData_dst, size);
    memoryUsage = size;
    return pData_dst;
}

void stagingBuffer::UnmapMemory()
{
    buffer_memory.UnmapMemory(memoryUsage);
    memoryUsage = 0;
}

void stagingBuffer::BufferData(const void *pData_src, VkDeviceSize size)
{
    Expand(size);
    buffer_memory.BufferData(pData_src, size);
}

VkImage stagingBuffer::AliasedImage2d(VkFormat format, VkExtent2D extent)
{
    //检查硬件是否支持线形布局的图像作为bit命令的来源
    if (!(FormatProperties(format).linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT)){
        return VkImage(VK_NULL_HANDLE);
    }
    VkDeviceSize imageDataSize = VkDeviceSize(FormatInfo(format).sizePerPixel) * extent.width * extent.height;
    if (imageDataSize > AllocationSize()){
        return VkImage(VK_NULL_HANDLE);
    }
    VkImageFormatProperties imageFormatProperties = {};
    vkGetPhysicalDeviceImageFormatProperties(graphicsBase::Base().PhysicalDevice(),
        format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 0, &imageFormatProperties);
    //检查各个参数是否在容许范围内
    if (extent.width > imageFormatProperties.maxExtent.width ||
        extent.height > imageFormatProperties.maxExtent.height ||
        imageDataSize > imageFormatProperties.maxResourceSize){
        return VkImage(VK_NULL_HANDLE); //如不满足要求，返回VK_NULL_HANDLE
    }

    //创建2D混叠图像
    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = format;
    imageCreateInfo.extent = { extent.width, extent.height, 1 };
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_LINEAR;
    imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;

    //图像存在先析构
    aliasedImage.~image();
    aliasedImage.Create(imageCreateInfo);

    //即便是线性图像，其中数据也未必是紧密排列的，过小的图像和奇数宽的图像可能需要在每行或整张图像末尾加入填充字节，以满足对齐要求
    /*CPU 3x3矩阵
     * 1 2 3
     * 4 5 6
     * 7 8 9
     *
     *GPU 3x3矩阵 -->gpu cache line是64B/128B 所以每必须对齐到64B/128B
     * 1 2 3 ... 64(填充值)
     * 4 5 6 ... 64(填充值)
     * 7 8 9 ... 64(填充值)
     *
     *以下是获取图像内存布局的具体参数:
     */

    // VkDeviceSize    offset;      子资源范围对应的图像数据在整个图像的数据中的起始位置，单位为字节
    // VkDeviceSize    size;        子资源范围对应的图像数据的大小，单位为字节
    // VkDeviceSize    rowPitch;    每行起始位置之间相距的字节数
    // VkDeviceSize    arrayPitch;  （适用于2D图像数组）每个图层起始位置之间相距的字节数
    // VkDeviceSize    depthPitch;  （适用于3D图像）每个深度层起始位置之间相距的字节数
    VkImageSubresource subResource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
    VkSubresourceLayout subresourceLayout = {};
    vkGetImageSubresourceLayout(graphicsBase::Base().Device(), aliasedImage, &subResource, &subresourceLayout);

    if (subresourceLayout.size != imageDataSize){
        return VkImage(VK_NULL_HANDLE);
    }
    aliasedImage.BindMemory(buffer_memory.Memory());
    return aliasedImage;
}


deviceLocalBuffer::deviceLocalBuffer(VkDeviceSize size, VkBufferUsageFlags desiredUsages_Without_transfer_dst)
{
   Create(size, desiredUsages_Without_transfer_dst);
}

void deviceLocalBuffer::Create(VkDeviceSize size, VkBufferUsageFlags desiredUsages_Without_transfer_dst)
{
    VkBufferCreateInfo bufferCreateInfo = {};
    bufferCreateInfo.size = size;
    bufferCreateInfo.usage = desiredUsages_Without_transfer_dst | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    //短路执行，第一行的false||是为了对齐
    false ||
    buffer_memory.CreateBuffer(bufferCreateInfo) ||
    buffer_memory.AllocateMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) && //&&运算符优先级高于||
    buffer_memory.AllocateMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ||
    buffer_memory.BindMemory();
}

void deviceLocalBuffer::Recreate(VkDeviceSize size, VkBufferUsageFlags desiredUsages_Without_transfer_dst)
{
    graphicsBase::Base().WaitIdle(); //deviceLocalBuffer封装的缓冲区可能会在每一帧中被频繁使用，重建它之前应确保物理设备没有在使用它
    buffer_memory.~bufferMemory();
    Create(size, desiredUsages_Without_transfer_dst);
}

void deviceLocalBuffer::CmdUpdateBuffer(VkCommandBuffer commandBuffer, const void *pData_src, VkDeviceSize size_Limited_to_65536, VkDeviceSize offset) const
{
    vkCmdUpdateBuffer(commandBuffer, buffer_memory.Buffer(), offset, size_Limited_to_65536, pData_src);
}

void deviceLocalBuffer::CmdUpdateBuffer(VkCommandBuffer commandBuffer, const void* data,VkDeviceSize size)
{
    vkCmdUpdateBuffer(commandBuffer, buffer_memory.Buffer(), 0, size,data);
}

void deviceLocalBuffer::TransferData(const void *pData_src, VkDeviceSize size, VkDeviceSize offset) const
{
    //具有VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT属性，直接调用bufferMemory.BufferData(...)
    if (buffer_memory.MemoryProperties() & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        buffer_memory.BufferData(pData_src, size, offset);
        return;
    }
    //先将数据上传到暂存缓冲区 CPU内存(cpu可见,gpu不可见) -> CPU内存暂存区(cpu可见,gpu不可见)
    stagingBuffer::BufferData_MainThread(pData_src, size);
    //创建拷贝命令 CPU内存暂存区(cpu可见,gpu不可见)->GPU显存(cpu不可见,gpu可见)
    auto& commandBuffer = graphicsBase::Plus().CommandBuffer_Transfer();
    commandBuffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VkBufferCopy region = { 0, offset, size };
    vkCmdCopyBuffer(commandBuffer,stagingBuffer::Buffer_MainThread(),buffer_memory.Buffer(),1,&region);
    commandBuffer.End();
    //执行拷贝命令
    graphicsBase::Plus().ExecuteCommandBuffer_Graphics(commandBuffer);
}

void deviceLocalBuffer::TransferData(const void *pData_src, uint32_t elementCount, VkDeviceSize elementSize, VkDeviceSize stride_src, VkDeviceSize stride_dst, VkDeviceSize offset) const
{
    //具有VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT属性
    if(buffer_memory.MemoryProperties() & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT){
        void* pData_dst = nullptr;
        buffer_memory.MapMemory(pData_dst,VkDeviceSize(stride_dst * elementCount),offset);
        for(size_t i = 0; i < elementCount;i++){
            memcpy(stride_dst * i + static_cast<uint8_t*>(pData_dst), stride_src * i + static_cast<const uint8_t*>(pData_src), size_t(elementSize));
        }
        buffer_memory.UnmapMemory(VkDeviceSize(stride_dst * elementCount),offset);
        return;
    }
    stagingBuffer::BufferData_MainThread(pData_src, elementCount * stride_src);

    auto& commandBuffer = graphicsBase::Plus().CommandBuffer_Transfer();
    commandBuffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    std::vector<VkBufferCopy> regions(elementCount);
    for (size_t i = 0; i < regions.size(); i++){
        regions[i] = VkBufferCopy{ stride_src * i, stride_dst * i + offset, elementSize };
    }
    vkCmdCopyBuffer(commandBuffer, stagingBuffer::Buffer_MainThread(), buffer_memory.Buffer(), elementCount, regions.data());
    commandBuffer.End();
    graphicsBase::Plus().ExecuteCommandBuffer_Graphics(commandBuffer);
}

vertexBuffer::vertexBuffer(VkDeviceSize size, VkBufferUsageFlags otherUsages):
    deviceLocalBuffer(size,VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | otherUsages)
{

}

void vertexBuffer::Create(VkDeviceSize size, VkBufferUsageFlags otherUsages)
{
    deviceLocalBuffer::Create(size,VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | otherUsages);
}

void vertexBuffer::Recreate(VkDeviceSize size, VkBufferUsageFlags otherUsages)
{
    deviceLocalBuffer::Recreate(size,VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | otherUsages);
}

indexBuffer::indexBuffer(VkDeviceSize size, VkBufferUsageFlags otherUsages):
    deviceLocalBuffer(size,VK_BUFFER_USAGE_INDEX_BUFFER_BIT | otherUsages)
{

}

void indexBuffer::Create(VkDeviceSize size, VkBufferUsageFlags otherUsages)
{
    deviceLocalBuffer::Create(size,VK_BUFFER_USAGE_INDEX_BUFFER_BIT | otherUsages);
}

void indexBuffer::Recreate(VkDeviceSize size, VkBufferUsageFlags otherUsages)
{
    deviceLocalBuffer::Recreate(size,VK_BUFFER_USAGE_INDEX_BUFFER_BIT | otherUsages);
}

uniformBuffer::uniformBuffer(VkDeviceSize size, VkBufferUsageFlags otherUsages):
    deviceLocalBuffer(size,VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | otherUsages)
{

}

void uniformBuffer::Create(VkDeviceSize size, VkBufferUsageFlags otherUsages)
{
    deviceLocalBuffer::Create(size,VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | otherUsages);
}

void uniformBuffer::Recreate(VkDeviceSize size, VkBufferUsageFlags otherUsages)
{
    deviceLocalBuffer::Recreate(size,VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | otherUsages);
}

VkDeviceSize uniformBuffer::CalculateAlignedSize(VkDeviceSize dataSize)
{
    const auto& alignment = graphicsBase::Base().PhysicalDeviceProperties().limits.minUniformBufferOffsetAlignment;
    return dataSize + alignment - 1 & ~(alignment - 1); //等价于(dataSize + alignment - 1) / alignment * alignment
}

storageBuffer::storageBuffer(VkDeviceSize size, VkBufferUsageFlags otherUsages):
    deviceLocalBuffer(size,VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT | otherUsages)
{

}

void storageBuffer::Create(VkDeviceSize size, VkBufferUsageFlags otherUsages)
{
    deviceLocalBuffer::Create(size,VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT | otherUsages);
}

void storageBuffer::Recreate(VkDeviceSize size, VkBufferUsageFlags otherUsages)
{
    deviceLocalBuffer::Recreate(size,VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT | otherUsages);
}

VkDeviceSize storageBuffer::CalculateAlignedSize(VkDeviceSize dataSize)
{
    const auto& alignment = graphicsBase::Base().PhysicalDeviceProperties().limits.minStorageBufferOffsetAlignment;
    return dataSize + alignment - 1 & ~(alignment - 1); //等价于(dataSize + alignment - 1) / alignment * alignment
}
