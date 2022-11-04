#pragma once

#include "foray_asvgf_atrous.hpp"
#include "foray_asvgf_atrousgradient.hpp"
#include "foray_asvgf_creategradientsamples.hpp"
#include "foray_asvgf_estimatevariance.hpp"
#include "foray_asvgf_temporalaccumulation.hpp"
#include <stages/foray_denoiserstage.hpp>

namespace foray::asvgf {
    class ASvgfDenoiserStage : public foray::stages::DenoiserStage
    {
      public:
        virtual void                    Init(core::Context* context, const stages::DenoiserConfig& config) override;

        virtual std::string GetUILabel() override { return "ASVGF Denoiser"; }

        virtual void DisplayImguiConfiguration() override;

        virtual void IgnoreHistoryNextFrame() override;

      protected:

        struct {
          core::ManagedImage* PrimaryInput;
          core::ManagedImage* AlbedoInput;
          core::ManagedImage* NormalInput;
          core::ManagedImage* LinearZInput;
          core::ManagedImage* MotionInput;
          core::ManagedImage* MeshInstanceIdxInput;
        } mInputs;

        struct {

        } mASvgfImages;

        core::ManagedImage*        mPrimaryOutput      = nullptr;

        CreateGradientSamplesStage mCreateGradientSamplesStage;
        ATrousGradientStage        mAtrousGradientStage;
        TemporalAccumulationStage  mTemporalAccumulationStage;
        EstimateVarianceStage      mEstimateVarianceStage;
        ATrousStage                mAtrousStage;
    };
}  // namespace foray::asvgf
