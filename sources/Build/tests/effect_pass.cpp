#include <cassert>
#include <string>
#include "lwEffectPassState.h"

struct Effect
{
    std::string calls;
    int begin = 0, beginPass = 0, commit = 0, endPass = 0, end = 0;
    int Begin(uint32_t* passes, uint32_t) { calls += 'B'; *passes = 2; return begin; }
    int BeginPass(uint32_t) { calls += 'P'; return beginPass; }
    int CommitChanges() { calls += 'C'; return commit; }
    int EndPass() { calls += 'p'; return endPass; }
    int End() { calls += 'E'; return end; }
};

int main()
{
    uint32_t passes = 0;
    {
        Effect effect;
        lwEffectPassState state;
        assert(!state.Pass(&effect, 0, 2));
        assert(state.Begin(&effect, 0, passes) && passes == 2);
        assert(!state.Pass(&effect, 2, passes));
        assert(state.Pass(&effect, 0, passes));
        assert(state.Pass(&effect, 1, passes));
        assert(state.End(&effect));
        assert(effect.calls == "BPCpPCpE");
        assert(!state.End(&effect));
    }
    {
        Effect effect;
        lwEffectPassState state;
        effect.commit = -1;
        assert(state.Begin(&effect, 0, passes));
        assert(!state.Pass(&effect, 0, passes));
        effect.endPass = -1;
        assert(!state.End(&effect));
        assert(effect.calls == "BPCpE"); // End must run even when EndPass fails.
        effect.commit = effect.endPass = 0;
        assert(state.Begin(&effect, 0, passes));
        assert(state.End(&effect));
    }
    {
        Effect effect;
        lwEffectPassState state;
        effect.begin = -1;
        assert(!state.Begin(&effect, 0, passes));
        assert(!state.End(&effect) && effect.calls == "B");
        effect.begin = 0;
        effect.beginPass = -1;
        assert(state.Begin(&effect, 0, passes));
        assert(!state.Pass(&effect, 0, passes));
        assert(state.End(&effect));
        assert(effect.calls == "BBPE");
    }
}
