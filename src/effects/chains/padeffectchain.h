#pragma once

#include "effects/chains/pergroupeffectchain.h"

/// PadEffectChain is the effect chain used by the Pad FX banks.
///
/// Each chain is hardwired to one input (a deck or one of its stems) and
/// exposes eight slots, one per pad in the active bank. The chain runs in its
/// own SignalProcessingStage so Pad FX never share a pass with the deck Quick
/// FX or the standard effect units.
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
