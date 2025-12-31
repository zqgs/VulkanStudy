#ifndef VKBASE__H
#define VKBASE__H

#include "VKBase.h"
#include "VKFormat.h"

namespace vulkan {

    class graphicsBasePlus {
        std::vector<VkFormatProperties> formatProperties;
        //渲染需要的pool和buffer
        commandPool commandPool_graphics;
        commandPool commandPool_presentation;
        commandPool commandPool_compute;
        commandBuffer commandBuffer_transfer; //从commandPool_graphics分配
        commandBuffer commandBuffer_presentation;

        //单例
        static graphicsBasePlus singleton;
        graphicsBasePlus();
        graphicsBasePlus(graphicsBasePlus&&) = delete;
        ~graphicsBasePlus() = default;

    public:
        //Getter
        const VkFormatProperties& FormatProperties(VkFormat format) const;
        const commandPool& CommandPool_Graphics() const { return commandPool_graphics; }
        const commandPool& CommandPool_Compute() const { return commandPool_compute; }
        const commandBuffer& CommandBuffer_Transfer() const { return commandBuffer_transfer; }

        //简化命令提交
        result_t ExecuteCommandBuffer_Graphics(VkCommandBuffer commandBuffer) const;

        //该函数专用于向呈现队列提交用于接收交换链图像的队列族所有权的命令缓冲区
        result_t AcquireImageOwnership_Presentation(VkSemaphore semaphore_renderingIsOver, VkSemaphore semaphore_ownershipIsTransfered, VkFence fence = (VkFence)VK_NULL_HANDLE) const;
    };

/*暂存缓冲区：
    - 从具有 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT 属性的内存中分配
    - 可通过 vkMapMemory 函数由 CPU 访问
    - 用作数据上传和读取的中间缓冲区
 */
    class stagingBuffer {
        //专用于主线程的暂存缓冲区
        class stagingBuffer_mainThread {
        public:
            static stagingBuffer& Get() {
                static stagingBuffer instance;
                return instance;
            }
        private:
            stagingBuffer_mainThread() = delete;
        };
    protected:
        bufferMemory buffer_memory;
        VkDeviceSize memoryUsage = 0; //每次映射的内存大小
        image aliasedImage;
    public:
        stagingBuffer() = default;
        stagingBuffer(VkDeviceSize size);
        //Getter
        operator VkBuffer() const { return buffer_memory.Buffer(); }
        const VkBuffer* Address() const { return buffer_memory.AddressOfBuffer(); }
        VkDeviceSize AllocationSize() const { return buffer_memory.AllocationSize(); }
        VkImage AliasedImage() const { return aliasedImage; }
        //Const Function
        //该函数用于从缓冲区取回数据
        void RetrieveData(void* pData_src, VkDeviceSize size) const;
        //Non-const Function
        //该函数用于在所分配设备内存大小不够时重新分配内存
        void Expand(VkDeviceSize size);
        //该函数用于手动释放所有内存并销毁设备内存和缓冲区的handle
        void Release();
        void* MapMemory(VkDeviceSize size);
        void UnmapMemory();
        //该函数用于向缓冲区写入数据
        void BufferData(const void* pData_src, VkDeviceSize size);
        //该函数创建线性布局的混叠2d图像
        VkImage AliasedImage2d(VkFormat format, VkExtent2D extent);


        //Static Function
        static VkBuffer Buffer_MainThread() {
            return stagingBuffer_mainThread::Get();
        }
        static void Expand_MainThread(VkDeviceSize size) {
            stagingBuffer_mainThread::Get().Expand(size);
        }
        static void Release_MainThread() {
            stagingBuffer_mainThread::Get().Release();
        }
        static void* MapMemory_MainThread(VkDeviceSize size) {
            return stagingBuffer_mainThread::Get().MapMemory(size);
        }
        static void UnmapMemory_MainThread() {
            stagingBuffer_mainThread::Get().UnmapMemory();
        }
        static void BufferData_MainThread(const void* pData_src, VkDeviceSize size) {
            stagingBuffer_mainThread::Get().BufferData(pData_src, size);
        }
        static void RetrieveData_MainThread(void* pData_src, VkDeviceSize size) {
            stagingBuffer_mainThread::Get().RetrieveData(pData_src, size);
        }
        static VkImage AliasedImage2d_MainThread(VkFormat format, VkExtent2D extent) {
            return stagingBuffer_mainThread::Get().AliasedImage2d(format, extent);
        }
    };

/*设备本地缓冲区：
    - 从具有 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT 属性的内存中分配
    - 针对 GPU 访问进行了优化
    - 不保证可进行 CPU 映射，也不应依赖 CPU 映射
    - 数据上传必须通过暂存缓冲区和复制命令进行
 */
    class deviceLocalBuffer {
    protected:
        bufferMemory buffer_memory;
    public:
        deviceLocalBuffer() = default;
        deviceLocalBuffer(VkDeviceSize size, VkBufferUsageFlags desiredUsages_Without_transfer_dst);
        //Getter
        operator VkBuffer() const { return buffer_memory.Buffer(); }
        const VkBuffer* Address() const { return buffer_memory.AddressOfBuffer(); }
        VkDeviceSize AllocationSize() const { return buffer_memory.AllocationSize(); }
        //Non-const Function
        void Create(VkDeviceSize size, VkBufferUsageFlags desiredUsages_Without_transfer_dst);
        void Recreate(VkDeviceSize size, VkBufferUsageFlags desiredUsages_Without_transfer_dst);

        //内存暂存区->设备内存区 有以下两种
        //1.若数据量不大于65536个字节，用vkCmdUpdateBuffer(...)命令直接更新缓冲区
        void CmdUpdateBuffer(VkCommandBuffer commandBuffer, const void* pData_src, VkDeviceSize size_Limited_to_65536, VkDeviceSize offset = 0) const;
        void CmdUpdateBuffer(VkCommandBuffer commandBuffer, const void* data, VkDeviceSize size); //适用于从缓冲区开头更新连续的数据块

        //2.大于65536字节,在命令缓冲区使用vkCmdCopyBuffer进行更新
        void TransferData(const void* pData_src, VkDeviceSize size, VkDeviceSize offset = 0) const;//适用于更新连续的数据块
        void TransferData(const void* pData_src,
                          uint32_t elementCount,
                          VkDeviceSize elementSize,
                          VkDeviceSize stride_src,
                          VkDeviceSize stride_dst,
                          VkDeviceSize offset = 0) const; //适用于更新不连续的多块数据，stride是每组数据间的步长，offset是目标缓冲区中的offset
        template<typename T>
        void TransferData(const T& data_src) const {
            TransferData(&data_src, sizeof(T));
        }
    };

    //为顶点缓冲区创建专用的类型，vertexBuffer继承deviceLocalBuffer，在创建缓冲区时默认指定VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
    class vertexBuffer : public deviceLocalBuffer{
    public:
        vertexBuffer() = default;
        vertexBuffer(VkDeviceSize size,VkBufferUsageFlags otherUsages = 0);
        void Create(VkDeviceSize size,VkBufferUsageFlags otherUsages = 0);
        void Recreate(VkDeviceSize size, VkBufferUsageFlags otherUsages = 0);
    };

    //为索引缓冲区创建专用的类型，indexBuffer继承deviceLocalBuffer，在创建缓冲区时默认指定VK_BUFFER_USAGE_INDEX_BUFFER_BIT
    class indexBuffer : public deviceLocalBuffer{
    public:
        indexBuffer() = default;
        indexBuffer(VkDeviceSize size, VkBufferUsageFlags otherUsages = 0);
        void Create(VkDeviceSize size,VkBufferUsageFlags otherUsages = 0);
        void Recreate(VkDeviceSize size, VkBufferUsageFlags otherUsages = 0);
    };

    //为uniform缓冲区创建专用的类型，uniformBuffer继承deviceLocalBuffer，在创建缓冲区时默认指定VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
    class uniformBuffer : public deviceLocalBuffer{
    public:
        uniformBuffer() = default;
        uniformBuffer(VkDeviceSize size, VkBufferUsageFlags otherUsages = 0);
        void Create(VkDeviceSize size, VkBufferUsageFlags otherUsages = 0);
        void Recreate(VkDeviceSize size, VkBufferUsageFlags otherUsages = 0);
        //uniform缓冲与其他缓冲不同,该缓冲的长度必须是物理设备（GPU）要求的整数倍(该值可以从物理设备属性中提取)
        static VkDeviceSize CalculateAlignedSize(VkDeviceSize dataSize);
    };
    /*为storage缓冲区创建专用的类型，storageBuffer继承deviceLocalBuffer，在创建缓冲区时默认指定VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
     * 以下场景会用到storage buffer
     *  GPU 生成数据
     *      1.Compute Shader 算法结果
     *      2.图像处理输出
     *      3.点云 / 特征点 / 直方图
     *      4.后处理结果
     *  GPU <-> GPU
     *      1.Compute → Graphics
     *      2.多个 shader stage 共享数据
     *  GPU -> CPU
     *      1.读取计算结果
     *      2.调试数据
     *      3.离线处理结果
     */
    class storageBuffer : public deviceLocalBuffer{
    public:
        storageBuffer() = default;
        storageBuffer(VkDeviceSize size, VkBufferUsageFlags otherUsages = 0);
        void Create(VkDeviceSize size, VkBufferUsageFlags otherUsages = 0);
        void Recreate(VkDeviceSize size, VkBufferUsageFlags otherUsages = 0);
        //storage缓冲与其他缓冲不同,该缓冲的长度必须是物理设备（GPU）要求的整数倍(该值可以从物理设备属性中提取)
        static VkDeviceSize CalculateAlignedSize(VkDeviceSize dataSize);
    };

/*封装texture类，用于load file或Memory数据
*/
    class texture{
    public:
        static std::unique_ptr<uint8_t[]> LoadFile(const char* address, VkExtent2D& extent, formatInfo requniredFormatInfo) {
            if(ENABLE_DEBUG_MESSENGER){ //有些格式要求是无法满足的，因此加入在Debug Build下检查requiredFormatInfo
                //若要求数据为浮点数，stb_image只支持32位浮点数
                if(!(requniredFormatInfo.rawDataType == formatInfo::floatingPoint && requniredFormatInfo.sizePerComponent == 4) &&
                  //若要求数据为整形，stb_image只支持8位或16位每通道
                   !(requniredFormatInfo.rawDataType == formatInfo::integer && Between_Closed<int32_t>(1,requniredFormatInfo.sizePerComponent,2))){
                    qDebug("[ texture ] ERROR\nRequired format is not available for source image data!\n"),
                    abort();
                }
            }

            //用于接收长和宽，extent.width和extent.height是uint32_t，因此需要转换
            int& width = reinterpret_cast<int&>(extent.width);
            int& height = reinterpret_cast<int&>(extent.height);
            //用于接收原始色彩通道数，读完图后我用不着它，所以也没有将其初始化
            int channelCount;

            void* pImageData = nullptr;

            //读取图像。输出整形
            if(requniredFormatInfo.rawDataType == formatInfo::integer){
                //读取整形8bit图像
                if(requniredFormatInfo.sizePerComponent == 1){
                    pImageData = stbi_load(address,&width,&height,&channelCount,requniredFormatInfo.componentCount);
                }
                //读取整形16bit图像
                else{
                    pImageData = stbi_load_16(address,&width,&height,&channelCount,requniredFormatInfo.componentCount);
                }
            }
            //读取图像。输出浮点型(仅支持32位float)
            else{
                pImageData = stbi_loadf(address,&width,&height,&channelCount,requniredFormatInfo.componentCount);
            }
            if(!pImageData){
                qDebug("[ texture ] ERROR\nFailed to load the file: %s\n", address);
            }
            return std::unique_ptr<uint8_t[]>(static_cast<uint8_t*>(pImageData));
        }
        static std::unique_ptr<uint8_t[]> LoadFile(const uint8_t* address, size_t addrSize, VkExtent2D& extent, formatInfo requniredFormatInfo) {
            if(ENABLE_DEBUG_MESSENGER){ //有些格式要求是无法满足的，因此加入在Debug Build下检查requiredFormatInfo
                //若要求数据为浮点数，stb_image只支持32位浮点数
                if(!(requniredFormatInfo.rawDataType == formatInfo::floatingPoint && requniredFormatInfo.sizePerComponent == 4) &&
                  //若要求数据为整形，stb_image只支持8位或16位每通道
                   !(requniredFormatInfo.rawDataType == formatInfo::integer && Between_Closed<int32_t>(1,requniredFormatInfo.sizePerComponent,2))){
                    qDebug("[ texture ] ERROR\nRequired format is not available for source image data!\n"),
                    abort();
                }
            }

            //用于接收长和宽，extent.width和extent.height是uint32_t，因此需要转换
            int& width = reinterpret_cast<int&>(extent.width);
            int& height = reinterpret_cast<int&>(extent.height);
            //用于接收原始色彩通道数，读完图后我用不着它，所以也没有将其初始化
            int channelCount;

            void* pImageData = nullptr;

            //读取图像。输出整形
            if(requniredFormatInfo.rawDataType == formatInfo::integer){
                //读取整形8bit图像
                if(requniredFormatInfo.sizePerComponent == 1){
                    pImageData = stbi_load_from_memory(address,addrSize,&width,&height,&channelCount,requniredFormatInfo.componentCount);
                }
                //读取整形16bit图像
                else{
                    pImageData = stbi_load_16_from_memory(address,addrSize,&width,&height,&channelCount,requniredFormatInfo.componentCount);
                }
            }
            //读取图像。输出浮点型(仅支持32位float)
            else{
                pImageData = stbi_loadf_from_memory(address,addrSize,&width,&height,&channelCount,requniredFormatInfo.componentCount);
            }
            if(!pImageData){
                qDebug("[ texture ] ERROR\nFailed to load image data from the given address!\n");
            }
            return std::unique_ptr<uint8_t[]>(static_cast<uint8_t*>(pImageData));
        }
    };

/*封装imageOperation图像相关操作类
*/
    struct imageOperation{
        //用于指定管线屏障和内存屏障的参数集合
        struct imageMemoryBarrierParameterPack{
            const bool isNeeded = false;//是否需要屏障，默认为false
            const VkPipelineStageFlags stage = 0; //为srcStages或dstStages -- 管线阶段(管线屏障)
            const VkAccessFlags access = 0;//srcAccessMask 或 dstAccessMask -- 源操作、目标操作(内存屏障)
            const VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED; //old layout 或 new layout
            imageMemoryBarrierParameterPack() = default;
            imageMemoryBarrierParameterPack(VkPipelineStageFlags s,VkAccessFlags a,VkImageLayout l):
                stage(s),
                access(a),
                layout(l){}
        };
        //copy图像(buffer->image后，图像layout是transfer，需要转成present才能显示)
        static void  CmdCopyBufferToImage(VkCommandBuffer commandBuffer, //命令缓冲区
                                          VkBuffer buffer,//作为数据源的缓冲区
                                          VkImage image,//接受数据的目标图像
                                          const VkBufferImageCopy&  region, //指定将缓冲区的那些部分拷贝到图像的哪些部分
                                          imageMemoryBarrierParameterPack imb_from,
                                          imageMemoryBarrierParameterPack imb_to){

            VkImageMemoryBarrier imageMemoryBarrier{
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                nullptr,
                imb_from.access,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                imb_from.layout,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                image,
                VkImageSubresourceRange{
                    region.imageSubresource.aspectMask,
                    region.imageSubresource.mipLevel,
                    1,
                    region.imageSubresource.baseArrayLayer,
                    region.imageSubresource.layerCount
                }
            };
            //拷贝前需管线屏障
            if(imb_from.isNeeded){
                vkCmdPipelineBarrier(commandBuffer,
                                     imb_from.stage,
                                     VK_PIPELINE_STAGE_TRANSFER_BIT,
                                     0,
                                     0,
                                     nullptr,
                                     0,
                                     nullptr,
                                     1,
                                     &imageMemoryBarrier);
            }
            /*调用拷贝命令
             * 1.图像的内存布局应为VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL(传输命令需要)
             * 2.拷贝发生的管线阶段为VK_PIPELINE_STAGE_TRANSFER_BIT
             * 3.拷贝对应的操作类型VK_ACCESS_TRANSFER_WRITE_BIT
            */
            vkCmdCopyBufferToImage(commandBuffer,buffer,image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&region);

            //拷贝后需要管线屏障
            if(imb_to.isNeeded){
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                imageMemoryBarrier.dstAccessMask = imb_to.access;
                imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                imageMemoryBarrier.newLayout = imb_to.layout;
                vkCmdPipelineBarrier(commandBuffer,
                                     VK_PIPELINE_STAGE_TRANSFER_BIT,
                                     imb_to.stage,
                                     0,
                                     0,
                                     nullptr,
                                     0,
                                     nullptr,
                                     1,
                                     &imageMemoryBarrier);
            }
        }
        //blit图像(只能image->image 且image layout还是transfer，如果需要渲染<layout转成attachment -- 给render_pass使用>或者显示<layout转成present -- 给交换链呈现>)
        static void CmdBlitImage(VkCommandBuffer commandBuffer, //命令缓冲区
                                 VkImage image_src,//数据源图像
                                 VkImage image_dst,//目标图像
                                 const VkImageBlit& region, //指定将源图像拷贝目标图像的哪些部分
                                 imageMemoryBarrierParameterPack imb_dst_from,
                                 imageMemoryBarrierParameterPack imb_dst_to,
                                 VkFilter filter = VK_FILTER_LINEAR){//若图像可能会被缩放，在此指定滤波方式
            VkImageMemoryBarrier imageMemoryBarrier{
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                nullptr,
                imb_dst_from.access,//指定源图像操作是什么    -- 过去是被谁在操作
                VK_ACCESS_TRANSFER_WRITE_BIT,//将来会在transfer阶段写入
                imb_dst_from.layout,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                image_src,
                VkImageSubresourceRange{
                    region.dstSubresource.aspectMask,
                    region.dstSubresource.mipLevel,
                    1,
                    region.dstSubresource.baseArrayLayer,
                    region.dstSubresource.layerCount
                }
            };
            //拷贝前需管线屏障
            if(imb_dst_from.isNeeded){
                vkCmdPipelineBarrier(commandBuffer,
                                     imb_dst_from.stage,
                                     VK_PIPELINE_STAGE_TRANSFER_BIT,
                                     0,
                                     0,
                                     nullptr,
                                     0,
                                     nullptr,
                                     1,
                                     &imageMemoryBarrier);
            }
            /*调用拷贝命令
             * 1.源图像的内存布局应为VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL(传输命令需要)
             * 2.目标图像的内存布局应为VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL(传输命令需要)
            */
            vkCmdBlitImage(commandBuffer,
                           image_src,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           image_dst,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1,
                           &region,
                           filter);

            //拷贝后需要管线屏障(想要知道拷贝是否完成，阶段开始Write,阶段结束..)
            if(imb_dst_to.isNeeded){
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; //blit命令结束
                imageMemoryBarrier.dstAccessMask = imb_dst_to.access;//目标操作 --可能是渲染管线，可能是交换链呈现
                imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                imageMemoryBarrier.newLayout = imb_dst_to.layout;
                vkCmdPipelineBarrier(commandBuffer,
                                     VK_PIPELINE_STAGE_TRANSFER_BIT,
                                     imb_dst_to.stage,
                                     0,
                                     0,
                                     nullptr,
                                     0,
                                     nullptr,
                                     1,
                                     &imageMemoryBarrier);
            }
        }
    };
}

extern formatInfo FormatInfo(VkFormat format);
extern VkFormat Corresponding16BitFloatFormat(VkFormat format_32BitFloat);
extern const VkFormatProperties& FormatProperties(VkFormat format);


struct graphicsPipelineCreateInfoPack {
    VkGraphicsPipelineCreateInfo createInfo;
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    //Vertex Input
    VkPipelineVertexInputStateCreateInfo vertexInputStateCi;
    std::vector<VkVertexInputBindingDescription> vertexInputBindings;
    std::vector<VkVertexInputAttributeDescription> vertexInputAttributes;
    //Input Assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCi;
    //Tessellation
    VkPipelineTessellationStateCreateInfo tessellationStateCi;
    //Viewport
    VkPipelineViewportStateCreateInfo viewportStateCi;
    std::vector<VkViewport> viewports;
    std::vector<VkRect2D> scissors;
    uint32_t dynamicViewportCount = 1; //动态视口/剪裁不会用到上述的vector，因此动态视口和剪裁的个数向这俩变量手动指定
    uint32_t dynamicScissorCount = 1;
    //Rasterization
    VkPipelineRasterizationStateCreateInfo rasterizationStateCi;
    //Multisample
    VkPipelineMultisampleStateCreateInfo multisampleStateCi;
    //Depth & Stencil
    VkPipelineDepthStencilStateCreateInfo depthStencilStateCi;
    //Color Blend
    VkPipelineColorBlendStateCreateInfo colorBlendStateCi;
    std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates;
    //Dynamic
    VkPipelineDynamicStateCreateInfo dynamicStateCi;
    std::vector<VkDynamicState> dynamicStates;
    //--------------------
    graphicsPipelineCreateInfoPack():
        createInfo{},
        vertexInputStateCi{},
        inputAssemblyStateCi{},
        tessellationStateCi{},
        viewportStateCi{},
        rasterizationStateCi{},
        multisampleStateCi{},
        depthStencilStateCi{},
        colorBlendStateCi{},
        dynamicStateCi{}
    {
        createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        vertexInputStateCi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        inputAssemblyStateCi.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        tessellationStateCi.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
        viewportStateCi.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        rasterizationStateCi.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        multisampleStateCi.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        depthStencilStateCi.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        colorBlendStateCi.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        dynamicStateCi.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;

        SetCreateInfos();
        //若非派生管线，createInfo.basePipelineIndex不得为0，设置为-1
        createInfo.basePipelineIndex = -1;
    }
    //移动构造器，所有指针都要重新赋值
    graphicsPipelineCreateInfoPack(const graphicsPipelineCreateInfoPack& other) {
        createInfo = other.createInfo;
        SetCreateInfos();

        vertexInputStateCi = other.vertexInputStateCi;
        inputAssemblyStateCi = other.inputAssemblyStateCi;
        tessellationStateCi = other.tessellationStateCi;
        viewportStateCi = other.viewportStateCi;
        rasterizationStateCi = other.rasterizationStateCi;
        multisampleStateCi = other.multisampleStateCi;
        depthStencilStateCi = other.depthStencilStateCi;
        colorBlendStateCi = other.colorBlendStateCi;
        dynamicStateCi = other.dynamicStateCi;

        shaderStages = other.shaderStages;
        vertexInputBindings = other.vertexInputBindings;
        vertexInputAttributes = other.vertexInputAttributes;
        viewports = other.viewports;
        scissors = other.scissors;
        colorBlendAttachmentStates = other.colorBlendAttachmentStates;
        dynamicStates = other.dynamicStates;
        UpdateAllArrayAddresses();
    }
    //Getter，这里我没用const修饰符
    operator VkGraphicsPipelineCreateInfo& () { return createInfo; }
    //Non-const Function
    //该函数用于将各个vector中数据的地址赋值给各个创建信息中相应成员，并相应改变各个count
    void UpdateAllArrays() {
        createInfo.stageCount = uint32_t(shaderStages.size());
        vertexInputStateCi.vertexBindingDescriptionCount = uint32_t(vertexInputBindings.size());
        vertexInputStateCi.vertexAttributeDescriptionCount = uint32_t(vertexInputAttributes.size());
        viewportStateCi.viewportCount = viewports.size() ? uint32_t(viewports.size()) : dynamicViewportCount;
        viewportStateCi.scissorCount = scissors.size() ? uint32_t(scissors.size()) : dynamicScissorCount;
        colorBlendStateCi.attachmentCount = uint32_t(colorBlendAttachmentStates.size());
        dynamicStateCi.dynamicStateCount = uint32_t(dynamicStates.size());
        UpdateAllArrayAddresses();
    }
private:
    //该函数用于将创建信息的地址赋值给basePipelineIndex中相应成员
    void SetCreateInfos() {
        createInfo.pVertexInputState = &vertexInputStateCi;
        createInfo.pInputAssemblyState = &inputAssemblyStateCi;
        createInfo.pTessellationState = &tessellationStateCi;
        createInfo.pViewportState = &viewportStateCi;
        createInfo.pRasterizationState = &rasterizationStateCi;
        createInfo.pMultisampleState = &multisampleStateCi;
        createInfo.pDepthStencilState = &depthStencilStateCi;
        createInfo.pColorBlendState = &colorBlendStateCi;
        createInfo.pDynamicState = &dynamicStateCi;
    }
    //该函数用于将各个vector中数据的地址赋值给各个创建信息中相应成员，但不改变各个count
    void UpdateAllArrayAddresses() {
        createInfo.pStages = shaderStages.data();
        vertexInputStateCi.pVertexBindingDescriptions = vertexInputBindings.data();
        vertexInputStateCi.pVertexAttributeDescriptions = vertexInputAttributes.data();
        viewportStateCi.pViewports = viewports.data();
        viewportStateCi.pScissors = scissors.data();
        colorBlendStateCi.pAttachments = colorBlendAttachmentStates.data();
        dynamicStateCi.pDynamicStates = dynamicStates.data();
    }
};



#endif // VKBASE__H
