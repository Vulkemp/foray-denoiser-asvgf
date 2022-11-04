#include "foray_asvgf.hpp"

namespace foray::asvgf {
    void ASvgfDenoiserStage::Init(core::Context* context, const stages::DenoiserConfig& config)
    {
        mInputs.AlbedoInput = config.GBufferOutputs[(size_t)stages::GBufferStage::EOutput::Albedo];
        Assert(!!mInputs.AlbedoInput);
        mInputs.NormalInput = config.GBufferOutputs[(size_t)stages::GBufferStage::EOutput::Normal];
        Assert(!!mInputs.NormalInput);
        mInputs.MotionInput = config.GBufferOutputs[(size_t)stages::GBufferStage::EOutput::Motion];
        Assert(!!mInputs.MotionInput);
        mInputs.PrimaryInput = config.PrimaryInput;
        Assert(!!mInputs.PrimaryInput);
        mPrimaryOutput = config.PrimaryOutput;
        Assert(!!mPrimaryOutput);

        // Noise Source Inputs
        // 
    }

    void ASvgfDenoiserStage::DisplayImguiConfiguration() {}

    void ASvgfDenoiserStage::IgnoreHistoryNextFrame() {}

}  // namespace foray::asvgf
