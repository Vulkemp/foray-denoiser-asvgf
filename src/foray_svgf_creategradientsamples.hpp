#pragma once

#include <stages/foray_computestage.hpp>

namespace foray::svgf {
    class CreateGradientSamplesStage : public foray::stages::ComputeStage
    {
        public:
            void Init(core::ManagedImage* primaryInput, core::ManagedImage* seedTexture, core::ManagedImage* historyPrimaryInput, core::ManagedImage* gbufferLinearZ, core::ManagedImage* gbufferMeshInstanceIdx);
            
        protected:
        core::ManagedImage* mPrimaryInput = nullptr;
        core::ManagedImage* mSeedTexture = nullptr;
        core::ManagedImage* mHistoryPrimaryInput = nullptr;
        core::ManagedImage* mGbufferLinearZ = nullptr;
        core::ManagedImage* mGbufferMeshInstanceIdx = nullptr;

        virtual void ApiInitShader() override;
        virtual void ApiCreateDescriptorSetLayout() override;
        virtual void ApiCreatePipelineLayout() override;
        virtual void ApiBeforeFrame(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo) override;
        virtual void ApiBeforeDispatch(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo, glm::uvec3& groupSize) override;
    };
}  // namespace foray::svgf
