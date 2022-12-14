#pragma once

#include <stages/foray_computestage.hpp>

namespace foray::asvgf {
    class ASvgfDenoiserStage;

    class TemporalAccumulationStage : public foray::stages::ComputeStageBase
    {
        friend ASvgfDenoiserStage;
      public:
        void Init(ASvgfDenoiserStage* aSvgfStage);

        void UpdateDescriptorSet();

      protected:
        ASvgfDenoiserStage* mASvgfStage = nullptr;

        struct PushConstant
        {
            // Read array index
            uint32_t ReadIdx = 0;
            // Write array index
            uint32_t WriteIdx = 1;
            // Maximum deviation of the sinus of previous and current normal (Default 0.05)
            fp32_t MaxNormalDeviation = 0.25881963644f;
            // Deviation multiplier (Default 1.0)
            fp32_t MaxPositionDifference = 0.25f;
            // Combined Weight Threshhold (Default 0.01)
            fp32_t WeightThreshhold = 0.01f;
            // Minimum weight assigned to new data
            fp32_t MinNewDataWeight = 0.1f;
            // Multiplier to apply to the antilag alpha (higher values make antilag more agressive, Default 1.0)
            fp32_t AntilagMultiplier = 1.f;
            // Array ReadIdx of LuminanceMaxDiffTex
            uint32_t LuminanceMaxDiffReadIdx;
            // Set to true to enable use of historic data
            VkBool32 EnableHistory = VK_TRUE;
            // Set to a non-zero value to enable debug outputs
            uint32_t DebugOutputMode = 0;
        } mPushC;


        virtual void ApiInitShader() override;
        virtual void ApiCreateDescriptorSet() override;
        virtual void ApiCreatePipelineLayout() override;
        virtual void ApiBeforeFrame(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo) override;
        virtual void ApiBeforeDispatch(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo, glm::uvec3& groupSize) override;
    };
}  // namespace foray::asvgf