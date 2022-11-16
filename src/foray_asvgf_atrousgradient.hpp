#pragma once

#include <stages/foray_computestage.hpp>

namespace foray::asvgf {
    class ASvgfDenoiserStage;

    class ATrousGradientStage : public foray::stages::ComputeStage
    {
        friend ASvgfDenoiserStage;
        public:
            void Init(ASvgfDenoiserStage* aSvgfStage);

            void RecordFrame(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo) override;
            
            void UpdateDescriptorSet();
        protected:
        ASvgfDenoiserStage* mASvgfStage = nullptr;

        struct PushConstant{
            uint32_t IterationIdx;
            uint32_t ReadIdx;
            uint32_t WriteIdx;
            uint32_t DebugMode = 0;
        } mPushC;

        uint32_t mIterationCount = 5;

        virtual void ApiInitShader() override;
        virtual void ApiCreateDescriptorSet() override;
        virtual void ApiCreatePipelineLayout() override;

        virtual void BeforeIteration(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo);
        virtual void DispatchIteration(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo);
    };
}  // namespace foray::asvgf