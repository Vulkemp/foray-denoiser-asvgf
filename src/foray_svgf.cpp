#include "foray_svgf.hpp"

namespace foray::svgf {
    void SvgfDenoiserStage::Init(core::Context* context, const stages::DenoiserConfig& config)
    {
        mAlbedoInput = config.AlbedoInput;
        Assert(!!mAlbedoInput);
        mNormalInput = config.NormalInput;
        Assert(!!mNormalInput);
        mMotionInput = config.MotionInput;
        Assert(!!mMotionInput);
        mPrimaryOutput = config.PrimaryOutput;
        Assert(!!mPrimaryOutput);

        auto iter = config.AuxiliaryInputs.find(INPUT_DIRECT);
        Assert(iter != config.AuxiliaryInputs.end());
        mDirectLightInput = iter->second;

        iter = config.AuxiliaryInputs.find(INPUT_INDIRECT);
        Assert(iter != config.AuxiliaryInputs.end());
        mIndirectLightInput = iter->second;
    }

    void SvgfDenoiserStage::DisplayImguiConfiguration() {}

    void SvgfDenoiserStage::IgnoreHistoryNextFrame() {}

}  // namespace foray::svgf
