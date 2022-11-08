#include "foray_asvgf_creategradientsamples.hpp"
#include "foray_asvgf.hpp"

namespace foray::asvgf {
    void CreateGradientSamplesStage::Init(ASvgfDenoiserStage* aSvgfStage)
    {
        Destroy();
        mASvgfStage = aSvgfStage;
        stages::ComputeStage::Init(mASvgfStage->mContext);
    }

    void CreateGradientSamplesStage::ApiInitShader()
    {
        mShader.LoadFromSource(mContext, SHADER_DIR "/creategradientsamples.comp");
    }
    void CreateGradientSamplesStage::ApiCreateDescriptorSet()
    {
        UpdateDescriptorSet();
    }
    void CreateGradientSamplesStage::UpdateDescriptorSet()
    {
        mDescriptorSet.SetDescriptorAt(0, mASvgfStage->mInputs.PrimaryInput, VkImageLayout::VK_IMAGE_LAYOUT_GENERAL, nullptr, VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                       VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
        mDescriptorSet.SetDescriptorAt(1, mASvgfStage->mASvgfImages.Seed, VkImageLayout::VK_IMAGE_LAYOUT_GENERAL, nullptr, VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                       VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
        mDescriptorSet.SetDescriptorAt(2, mASvgfStage->mHistoryImages.PrimaryInput, VkImageLayout::VK_IMAGE_LAYOUT_GENERAL, nullptr,
                                       VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
        mDescriptorSet.SetDescriptorAt(3, mASvgfStage->mInputs.LinearZ, VkImageLayout::VK_IMAGE_LAYOUT_GENERAL, nullptr, VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                       VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
        mDescriptorSet.SetDescriptorAt(4, mASvgfStage->mInputs.MeshInstanceIdx, VkImageLayout::VK_IMAGE_LAYOUT_GENERAL, nullptr, VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                       VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
        mDescriptorSet.SetDescriptorAt(5, mASvgfStage->mInputs.NoiseTexture, VkImageLayout::VK_IMAGE_LAYOUT_GENERAL, nullptr, VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                       VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
        mDescriptorSet.SetDescriptorAt(6, mASvgfStage->mASvgfImages.LuminanceMaxDiff, VkImageLayout::VK_IMAGE_LAYOUT_GENERAL, nullptr,
                                       VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
        mDescriptorSet.SetDescriptorAt(7, mASvgfStage->mASvgfImages.MomentsAndLinearZ, VkImageLayout::VK_IMAGE_LAYOUT_GENERAL, nullptr,
                                       VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
        if(mDescriptorSet.Exists())
        {
            mDescriptorSet.Update();
        }
        else
        {
            mDescriptorSet.Create(mContext, "ASvgf.CreateGradientSamples");
        }
    }
    void CreateGradientSamplesStage::ApiCreatePipelineLayout()
    {
        mPipelineLayout.AddDescriptorSetLayout(mDescriptorSet.GetDescriptorSetLayout());
        mPipelineLayout.AddPushConstantRange<uint32_t>(VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
        mPipelineLayout.Build(mContext);
    }
    void CreateGradientSamplesStage::ApiBeforeFrame(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo)
    {
        std::vector<VkImageMemoryBarrier2> vkBarriers;

        {  // Read Only Images
            std::vector<core::ManagedImage*> readOnlyImages({mASvgfStage->mInputs.PrimaryInput, &(mASvgfStage->mHistoryImages.PrimaryInput.GetHistoryImage()),
                                                             mASvgfStage->mInputs.LinearZ, mASvgfStage->mInputs.MeshInstanceIdx, mASvgfStage->mInputs.NoiseTexture});

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
        {  // Write (+Read) Images
            std::vector<core::ManagedImage*> images{&(mASvgfStage->mASvgfImages.LuminanceMaxDiff), &(mASvgfStage->mASvgfImages.MomentsAndLinearZ)};

            core::ImageLayoutCache::Barrier2 barrier{
                .SrcStageMask     = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .SrcAccessMask    = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                .DstStageMask     = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .DstAccessMask    = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT,
                .NewLayout        = VkImageLayout::VK_IMAGE_LAYOUT_GENERAL};
            vkBarriers.push_back(renderInfo.GetImageLayoutCache().MakeBarrier(mASvgfStage->mASvgfImages.Seed, barrier));


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
    void CreateGradientSamplesStage::ApiBeforeDispatch(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo, glm::uvec3& groupSize)
    {
        uint32_t frameIdx = (uint32_t)renderInfo.GetFrameNumber();
        vkCmdPushConstants(cmdBuffer, mPipelineLayout, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(frameIdx), &frameIdx);

        VkExtent3D size = mASvgfStage->mASvgfImages.LuminanceMaxDiff.GetExtent3D();

        glm::uvec2 localSize(16, 16);
        glm::uvec2 strataFrameSize(size.width, size.height);

        groupSize = glm::uvec3((strataFrameSize.x + localSize.x - 1) / localSize.x, (strataFrameSize.y + localSize.y - 1) / localSize.y, 1);
    }
}  // namespace foray::asvgf
