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
        friend CreateGradientSamplesStage;

      public:
        virtual void Init(core::Context* context, const stages::DenoiserConfig& config) override;

        virtual std::string GetUILabel() override { return "ASVGF Denoiser"; }

        virtual void DisplayImguiConfiguration() override;

        virtual void IgnoreHistoryNextFrame() override;

      protected:
        struct
        {
            core::ManagedImage* PrimaryInput         = nullptr;
            core::ManagedImage* AlbedoInput          = nullptr;
            core::ManagedImage* NormalInput          = nullptr;
            core::ManagedImage* LinearZInput         = nullptr;
            core::ManagedImage* MotionInput          = nullptr;
            core::ManagedImage* MeshInstanceIdxInput = nullptr;
            core::ManagedImage* NoiseTexture         = nullptr;
        } mInputs;

        struct
        {
            core::ManagedImage* LuminanceMaxDiff  = nullptr;
            core::ManagedImage* MomentsAndLinearZ = nullptr;
            core::ManagedImage* Seed = nullptr;
        } mASvgfImages;

        struct
        {
            core::ManagedImage* PrimaryInput    = nullptr;
            core::ManagedImage* LinearZ         = nullptr;
            core::ManagedImage* MeshInstanceIdx = nullptr;
            core::ManagedImage* Normals         = nullptr;
        } mHistoryImages;

        core::ManagedImage* mPrimaryOutput = nullptr;

        CreateGradientSamplesStage mCreateGradientSamplesStage;
        ATrousGradientStage        mAtrousGradientStage;
        TemporalAccumulationStage  mTemporalAccumulationStage;
        EstimateVarianceStage      mEstimateVarianceStage;
        ATrousStage                mAtrousStage;
    };
}  // namespace foray::asvgf
