#pragma once

#include <stages/foray_computestage.hpp>

namespace foray::asvgf {
    class ASvgfDenoiserStage;

    class ATrousStage : public foray::stages::ComputeStage
    {
        friend ASvgfDenoiserStage;

      public:
        void Init(ASvgfDenoiserStage* aSvgfStage);

        void RecordFrame(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo) override;

        void UpdateDescriptorSet();

      protected:
        ASvgfDenoiserStage* mASvgfStage = nullptr;

        struct PushConstant
        {
            // Iteration Count
            uint32_t IterationIdx = 0;
            // Read array index
            uint32_t ReadIdx = 0;
            uint32_t WriteIdx = 0;
            VkBool32 LastIteration = 0;
            // Kernel used for filtering
            uint32_t UsedKernel = 0;
            uint32_t DebugMode  = 0;
        } mPushC;

        uint32_t mIterationCount = 5;

        virtual void ApiInitShader() override;
        virtual void ApiCreateDescriptorSet() override;
        virtual void ApiCreatePipelineLayout() override;

        virtual void BeforeIteration(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo);
        virtual void DispatchIteration(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo);
    };
}  // namespace foray::asvgf