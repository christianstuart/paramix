#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QMenu>
#include <QObject>
#include <QStringList>

#include "preferences/configobject.h"

/// One learnable control of the controller mapping inventory. The entry mirrors
/// exactly what ControlPickerMenu registers for the classic learning wizard, so
/// both the wizard and the QML mapping UI present the same titles.
struct ControlPickerEntry {
    ConfigKey key;
    QString title;
    QString description;
    /// Translated top-level category, for example "Mixer".
    QString category;
    /// Full translated menu path, for example ["Mixer", "Equalizers"].
    QStringList categoryPath;
};

/// Flat list model of the learnable control inventory. This is the same data
/// ControlPickerMenu feeds into its submenus, exposed without the QMenu API so
/// that non-QtWidgets consumers (the QML controller mapping UI) can search it.
class ControlPickerModel : public QAbstractListModel {
    Q_OBJECT
  public:
    enum Roles {
        GroupRole = Qt::UserRole + 1,
        KeyRole,
        ConfigKeyRole,
        TitleRole,
        DescriptionRole,
        CategoryRole,
        CategoryPathRole,
    };
    Q_ENUM(Roles)

    explicit ControlPickerModel(QObject* pParent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE int count() const {
        return m_entries.size();
    }
    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE int rowForConfigKey(const QString& group, const QString& key) const;

    const QList<ControlPickerEntry>& entries() const {
        return m_entries;
    }
    void append(ControlPickerEntry entry);
    void clear();

    /// Builds the complete inventory without requiring the caller to own or
    /// show a QMenu. The returned model is owned by the caller (or by parent).
    static ControlPickerModel* createDetached(QObject* pParent = nullptr);

  private:
    QList<ControlPickerEntry> m_entries;
};

class ControlPickerMenu : public QMenu {
    Q_OBJECT
  public:
    ControlPickerMenu(QWidget* pParent);
    virtual ~ControlPickerMenu();

    const QList<ConfigKey>& controlsAvailable() const {
        return m_controlsAvailable;
    }

    /// The flat inventory backing this menu, in registration order.
    ControlPickerModel* controlModel() const {
        return m_pControlModel;
    }

    bool controlExists(const ConfigKey& key) const;
    QString descriptionForConfigKey(const ConfigKey& key) const;
    QString controlTitleForConfigKey(const ConfigKey& key) const;

    // share translated group strings
    QMap<QString, QString> getNumGroupsTrMap() const {
        return m_numGroupsTrMap;
    }
    QMap<QString, QString> getOtherGroupsTrMap() const {
        return m_otherGroupsTrMap;
    }

  signals:
    // Emitted when the user selects a control from the menu.
    void controlPicked(const ConfigKey& control);

  private slots:
    // Triggered when user selects a control from the menu.
    void controlChosen(int controlIndex);

  private:
    QStringList categoryPathForMenu(const QMenu* pMenu) const;

    QMenu* addSubmenu(QString title, QMenu* pParent = NULL);
    void addSingleControl(const QString& group,
            const QString& control,
            const QString& title,
            const QString& description,
            QMenu* pMenu,
            const QString& prefix = QString(),
            const QString& actionTitle = QString());
    void addControl(const QString& group,
            const QString& control,
            const QString& title,
            const QString& helpText,
            QMenu* pMenu,
            bool addReset = false,
            const QString& prefix = QString());
    void addPlayerControl(const QString& control,
            const QString& title,
            const QString& helpText,
            QMenu* pMenu,
            bool deckControls,
#ifdef __STEM__
            bool deckStemControls,
#endif
            bool samplerControls,
            bool previewdeckControls,
            bool addReset = false);
    void addDeckAndSamplerControl(const QString& control,
            const QString& title,
            const QString& helpText,
            QMenu* pMenu,
#ifdef __STEM__
            bool stemsControls = false,
#endif
            bool addReset = false);
    void addDeckAndPreviewDeckControl(const QString& control,
            const QString& title,
            const QString& helpText,
            QMenu* pMenu,
#ifdef __STEM__
            bool stemsControls = false,
#endif
            bool addReset = false);
    void addDeckAndSamplerAndPreviewDeckControl(const QString& control,
            const QString& title,
            const QString& helpText,
            QMenu* pMenu,
#ifdef __STEM__
            bool stemsControls = false,
#endif
            bool addReset = false);
    void addDeckControl(const QString& control,
            const QString& title,
            const QString& helpText,
            QMenu* pMenu,
#ifdef __STEM__
            bool stemsControls = false,
#endif
            bool addReset = false);
    void addSamplerControl(const QString& control,
            const QString& title,
            const QString& helpText,
            QMenu* pMenu,
            bool addReset = false);
    void addPreviewDeckControl(const QString& control,
            const QString& title,
            const QString& helpText,
            QMenu* pMenu,
            bool addReset = false);
    void addMicrophoneAndAuxControl(const QString& control,
            const QString& title,
            const QString& helpText,
            QMenu* pMenu,
            bool microphoneControls,
            bool auxControls,
            bool addReset = false);
    void addLibraryControl(const QString& control,
            const QString& title,
            const QString& helpText,
            QMenu* pMenu);

    int addAvailableControl(const ConfigKey& key, const QString& title, const QString& description);

    QString m_effectMainOutputStr;
    QString m_effectHeadphoneOutputStr;
    QString m_deckStr;
    QString m_deckStemStr;
    QString m_previewdeckStr;
    QString m_samplerStr;
    QString m_resetStr;
    QString m_microphoneStr;
    QString m_auxStr;
    QString m_effectRackStr;
    QString m_effectUnitStr;
    QString m_effectStr;
    QString m_parameterStr;
    QString m_buttonParameterStr;
    QString m_libraryStr;

    QList<ConfigKey> m_controlsAvailable;
    QHash<ConfigKey, QString> m_descriptionsByKey;
    QHash<ConfigKey, QString> m_titlesByKey;
    ControlPickerModel* m_pControlModel;

    QMap<QString, QString> m_numGroupsTrMap;
    QMap<QString, QString> m_otherGroupsTrMap;
};
