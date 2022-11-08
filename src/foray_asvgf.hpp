#pragma once

#include "foray_asvgf_atrous.hpp"
#include "foray_asvgf_atrousgradient.hpp"
#include "foray_asvgf_creategradientsamples.hpp"
#include "foray_asvgf_estimatevariance.hpp"
#include "foray_asvgf_temporalaccumulation.hpp"
#include <stages/foray_denoiserstage.hpp>
#include <util/foray_historyimage.hpp>

namespace foray::asvgf {
    class ASvgfDenoiserStage : public foray::stages::DenoiserStage
    {
        friend CreateGradientSamplesStage;
        friend ATrousGradientStage;

      public:
        virtual void Init(core::Context* context, const stages::DenoiserConfig& config) override;

        virtual void RecordFrame(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo) override;

        virtual std::string GetUILabel() override { return "ASVGF Denoiser"; }

        virtual void DisplayImguiConfiguration() override;

        virtual void IgnoreHistoryNextFrame() override;

        virtual void Resize(const VkExtent2D& extent) override;

        virtual void Destroy() override;

      protected:
        struct
        {
            core::ManagedImage* PrimaryInput    = nullptr;
            core::ManagedImage* Albedo          = nullptr;
            core::ManagedImage* Normal          = nullptr;
            core::ManagedImage* LinearZ         = nullptr;
            core::ManagedImage* Motion          = nullptr;
            core::ManagedImage* MeshInstanceIdx = nullptr;
            core::ManagedImage* NoiseTexture    = nullptr;
        } mInputs;

        struct
        {
            core::ManagedImage LuminanceMaxDiff;
            core::ManagedImage MomentsAndLinearZ;
            core::ManagedImage Seed;
        } mASvgfImages;

        struct
        {
            util::HistoryImage PrimaryInput;
            util::HistoryImage LinearZ;
            util::HistoryImage MeshInstanceIdx;
            util::HistoryImage Normal;
        } mHistoryImages;

        core::ManagedImage* mPrimaryOutput = nullptr;

        CreateGradientSamplesStage mCreateGradientSamplesStage;
        ATrousGradientStage        mAtrousGradientStage;
        TemporalAccumulationStage  mTemporalAccumulationStage;
        EstimateVarianceStage      mEstimateVarianceStage;
        ATrousStage                mAtrousStage;

        VkExtent2D CalculateStrataCount(const VkExtent2D& extent);

        void CopyToHistory(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo);
    };
}  // namespace foray::asvgf
