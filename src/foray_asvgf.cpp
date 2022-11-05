#include "foray_asvgf.hpp"
#include <spdlog/fmt/fmt.h>

namespace foray::asvgf {
    void ASvgfDenoiserStage::Init(core::Context* context, const stages::DenoiserConfig& config)
    {
        Destroy();
        mContext       = context;
        mInputs.Albedo = config.GBufferOutputs[(size_t)stages::GBufferStage::EOutput::Albedo];
        Assert(!!mInputs.Albedo);
        mInputs.Normal = config.GBufferOutputs[(size_t)stages::GBufferStage::EOutput::Normal];
        Assert(!!mInputs.Normal);
        mInputs.Motion = config.GBufferOutputs[(size_t)stages::GBufferStage::EOutput::Motion];
        Assert(!!mInputs.Motion);
        mInputs.LinearZ = config.GBufferOutputs[(size_t)stages::GBufferStage::EOutput::LinearZ];
        Assert(!!mInputs.LinearZ);
        mInputs.MeshInstanceIdx = config.GBufferOutputs[(size_t)stages::GBufferStage::EOutput::MeshInstanceIdx];
        Assert(!!mInputs.MeshInstanceIdx);
        mInputs.PrimaryInput = config.PrimaryInput;
        Assert(!!mInputs.PrimaryInput);
        mPrimaryOutput = config.PrimaryOutput;
        Assert(!!mPrimaryOutput);

        // Noise Source Inputs
        //
        mInputs.NoiseTexture = config.AuxiliaryInputs.at(std::string("Noise Source"));

        VkExtent2D strataCount = CalculateStrataCount(mContext->GetSwapchainSize());

        VkImageUsageFlags usageFlags = VkImageUsageFlagBits::VK_IMAGE_USAGE_STORAGE_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT;
        {  // Create ASvgf Stage owned Images

            core::ManagedImage::CreateInfo ci(usageFlags, VkFormat::VK_FORMAT_R32G32_SFLOAT, strataCount, "ASvgf.LuminanceMaxDiff");
            mASvgfImages.LuminanceMaxDiff.Create(mContext, ci);
        }
        {
            core::ManagedImage::CreateInfo ci(usageFlags, VkFormat::VK_FORMAT_R32G32B32A32_SFLOAT, strataCount, "ASvgf.MomentsAndLinearZ");
            mASvgfImages.MomentsAndLinearZ.Create(mContext, ci);
        }

        {
            core::ManagedImage::CreateInfo ci(usageFlags, VkFormat::VK_FORMAT_R16_UINT, strataCount, "ASvgf.Seed");
            mASvgfImages.Seed.Create(mContext, ci);
        }

        {  // Create History Images
            std::vector<core::ManagedImage*> historyImages{&mHistoryImages.LinearZ, &mHistoryImages.MeshInstanceIdx, &mHistoryImages.Normal, &mHistoryImages.PrimaryInput};
            std::vector<core::ManagedImage*> srcImages{mInputs.LinearZ, mInputs.MeshInstanceIdx, mInputs.Normal, mInputs.PrimaryInput};

            core::ManagedImage::CreateInfo ci;

            for(int32_t i = 0; i < historyImages.size(); i++)
            {
                core::ManagedImage* historyImage = historyImages[i];
                core::ManagedImage* srcImage     = srcImages[i];

                ci      = srcImage->GetCreateInfo();
                ci.Name = fmt::format("History.{}", ci.Name);
                historyImage->Create(mContext, ci);
            }
        }

        mCreateGradientSamplesStage.Init(this);
    }

    void ASvgfDenoiserStage::RecordFrame(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo)
    {
        mCreateGradientSamplesStage.RecordFrame(cmdBuffer, renderInfo);

        CopyToHistory(cmdBuffer, renderInfo);
    }

    void ASvgfDenoiserStage::CopyToHistory(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo)
    {
        std::vector<core::ManagedImage*> historyImages{&mHistoryImages.LinearZ, &mHistoryImages.MeshInstanceIdx, &mHistoryImages.Normal, &mHistoryImages.PrimaryInput};
        std::vector<core::ManagedImage*> srcImages{mInputs.LinearZ, mInputs.MeshInstanceIdx, mInputs.Normal, mInputs.PrimaryInput};

        {  // Image memory barriers
            std::vector<VkImageMemoryBarrier2> vkbarriers;

            for(core::ManagedImage* image : historyImages)
            {
                core::ImageLayoutCache::Barrier2 barrier{
                    .SrcStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                    .SrcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT,
                    .DstStageMask  = VK_PIPELINE_STAGE_2_COPY_BIT,
                    .DstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    .NewLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                };

                vkbarriers.push_back(renderInfo.GetImageLayoutCache().Set(image, barrier));
            }
            for(core::ManagedImage* image : srcImages)
            {
                core::ImageLayoutCache::Barrier2 barrier{
                    .SrcStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                    .SrcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
                    .DstStageMask  = VK_PIPELINE_STAGE_2_COPY_BIT,
                    .DstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
                    .NewLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                };

                vkbarriers.push_back(renderInfo.GetImageLayoutCache().Set(image, barrier));
            }

            VkDependencyInfo depInfo{
                .sType = VkStructureType::VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = (uint32_t)vkbarriers.size(), .pImageMemoryBarriers = vkbarriers.data()};

            vkCmdPipelineBarrier2(cmdBuffer, &depInfo);
        }

        for(int32_t i = 0; i < historyImages.size(); i++)
        {
            core::ManagedImage* historyImage = historyImages[i];
            core::ManagedImage* srcImage     = srcImages[i];

            VkImageSubresourceLayers layer{.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1};

            VkImageCopy2 imageblit2{.sType          = VkStructureType::VK_STRUCTURE_TYPE_IMAGE_COPY_2,
                                    .srcSubresource = layer,
                                    .srcOffset      = VkOffset3D{0, 0, 0},
                                    .dstSubresource = layer,
                                    .dstOffset      = VkOffset3D{0, 0, 0},
                                    .extent         = srcImage->GetExtent3D()};

            VkCopyImageInfo2 copyInfo{
                .sType          = VkStructureType::VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2,
                .srcImage       = srcImage->GetImage(),
                .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .dstImage       = historyImage->GetImage(),
                .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .regionCount    = 1u,
                .pRegions       = &imageblit2,
            };
            vkCmdCopyImage2(cmdBuffer, &copyInfo);
        }
    }

    void ASvgfDenoiserStage::DisplayImguiConfiguration() {}

    void ASvgfDenoiserStage::IgnoreHistoryNextFrame() {}

    VkExtent2D ASvgfDenoiserStage::CalculateStrataCount(const VkExtent2D& extent)
    {
        uint32_t   strataSize = 3;
        return VkExtent2D{.width = (extent.width + strataSize - 1) / strataSize, .height = (extent.height + strataSize - 1) / strataSize};
    }

    void ASvgfDenoiserStage::OnResized(const VkExtent2D& extent)
    {
        VkExtent2D strataCount = CalculateStrataCount(extent);

        std::vector<core::ManagedImage*> strataImages({&(mASvgfImages.LuminanceMaxDiff), &(mASvgfImages.MomentsAndLinearZ), &(mASvgfImages.Seed)});
        std::vector<core::ManagedImage*> fullSizeImages({&(mHistoryImages.PrimaryInput),
                                                 &(mHistoryImages.LinearZ), &(mHistoryImages.MeshInstanceIdx), &(mHistoryImages.Normal)});
        for(core::ManagedImage* image : strataImages)
        {
            if(image->Exists())
            {
                image->Resize(strataCount);
            }
        }

        for(core::ManagedImage* image : fullSizeImages)
        {
            if(image->Exists())
            {
                image->Resize(extent);
            }
        }

        mCreateGradientSamplesStage.UpdateDescriptorSet();
    }

    void ASvgfDenoiserStage::Destroy()
    {
        std::vector<core::ManagedImage*> images({&(mASvgfImages.LuminanceMaxDiff), &(mASvgfImages.MomentsAndLinearZ), &(mASvgfImages.Seed), &(mHistoryImages.PrimaryInput),
                                                 &(mHistoryImages.LinearZ), &(mHistoryImages.MeshInstanceIdx), &(mHistoryImages.Normal)});
        for(core::ManagedImage* image : images)
        {
            image->Destroy();
        }

        std::vector<stages::RenderStage*> stages({&mCreateGradientSamplesStage});

        for(stages::RenderStage* stage : stages)
        {
            stage->Destroy();
        }
    }

}  // namespace foray::asvgf
