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

        VkExtent2D swapSize   = mContext->GetSwapchainSize();
        uint32_t   strataSize = 3;
        VkExtent2D strataCount{.width = (swapSize.width + strataSize - 1) / strataSize, .height = (swapSize.height + strataSize - 1) / strataSize};

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
    }

    void ASvgfDenoiserStage::DisplayImguiConfiguration() {}

    void ASvgfDenoiserStage::IgnoreHistoryNextFrame() {}

}  // namespace foray::asvgf
