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
    }
    void ATrousGradientStage::ApiCreateDescriptorSetLayout()
    {
        UpdateDescriptorSet();
    }
    void ATrousGradientStage::UpdateDescriptorSet()
    {
        mDescriptorSet.SetDescriptorAt(0, mASvgfStage->mASvgfImages.LuminanceMaxDiff, VkImageLayout::VK_IMAGE_LAYOUT_GENERAL, nullptr,
                                       VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
        mDescriptorSet.SetDescriptorAt(1, mASvgfStage->mASvgfImages.MomentsAndLinearZ, VkImageLayout::VK_IMAGE_LAYOUT_GENERAL, nullptr,
                                       VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
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
        mPipelineLayout.AddPushConstantRange<uint32_t>(VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
        mPipelineLayout.Build(mContext);
    }

    void ATrousGradientStage::ApiBeforeFrame(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo)
    {
        std::vector<VkImageMemoryBarrier2> vkBarriers;

        // Write (+Read) Images
        std::vector<core::ManagedImage*> images{&(mASvgfStage->mASvgfImages.LuminanceMaxDiff), &(mASvgfStage->mASvgfImages.MomentsAndLinearZ)};

        for(core::ManagedImage* image : images)
        {
            core::ImageLayoutCache::Barrier2 barrier{
                .SrcStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .SrcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                .DstStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .DstAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT,
                .NewLayout     = VkImageLayout::VK_IMAGE_LAYOUT_GENERAL,
                .SubresourceRange = VkImageSubresourceRange{.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1U, .layerCount = 2U}
            };
            vkBarriers.push_back(renderInfo.GetImageLayoutCache().MakeBarrier(image, barrier));
        }

        VkDependencyInfo depInfo{
            .sType = VkStructureType::VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = (uint32_t)vkBarriers.size(), .pImageMemoryBarriers = vkBarriers.data()};

        vkCmdPipelineBarrier2(cmdBuffer, &depInfo);
    }

    void ATrousGradientStage::ApiBeforeDispatch(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo, glm::uvec3& groupSize)
    {
        uint32_t IterationCount = 5;
        vkCmdPushConstants(cmdBuffer, mPipelineLayout, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(IterationCount), &IterationCount);

        VkExtent3D size = mASvgfStage->mASvgfImages.LuminanceMaxDiff.GetExtent3D();

        glm::uvec2 localSize(16, 16);
        glm::uvec2 strataFrameSize(size.width, size.height);

        groupSize = glm::uvec3((strataFrameSize.x + localSize.x - 1) / localSize.x, (strataFrameSize.y + localSize.y - 1) / localSize.y, 1);
    }
}  // namespace foray::asvgf
