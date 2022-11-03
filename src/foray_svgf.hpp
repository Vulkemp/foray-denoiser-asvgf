#pragma once

#include "foray_svgf_atrous.hpp"
#include "foray_svgf_atrousgradient.hpp"
#include "foray_svgf_creategradientsamples.hpp"
#include "foray_svgf_estimatevariance.hpp"
#include "foray_svgf_temporalaccumulation.hpp"
#include <stages/foray_denoiserstage.hpp>

namespace foray::svgf {
    class SvgfDenoiserStage : public foray::stages::DenoiserStage
    {
      public:
        virtual void                    Init(core::Context* context, const stages::DenoiserConfig& config) override;
        inline static const std::string INPUT_DIRECT   = "Direct";
        inline static const std::string INPUT_INDIRECT = "Indirect";

        virtual std::string GetUILabel() { return "ASVGF Denoiser"; }

        virtual void DisplayImguiConfiguration();

        virtual void IgnoreHistoryNextFrame();

      protected:
        core::ManagedImage*        mDirectLightInput   = nullptr;
        core::ManagedImage*        mIndirectLightInput = nullptr;
        core::ManagedImage*        mAlbedoInput        = nullptr;
        core::ManagedImage*        mNormalInput        = nullptr;
        core::ManagedImage*        mMotionInput        = nullptr;
        core::ManagedImage*        mPrimaryOutput      = nullptr;
        CreateGradientSamplesStage mCreateGradientSamplesStage;
        ATrousGradientStage        mAtrousGradientStage;
        TemporalAccumulationStage  mTemporalAccumulationStage;
        EstimateVarianceStage      mEstimateVarianceStage;
        ATrousStage                mAtrousStage;
    };
}  // namespace foray::svgf
