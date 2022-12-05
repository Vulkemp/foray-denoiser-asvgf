#include "foray_asvgf_atrous.hpp"
#include "foray_asvgf.hpp"

namespace foray::asvgf {
    void ATrousStage::Init(ASvgfDenoiserStage* aSvgfStage)
    {
        Destroy();
        mASvgfStage = aSvgfStage;
        stages::ComputeStage::Init(mASvgfStage->mContext);
    }

    void ATrousStage::ApiInitShader()
    {
        mShaderKeys.push_back(mShader.CompileFromSource(mContext, ASVGF_SHADER_DIR "/atrous.comp"));
    }
    void ATrousStage::ApiCreateDescriptorSet()
    {
        UpdateDescriptorSet();
    }

    void ATrousStage::UpdateDescriptorSet()
    {
        std::vector<core::ManagedImage*> images{mASvgfStage->mInputs.LinearZ, mASvgfStage->mInputs.Normal, mASvgfStage->mInputs.Albedo, &mASvgfStage->mATrousImage,
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
            mDescriptorSet.Create(mContext, "ASvgf.ATrous");
        }
    }

    void ATrousStage::ApiCreatePipelineLayout()
    {
        mPipelineLayout.AddDescriptorSetLayout(mDescriptorSet.GetDescriptorSetLayout());
        mPipelineLayout.AddPushConstantRange<PushConstant>(VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
        mPipelineLayout.Build(mContext);
    }

    void ATrousStage::RecordFrame(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo)
    {
        mContext->VkbDispatchTable->cmdBindPipeline(cmdBuffer, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE, mPipeline);

        VkDescriptorSet descriptorSet = mDescriptorSet.GetDescriptorSet();

        mContext->VkbDispatchTable->cmdBindDescriptorSets(cmdBuffer, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE, mPipelineLayout, 0U, 1U, &descriptorSet, 0U, nullptr);

        // Reset PushC IterationIdx


        for(uint32_t i = 0; i < mIterationCount; i++)
        {
            mPushC.IterationIdx = i;
            if(i + 1 == mIterationCount)
            {
                mPushC.LastIteration = VK_TRUE;
                mPushC.DebugMode     = mASvgfStage->mDebugMode;
            }
            else
            {
                mPushC.LastIteration = VK_FALSE;
                mPushC.DebugMode     = DEBUG_NONE;
            }
            BeforeIteration(cmdBuffer, renderInfo);
            DispatchIteration(cmdBuffer, renderInfo);
        }
    }

    void ATrousStage::BeforeIteration(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo)
    {
        mPushC.ReadIdx  = (mPushC.IterationIdx + mASvgfStage->mATrousLastArrayWriteIdx) % 2;
        mPushC.WriteIdx = (mPushC.IterationIdx + mASvgfStage->mATrousLastArrayWriteIdx + 1) % 2;

        std::vector<VkImageMemoryBarrier2> vkBarriers;

        {  // Read Only Images
            std::vector<core::ManagedImage*> readOnlyImages({
                mASvgfStage->mInputs.LinearZ,
                mASvgfStage->mInputs.Normal,
                mASvgfStage->mInputs.Albedo,
            });

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

        {  // Write (+Read) Images
            std::vector<core::ManagedImage*> images{&(mASvgfStage->mATrousImage)};

            for(core::ManagedImage* image : images)
            {
                core::ImageLayoutCache::Barrier2 writeBarrier{.SrcStageMask     = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                                                              .SrcAccessMask    = VK_ACCESS_2_MEMORY_READ_BIT,
                                                              .DstStageMask     = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                                              .DstAccessMask    = VK_ACCESS_2_SHADER_WRITE_BIT,
                                                              .NewLayout        = VkImageLayout::VK_IMAGE_LAYOUT_GENERAL,
                                                              .SubresourceRange = VkImageSubresourceRange{.aspectMask     = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
                                                                                                          .levelCount     = 1U,
                                                                                                          .baseArrayLayer = mPushC.WriteIdx,
                                                                                                          .layerCount     = 1U}};
                vkBarriers.push_back(renderInfo.GetImageLayoutCache().MakeBarrier(image, writeBarrier));
                core::ImageLayoutCache::Barrier2 readBarrier{.SrcStageMask     = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                                                             .SrcAccessMask    = VK_ACCESS_2_MEMORY_WRITE_BIT,
                                                             .DstStageMask     = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                                             .DstAccessMask    = VK_ACCESS_2_SHADER_READ_BIT,
                                                             .NewLayout        = VkImageLayout::VK_IMAGE_LAYOUT_GENERAL,
                                                             .SubresourceRange = VkImageSubresourceRange{.aspectMask     = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
                                                                                                         .levelCount     = 1U,
                                                                                                         .baseArrayLayer = mPushC.ReadIdx,
                                                                                                         .layerCount     = 1U}};
                vkBarriers.push_back(renderInfo.GetImageLayoutCache().MakeBarrier(image, readBarrier));
            }

            core::ImageLayoutCache::Barrier2 barrier{
                .SrcStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .SrcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                .DstStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .DstAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT,
                .NewLayout     = VkImageLayout::VK_IMAGE_LAYOUT_GENERAL,
            };
            vkBarriers.push_back(renderInfo.GetImageLayoutCache().MakeBarrier(mASvgfStage->mPrimaryOutput, barrier));
        }

        VkDependencyInfo depInfo{
            .sType = VkStructureType::VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = (uint32_t)vkBarriers.size(), .pImageMemoryBarriers = vkBarriers.data()};

        vkCmdPipelineBarrier2(cmdBuffer, &depInfo);
    }

    void ATrousStage::DispatchIteration(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo)
    {
        vkCmdPushConstants(cmdBuffer, mPipelineLayout, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(mPushC), &mPushC);

        glm::uvec3 groupSize;
        {  // Calculate Group Size
            VkExtent2D size = mASvgfStage->mPrimaryOutput->GetExtent2D();

            glm::uvec2 localSize(16, 16);
            glm::uvec2 FrameSize(size.width, size.height);

            groupSize = glm::uvec3((FrameSize.x + localSize.x - 1) / localSize.x, (FrameSize.y + localSize.y - 1) / localSize.y, 1);
        }

        mContext->VkbDispatchTable->cmdDispatch(cmdBuffer, groupSize.x, groupSize.y, groupSize.z);
    }
}  // namespace foray::asvgf