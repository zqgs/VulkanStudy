#include "EasyVulkan.h"

namespace easyVulkan {
    using namespace vulkan;

    const renderPassWithFramebuffers& CreateRpwf_Screen()
    {
        static renderPassWithFramebuffers rpwf;

        VkAttachmentDescription attachmentDescription = {};
        attachmentDescription.format = graphicsBase::Base().SwapchainCreateInfo().imageFormat;
        attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
        attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference attachmentReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
        VkSubpassDescription subpassDescription = {};
        subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpassDescription.colorAttachmentCount = 1;
        subpassDescription.pColorAttachments = &attachmentReference;

        VkSubpassDependency subpassDependency = {};
        subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        subpassDependency.dstSubpass = 0;
        subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; //不早于提交命令缓冲区时等待semaphore对应的waitDstStageMask
        subpassDependency.srcAccessMask = 0;
        subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        subpassDependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        VkRenderPassCreateInfo renderPassCreateInfo = {};
        renderPassCreateInfo.attachmentCount = 1;
        renderPassCreateInfo.pAttachments = &attachmentDescription;
        renderPassCreateInfo.subpassCount = 1;
        renderPassCreateInfo.pSubpasses = &subpassDescription;
        renderPassCreateInfo.dependencyCount = 1;
        renderPassCreateInfo.pDependencies = &subpassDependency;
        rpwf.renderPass.Create(renderPassCreateInfo);

        //创建一组最简单的帧缓冲
        auto CreateFramebuffers = [&] {
            rpwf.framebuffers.resize(graphicsBase::Base().SwapchainImageCount());
            VkFramebufferCreateInfo framebufferCreateInfo = {};
            framebufferCreateInfo.renderPass = rpwf.renderPass;
            framebufferCreateInfo.attachmentCount = 1;
            framebufferCreateInfo.width = windowSize.width;
            framebufferCreateInfo.height = windowSize.height;
            framebufferCreateInfo.layers = 1;

            for (uint32_t i = 0; i < graphicsBase::Base().SwapchainImageCount(); i++) {
                VkImageView attachment = graphicsBase::Base().SwapchainImageView(i);
                framebufferCreateInfo.pAttachments = &attachment;
                rpwf.framebuffers[i].Create(framebufferCreateInfo);
            }
        };
        auto DestroyFramebuffers = [&] {
            rpwf.framebuffers.clear(); //清空vector中的元素时会逐一执行析构函数
        };
        CreateFramebuffers();

        ExecuteOnce(rpwf);
        graphicsBase::Base().AddCallback_CreateSwapchain(CreateFramebuffers);
        graphicsBase::Base().AddCallback_DestroySwapchain(DestroyFramebuffers);
        return rpwf;
    }

    void BootScreen(const char *imagePath, VkFormat imageFormat)
    {
        /*加载图像*/
        VkExtent2D imageExtent;
        std::unique_ptr<uint8_t[]> pImageData = texture::LoadFile(imagePath,imageExtent,FormatInfo(imageFormat));
        if(!pImageData){
            qDebug()<<QString("%1 load failed!").arg(imagePath);
            return;
        }
        qDebug()<<QString("%1 load success!").arg(imagePath);
        //将图像数据存入暂存缓冲区
        stagingBuffer::BufferData_MainThread(pImageData.get(), FormatInfo(imageFormat).sizePerPixel * imageExtent.width * imageExtent.height);


        /*创建渲染循环*/
        //1. 交换链获取GPU可用图像CMD，需知道CMD执行是否完成需要指定信号
        semaphore semaphore_available; //需要知道交换链是否给GPU分配了可以使用图像
        //2. 提交从交换链获取GPU可用图像CMD
        graphicsBase::Base().SwapImage(semaphore_available);

        /*创建录制命令 -- 创建录制命令，需要有命令池，创建命令池需要缓冲区*/
        //1.开始创建命令缓冲区(作用:渲染，从Graphics创建命令缓冲池)
        commandBuffer command_buffer;
        graphicsBase::Plus().CommandPool_Graphics().AllocateBuffers(command_buffer);

        //2.录制命令(预留)
        command_buffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        //2.1 启动传输命令前需要确认图像是否与交换链图像参数一致
        VkExtent2D swapchainImageSize =  graphicsBase::Base().SwapchainCreateInfo().imageExtent;
        bool blit = imageExtent.width != swapchainImageSize.width ||
                    imageExtent.height != swapchainImageSize.height ||
                    imageFormat != graphicsBase::Base().SwapchainCreateInfo().imageFormat;
        imageMemory image_memory;
        if(blit){//加载图像参数与交换链参数不一致，需要blit
            //尝试创建能与buffer混叠的暂存图像 -- 创建出来的图像layout是VK_IMAGE_LAYOUT_PREINITIALIZED -- 告诉GPU不可以在layout转换时丢弃数据。layout是UNDEFINED --- 可以丢弃
            VkImage image = stagingBuffer::AliasedImage2d_MainThread(imageFormat, imageExtent);
            if(image){
                //增加图像内存屏障 -- 确保image构造完成
                VkImageMemoryBarrier imageMemoryBarrier = {};
                imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                imageMemoryBarrier.pNext = nullptr;
                imageMemoryBarrier.srcAccessMask = 0;//过去没有操作 -- 因为是从TOP_PIPE阶段开始的
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;//将来图像需要给到transfer阶段做blit写入的
                imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;//没有队列族转移
                imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;//没有队列族转移
                imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;//创建出来就是这个layout
                imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;//VkImage是作为源输入给到blit，必须为VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
                imageMemoryBarrier.image = image;//指定哪一张图像需要内存屏障
                imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;//RGBA纹理通道
                imageMemoryBarrier.subresourceRange.baseMipLevel = 0;//默认层级
                imageMemoryBarrier.subresourceRange.levelCount = 1;
                imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
                imageMemoryBarrier.subresourceRange.layerCount = 1;
                vkCmdPipelineBarrier(command_buffer,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&imageMemoryBarrier);
            }
            else{
                //用buffer创建混叠图像失败后，手动创建一张图像
                VkImageCreateInfo imageCreateInfo = {};
                imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;//2D图像
                imageCreateInfo.format = imageFormat;
                imageCreateInfo.extent = VkExtent3D{imageExtent.width,imageExtent.height,1};
                imageCreateInfo.mipLevels = 1;
                imageCreateInfo.arrayLayers = 1;
                imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT; //按1个字节采样
                imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
                image_memory.Create(imageCreateInfo,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);//内存已经在VKBuffer里了，VkBuffer属于GPU内存 -- 所以使用local

                //image memory创建完成之后，需要创建vkImage -- 此时数据在VkBuffer内，新建的ImageMemory是空的，所以将buffer拷贝到image
                VkBufferImageCopy region_copy{};
                region_copy.bufferOffset = 0;region_copy.bufferRowLength = 0;region_copy.bufferImageHeight = 0;//buffer从起始位置紧密排列
                region_copy.imageOffset = VkOffset3D{};//图像数据被拷入交换链图像的起始位置,所以也是0
                region_copy.imageSubresource = VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
                region_copy.imageExtent = imageCreateInfo.extent;

                imageOperation::CmdCopyBufferToImage(command_buffer,
                                                     stagingBuffer::Buffer_MainThread(),
                                                     image_memory.Image(),
                                                     region_copy,
                                                     imageOperation::imageMemoryBarrierParameterPack(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, //过去管线起点开始
                                                                                                     0,
                                                                                                     VK_IMAGE_LAYOUT_UNDEFINED),
                                                     imageOperation::imageMemoryBarrierParameterPack(VK_PIPELINE_STAGE_TRANSFER_BIT,//将来传输命令
                                                                                                     0,
                                                                                                     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL));
                image = image_memory.Image();
            }
            //得到VkImage之后,开始blit操作 region描述的内容可以实现缩放、翻转
            VkImageBlit region_blit = {
                { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
                { {}, { int32_t(imageExtent.width), int32_t(imageExtent.height), 1 } },
                { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
                { {}, { int32_t(swapchainImageSize.width), int32_t(swapchainImageSize.height), 1 } }
            };
            imageOperation::CmdBlitImage(command_buffer,
                                         image, //图像layout:VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
                                         graphicsBase::Base().SwapchainImage(graphicsBase::Base().CurrentImageIndex()),//图像layout:VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
                                         region_blit,
                                         imageOperation::imageMemoryBarrierParameterPack(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                                                                         0,
                                                                                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
                                         imageOperation::imageMemoryBarrierParameterPack(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                                                                         0,
                                                                                         VK_IMAGE_LAYOUT_PRESENT_SRC_KHR));
        }
        else{//参数一致，直接copy
            VkBufferImageCopy region_copy{};
            region_copy.bufferOffset = 0;region_copy.bufferRowLength = 0;region_copy.bufferImageHeight = 0;//buffer从起始位置紧密排列
            region_copy.imageOffset = VkOffset3D{};//图像数据被拷入交换链图像的起始位置,所以也是0

            region_copy.imageSubresource = VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
            region_copy.imageExtent = VkExtent3D{imageExtent.width,imageExtent.height,1};
            imageOperation::CmdCopyBufferToImage(command_buffer,
                                                 stagingBuffer::Buffer_MainThread(),
                                                 graphicsBase::Base().SwapchainImage(graphicsBase::Base().CurrentImageIndex()),
                                                 region_copy,
                                                 imageOperation::imageMemoryBarrierParameterPack(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, //不需要等待任何已有的GPU操作(同步起点)
                                                                                                 0,
                                                                                                 VK_IMAGE_LAYOUT_UNDEFINED),
                                                 imageOperation::imageMemoryBarrierParameterPack(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,//等待之前所有的GPU操作结束(同步终点)
                                                                                                 0,
                                                                                                 VK_IMAGE_LAYOUT_PRESENT_SRC_KHR));
        }

        command_buffer.End();

        /*提交录制命令 -- 提交后有两种方式可以获得命令是否已经执行结束(GPU侧:信号,CPU侧:栅栏)*/
        //1. 使用CPU侧同步，创建栅栏(fence)
        fence fence_sync_flag;

        //2. 提交录制命令前必须确保GPU已经从GPU拿到可用的图像(需要等待semaphore_available信号<由GPU完成同步>)
        VkPipelineStageFlags waitDstStage = VK_PIPELINE_STAGE_TRANSFER_BIT; //拷贝和blit属于传输命令
        VkSubmitInfo submit_info = {};
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = semaphore_available.Address(); //指定执行命令前需要信号
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = command_buffer.Address();//指定执行的命令缓冲
        submit_info.pWaitDstStageMask = &waitDstStage;
        graphicsBase::Base().SubmitCommandBuffer_Graphics(submit_info,fence_sync_flag);//命令缓冲执行结束后输出状态

        /*提交呈现命令 -- 提交呈现命令前提是命令缓冲区已经执行结束(CPU测需要等待fence,GPU测需要等待渲染结束信号量<在submit输出可以指定>)*/
        //1. 等待命令缓冲执行结束
        fence_sync_flag.WaitAndReset();
        //2. 呈现图像并向交换链归还使用完毕的图像
        graphicsBase::Base().PresentImage();

        /*释放命令缓冲*/
        graphicsBase::Plus().CommandPool_Graphics().FreeBuffers(command_buffer);
    }

}
