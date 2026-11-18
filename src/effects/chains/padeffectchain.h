#pragma once

#include "effects/chains/pergroupeffectchain.h"

/// PadEffectChain is a per-stem effect chain used by the Pad FX banks.
///
/// Each chain is hardwired to one stem input and exposes four slots, one per
/// pad in the active bank. The chain runs in its own SignalProcessingStage so
/// Pad FX never share a pass with the deck Quick FX or the standard effect
/// units.
class PadEffectChain : public PerGroupEffectChain {
    Q_OBJECT
  public:
    PadEffectChain(const ChannelHandleAndGroup& handleAndGroup,
            EffectsManager* pEffectsManager,
            EffectsMessengerPointer pEffectsMessenger);

    static QString formatEffectChainGroup(const QString& group);
    static QString formatEffectSlotGroup(const QString& group,
            int iEffectSlotNumber);
};
