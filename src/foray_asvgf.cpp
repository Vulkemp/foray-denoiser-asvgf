#include "foray_asvgf.hpp"
#include <bench/foray_devicebenchmark.hpp>
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
        mInputs.Positions = config.GBufferOutputs[(size_t)stages::GBufferStage::EOutput::Position];
        Assert(!!mInputs.Positions);
        mInputs.PrimaryInput = config.PrimaryInput;
        Assert(!!mInputs.PrimaryInput);
        mPrimaryOutput = config.PrimaryOutput;
        Assert(!!mPrimaryOutput);

        VkExtent2D renderSize  = mInputs.PrimaryInput->GetExtent2D();
        VkExtent2D strataCount = CalculateStrataCount(mContext->GetSwapchainSize());

        mBenchmark = config.Benchmark;
        if(!!mBenchmark)
        {
            std::vector<const char*> queryNames({bench::BenchmarkTimestamp::BEGIN, TIMESTAMP_CreateGradientSamples, TIMESTAMP_ATrousGradient, TIMESTAMP_TemporalAccumulation,
                                                 TIMESTAMP_EstimateVariance, TIMESTAMP_ATrousColor, bench::BenchmarkTimestamp::END});
            mBenchmark->Create(mContext, queryNames);
        }

        VkImageUsageFlags usageFlags = VkImageUsageFlagBits::VK_IMAGE_USAGE_STORAGE_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT;
        {  // Create ASvgf Stage owned Images

            core::ManagedImage::CreateInfo ci(usageFlags, VkFormat::VK_FORMAT_R16G16_SFLOAT, strataCount, "ASvgf.LuminanceMaxDiff");
            ci.ImageCI.arrayLayers                     = 2;
            ci.ImageViewCI.viewType                    = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            ci.ImageViewCI.subresourceRange.layerCount = 2U;
            mASvgfImages.LuminanceMaxDiff.Create(mContext, ci);
        }
        {
            core::ManagedImage::CreateInfo ci(usageFlags, VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT, strataCount, "ASvgf.MomentsAndLinearZ");
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
            std::vector<core::ManagedImage*> srcImages{mInputs.LinearZ, mInputs.MeshInstanceIdx, mInputs.Normal, mInputs.PrimaryInput, mInputs.Positions};
            std::vector<util::HistoryImage*> historyImages{&mHistoryImages.LinearZ, &mHistoryImages.MeshInstanceIdx, &mHistoryImages.Normal, &mHistoryImages.PrimaryInput,
                                                           &mHistoryImages.Positions};

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
        core::ManagedImage::CreateInfo ci(usage, VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT, renderSize, "ASvgf.Accu.Color");
        ci.ImageCI.arrayLayers                     = 2;
        ci.ImageViewCI.viewType                    = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        ci.ImageViewCI.subresourceRange.layerCount = 2U;
        mAccumulatedImages.Color.Create(mContext, ci);
    }
    {  // Accumulated Moments
        VkImageUsageFlags              usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_STORAGE_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        core::ManagedImage::CreateInfo ci(usage, VkFormat::VK_FORMAT_R16G16_SFLOAT, renderSize, "ASvgf.Accu.Moments");
        ci.ImageCI.arrayLayers                     = 2;
        ci.ImageViewCI.viewType                    = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        ci.ImageViewCI.subresourceRange.layerCount = 2U;
        mAccumulatedImages.Moments.Create(mContext, ci);
    }
    {  // Accumulated History
        VkImageUsageFlags              usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_STORAGE_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        core::ManagedImage::CreateInfo ci(usage, VkFormat::VK_FORMAT_R16_SFLOAT, renderSize, "ASvgf.Accu.HistoryLength");
        ci.ImageCI.arrayLayers                     = 2;
        ci.ImageViewCI.viewType                    = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        ci.ImageViewCI.subresourceRange.layerCount = 2U;
        mAccumulatedImages.HistoryLength.Create(mContext, ci);
    }
}  // namespace foray::asvgf

{
    VkImageUsageFlags              usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_STORAGE_BIT;
    core::ManagedImage::CreateInfo ci(usage, VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT, renderSize, "ASvgf.ATrous");
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
mInitialized = true;
}

void ASvgfDenoiserStage::RecordFrame(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo)
{
    if(mHistoryImages.Valid)
    {
        renderInfo.GetImageLayoutCache().Set(mHistoryImages.PrimaryInput, VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        renderInfo.GetImageLayoutCache().Set(mHistoryImages.LinearZ, VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        renderInfo.GetImageLayoutCache().Set(mHistoryImages.MeshInstanceIdx, VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        renderInfo.GetImageLayoutCache().Set(mHistoryImages.Normal, VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        renderInfo.GetImageLayoutCache().Set(mHistoryImages.Positions, VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    }

    uint32_t                frameIdx = renderInfo.GetFrameNumber();
    VkPipelineStageFlagBits compute  = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

    if(!!mBenchmark)
    {
        mBenchmark->CmdResetQuery(cmdBuffer, frameIdx);
        mBenchmark->CmdWriteTimestamp(cmdBuffer, frameIdx, bench::BenchmarkTimestamp::BEGIN, compute);
    }
    mCreateGradientSamplesStage.RecordFrame(cmdBuffer, renderInfo);
    if(!!mBenchmark)
    {
        mBenchmark->CmdWriteTimestamp(cmdBuffer, frameIdx, TIMESTAMP_CreateGradientSamples, compute);
    }
    mAtrousGradientStage.RecordFrame(cmdBuffer, renderInfo);
    if(!!mBenchmark)
    {
        mBenchmark->CmdWriteTimestamp(cmdBuffer, frameIdx, TIMESTAMP_ATrousGradient, compute);
    }
    mTemporalAccumulationStage.RecordFrame(cmdBuffer, renderInfo);
    if(!!mBenchmark)
    {
        mBenchmark->CmdWriteTimestamp(cmdBuffer, frameIdx, TIMESTAMP_TemporalAccumulation, compute);
    }
    mEstimateVarianceStage.RecordFrame(cmdBuffer, renderInfo);
    if(!!mBenchmark)
    {
        mBenchmark->CmdWriteTimestamp(cmdBuffer, frameIdx, TIMESTAMP_EstimateVariance, compute);
    }
    mAtrousStage.RecordFrame(cmdBuffer, renderInfo);
    if(!!mBenchmark)
    {
        mBenchmark->CmdWriteTimestamp(cmdBuffer, frameIdx, TIMESTAMP_ATrousColor, VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TRANSFER_BIT);
    }

    CopyToHistory(cmdBuffer, renderInfo);
    mHistoryImages.Valid = true;
    if(!!mBenchmark)
    {
        mBenchmark->CmdWriteTimestamp(cmdBuffer, frameIdx, bench::BenchmarkTimestamp::END, VkPipelineStageFlagBits::VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    }
}

void ASvgfDenoiserStage::CopyToHistory(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo)
{
    std::vector<util::HistoryImage*> historyImages{&mHistoryImages.LinearZ, &mHistoryImages.MeshInstanceIdx, &mHistoryImages.Normal, &mHistoryImages.PrimaryInput,
                                                   &mHistoryImages.Positions};

    util::HistoryImage::sMultiCopySourceToHistory(historyImages, cmdBuffer, renderInfo);
}

void ASvgfDenoiserStage::DisplayImguiConfiguration()
{
    const char* debugModes[] = {"DEBUG_NONE",
                                "DEBUG_CGS_LUMMAXDIFF",
                                "DEBUG_CGS_MOMENTSLINZ",
                                "DEBUG_GSATROUS_LUMMAXDIFF",
                                "DEBUG_GSATROUS_SIGMALUM",
                                "DEBUG_TEMPACCU_OUTPUT",
                                "DEBUG_TEMPACCU_WEIGHTS",
                                "DEBUG_TEMPACCU_ALPHA",
                                "DEBUG_ESTVAR_VARIANCE"};
    int         debugMode    = (int)mDebugMode;
    if(ImGui::Combo("Debug Mode", &debugMode, debugModes, sizeof(debugModes) / sizeof(const char*)))
    {
        mDebugMode = (uint32_t)debugMode;
    }
    if(ImGui::CollapsingHeader("Gradient Samples ATrous Settings"))
    {
        int iterations = (int)mAtrousGradientStage.mIterationCount;
        if(ImGui::SliderInt("GS ATrous Iterations", &iterations, 1, 15))
        {
            mAtrousGradientStage.mIterationCount = (uint32_t)iterations;
        }
        if(ImGui::IsItemHovered())
            ImGui::SetTooltip("Default Value 5 iterations");

        const char* atrousKernelModes[] = {"Box3", "Box5", "ATrous", "Subsampled"};
        int         atrousKernelIdx     = (int)mAtrousGradientStage.mPushC.KernelMode;
        if(ImGui::Combo("GS Kernel Mode", &atrousKernelIdx, atrousKernelModes, 4))
        {
            mAtrousGradientStage.mPushC.KernelMode = (uint32_t)atrousKernelIdx;
        }
        if(ImGui::IsItemHovered())
            ImGui::SetTooltip("Default Value \"Box3\"");
    }
    if(ImGui::CollapsingHeader("Temporal Acccumulation Settings"))
    {
        float maxNormalDiffDegrees = glm::degrees(glm::asin(mTemporalAccumulationStage.mPushC.MaxNormalDeviation));
        if(ImGui::SliderFloat("Max Normal Deviation (deg)", &maxNormalDiffDegrees, 0.01f, 179.99f))
        {
            mTemporalAccumulationStage.mPushC.MaxNormalDeviation = glm::sin(glm::radians(maxNormalDiffDegrees));
        }
        if(ImGui::IsItemHovered())
            ImGui::SetTooltip("In degrees. Default Value 15 degrees");

        float maxPositionDiff = mTemporalAccumulationStage.mPushC.MaxPositionDifference;
        if(ImGui::SliderFloat("Max Position Difference (m)", &maxPositionDiff, 0.0001f, 2.f))
        {
            mTemporalAccumulationStage.mPushC.MaxPositionDifference = maxPositionDiff;
        }
        if(ImGui::IsItemHovered())
            ImGui::SetTooltip("In meters. Default Value 0.15 meters");

        float weightThreshhold = mTemporalAccumulationStage.mPushC.WeightThreshhold;
        if(ImGui::SliderFloat("Weight Threshhold", &weightThreshhold, 0.00001f, 1.f))
        {
            mTemporalAccumulationStage.mPushC.WeightThreshhold = weightThreshhold;
        }
        if(ImGui::IsItemHovered())
            ImGui::SetTooltip("Default Value 0.01");

        float minNewDataWeight = mTemporalAccumulationStage.mPushC.MinNewDataWeight;
        if(ImGui::SliderFloat("Minimum New Data Weight", &minNewDataWeight, 0.00001f, 1.f))
        {
            mTemporalAccumulationStage.mPushC.MinNewDataWeight = minNewDataWeight;
        }
        if(ImGui::IsItemHovered())
            ImGui::SetTooltip("Default Value 0.1");

        float antilagMulti = mTemporalAccumulationStage.mPushC.AntilagMultiplier;
        if(ImGui::SliderFloat("Antilag Multiplier", &antilagMulti, 0.0f, 3.f))
        {
            mTemporalAccumulationStage.mPushC.AntilagMultiplier = antilagMulti;
        }
        if(ImGui::IsItemHovered())
            ImGui::SetTooltip("Higher values make antilag more aggressive. Default Value 1.0");
    }
    if(ImGui::CollapsingHeader("Color Atrous Settings"))
    {
        int iterations = (int)mAtrousStage.mIterationCount;
        if(ImGui::SliderInt("CA Iterations", &iterations, 1, 15))
        {
            mAtrousStage.mIterationCount = (uint32_t)iterations;
        }
        if(ImGui::IsItemHovered())
            ImGui::SetTooltip("Default Value 5 iterations");

        const char* atrousKernelModes[] = {"Box3", "Box5", "ATrous", "Subsampled"};
        int         atrousKernelIdx     = (int)mAtrousStage.mPushC.KernelMode;
        if(ImGui::Combo("CA Kernel Mode", &atrousKernelIdx, atrousKernelModes, 4))
        {
            mAtrousStage.mPushC.KernelMode = (uint32_t)atrousKernelIdx;
        }
        if(ImGui::IsItemHovered())
            ImGui::SetTooltip("Default Value \"ATrous\"");
    }
}

void ASvgfDenoiserStage::IgnoreHistoryNextFrame()
{
    mHistoryImages.Valid = false;
}

VkExtent2D ASvgfDenoiserStage::CalculateStrataCount(const VkExtent2D& extent)
{
    uint32_t strataSize = 3;
    return VkExtent2D{.width = (extent.width + strataSize - 1) / strataSize, .height = (extent.height + strataSize - 1) / strataSize};
}

void ASvgfDenoiserStage::Resize(const VkExtent2D& extent)
{
    if(!mInitialized)
    {
        return;
    }

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

    std::vector<util::HistoryImage*> historyImages{&mHistoryImages.LinearZ, &mHistoryImages.MeshInstanceIdx, &mHistoryImages.Normal, &mHistoryImages.PrimaryInput,
                                                   &mHistoryImages.Positions};

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

void ASvgfDenoiserStage::OnShadersRecompiled(const std::unordered_set<uint64_t>& recompiled)
{
    mCreateGradientSamplesStage.OnShadersRecompiled(recompiled);
    mAtrousGradientStage.OnShadersRecompiled(recompiled);
    mTemporalAccumulationStage.OnShadersRecompiled(recompiled);
    mEstimateVarianceStage.OnShadersRecompiled(recompiled);
    mAtrousStage.OnShadersRecompiled(recompiled);
}

void ASvgfDenoiserStage::Destroy()
{
    std::vector<core::ManagedImage*> images({&(mASvgfImages.LuminanceMaxDiff), &(mASvgfImages.MomentsAndLinearZ), &(mASvgfImages.Seed), &(mAccumulatedImages.Color),
                                             &(mAccumulatedImages.Moments), &(mAccumulatedImages.HistoryLength), &mATrousImage});

    for(core::ManagedImage* image : images)
    {
        image->Destroy();
    }

    std::vector<util::HistoryImage*> historyImages(
        {&(mHistoryImages.PrimaryInput), &(mHistoryImages.LinearZ), &(mHistoryImages.MeshInstanceIdx), &(mHistoryImages.Normal), &(mHistoryImages.Positions)});

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

    if(!!mBenchmark)
    {
        mBenchmark->Destroy();
        mBenchmark = nullptr;
    }

    mInitialized = false;
}

}  // namespace foray::asvgf
