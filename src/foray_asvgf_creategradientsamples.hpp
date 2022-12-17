#pragma once

#include <stages/foray_computestage.hpp>

namespace foray::asvgf {
    class ASvgfDenoiserStage;

    class CreateGradientSamplesStage : public foray::stages::ComputeStageBase
    {
        public:
            void Init(ASvgfDenoiserStage* aSvgfStage);
            
            void UpdateDescriptorSet();
        protected:
        ASvgfDenoiserStage* mASvgfStage = nullptr;

        struct PushConstant{
            uint32_t FrameIdx;
            uint32_t DebugMode;
        };

        virtual void ApiInitShader() override;
        virtual void ApiCreateDescriptorSet() override;
        virtual void ApiCreatePipelineLayout() override;
        virtual void ApiBeforeFrame(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo) override;
        virtual void ApiBeforeDispatch(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo, glm::uvec3& groupSize) override;
    };
}  // namespace foray::asvgf
