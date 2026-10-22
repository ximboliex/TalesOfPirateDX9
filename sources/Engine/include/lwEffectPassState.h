#pragma once
#include <stdint.h>

// D3DX9 requires Begin/BeginPass/EndPass/End to be balanced even when committing
// parameters or ending a pass fails. Keep this state independent of the device
// so the failure paths can be exercised without a graphics driver.
class lwEffectPassState
{
    bool begun_ = false;
    bool pass_ = false;
public:
    template<class Effect> bool End(Effect* effect)
    {
        if (!effect || !begun_) return false;
        const bool pass_ok = !pass_ || effect->EndPass() >= 0;
        const bool end_ok = effect->End() >= 0;
        pass_ = begun_ = false;
        return pass_ok && end_ok;
    }
    template<class Effect> bool Begin(Effect* effect, uint32_t flags, uint32_t& passes)
    {
        if (!effect) return false;
        if (begun_ && !End(effect)) return false;
        passes = 0;
        begun_ = effect->Begin(&passes, flags) >= 0;
        return begun_;
    }
    template<class Effect> bool Pass(Effect* effect, uint32_t index, uint32_t passes)
    {
        if (!effect || !begun_ || index >= passes) return false;
        if (pass_)
        {
            pass_ = false;
            if (effect->EndPass() < 0) return false;
        }
        if (effect->BeginPass(index) < 0) return false;
        pass_ = true;
        return effect->CommitChanges() >= 0;
    }
};
