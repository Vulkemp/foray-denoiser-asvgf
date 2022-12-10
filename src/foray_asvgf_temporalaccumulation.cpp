#include "foray_asvgf_temporalaccumulation.hpp"
#include "foray_asvgf.hpp"

namespace foray::asvgf {
    void TemporalAccumulationStage::Init(ASvgfDenoiserStage* aSvgfStage)
    {
        Destroy();
        mASvgfStage = aSvgfStage;
        stages::ComputeStage::Init(mASvgfStage->mContext);
    }

    void TemporalAccumulationStage::ApiInitShader()
    {
        mShaderKeys.push_back(mShader.CompileFromSource(mContext, ASVGF_SHADER_DIR "/temporalaccumulation.comp"));
    }
    void TemporalAccumulationStage::ApiCreateDescriptorSet()
    {
        UpdateDescriptorSet();
    }
    void TemporalAccumulationStage::UpdateDescriptorSet()
    {
        std::vector<core::ManagedImage*> images{mASvgfStage->mInputs.PrimaryInput,
                                                mASvgfStage->mInputs.Motion,
                                                mASvgfStage->mInputs.Positions,
                                                &mASvgfStage->mHistoryImages.Positions.GetHistoryImage(),
                                                mASvgfStage->mInputs.Normal,
                                                &mASvgfStage->mHistoryImages.Normal.GetHistoryImage(),
                                                mASvgfStage->mInputs.MeshInstanceIdx,
                                                &mASvgfStage->mHistoryImages.MeshInstanceIdx.GetHistoryImage(),
                                                &mASvgfStage->mAccumulatedImages.Color,
                                                &mASvgfStage->mAccumulatedImages.Moments,
                                                &mASvgfStage->mAccumulatedImages.HistoryLength,
                                                &mASvgfStage->mASvgfImages.LuminanceMaxDiff,
                                                mASvgfStage->mPrimaryOutput};

        for(size_t i = 0; i < images.size(); i++)
        {
            mDescriptorSet.SetDescriptorAt(i, images[i], VkImageLayout::VK_IMAGE_LAYOUT_GENERAL, nullptr, VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                           VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
        }

        if(mDescriptorSet.Exists())
        {
            mDescriptorSet.Update();
        }
        else
        {
            mDescriptorSet.Create(mContext, "ASvgf.TemporalAccumulation");
        }
    }

    void TemporalAccumulationStage::ApiCreatePipelineLayout()
    {
        mPipelineLayout.AddDescriptorSetLayout(mDescriptorSet.GetDescriptorSetLayout());
        mPipelineLayout.AddPushConstantRange<PushConstant>(VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
        mPipelineLayout.Build(mContext);
    }

    void TemporalAccumulationStage::ApiBeforeFrame(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo)
    {
        std::vector<VkImageMemoryBarrier2> vkBarriers;

        {  // Read Only Images
            std::vector<core::ManagedImage*> readOnlyImages({mASvgfStage->mInputs.PrimaryInput, mASvgfStage->mInputs.Motion, mASvgfStage->mInputs.Positions,
                                                             &mASvgfStage->mHistoryImages.Positions.GetHistoryImage(), mASvgfStage->mInputs.Normal, mASvgfStage->mInputs.Normal,
                                                             &mASvgfStage->mHistoryImages.Normal.GetHistoryImage(), mASvgfStage->mInputs.MeshInstanceIdx,
                                                             &mASvgfStage->mHistoryImages.MeshInstanceIdx.GetHistoryImage()});

            for(core::ManagedImage* image : readOnlyImages)
            {
                core::ImageLayoutCache::Barrier2 barrier{
                    .SrcStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                    .SrcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
                    .DstStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .DstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
                    .NewLayout     = VkImageLayout::VK_IMAGE_LAYOUT_GENERAL,
                };
                vkBarriers.push_back(renderInfo.GetImageLayoutCache().MakeBarrier(image, barrier));
            }
        }
        {  // Output (Debug) image
            core::ImageLayoutCache::Barrier2 barrier{.SrcStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                                                     .SrcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                                                     .DstStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                                     .DstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
                                                     .NewLayout     = VkImageLayout::VK_IMAGE_LAYOUT_GENERAL};
            vkBarriers.push_back(renderInfo.GetImageLayoutCache().MakeBarrier(mASvgfStage->mPrimaryOutput, barrier));
        }
        {  // Luminance MaxDiff texture
            core::ImageLayoutCache::Barrier2 barrier{
                .SrcStageMask     = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .SrcAccessMask    = VK_ACCESS_2_MEMORY_WRITE_BIT,
                .DstStageMask     = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .DstAccessMask    = VK_ACCESS_2_SHADER_READ_BIT,
                .NewLayout        = VkImageLayout::VK_IMAGE_LAYOUT_GENERAL,
                .SubresourceRange = VkImageSubresourceRange{.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1U, .layerCount = 2U}};
            vkBarriers.push_back(renderInfo.GetImageLayoutCache().MakeBarrier(&mASvgfStage->mASvgfImages.LuminanceMaxDiff, barrier));
        }
        {  // Write (+Read) Images
            std::vector<core::ManagedImage*> images{&(mASvgfStage->mAccumulatedImages.Color), &(mASvgfStage->mAccumulatedImages.Moments),
                                                    &(mASvgfStage->mAccumulatedImages.HistoryLength)};

            for(core::ManagedImage* image : images)
            {
                core::ImageLayoutCache::Barrier2 barrier{
                    .SrcStageMask     = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                    .SrcAccessMask    = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                    .DstStageMask     = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .DstAccessMask    = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT,
                    .NewLayout        = VkImageLayout::VK_IMAGE_LAYOUT_GENERAL,
                    .SubresourceRange = VkImageSubresourceRange{.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1U, .layerCount = 2U}};
                vkBarriers.push_back(renderInfo.GetImageLayoutCache().MakeBarrier(image, barrier));
            }
        }

        VkDependencyInfo depInfo{
            .sType = VkStructureType::VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = (uint32_t)vkBarriers.size(), .pImageMemoryBarriers = vkBarriers.data()};

        vkCmdPipelineBarrier2(cmdBuffer, &depInfo);
    }

    void TemporalAccumulationStage::ApiBeforeDispatch(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo, glm::uvec3& groupSize)
    {
        mPushC.ReadIdx                                     = renderInfo.GetFrameNumber() % 2;
        mPushC.WriteIdx                                    = (renderInfo.GetFrameNumber() + 1) % 2;
        mPushC.LuminanceMaxDiffReadIdx                     = mASvgfStage->mASvgfImages.LastArrayWriteIdx;
        mPushC.EnableHistory                               = mASvgfStage->mHistoryImages.Valid;
        mPushC.DebugOutputMode                             = mASvgfStage->mDebugMode;
        mASvgfStage->mAccumulatedImages.LastArrayWriteIdx = mPushC.WriteIdx;
        vkCmdPushConstants(cmdBuffer, mPipelineLayout, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(mPushC), &mPushC);

        VkExtent3D size = mASvgfStage->mInputs.PrimaryInput->GetExtent3D();

        glm::uvec2 localSize(16, 16);
        glm::uvec2 FrameSize(size.width, size.height);

        groupSize = glm::uvec3((FrameSize.x + localSize.x - 1) / localSize.x, (FrameSize.y + localSize.y - 1) / localSize.y, 1);
    }
}  // namespace foray::asvgf
