#include "foray_asvgf_estimatevariance.hpp"
#include "foray_asvgf.hpp"

namespace foray::asvgf {
    void EstimateVarianceStage::Init(ASvgfDenoiserStage* aSvgfStage)
    {
        Destroy();
        mASvgfStage = aSvgfStage;
        stages::ComputeStage::Init(mASvgfStage->mContext);
    }

    void EstimateVarianceStage::ApiInitShader()
    {
        mShader.LoadFromSource(mContext, SHADER_DIR "/estimatevariance.comp");
        mShaderSourcePaths.push_back(SHADER_DIR "/estimatevariance.comp");
    }
    void EstimateVarianceStage::ApiCreateDescriptorSet()
    {
        UpdateDescriptorSet();
    }
    void EstimateVarianceStage::UpdateDescriptorSet()
    {
        std::vector<core::ManagedImage*> images{&mASvgfStage->mAccumulatedImages.Color,
                                                &mASvgfStage->mAccumulatedImages.Moments,
                                                &mASvgfStage->mAccumulatedImages.HistoryLength,
                                                mASvgfStage->mInputs.PrimaryInput,
                                                mASvgfStage->mInputs.LinearZ,
                                                mASvgfStage->mInputs.MeshInstanceIdx,
                                                mASvgfStage->mInputs.Normal,
                                                &mASvgfStage->mATrousImage,
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
            mDescriptorSet.Create(mContext, "ASvgf.EstimateVariance");
        }
    }

    void EstimateVarianceStage::ApiCreatePipelineLayout()
    {
        mPipelineLayout.AddDescriptorSetLayout(mDescriptorSet.GetDescriptorSetLayout());
        mPipelineLayout.AddPushConstantRange<PushConstant>(VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
        mPipelineLayout.Build(mContext);
    }

    void EstimateVarianceStage::ApiBeforeFrame(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo)
    {
        std::vector<VkImageMemoryBarrier2> vkBarriers;

        {  // Read Only Images
            std::vector<core::ManagedImage*> accuImages(
                {&mASvgfStage->mAccumulatedImages.Color, &mASvgfStage->mAccumulatedImages.Moments, &mASvgfStage->mAccumulatedImages.HistoryLength});
            std::vector<core::ManagedImage*> readOnlyImages(
                {mASvgfStage->mInputs.PrimaryInput, mASvgfStage->mInputs.LinearZ, mASvgfStage->mInputs.MeshInstanceIdx, mASvgfStage->mInputs.Normal});


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
            for(core::ManagedImage* image : accuImages)
            {
                core::ImageLayoutCache::Barrier2 barrier{
                    .SrcStageMask     = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                    .SrcAccessMask    = VK_ACCESS_2_MEMORY_WRITE_BIT,
                    .DstStageMask     = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .DstAccessMask    = VK_ACCESS_2_SHADER_READ_BIT,
                    .NewLayout        = VkImageLayout::VK_IMAGE_LAYOUT_GENERAL,
                    .SubresourceRange = VkImageSubresourceRange{.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1U, .layerCount = 2U}};
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

        {  // Write (+Read) Images
            std::vector<core::ManagedImage*> images{&(mASvgfStage->mATrousImage)};

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

            core::ImageLayoutCache::Barrier2 barrier{
                .SrcStageMask     = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .SrcAccessMask    = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                .DstStageMask     = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .DstAccessMask    = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT,
                .NewLayout        = VkImageLayout::VK_IMAGE_LAYOUT_GENERAL,};
            vkBarriers.push_back(renderInfo.GetImageLayoutCache().MakeBarrier(mASvgfStage->mPrimaryOutput, barrier));
        }

        VkDependencyInfo depInfo{
            .sType = VkStructureType::VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = (uint32_t)vkBarriers.size(), .pImageMemoryBarriers = vkBarriers.data()};

        vkCmdPipelineBarrier2(cmdBuffer, &depInfo);
    }

    void EstimateVarianceStage::ApiBeforeDispatch(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo, glm::uvec3& groupSize)
    {
        PushConstant pushC                    = PushConstant();
        pushC.ReadIdx                         = mASvgfStage->mAccumulatedImages.LastArrayWriteIdx;
        pushC.WriteIdx                        = 0;
        pushC.DebugMode                       = mASvgfStage->mDebugMode;
        mASvgfStage->mATrousLastArrayWriteIdx = pushC.WriteIdx;
        vkCmdPushConstants(cmdBuffer, mPipelineLayout, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushC), &pushC);

        VkExtent3D size = mASvgfStage->mInputs.PrimaryInput->GetExtent3D();

        glm::uvec2 localSize(16, 16);
        glm::uvec2 FrameSize(size.width, size.height);

        groupSize = glm::uvec3((FrameSize.x + localSize.x - 1) / localSize.x, (FrameSize.y + localSize.y - 1) / localSize.y, 1);
    }
}  // namespace foray::asvgf