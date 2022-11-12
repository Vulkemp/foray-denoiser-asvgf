#pragma once

#include <stages/foray_computestage.hpp>

namespace foray::asvgf {
    class ASvgfDenoiserStage;

    class ATrousStage : public foray::stages::ComputeStage
    {
        friend ASvgfDenoiserStage;

      public:
        void Init(ASvgfDenoiserStage* aSvgfStage);

        void UpdateDescriptorSet();

      protected:
        ASvgfDenoiserStage* mASvgfStage = nullptr;

        struct PushConstant
        {
            // Iteration Count
            uint32_t IterationCount = 0;
            // Read array index
            uint32_t ReadIdx = 0;
            // Kernel used for filtering
            uint32_t UsedKernel = 0;
        } mPushC;

        virtual void ApiInitShader() override;
        virtual void ApiCreateDescriptorSet() override;
        virtual void ApiCreatePipelineLayout() override;
        virtual void ApiBeforeFrame(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo) override;
        virtual void ApiBeforeDispatch(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo, glm::uvec3& groupSize) override;
    };
}  // namespace foray::asvgf