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
            ci.ImageCI.arrayLayers = 2;
            ci.ImageViewCI.viewType = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            mASvgfImages.LuminanceMaxDiff.Create(mContext, ci);
        }
        {
            core::ManagedImage::CreateInfo ci(usageFlags, VkFormat::VK_FORMAT_R32G32B32A32_SFLOAT, strataCount, "ASvgf.MomentsAndLinearZ");
            ci.ImageCI.arrayLayers = 2;
            ci.ImageViewCI.viewType = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            mASvgfImages.MomentsAndLinearZ.Create(mContext, ci);
        }

        {
            core::ManagedImage::CreateInfo ci(usageFlags, VkFormat::VK_FORMAT_R16_UINT, strataCount, "ASvgf.Seed");
            mASvgfImages.Seed.Create(mContext, ci);
        }

        {  // Create History Images
            std::vector<core::ManagedImage*> srcImages{mInputs.LinearZ, mInputs.MeshInstanceIdx, mInputs.Normal, mInputs.PrimaryInput};
            std::vector<util::HistoryImage*> historyImages{&mHistoryImages.LinearZ, &mHistoryImages.MeshInstanceIdx, &mHistoryImages.Normal, &mHistoryImages.PrimaryInput};

            for(int32_t i = 0; i < historyImages.size(); i++)
            {
                util::HistoryImage* historyImage = historyImages[i];
                core::ManagedImage* srcImage     = srcImages[i];

                historyImage->Create(mContext, srcImage);
            }
        }

        mCreateGradientSamplesStage.Init(this);
        mAtrousGradientStage.Init(this);
    }

    void ASvgfDenoiserStage::RecordFrame(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo)
    {
        mCreateGradientSamplesStage.RecordFrame(cmdBuffer, renderInfo);
        mAtrousGradientStage.RecordFrame(cmdBuffer, renderInfo);

        CopyToHistory(cmdBuffer, renderInfo);
    }

    void ASvgfDenoiserStage::CopyToHistory(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo)
    {
        std::vector<util::HistoryImage*> historyImages{&mHistoryImages.LinearZ, &mHistoryImages.MeshInstanceIdx, &mHistoryImages.Normal, &mHistoryImages.PrimaryInput};

        util::HistoryImage::sMultiCopySourceToHistory(historyImages, cmdBuffer, renderInfo);
    }

    void ASvgfDenoiserStage::DisplayImguiConfiguration() {}

    void ASvgfDenoiserStage::IgnoreHistoryNextFrame() {}

    VkExtent2D ASvgfDenoiserStage::CalculateStrataCount(const VkExtent2D& extent)
    {
        uint32_t strataSize = 3;
        return VkExtent2D{.width = (extent.width + strataSize - 1) / strataSize, .height = (extent.height + strataSize - 1) / strataSize};
    }

    void ASvgfDenoiserStage::OnResized(const VkExtent2D& extent)
    {
        VkExtent2D strataCount = CalculateStrataCount(extent);

        std::vector<core::ManagedImage*> strataImages({&(mASvgfImages.LuminanceMaxDiff), &(mASvgfImages.MomentsAndLinearZ), &(mASvgfImages.Seed)});
        std::vector<core::ManagedImage*> fullSizeImages;
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

        std::vector<util::HistoryImage*> historyImages{&mHistoryImages.LinearZ, &mHistoryImages.MeshInstanceIdx, &mHistoryImages.Normal, &mHistoryImages.PrimaryInput};

        for(util::HistoryImage* image : historyImages)
        {
            if(image->Exists())
            {
                image->Resize(extent);
            }
        }

        mCreateGradientSamplesStage.UpdateDescriptorSet();
        mAtrousGradientStage.UpdateDescriptorSet();
    }

    void ASvgfDenoiserStage::Destroy()
    {
        std::vector<core::ManagedImage*> images({&(mASvgfImages.LuminanceMaxDiff), &(mASvgfImages.MomentsAndLinearZ), &(mASvgfImages.Seed)});

        for(core::ManagedImage* image : images)
        {
            image->Destroy();
        }

        std::vector<util::HistoryImage*> historyImages({&(mHistoryImages.PrimaryInput), &(mHistoryImages.LinearZ), &(mHistoryImages.MeshInstanceIdx), &(mHistoryImages.Normal)});

        for(util::HistoryImage* image : historyImages)
        {
            image->Destroy();
        }

        std::vector<stages::RenderStage*> stages({&mCreateGradientSamplesStage, &mAtrousGradientStage});

        for(stages::RenderStage* stage : stages)
        {
            stage->Destroy();
        }
    }

}  // namespace foray::asvgf
