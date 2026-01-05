#include "Samples2DCompute.h"

using namespace vulkan;

Samples2DCmp::Samples2DCmp()
{
    //创建描述符布局
    VkDescriptorSetLayoutBinding descriptorSetLayoutBinding_imageArray[2] ={};
    descriptorSetLayoutBinding_imageArray[0].binding = 0;
    descriptorSetLayoutBinding_imageArray[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descriptorSetLayoutBinding_imageArray[0].descriptorCount = 1;
    descriptorSetLayoutBinding_imageArray[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    descriptorSetLayoutBinding_imageArray[1].binding = 1;
    descriptorSetLayoutBinding_imageArray[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descriptorSetLayoutBinding_imageArray[1].descriptorCount = 1;
    descriptorSetLayoutBinding_imageArray[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
    descriptorSetLayoutCreateInfo.bindingCount = sizeof(descriptorSetLayoutBinding_imageArray)/sizeof(VkDescriptorSetLayoutBinding);
    descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBinding_imageArray;
    descriptorSetLayout_compute.Create(descriptorSetLayoutCreateInfo);

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayout_compute.Address();
    pipelineLayout_compute.Create(pipelineLayoutCreateInfo);

    struct ShaderStruct
    {
        VkShaderStageFlagBits stage;
        QString               shader;
        QString               output;
    };

    QList<ShaderStruct> shader_struct_list{
        ShaderStruct
        {   VK_SHADER_STAGE_COMPUTE_BIT,
            QString("%1/shader/Samples2D.comp").arg(CODE_DIR),
            QString("%1/Samples2D.comp.spv").arg(APP_PATH)
        },
    };

    static std::vector<shaderModule> shaderModules;
    static std::vector<VkPipelineShaderStageCreateInfo> shaderStageCreateInfos_compute;
    for(const ShaderStruct& item : shader_struct_list){
        compileShader(VK_GLSLC, item.shader, item.output);

        // 创建 shader module（注意：不能是 static）
        shaderModules.emplace_back(item.output.toStdString().c_str());

        shaderStageCreateInfos_compute.push_back(shaderModules.back().StageCreateInfo(item.stage));
    }

    //创建管线
    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = pipelineLayout_compute;
    pipelineInfo.stage  = shaderStageCreateInfos_compute[0];
    pipelineInfo.basePipelineHandle = (VkPipeline)VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex  = -1;
    pipeline_compute.Create(pipelineInfo);


    //创建描述符池
    VkDescriptorPoolSize descriptorPoolSizes[] =
    {
        VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2 },
    };
    descriptor_pool = std::unique_ptr<descriptorPool>(new descriptorPool(1, descriptorPoolSizes));

    //分配描述符集
    descriptor_pool->AllocateSets(descriptorSet_compute,descriptorSetLayout_compute);
}

Samples2DCmp::~Samples2DCmp()
{

}

void Samples2DCmp::initResource(const QString &ImagePath)
{
    /*加载图像*/
    VkFormat imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
    std::unique_ptr<uint8_t[]> pImageData = texture::LoadFile(ImagePath.toLocal8Bit().data(),imageExtent,FormatInfo(imageFormat));
    if(!pImageData){
        qDebug()<<QString("%1 load failed!").arg(ImagePath);
        return;
    }

    //将图像数据存入暂存缓冲区
    stagingBuffer::BufferData_MainThread(pImageData.get(), FormatInfo(imageFormat).sizePerPixel * imageExtent.width * imageExtent.height);

    //创建能被着色器读取的Image
    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;//2D图像
    imageCreateInfo.format = imageFormat;
    imageCreateInfo.extent = VkExtent3D{imageExtent.width,imageExtent.height,1};
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT; //按1个字节采样
    imageCreateInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT  | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    image_memory_src.Create(imageCreateInfo,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);//内存已经在VKBuffer里了，VkBuffer属于GPU内存 -- 所以使用local
    //创建源图像视图 -- imageView并不关心image是否有数据，只是定义了image的访问方式
    image_view_src.Create(image_memory_src.Image(),VK_IMAGE_VIEW_TYPE_2D,VK_FORMAT_R8G8B8A8_UNORM,VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });


    //创建目标图像,用于存储计算着色器输出的结果
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;//2D图像
    imageCreateInfo.format = imageFormat;
    imageCreateInfo.extent = VkExtent3D{imageExtent.width,imageExtent.height,1};
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT; //按1个字节采样
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageCreateInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    image_memory_dst.Create(imageCreateInfo,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);//内存已经在VKBuffer里了，VkBuffer属于GPU内存 -- 所以使用local

    //创建目标图像视图 -- imageView并不关心image是否有数据，只是定义了image的访问方式
    image_view_dst.Create(image_memory_dst.Image(),VK_IMAGE_VIEW_TYPE_2D,VK_FORMAT_R8G8B8A8_UNORM,VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

    //将输入输出图像写入描述符，给计算着色器使用
    VkDescriptorImageInfo srcImageDescInfo = {};
    srcImageDescInfo.imageView = image_view_src;
    srcImageDescInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    descriptorSet_compute.write(srcImageDescInfo,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,0);

    VkDescriptorImageInfo dstImageDescInfo = {};
    dstImageDescInfo.imageView = image_view_dst;
    dstImageDescInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    descriptorSet_compute.write(dstImageDescInfo,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1);
}

void Samples2DCmp::runDispatch(const commandBuffer& command_buffer)
{
    VkBufferImageCopy region_copy{};
    region_copy.bufferOffset = 0;region_copy.bufferRowLength = 0;region_copy.bufferImageHeight = 0;//buffer从起始位置紧密排列
    region_copy.imageOffset = VkOffset3D{};//图像数据被拷入交换链图像的起始位置,所以也是0
    region_copy.imageSubresource = VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
    region_copy.imageExtent = VkExtent3D{imageExtent.width,imageExtent.height,1};

    //使用图像内存屏障将buffer拷贝到image
    imageOperation::CmdCopyBufferToImage(command_buffer,
                                         stagingBuffer::Buffer_MainThread(),
                                         image_memory_src.Image(),
                                         region_copy,
                                         imageOperation::imageMemoryBarrierParameterPack(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, //过去管线起点开始
                                                                                         0,
                                                                                         VK_IMAGE_LAYOUT_UNDEFINED),
                                         imageOperation::imageMemoryBarrierParameterPack(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,//将来传输命令
                                                                                         0,
                                                                                         VK_IMAGE_LAYOUT_GENERAL));

    //增加图像内存屏障 -- 确保image构造完成
    VkImageMemoryBarrier imageMemoryBarrier = {};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.pNext = nullptr;
    imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;//过去需要等待着色器写入完成
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;//将来图像需要给到传输通道读取
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;//没有队列族转移
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;//没有队列族转移
    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;//创建出来就是这个layout
    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;//VkImage是作为源输入给到计算着色器，必须为VK_IMAGE_LAYOUT_GENERAL
    imageMemoryBarrier.image = image_memory_dst.Image();//指定哪一张图像需要内存屏障
    imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;//RGBA纹理通道
    imageMemoryBarrier.subresourceRange.baseMipLevel = 0;//默认层级
    imageMemoryBarrier.subresourceRange.levelCount = 1;
    imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
    imageMemoryBarrier.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(command_buffer,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&imageMemoryBarrier);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_compute);
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout_compute, 0, 1, descriptorSet_compute.Address(), 0, nullptr);
    vkCmdDispatch(command_buffer, (imageExtent.width + 15) / 16, (imageExtent.height + 15) / 16, 1);

    //得到VkImage之后,开始blit操作 region描述的内容可以实现缩放、翻转
    VkExtent2D swapchainImageSize =  graphicsBase::Base().SwapchainCreateInfo().imageExtent;
    VkImageBlit region_blit = {
        { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        { {}, { int32_t(imageExtent.width), int32_t(imageExtent.height), 1 } },
        { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        { {}, { int32_t(swapchainImageSize.width), int32_t(swapchainImageSize.height), 1 } }
    };
    imageOperation::CmdBlitImage(command_buffer,
                                 image_memory_dst.Image(), //图像layout:VK_IMAGE_LAYOUT_GENERAL
                                 graphicsBase::Base().SwapchainImage(graphicsBase::Base().CurrentImageIndex()),//图像layout:VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
                                 region_blit,
                                 imageOperation::imageMemoryBarrierParameterPack(VK_PIPELINE_STAGE_TRANSFER_BIT,
                                                                                 0,
                                                                                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
                                 imageOperation::imageMemoryBarrierParameterPack(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                                                                 0,
                                                                                 VK_IMAGE_LAYOUT_PRESENT_SRC_KHR));
}
