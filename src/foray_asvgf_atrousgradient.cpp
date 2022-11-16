#include "foray_asvgf_atrousgradient.hpp"
#include "foray_asvgf.hpp"

namespace foray::asvgf {
    void ATrousGradientStage::Init(ASvgfDenoiserStage* aSvgfStage)
    {
        Destroy();
        mASvgfStage = aSvgfStage;
        stages::ComputeStage::Init(mASvgfStage->mContext);
    }

    void ATrousGradientStage::ApiInitShader()
    {
        mShader.LoadFromSource(mContext, SHADER_DIR "/atrousgradient.comp");
        mShaderSourcePaths.push_back(SHADER_DIR "/atrousgradient.comp");
    }
    void ATrousGradientStage::ApiCreateDescriptorSet()
    {
        UpdateDescriptorSet();
    }
    void ATrousGradientStage::UpdateDescriptorSet()
    {
        mDescriptorSet.SetDescriptorAt(0, mASvgfStage->mASvgfImages.LuminanceMaxDiff, VkImageLayout::VK_IMAGE_LAYOUT_GENERAL, nullptr,
                                       VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
        mDescriptorSet.SetDescriptorAt(1, mASvgfStage->mASvgfImages.MomentsAndLinearZ, VkImageLayout::VK_IMAGE_LAYOUT_GENERAL, nullptr,
                                       VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);

        mDescriptorSet.SetDescriptorAt(2, mASvgfStage->mPrimaryOutput, VkImageLayout::VK_IMAGE_LAYOUT_GENERAL, nullptr, VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                       VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);

        if(mDescriptorSet.Exists())
        {
            mDescriptorSet.Update();
        }
        else
        {
            mDescriptorSet.Create(mContext, "ASvgf.AtrousGradient");
        }
    }

    void ATrousGradientStage::ApiCreatePipelineLayout()
    {
        mPipelineLayout.AddDescriptorSetLayout(mDescriptorSet.GetDescriptorSetLayout());
        mPipelineLayout.AddPushConstantRange<PushConstant>(VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
        mPipelineLayout.Build(mContext);
    }

    void ATrousGradientStage::RecordFrame(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo) 
    {
        mContext->VkbDispatchTable->cmdBindPipeline(cmdBuffer, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE, mPipeline);

        VkDescriptorSet descriptorSet = mDescriptorSet.GetDescriptorSet();

        mContext->VkbDispatchTable->cmdBindDescriptorSets(cmdBuffer, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE, mPipelineLayout, 0U, 1U, &descriptorSet, 0U, nullptr);

        // Reset PushC IterationIdx


        for (uint32_t i = 0; i < mIterationCount; i++)
        {
            mPushC.IterationIdx = i;
            BeforeIteration(cmdBuffer, renderInfo);
            DispatchIteration(cmdBuffer, renderInfo);
        }
    }
    void ATrousGradientStage::BeforeIteration(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo)
    {
        mPushC.ReadIdx = mPushC.IterationIdx % 2;
        mPushC.WriteIdx = (mPushC.IterationIdx + 1) % 2;

        std::vector<VkImageMemoryBarrier2> vkBarriers;

        {  // Output (Debug) image
            core::ImageLayoutCache::Barrier2 barrier{.SrcStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                                                     .SrcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                                                     .DstStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                                     .DstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
                                                     .NewLayout     = VkImageLayout::VK_IMAGE_LAYOUT_GENERAL};
            vkBarriers.push_back(renderInfo.GetImageLayoutCache().MakeBarrier(mASvgfStage->mPrimaryOutput, barrier));
        }

        // Write (+Read) Images
        std::vector<core::ManagedImage*> images{&(mASvgfStage->mASvgfImages.LuminanceMaxDiff), &(mASvgfStage->mASvgfImages.MomentsAndLinearZ)};

        for(core::ManagedImage* image : images)
        {
            // Write access
            core::ImageLayoutCache::Barrier2 writeBarrier{
                .SrcStageMask     = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .SrcAccessMask    = VK_ACCESS_2_MEMORY_READ_BIT,
                .DstStageMask     = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .DstAccessMask    = VK_ACCESS_2_SHADER_WRITE_BIT,
                .NewLayout        = VkImageLayout::VK_IMAGE_LAYOUT_GENERAL,
                .SubresourceRange = VkImageSubresourceRange{.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1U, .baseArrayLayer = mPushC.WriteIdx, .layerCount = 1U}};
            vkBarriers.push_back(renderInfo.GetImageLayoutCache().MakeBarrier(image, writeBarrier));
            // Read access
            core::ImageLayoutCache::Barrier2 readBarrier{
                .SrcStageMask     = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .SrcAccessMask    = VK_ACCESS_2_MEMORY_WRITE_BIT,
                .DstStageMask     = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .DstAccessMask    = VK_ACCESS_2_SHADER_READ_BIT,
                .NewLayout        = VkImageLayout::VK_IMAGE_LAYOUT_GENERAL,
                .SubresourceRange = VkImageSubresourceRange{.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1U, .baseArrayLayer = mPushC.ReadIdx, .layerCount = 1U}};
            vkBarriers.push_back(renderInfo.GetImageLayoutCache().MakeBarrier(image, readBarrier));
        }

        VkDependencyInfo depInfo{
            .sType = VkStructureType::VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = (uint32_t)vkBarriers.size(), .pImageMemoryBarriers = vkBarriers.data()};

        vkCmdPipelineBarrier2(cmdBuffer, &depInfo);
    }

    void ATrousGradientStage::DispatchIteration(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo)
    {
        if (mPushC.IterationIdx + 1 == mIterationCount)
        {
            mPushC.DebugMode = mASvgfStage->mDebugMode;
            mASvgfStage->mASvgfImages.LastArrayWriteIdx = mPushC.WriteIdx;
        }
        else
        {
            mPushC.DebugMode = DEBUG_NONE;
        }
        vkCmdPushConstants(cmdBuffer, mPipelineLayout, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(mPushC), &mPushC);

        glm::uvec3 groupSize;
        { // Calculate Group Size
            VkExtent3D size = mASvgfStage->mASvgfImages.LuminanceMaxDiff.GetExtent3D();

            glm::uvec2 localSize(16, 16);
            glm::uvec2 strataFrameSize(size.width, size.height);

            groupSize = glm::uvec3((strataFrameSize.x + localSize.x - 1) / localSize.x, (strataFrameSize.y + localSize.y - 1) / localSize.y, 1);
        }

        mContext->VkbDispatchTable->cmdDispatch(cmdBuffer, groupSize.x, groupSize.y, groupSize.z);
    }
}  // namespace foray::asvgf
