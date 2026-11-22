#include "effects/chains/padeffectchain.h"

#include "effects/effectslot.h"
#include "effects/padfx/padfxdefs.h"
#include "moc_padeffectchain.cpp"

PadEffectChain::PadEffectChain(const ChannelHandleAndGroup& handleAndGroup,
        EffectsManager* pEffectsManager,
        EffectsMessengerPointer pEffectsMessenger)
        : PerGroupEffectChain(handleAndGroup,
                  formatEffectChainGroup(handleAndGroup.name()),
                  SignalProcessingStage::PadFx,
                  pEffectsManager,
                  pEffectsMessenger,
                  true) {
    for (int i = 0; i < mixxx::padfx::kNumPadsPerBank; ++i) {
        addEffectSlot(formatEffectSlotGroup(handleAndGroup.name(), i));
        // Pad slots are silent until the pad control layer loads an effect
        // and explicitly enables it.
        m_effectSlots.at(i)->setEnabled(false);
    }
    enableForInputChannel(handleAndGroup);
}

// static
QString PadEffectChain::formatEffectChainGroup(const QString& group) {
    return QStringLiteral("[PadEffectRack1_%1]").arg(group);
}

// static
QString PadEffectChain::formatEffectSlotGroup(const QString& group,
        int iEffectSlotNumber) {
    return QStringLiteral("[PadEffectRack1_%1_Effect%2]")
            .arg(group, QString::number(iEffectSlotNumber + 1));
}
