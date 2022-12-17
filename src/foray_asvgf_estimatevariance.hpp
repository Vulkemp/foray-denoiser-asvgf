#pragma once

#include <stages/foray_computestage.hpp>

namespace foray::asvgf {
    class ASvgfDenoiserStage;

    class EstimateVarianceStage : public foray::stages::ComputeStageBase
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
            // History Length Threshold
            float HistoryLengthThreshold = 4.0;
            uint32_t DebugMode;
        };

        virtual void ApiInitShader() override;
        virtual void ApiCreateDescriptorSet() override;
        virtual void ApiCreatePipelineLayout() override;
        virtual void ApiBeforeFrame(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo) override;
        virtual void ApiBeforeDispatch(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo, glm::uvec3& groupSize) override;
    };
}  // namespace foray::asvgf