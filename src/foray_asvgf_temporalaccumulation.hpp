#pragma once

#include <stages/foray_computestage.hpp>

namespace foray::asvgf {
    class ASvgfDenoiserStage;

    class TemporalAccumulationStage : public foray::stages::ComputeStage
    {
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
            fp32_t MaxNormalDeviation = 0.05f;
            // Deviation multiplier (Default 1.0)
            fp32_t DepthDeviationToleranceMultiplier = 1.f;
            // Combined Weight Threshhold (Default 0.01)
            fp32_t WeightThreshhold = 0.01f;
            // Minimum weight assigned to new data
            fp32_t   MinNewDataWeight = 0.1f;
            // Set to true to enable use of historic data
            VkBool32 EnableHistory = VK_TRUE;
            // Set to a non-zero value to enable debug outputs
            uint32_t DebugOutputMode = 0;
        };


        virtual void ApiInitShader() override;
        virtual void ApiCreateDescriptorSet() override;
        virtual void ApiCreatePipelineLayout() override;
        virtual void ApiBeforeFrame(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo) override;
        virtual void ApiBeforeDispatch(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo, glm::uvec3& groupSize) override;
    };
}  // namespace foray::asvgf