#include "foray_asvgf.hpp"
#include <imgui/imgui.h>
#include <nameof/nameof.hpp>
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

        VkExtent2D renderSize  = mInputs.PrimaryInput->GetExtent2D();
        VkExtent2D strataCount = CalculateStrataCount(mContext->GetSwapchainSize());

        VkImageUsageFlags usageFlags = VkImageUsageFlagBits::VK_IMAGE_USAGE_STORAGE_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT;
        {  // Create ASvgf Stage owned Images

            core::ManagedImage::CreateInfo ci(usageFlags, VkFormat::VK_FORMAT_R32G32_SFLOAT, strataCount, "ASvgf.LuminanceMaxDiff");
            ci.ImageCI.arrayLayers                     = 2;
            ci.ImageViewCI.viewType                    = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            ci.ImageViewCI.subresourceRange.layerCount = 2U;
            mASvgfImages.LuminanceMaxDiff.Create(mContext, ci);
        }
        {
            core::ManagedImage::CreateInfo ci(usageFlags, VkFormat::VK_FORMAT_R32G32B32A32_SFLOAT, strataCount, "ASvgf.MomentsAndLinearZ");
            ci.ImageCI.arrayLayers                     = 2;
            ci.ImageViewCI.viewType                    = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            ci.ImageViewCI.subresourceRange.layerCount = 2U;
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

        { // Create Accumulation Images
         {// Accumulated Color
          VkImageUsageFlags usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_STORAGE_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        core::ManagedImage::CreateInfo ci(usage, VkFormat::VK_FORMAT_R32G32B32A32_SFLOAT, renderSize, "ASvgf.Accu.Color");
        ci.ImageCI.arrayLayers                     = 2;
        ci.ImageViewCI.viewType                    = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        ci.ImageViewCI.subresourceRange.layerCount = 2U;
        mAccumulatedImages.Color.Create(mContext, ci);
    }
    {  // Accumulated Moments
        VkImageUsageFlags              usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_STORAGE_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        core::ManagedImage::CreateInfo ci(usage, VkFormat::VK_FORMAT_R32G32_SFLOAT, renderSize, "ASvgf.Accu.Moments");
        ci.ImageCI.arrayLayers                     = 2;
        ci.ImageViewCI.viewType                    = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        ci.ImageViewCI.subresourceRange.layerCount = 2U;
        mAccumulatedImages.Moments.Create(mContext, ci);
    }
    {  // Accumulated History
        VkImageUsageFlags              usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_STORAGE_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        core::ManagedImage::CreateInfo ci(usage, VkFormat::VK_FORMAT_R32_SFLOAT, renderSize, "ASvgf.Accu.HistoryLength");
        ci.ImageCI.arrayLayers                     = 2;
        ci.ImageViewCI.viewType                    = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        ci.ImageViewCI.subresourceRange.layerCount = 2U;
        mAccumulatedImages.HistoryLength.Create(mContext, ci);
    }
}  // namespace foray::asvgf

{
    VkImageUsageFlags              usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_STORAGE_BIT;
    core::ManagedImage::CreateInfo ci(usage, VkFormat::VK_FORMAT_R32G32B32A32_SFLOAT, renderSize, "ASvgf.ATrous");
    ci.ImageCI.arrayLayers                     = 2;
    ci.ImageViewCI.viewType                    = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    ci.ImageViewCI.subresourceRange.layerCount = 2U;
    mATrousImage.Create(mContext, ci);
}

mCreateGradientSamplesStage.Init(this);
mAtrousGradientStage.Init(this);
mTemporalAccumulationStage.Init(this);
mEstimateVarianceStage.Init(this);
mAtrousStage.Init(this);
}

void ASvgfDenoiserStage::RecordFrame(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo)
{
    if(mHistoryImages.Valid)
    {
        renderInfo.GetImageLayoutCache().Set(mHistoryImages.PrimaryInput, VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        renderInfo.GetImageLayoutCache().Set(mHistoryImages.LinearZ, VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        renderInfo.GetImageLayoutCache().Set(mHistoryImages.MeshInstanceIdx, VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        renderInfo.GetImageLayoutCache().Set(mHistoryImages.Normal, VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    }

    mCreateGradientSamplesStage.RecordFrame(cmdBuffer, renderInfo);
    mAtrousGradientStage.RecordFrame(cmdBuffer, renderInfo);
    mTemporalAccumulationStage.RecordFrame(cmdBuffer, renderInfo);
    mEstimateVarianceStage.RecordFrame(cmdBuffer, renderInfo);
    mAtrousStage.RecordFrame(cmdBuffer, renderInfo);

    CopyToHistory(cmdBuffer, renderInfo);
    mHistoryImages.Valid = true;
}

void ASvgfDenoiserStage::CopyToHistory(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo)
{
    std::vector<util::HistoryImage*> historyImages{&mHistoryImages.LinearZ, &mHistoryImages.MeshInstanceIdx, &mHistoryImages.Normal, &mHistoryImages.PrimaryInput};

    util::HistoryImage::sMultiCopySourceToHistory(historyImages, cmdBuffer, renderInfo);
}

void ASvgfDenoiserStage::DisplayImguiConfiguration()
{
    const char* debugModes[] = {"DEBUG_NONE",
                                "DEBUG_CGS_LUMMAXDIFF",
                                "DEBUG_CGS_MOMENTSLINZ",
                                "DEBUG_GSATROUS_RESERVED0",
                                "DEBUG_GSATROUS_RESERVED1",
                                "DEBUG_TEMPACCU_OUTPUT",
                                "DEBUG_TEMPACCU_WEIGHTS",
                                "DEBUG_TEMPACCU_ALPHA",
                                "DEBUG_ESTVAR_VARIANCE",
                                "DEBUG_VARIO"};
    int         debugMode    = (int)mDebugMode;
    if(ImGui::Combo("Debug Mode", &debugMode, debugModes, sizeof(debugModes) / sizeof(const char*)))
    {
        mDebugMode = (uint32_t)debugMode;
    }
    if (ImGui::CollapsingHeader("Gradient Samples Atrous Settings"))
    {
        int iterations = (int)mAtrousGradientStage.mPushC.IterationCount;
        if (ImGui::SliderInt("Iterations", &iterations, 1, 9))
        {
            mAtrousGradientStage.mPushC.IterationCount = (uint32_t)iterations;
        }
    }
    if (ImGui::CollapsingHeader("Color Atrous Settings"))
    {
        int iterations = (int)mAtrousStage.mPushC.IterationCount;
        if (ImGui::SliderInt("Iterations", &iterations, 1, 9))
        {
            mAtrousStage.mPushC.IterationCount = (uint32_t)iterations;
        }
        const char* atrousKernelModes[] = {"Box3", "Box5", "atrous", "subsampled"};
        int         atrousKernelIdx     = (int)mAtrousStage.mPushC.UsedKernel;
        if(ImGui::Combo("Kernel Mode", &atrousKernelIdx, atrousKernelModes, 4))
        {
            mAtrousStage.mPushC.UsedKernel = (uint32_t)atrousKernelIdx;
        }
    }
}

void ASvgfDenoiserStage::IgnoreHistoryNextFrame() {}

VkExtent2D ASvgfDenoiserStage::CalculateStrataCount(const VkExtent2D& extent)
{
    uint32_t strataSize = 3;
    return VkExtent2D{.width = (extent.width + strataSize - 1) / strataSize, .height = (extent.height + strataSize - 1) / strataSize};
}

void ASvgfDenoiserStage::Resize(const VkExtent2D& extent)
{
    VkExtent2D strataCount = CalculateStrataCount(extent);

    std::vector<core::ManagedImage*> strataImages({&(mASvgfImages.LuminanceMaxDiff), &(mASvgfImages.MomentsAndLinearZ), &(mASvgfImages.Seed)});
    for(core::ManagedImage* image : strataImages)
    {
        if(image->Exists())
        {
            image->Resize(strataCount);
        }
    }

    std::vector<core::ManagedImage*> fullSizeImages({&(mAccumulatedImages.Color), &(mAccumulatedImages.Moments), &(mAccumulatedImages.HistoryLength), &mATrousImage});
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
    mHistoryImages.Valid = false;

    mCreateGradientSamplesStage.UpdateDescriptorSet();
    mAtrousGradientStage.UpdateDescriptorSet();
    mTemporalAccumulationStage.UpdateDescriptorSet();
    mEstimateVarianceStage.UpdateDescriptorSet();
    mAtrousStage.UpdateDescriptorSet();
}

void ASvgfDenoiserStage::OnShadersRecompiled()
{
    mCreateGradientSamplesStage.OnShadersRecompiled();
    mAtrousGradientStage.OnShadersRecompiled();
    mTemporalAccumulationStage.OnShadersRecompiled();
    mEstimateVarianceStage.OnShadersRecompiled();
    mAtrousStage.OnShadersRecompiled();
}

void ASvgfDenoiserStage::Destroy()
{
    std::vector<core::ManagedImage*> images({&(mASvgfImages.LuminanceMaxDiff), &(mASvgfImages.MomentsAndLinearZ), &(mASvgfImages.Seed), &(mAccumulatedImages.Color),
                                             &(mAccumulatedImages.Moments), &(mAccumulatedImages.HistoryLength), &mATrousImage});

    for(core::ManagedImage* image : images)
    {
        image->Destroy();
    }

    std::vector<util::HistoryImage*> historyImages({&(mHistoryImages.PrimaryInput), &(mHistoryImages.LinearZ), &(mHistoryImages.MeshInstanceIdx), &(mHistoryImages.Normal)});

    for(util::HistoryImage* image : historyImages)
    {
        image->Destroy();
    }
    mHistoryImages.Valid = false;

    std::vector<stages::RenderStage*> stages({&mCreateGradientSamplesStage, &mAtrousGradientStage, &mTemporalAccumulationStage, &mEstimateVarianceStage, &mAtrousStage});

    for(stages::RenderStage* stage : stages)
    {
        stage->Destroy();
    }
}

}  // namespace foray::asvgf
