#pragma once

#include "foray_asvgf_atrous.hpp"
#include "foray_asvgf_atrousgradient.hpp"
#include "foray_asvgf_creategradientsamples.hpp"
#include "foray_asvgf_estimatevariance.hpp"
#include "foray_asvgf_temporalaccumulation.hpp"
#include <stages/foray_denoiserstage.hpp>
#include <util/foray_historyimage.hpp>
#include "shaders/debug.glsl.h"

namespace foray::asvgf {
    class ASvgfDenoiserStage : public foray::stages::DenoiserStage
    {
        friend CreateGradientSamplesStage;
        friend ATrousGradientStage;
        friend TemporalAccumulationStage;
        friend EstimateVarianceStage;
        friend ATrousStage;

      public:
        virtual void Init(core::Context* context, const stages::DenoiserConfig& config) override;

        virtual void RecordFrame(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo) override;

        virtual std::string GetUILabel() override { return "ASVGF Denoiser"; }

        virtual void DisplayImguiConfiguration() override;

        virtual void IgnoreHistoryNextFrame() override;

        virtual void Resize(const VkExtent2D& extent) override;

        virtual void OnShadersRecompiled() override;

        virtual void Destroy() override;

        FORAY_PROPERTY_V(DebugMode)

      protected:
        struct
        {
            core::ManagedImage* PrimaryInput    = nullptr;
            core::ManagedImage* Albedo          = nullptr;
            core::ManagedImage* Normal          = nullptr;
            core::ManagedImage* LinearZ         = nullptr;
            core::ManagedImage* Motion          = nullptr;
            core::ManagedImage* MeshInstanceIdx = nullptr;
        } mInputs;

        struct
        {
            core::ManagedImage LuminanceMaxDiff;
            core::ManagedImage MomentsAndLinearZ;
            core::ManagedImage Seed;
            uint32_t LastArrayWriteIdx = 0;
        } mASvgfImages;

        struct
        {
            util::HistoryImage PrimaryInput;
            util::HistoryImage LinearZ;
            util::HistoryImage MeshInstanceIdx;
            util::HistoryImage Normal;
            bool Valid = false;
        } mHistoryImages;

        struct
        {
            core::ManagedImage Color;
            core::ManagedImage Moments;
            core::ManagedImage HistoryLength;
            uint32_t LastArrayWriteIdx = 0;
        } mAccumulatedImages;

        core::ManagedImage mATrousImage;
        uint32_t mATrousLastArrayWriteIdx = 0;
        
        core::ManagedImage* mPrimaryOutput = nullptr;
        uint32_t mDebugMode = DEBUG_NONE;

        bench::DeviceBenchmark* mBenchmark = nullptr;

        CreateGradientSamplesStage mCreateGradientSamplesStage;
        ATrousGradientStage        mAtrousGradientStage;
        TemporalAccumulationStage  mTemporalAccumulationStage;
        EstimateVarianceStage      mEstimateVarianceStage;
        ATrousStage                mAtrousStage;

        inline static const char* TIMESTAMP_CreateGradientSamples = "CreateGradientSamples";
        inline static const char* TIMESTAMP_ATrousGradient = "ATrousGradient";
        inline static const char* TIMESTAMP_TemporalAccumulation = "TemporalAccumulation";
        inline static const char* TIMESTAMP_EstimateVariance = "EstimateVariance";
        inline static const char* TIMESTAMP_ATrousColor = "ATrousColor";

        VkExtent2D CalculateStrataCount(const VkExtent2D& extent);

        void CopyToHistory(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo);
    };
}  // namespace foray::asvgf
