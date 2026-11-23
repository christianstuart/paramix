#include "preferences/dialog/dlgprefbeats.h"

#include <algorithm>

#include "analyzer/analyzerbeats.h"
#include "defs_urls.h"
#include "moc_dlgprefbeats.cpp"
#include "paramix/analysis/bpmrange.h"

DlgPrefBeats::DlgPrefBeats(QWidget* parent, UserSettingsPointer pConfig)
        : DlgPreferencePage(parent),
          m_bpmSettings(pConfig),
          m_bAnalyzerEnabled(m_bpmSettings.getBpmDetectionEnabledDefault()),
          m_bFixedTempoEnabled(m_bpmSettings.getFixedTempoAssumptionDefault()),
          m_bFastAnalysisEnabled(m_bpmSettings.getFastAnalysisDefault()),
          m_bReanalyze(m_bpmSettings.getReanalyzeWhenSettingsChangeDefault()),
          m_bReanalyzeImported(m_bpmSettings.getReanalyzeImportedDefault()),
          m_bpmRangePreset(m_bpmSettings.getBpmRangePresetDefault()),
          m_bMatchExistingBpm(m_bpmSettings.getMatchExistingBpmRangeDefault()),
          m_stemStrategy(BeatDetectionSettings::StemStrategy::Disabled) {
    setupUi(this);

    m_availablePlugins = AnalyzerBeats::availablePlugins();
    for (const auto& info : std::as_const(m_availablePlugins)) {
        comboBoxBeatPlugin->addItem(info.name(), info.id());
    }

    for (int i = 0; i < paramix::analysis::kBpmRangePresetCount; ++i) {
        const auto range = paramix::analysis::kBpmRangePresets[i];
        const QString label = QString::fromUtf8(range.label);
        comboBoxBpmRange->addItem(range.automatic
                        ? tr("Automatic (%1 - %2 BPM)").arg(range.minBpm).arg(range.maxBpm)
                        : tr("%1 BPM").arg(label),
                i);
    }

    slotUpdate();

    // TODO (#13466) Keeping the setting hidden for now
    comboBoxStemStrategy->hide();
    labelStemStrategy->hide();

    // Connections
    connect(comboBoxBeatPlugin,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &DlgPrefBeats::pluginSelected);
    connect(checkBoxAnalyzerEnabled,
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
            &QCheckBox::checkStateChanged,
#else
            &QCheckBox::stateChanged,
#endif
            this,
            &DlgPrefBeats::analyzerEnabled);
    connect(checkBoxFixedTempo,
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
            &QCheckBox::checkStateChanged,
#else
            &QCheckBox::stateChanged,
#endif
            this,
            &DlgPrefBeats::fixedtempoEnabled);
    connect(checkBoxFastAnalysis,
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
            &QCheckBox::checkStateChanged,
#else
            &QCheckBox::stateChanged,
#endif
            this,
            &DlgPrefBeats::fastAnalysisEnabled);
    connect(checkBoxReanalyze,
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
            &QCheckBox::checkStateChanged,
#else
            &QCheckBox::stateChanged,
#endif
            this,
            &DlgPrefBeats::slotReanalyzeChanged);
    connect(checkBoxReanalyzeImported,
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
            &QCheckBox::checkStateChanged,
#else
            &QCheckBox::stateChanged,
#endif
            this,
            &DlgPrefBeats::slotReanalyzeImportedChanged);
    connect(comboBoxStemStrategy,
            &QComboBox::currentIndexChanged,
            this,
            &DlgPrefBeats::slotStemStrategyChanged);
    connect(comboBoxBpmRange,
            &QComboBox::currentIndexChanged,
            this,
            &DlgPrefBeats::slotBpmRangeChanged);
    connect(checkBoxMatchExistingBpm,
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
            &QCheckBox::checkStateChanged,
#else
            &QCheckBox::stateChanged,
#endif
            this,
            &DlgPrefBeats::slotMatchExistingBpmChanged);

    setScrollSafeGuard(comboBoxBeatPlugin);
    setScrollSafeGuard(comboBoxBpmRange);
}

DlgPrefBeats::~DlgPrefBeats() {
}

QUrl DlgPrefBeats::helpUrl() const {
    return QUrl(MIXXX_MANUAL_BEATS_URL);
}

void DlgPrefBeats::slotResetToDefaults() {
    if (m_availablePlugins.size() > 0) {
        m_selectedAnalyzerId = m_availablePlugins[0].id();
    }
    m_bAnalyzerEnabled = m_bpmSettings.getBpmDetectionEnabledDefault();
    m_bFixedTempoEnabled = m_bpmSettings.getFixedTempoAssumptionDefault();
    m_bFastAnalysisEnabled = m_bpmSettings.getFastAnalysisDefault();
    m_bReanalyze = m_bpmSettings.getReanalyzeWhenSettingsChangeDefault();
    m_bReanalyzeImported = m_bpmSettings.getReanalyzeImportedDefault();
    m_bpmRangePreset = m_bpmSettings.getBpmRangePresetDefault();
    m_bMatchExistingBpm = m_bpmSettings.getMatchExistingBpmRangeDefault();
    m_stemStrategy = m_bpmSettings.getStemStrategyDefault();

    updateGui();
}

void DlgPrefBeats::pluginSelected(int i) {
    if (i == -1) {
        return;
    }
    m_selectedAnalyzerId = m_availablePlugins[i].id();
    updateGui();
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
void DlgPrefBeats::analyzerEnabled(Qt::CheckState state) {
    m_bAnalyzerEnabled = (state == Qt::Checked);
#else
void DlgPrefBeats::analyzerEnabled(int i) {
    m_bAnalyzerEnabled = static_cast<bool>(i);
#endif
    updateGui();
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
void DlgPrefBeats::fixedtempoEnabled(Qt::CheckState state) {
    m_bFixedTempoEnabled = (state == Qt::Checked);
#else
void DlgPrefBeats::fixedtempoEnabled(int i) {
    m_bFixedTempoEnabled = static_cast<bool>(i);
#endif
    updateGui();
}

void DlgPrefBeats::slotUpdate() {
    // Read true values from config
    m_selectedAnalyzerId = m_bpmSettings.getBeatPluginId();
    m_bAnalyzerEnabled = m_bpmSettings.getBpmDetectionEnabled();
    m_bFixedTempoEnabled = m_bpmSettings.getFixedTempoAssumption();
    m_bReanalyze = m_bpmSettings.getReanalyzeWhenSettingsChange();
    m_bReanalyzeImported = m_bpmSettings.getReanalyzeImported();
    m_bFastAnalysisEnabled = m_bpmSettings.getFastAnalysis();
    m_bpmRangePreset = m_bpmSettings.getBpmRangePreset();
    m_bMatchExistingBpm = m_bpmSettings.getMatchExistingBpmRange();
    m_stemStrategy = m_bpmSettings.getStemStrategy();

    updateGui();
}

void DlgPrefBeats::updateGui() {
    checkBoxFixedTempo->setEnabled(m_bAnalyzerEnabled);
    comboBoxBeatPlugin->setEnabled(m_bAnalyzerEnabled);
    checkBoxAnalyzerEnabled->setChecked(m_bAnalyzerEnabled);
    // Fast analysis cannot be combined with non-constant tempo beatgrids.
    checkBoxFastAnalysis->setEnabled(m_bAnalyzerEnabled && m_bFixedTempoEnabled);
    checkBoxReanalyze->setEnabled(m_bAnalyzerEnabled);
    checkBoxReanalyzeImported->setEnabled(m_bAnalyzerEnabled);
    comboBoxBpmRange->setEnabled(m_bAnalyzerEnabled);
    checkBoxMatchExistingBpm->setEnabled(m_bAnalyzerEnabled);

    if (!m_bAnalyzerEnabled) {
        return;
    }

    if (m_availablePlugins.size() > 0) {
        bool found = false;
        for (int i = 0; i < m_availablePlugins.size(); ++i) {
            const auto& info = m_availablePlugins.at(i);
            if (info.id() == m_selectedAnalyzerId) {
                found = true;
                comboBoxBeatPlugin->setCurrentIndex(i);
                if (!m_availablePlugins[i].isConstantTempoSupported()) {
                    checkBoxFixedTempo->setEnabled(false);
                }
                break;
            }
        }
        if (!found) {
            comboBoxBeatPlugin->setCurrentIndex(0);
            m_selectedAnalyzerId = m_availablePlugins[0].id();
        }
    }

    checkBoxFixedTempo->setChecked(m_bFixedTempoEnabled);
    // Fast analysis cannot be combined with non-constant tempo beatgrids.
    checkBoxFastAnalysis->setChecked(m_bFastAnalysisEnabled && m_bFixedTempoEnabled);

    checkBoxReanalyze->setChecked(m_bReanalyze);
    checkBoxReanalyzeImported->setChecked(m_bReanalyzeImported);
    comboBoxBpmRange->setCurrentIndex(std::clamp(m_bpmRangePreset,
            0,
            paramix::analysis::kBpmRangePresetCount - 1));
    checkBoxMatchExistingBpm->setChecked(m_bMatchExistingBpm);

    comboBoxStemStrategy->setCurrentIndex(
            m_stemStrategy == BeatDetectionSettings::StemStrategy::Enforced
                    ? 1
                    : 0);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
void DlgPrefBeats::slotReanalyzeChanged(Qt::CheckState state) {
    m_bReanalyze = (state == Qt::Checked);
#else
void DlgPrefBeats::slotReanalyzeChanged(int value) {
    m_bReanalyze = static_cast<bool>(value);
#endif
    updateGui();
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
void DlgPrefBeats::slotReanalyzeImportedChanged(Qt::CheckState state) {
    m_bReanalyzeImported = (state == Qt::Checked);
#else
void DlgPrefBeats::slotReanalyzeImportedChanged(int value) {
    m_bReanalyzeImported = static_cast<bool>(value);
#endif
    updateGui();
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
void DlgPrefBeats::fastAnalysisEnabled(Qt::CheckState state) {
    m_bFastAnalysisEnabled = (state == Qt::Checked);
#else
void DlgPrefBeats::fastAnalysisEnabled(int i) {
    m_bFastAnalysisEnabled = static_cast<bool>(i);
#endif
    updateGui();
}

void DlgPrefBeats::slotStemStrategyChanged(int index) {
    switch (index) {
    case 1:
        m_stemStrategy = BeatDetectionSettings::StemStrategy::Enforced;
        break;
    default:
        m_stemStrategy = BeatDetectionSettings::StemStrategy::Disabled;
        break;
    }
    updateGui();
}

void DlgPrefBeats::slotApply() {
    m_bpmSettings.setBeatPluginId(m_selectedAnalyzerId);
    m_bpmSettings.setBpmDetectionEnabled(m_bAnalyzerEnabled);
    m_bpmSettings.setFixedTempoAssumption(m_bFixedTempoEnabled);
    m_bpmSettings.setReanalyzeWhenSettingsChange(m_bReanalyze);
    m_bpmSettings.setReanalyzeImported(m_bReanalyzeImported);
    m_bpmSettings.setFastAnalysis(m_bFastAnalysisEnabled);
    m_bpmSettings.setBpmRangePreset(m_bpmRangePreset);
    m_bpmSettings.setMatchExistingBpmRange(m_bMatchExistingBpm);
    m_bpmSettings.setStemStrategy(m_stemStrategy);
}

void DlgPrefBeats::slotBpmRangeChanged(int index) {
    const QVariant data = comboBoxBpmRange->itemData(index);
    if (data.isValid()) {
        m_bpmRangePreset = data.toInt();
    }
    updateGui();
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
void DlgPrefBeats::slotMatchExistingBpmChanged(Qt::CheckState state) {
    m_bMatchExistingBpm = (state == Qt::Checked);
#else
void DlgPrefBeats::slotMatchExistingBpmChanged(int value) {
    m_bMatchExistingBpm = static_cast<bool>(value);
#endif
    updateGui();
}
