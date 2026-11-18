#include "preferences/dialog/dlgprefstems.h"

#include <QDir>
#include <QFileDialog>

#include "moc_dlgprefstems.cpp"
#include "preferences/configobject.h"
#include "stems/stemgpudetect.h"

namespace {
constexpr char kGroup[] = "[Stems]";
constexpr char kOutputDirectoryKey[] = "OutputDirectory";
constexpr char kModelPathKey[] = "ModelPath";
} // namespace

DlgPrefStems::DlgPrefStems(QWidget* pParent, UserSettingsPointer pConfig)
        : DlgPreferencePage(pParent),
          m_pConfig(std::move(pConfig)) {
    setupUi(this);
    connect(pushButtonBrowseOutput,
            &QPushButton::clicked,
            this,
            &DlgPrefStems::slotBrowseOutput);
    connect(pushButtonBrowseModel,
            &QPushButton::clicked,
            this,
            &DlgPrefStems::slotBrowseModel);
    connect(comboBoxFormat,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &DlgPrefStems::slotFormatChanged);
    setScrollSafeGuard(comboBoxFormat);

    slotUpdate();
}

DlgPrefStems::~DlgPrefStems() = default;

QUrl DlgPrefStems::helpUrl() const {
    return QUrl();
}

void DlgPrefStems::slotUpdate() {
    m_settings = mixxx::stems::readStemExportSettings(m_pConfig);
    m_outputDirectory = m_pConfig->getValue(ConfigKey(kGroup, kOutputDirectoryKey),
            QDir(m_pConfig->getSettingsPath()).filePath(QStringLiteral("Stems")));
    m_modelPath = m_pConfig->getValue(ConfigKey(kGroup, kModelPathKey), QString())
                          .trimmed();
    m_gpuAcceleration = mixxx::stems::isGpuAccelerationEnabled(m_pConfig);
    updateGui();
}

void DlgPrefStems::slotApply() {
    m_outputDirectory = lineEditOutputDirectory->text().trimmed();
    if (m_outputDirectory.isEmpty()) {
        m_outputDirectory = QDir(m_pConfig->getSettingsPath()).filePath(QStringLiteral("Stems"));
    }
    mixxx::stems::writeStemExportSettings(m_pConfig, m_settings);
    m_modelPath = lineEditModelPath->text().trimmed();
    m_pConfig->setValue(ConfigKey(kGroup, kOutputDirectoryKey), m_outputDirectory);
    if (m_modelPath.isEmpty()) {
        m_pConfig->remove(ConfigKey(kGroup, kModelPathKey));
    } else {
        m_pConfig->setValue(ConfigKey(kGroup, kModelPathKey), m_modelPath);
    }
    mixxx::stems::setGpuAccelerationEnabled(m_pConfig, m_gpuAcceleration);
}

void DlgPrefStems::slotResetToDefaults() {
    m_settings = mixxx::stems::StemExportSettings();
    m_outputDirectory = QDir(m_pConfig->getSettingsPath()).filePath(QStringLiteral("Stems"));
    m_modelPath.clear();
    m_gpuAcceleration = mixxx::stems::gpuAccelerationDefault();
    updateGui();
}

void DlgPrefStems::slotBrowseOutput() {
    const QString path = QFileDialog::getExistingDirectory(this,
            tr("Select Stem Files Folder"),
            m_outputDirectory);
    if (path.isEmpty()) {
        return;
    }
    m_outputDirectory = path;
    updateGui();
}

void DlgPrefStems::slotBrowseModel() {
    const QString path = QFileDialog::getOpenFileName(this,
            tr("Select Stem Separation Model"),
            m_modelPath,
            tr("ONNX model (*.onnx);;All files (*)"));
    if (path.isEmpty()) {
        return;
    }
    m_modelPath = path;
    updateGui();
}

void DlgPrefStems::slotFormatChanged(int index) {
    Q_UNUSED(index);
    m_settings.format = mixxx::stems::StemFileFormat::Flac;
    updateGui();
}

void DlgPrefStems::updateGui() {
    comboBoxFormat->setCurrentIndex(0);
    lineEditOutputDirectory->setText(m_outputDirectory);
    lineEditModelPath->setText(m_modelPath);
    checkBoxGpuAcceleration->setChecked(m_gpuAcceleration);
}
