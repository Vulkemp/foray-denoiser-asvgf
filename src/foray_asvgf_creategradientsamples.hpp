#pragma once

#include <stages/foray_computestage.hpp>

namespace foray::asvgf {
    class ASvgfDenoiserStage;

    class CreateGradientSamplesStage : public foray::stages::ComputeStage
    {
        public:
            void Init(ASvgfDenoiserStage* aSvgfStage);
            
            void UpdateDescriptorSet();
        protected:
        ASvgfDenoiserStage* mASvgfStage = nullptr;

        virtual void ApiInitShader() override;
        virtual void ApiCreateDescriptorSetLayout() override;
        virtual void ApiCreatePipelineLayout() override;
        virtual void ApiBeforeFrame(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo) override;
        virtual void ApiBeforeDispatch(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo, glm::uvec3& groupSize) override;
    };
}  // namespace foray::asvgf
