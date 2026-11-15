#pragma once

#include <QDataStream>
#include <QFile>
#include <vector>

#include "audio/types.h"
#include "control/pollingcontrolproxy.h"
#include "encoder/encoder.h"
#include "encoder/encodercallback.h"
#include "encoder/encodersettings.h"
#include "engine/enginevumeter.h"
#include "engine/sidechain/sidechainworker.h"
#include "preferences/usersettings.h"
#include "track/track_decl.h"

class ControlProxy;

class EngineRecord : public QObject, public EncoderCallback, public SideChainWorker {
    Q_OBJECT
  public:
    EngineRecord(UserSettingsPointer pConfig);
    ~EngineRecord() override;

    void process(const CSAMPLE* pBuffer, const std::size_t bufferSize) override;
    void shutdown() override {
    }

    // writes compressed audio to file
    void write(const unsigned char* header, const unsigned char* body, int headerLen, int bodyLen) override;
    // gets stream position
    int tell() override;
    // sets stream position
    void seek(int pos) override;
    // gets stream length
    int filelen() override;

    // creates or opens an audio file
    bool openFile();
    // closes the audio file
    void closeFile();
    int updateFromPreferences();
    bool fileOpen();
    bool openCueFile();
    void closeCueFile();

  signals:
    // emitted to notify RecordingManager
    void bytesRecorded(int bytes);

    // Emitted when recording state changes. 'recording' represents whether
    // recording is active and 'error' is true if an error occurred. Currently
    // only one error can occur: the specified file was unable to be opened for
    // writing.
    void isRecording(bool recording, bool error);
    void durationRecorded(quint64 durationInt);

  private:
    int getActiveTracks();
    // Check if the metadata has changed since the previous check. We also check
    // when was the last check performed to avoid using too much CPU and as well
    // to avoid changing the metadata during scratches.
    bool metaDataHasChanged();

    void writeCueLine();

    UserSettingsPointer m_pConfig;
    EncoderPointer m_pEncoder;
    QString m_encoding;
    QString m_fileName;
    QString m_baTitle;
    QString m_baAuthor;
    QString m_baAlbum;

    QFile m_file;
    QFile m_cueFile;
    QDataStream m_dataStream;

    PollingControlProxy m_sampleRateControl;
    ControlProxy* m_pRecReady;
    // Recording gain (trim) applied to the audio before the VU meter and encoding.
    ControlProxy* m_pRecGain;
    // Live level meter for the unfiltered/trimmed recording signal.
    EngineVuMeter m_vuMeter;
    // Trimmed copy of the incoming buffer, reused between callbacks.
    std::vector<CSAMPLE> m_gainBuffer;
    quint64 m_frames;
    mixxx::audio::SampleRate m_sampleRate;
    quint64 m_recordedDuration;
    QString getRecordedDurationStr();

    int m_iMetaDataLife;
    TrackPointer m_pCurrentTrack;

    QString m_cueFileName;
    quint64 m_cueTrack;
    // Mono/stereo recording, captured once per recording from channel_mode.
    bool m_recordMono;
    bool m_bCueIsEnabled;
    bool m_bTracklistAsCommentEnabled;
    bool m_bCueUsesFileAnnotation;
};
