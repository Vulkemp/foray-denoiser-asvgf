#include "foray_asvgf_creategradientsamples.hpp"

namespace foray::asvgf {
    void CreateGradientSamplesStage::Init(core::ManagedImage* primaryInput,
                                          core::ManagedImage* seedTexture,
                                          core::ManagedImage* historyPrimaryInput,
                                          core::ManagedImage* gbufferLinearZ,
                                          core::ManagedImage* gbufferMeshInstanceIdx)
    {
        Destroy();
        mPrimaryInput           = primaryInput;
        mSeedTexture            = seedTexture;
        mHistoryPrimaryInput    = historyPrimaryInput;
        mGbufferLinearZ         = gbufferLinearZ;
        mGbufferMeshInstanceIdx = gbufferMeshInstanceIdx;
    }


    void CreateGradientSamplesStage::ApiInitShader(){
        mShader.LoadFromSource(mContext, SHADER_DIR "/creategradientsamples.comp");
    }
    void CreateGradientSamplesStage::ApiCreateDescriptorSetLayout(){}
    void CreateGradientSamplesStage::ApiCreatePipelineLayout(){}
    void CreateGradientSamplesStage::ApiBeforeFrame(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo){}
    void CreateGradientSamplesStage::ApiBeforeDispatch(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo, glm::uvec3& groupSize){}
}  // namespace foray::asvgf
