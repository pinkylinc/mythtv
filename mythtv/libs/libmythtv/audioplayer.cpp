#include "mythplayer.h"
#include "audiooutput.h"
#include "audioplayer.h"

#define LOC QString("AudioPlayer: ")

static const QString _Location = AudioPlayer::tr("Audio Player");

AudioPlayer::AudioPlayer(MythPlayer *parent, bool muted)
  : m_parent(parent),     m_audioOutputMain(NULL),    m_audioOutputSecondary(NULL),   m_channels(-1),
    m_orig_channels(-1),  m_codec(0),              m_format(FORMAT_NONE),
    m_samplerate(44100),  m_codec_profile(0),
    m_stretchfactor(1.0f), m_timeadj(0.0f), m_passthru(false),
    m_lock(QMutex::Recursive), m_muted_on_creation(muted),
    m_main_device(QString::null), m_copy_device(QString::null), m_passthru_device(QString::null),
    m_no_audio_in(false), m_no_audio_out(true), m_controls_volume(true)
{
    m_controls_volume = gCoreContext->GetNumSetting("MythControlsVolume", 1);
}

AudioPlayer::~AudioPlayer()
{
    DeleteOutput();
    m_visuals.clear();

    //REMOVE ALL THE AUDIO SETTINGS VECTOR ITEMS
    for (uint i = 0; i < m_aoslist.size(); i++)
    {
        delete m_aoslist[i];
    }

    m_aoslist.clear();


}

void AudioPlayer::addVisual(MythTV::Visual *vis)
{
    if (!m_audioOutputMain)
        return;

    QMutexLocker lock(&m_lock);
    Visuals::iterator it = std::find(m_visuals.begin(), m_visuals.end(), vis);
    if (it == m_visuals.end())
    {
        m_visuals.push_back(vis);
        m_audioOutputMain->addVisual(vis);
    }
}

void AudioPlayer::removeVisual(MythTV::Visual *vis)
{
    if (!m_audioOutputMain)
        return;

    QMutexLocker lock(&m_lock);
    Visuals::iterator it = std::find(m_visuals.begin(), m_visuals.end(), vis);
    if (it != m_visuals.end())
    {
        m_visuals.erase(it);
        m_audioOutputMain->removeVisual(vis);
    }
}

void AudioPlayer::AddVisuals(void)
{
    if (!m_audioOutputMain)
        return;

    QMutexLocker lock(&m_lock);
    for (uint i = 0; i < m_visuals.size(); i++)
        m_audioOutputMain->addVisual(m_visuals[i]);
}

void AudioPlayer::RemoveVisuals(void)
{
    if (!m_audioOutputMain)
        return;

    QMutexLocker lock(&m_lock);
    for (uint i = 0; i < m_visuals.size(); i++)
        m_audioOutputMain->removeVisual(m_visuals[i]);
}

void AudioPlayer::ResetVisuals(void)
{
    if (!m_audioOutputMain)
        return;

    QMutexLocker lock(&m_lock);
    for (uint i = 0; i < m_visuals.size(); i++)
        m_visuals[i]->prepare();
}

void AudioPlayer::Reset(void)
{
    QMutexLocker lock(&m_lock);
    if (m_audioOutputMain)
        m_audioOutputMain->Reset();
    if (m_audioOutputSecondary)
        m_audioOutputSecondary->Reset();
}

void AudioPlayer::DeleteOutput(void)
{
    RemoveVisuals();
    QMutexLocker locker(&m_lock);
    if (m_audioOutputMain)
    {
        delete m_audioOutputMain;
        m_audioOutputMain = NULL;
    }
    if (m_audioOutputSecondary)
    {
        delete m_audioOutputSecondary;
        m_audioOutputSecondary = NULL;
    }
    m_no_audio_out = true;
}

QString AudioPlayer::ReinitAudio(void)
{
    bool want_audio = m_parent->IsAudioNeeded();
    QString errMsg = QString::null;
    QMutexLocker lock(&m_lock);

    if ((m_format == FORMAT_NONE) ||
        (m_channels <= 0) ||
        (m_samplerate <= 0))
    {
        m_no_audio_in = m_no_audio_out = true;
    }
    else
        m_no_audio_in = false;

    if (want_audio && !m_audioOutputMain)
    {
        /*
        // AudioOutput has never been created and we will want audio
        AudioSettings aos1 = AudioSettings(m_main_device,
                                          m_passthru_device,
                                          m_format, m_channels,
                                          m_codec, m_samplerate,
                                          AUDIOOUTPUT_VIDEO,
                                          m_controls_volume, m_passthru);

        if (m_no_audio_in)
            aos1.init = false;
        */


        if(m_aoslist.size() > 0)
        {
            AudioSettings* aos = m_aoslist[0];

            if (m_no_audio_in)
                aos->init = false;

            m_audioOutputMain = AudioOutput::OpenAudio(*aos);
        }

        if (!m_audioOutputMain)
        {
            errMsg = tr("Unable to create AudioOutput.");
        }
        else
        {
            errMsg = m_audioOutputMain->GetError();
        }
        AddVisuals();
    }
    else if (!m_no_audio_in && m_audioOutputMain)
    {
        //const AudioSettings settings(m_format, m_channels, m_codec,
        //                             m_samplerate, m_passthru, 0,
        //                             m_codec_profile);
        //m_audioOutputMain->Reconfigure(settings);
        if(m_aoslist.size() > 0)
        {
            AudioSettings* aos = m_aoslist[0];
            m_audioOutputMain->Reconfigure(aos);
        }

        errMsg = m_audioOutputMain->GetError();
        SetStretchFactor(m_stretchfactor);
    }

    //================
/*
    if (want_audio && !m_audioOutputSecondary)
    {
        // AudioOutput has never been created and we will want audio
        AudioSettings aos1 = AudioSettings(m_copy_device,
                                          m_passthru_device,
                                          m_format, m_channels,
                                          m_codec, m_samplerate,
                                          AUDIOOUTPUT_VIDEO,
                                          m_controls_volume, m_passthru);

        if (m_no_audio_in)
            aos1.init = false;

        m_audioOutputSecondary = AudioOutput::OpenAudio(aos1);
        if (!m_audioOutputSecondary)
        {
            errMsg = tr("Unable to create AudioOutput.");
        }
        else
        {
            errMsg = m_audioOutputSecondary->GetError();
        }
        AddVisuals();
    }
    else if (!m_no_audio_in && m_audioOutputSecondary)
    {
        const AudioSettings settings(m_format, m_channels, m_codec,
                                     m_samplerate, m_passthru, 0,
                                     m_codec_profile);
        m_audioOutputSecondary->Reconfigure(settings);
        errMsg = m_audioOutputSecondary->GetError();
        SetStretchFactor(m_stretchfactor);
    }

    //================
*/

    if (!errMsg.isEmpty())
    {
        LOG(VB_GENERAL, LOG_NOTICE, LOC + "Disabling Audio" +
                QString(", reason is: %1").arg(errMsg));
        ShowNotificationError(tr("Disabling Audio"),
                              _Location, errMsg);
        m_no_audio_out = true;
    }
    else if (m_no_audio_out && m_audioOutputMain)
    {
        LOG(VB_GENERAL, LOG_NOTICE, LOC + "Enabling Audio");
        m_no_audio_out = false;
    }

    if (m_muted_on_creation)
    {
        SetMuteState(kMuteAll);
        m_muted_on_creation = false;
    }

    ResetVisuals();

    return errMsg;
}

void AudioPlayer::CheckFormat(void)
{
    if (m_format == FORMAT_NONE)
        m_no_audio_in = m_no_audio_out = true;
}

bool AudioPlayer::Pause(bool pause)
{
    if (!m_audioOutputMain || m_no_audio_out)
        return false;
    QMutexLocker lock(&m_lock);
    m_audioOutputMain->Pause(pause);

    //---
    if (m_audioOutputSecondary)
        m_audioOutputSecondary->Pause(pause);
    //---

    return true;
}

bool AudioPlayer::IsPaused(void)
{
    if (!m_audioOutputMain || m_no_audio_out)
        return false;
    QMutexLocker lock(&m_lock);
    return m_audioOutputMain->IsPaused();
}

void AudioPlayer::PauseAudioUntilBuffered()
{
    if (!m_audioOutputMain || m_no_audio_out)
        return;

    QMutexLocker lock(&m_lock);
    m_audioOutputMain->PauseUntilBuffered();

    //---
    if (m_audioOutputSecondary)
        m_audioOutputSecondary->PauseUntilBuffered();
    //---

}

void AudioPlayer::SetAudioOutput(AudioOutput *ao)
{
    // delete current audio class if any
    DeleteOutput();
    m_lock.lock();
    m_audioOutputMain = ao;
    AddVisuals();
    m_lock.unlock();
}

uint AudioPlayer::GetVolume(void)
{
    if (!m_audioOutputMain || m_no_audio_out)
        return 0;
    QMutexLocker lock(&m_lock);
    return m_audioOutputMain->GetCurrentVolume();
}


/**
 * Set audio output device parameters.
 * codec_profile is currently only used for DTS
 */
/*
void AudioPlayer::SetAudioInfo(const QString &main_device,
                               const QString &passthru_device,
                               uint           samplerate,
                               int            codec_profile)
{
    //----------------
    m_main_device = m_passthru_device = QString::null;
    if (!main_device.isEmpty())
    {
        m_main_device = main_device;
        m_main_device.detach();
    }
    if (!passthru_device.isEmpty())
    {
        m_passthru_device = passthru_device;
        m_passthru_device.detach();
    }
    m_samplerate = (int)samplerate;
    m_codec_profile    = codec_profile;
    //-----------------

    m_copy_device = QString("ALSA:hdmi:CARD=HDMI,DEV=0");

    LOG(VB_AUDIO, LOG_INFO, QString("Audio profile log:\n    Main device = %1\n    Sample rate = %2\n    Codec Profile = %3\n    Copy device = %4")
        .arg(m_main_device)
        .arg(m_samplerate)
        .arg(m_codec_profile)
        .arg(m_copy_device)
    );
}
*/




//================

/**
 * Set audio output device parameters.
 * codec_profile is currently only used for DTS
 */
void AudioPlayer::AddAudioInfo(const QString &main_device,
                               const QString &passthru_device,
                               uint           samplerate,
                               int            codec_profile)
{

    AudioSettings* aos = new AudioSettings(main_device,
                                           passthru_device,
                                           m_format,
                                           m_channels,
                                           codec_profile,
                                           samplerate,
                                           AUDIOOUTPUT_VIDEO,
                                           m_controls_volume,
                                           m_passthru);
    m_aoslist.append(aos);

/*
    LOG(VB_AUDIO, LOG_INFO, QString("Audio profile log:\n    Main device = %1\n    Sample rate = %2\n    Codec Profile = %3\n    Copy device = %4")
        .arg(m_main_device)
        .arg(m_samplerate)
        .arg(m_codec_profile)
        .arg(m_copy_device)
    );
*/

}

//================






/**
 * Set audio output parameters.
 * codec_profile is currently only used for DTS
 */
void AudioPlayer::SetAudioParams(AudioFormat format, int orig_channels,
                                 int channels, int codec,
                                 int samplerate, bool passthru,
                                 int codec_profile)
{
    m_format        = CanProcess(format) ? format : FORMAT_S16;
    m_orig_channels = orig_channels;
    m_channels      = channels;
    m_codec         = codec;
    m_samplerate    = samplerate;
    m_passthru      = passthru;
    m_codec_profile = codec_profile;

    //UPDATE PARAMS INTO AudioSettings/AudioInfo class
    for (uint i = 0; i < m_aoslist.size(); i++)
    {
        AudioSettings* aos = m_aoslist[i];
        aos->format       = m_format;
        aos->channels     = m_orig_channels;    // OR m_channels?????
        aos->codec        = m_codec;
        aos->samplerate   = m_samplerate;
        aos->use_passthru = m_passthru;
        aos->codec_profile= m_codec_profile;
    }

    ResetVisuals();
}

void AudioPlayer::SetEffDsp(int dsprate)
{
    if (!m_audioOutputMain || !m_no_audio_out)
        return;
    QMutexLocker lock(&m_lock);
    m_audioOutputMain->SetEffDsp(dsprate);

    if(m_audioOutputSecondary)
        m_audioOutputSecondary->SetEffDsp(dsprate);
}

bool AudioPlayer::SetMuted(bool mute)
{
    bool is_muted = IsMuted();
    QMutexLocker lock(&m_lock);

    if (m_audioOutputMain && !m_no_audio_out && !is_muted && mute &&
        (kMuteAll == SetMuteState(kMuteAll)))
    {
        LOG(VB_AUDIO, LOG_INFO, QString("muting sound %1").arg(IsMuted()));
        return true;
    }
    else if (m_audioOutputMain && !m_no_audio_out && is_muted && !mute &&
             (kMuteOff == SetMuteState(kMuteOff)))
    {
        LOG(VB_AUDIO, LOG_INFO, QString("unmuting sound %1").arg(IsMuted()));
        return true;
    }

    LOG(VB_AUDIO, LOG_ERR,
        QString("not changing sound mute state %1").arg(IsMuted()));

    return false;
}

MuteState AudioPlayer::SetMuteState(MuteState mstate)
{
    if (!m_audioOutputMain || m_no_audio_out)
        return kMuteAll;
    QMutexLocker lock(&m_lock);

    if(m_audioOutputSecondary)
        m_audioOutputSecondary->SetMuteState(mstate);

    return m_audioOutputMain->SetMuteState(mstate);
}

MuteState AudioPlayer::IncrMuteState(void)
{
    if (!m_audioOutputMain || m_no_audio_out)
        return kMuteAll;
    return SetMuteState(VolumeBase::NextMuteState(GetMuteState()));
}

MuteState AudioPlayer::GetMuteState(void)
{
    if (!m_audioOutputMain || m_no_audio_out)
        return kMuteAll;
    QMutexLocker lock(&m_lock);
    return m_audioOutputMain->GetMuteState();
}

uint AudioPlayer::AdjustVolume(int change)
{
    if (!m_audioOutputMain || m_no_audio_out)
        return GetVolume();
    QMutexLocker lock(&m_lock);
    m_audioOutputMain->AdjustCurrentVolume(change);

    if(m_audioOutputSecondary)
        m_audioOutputSecondary->AdjustCurrentVolume(change);

    return GetVolume();
}

uint AudioPlayer::SetVolume(int newvolume)
{
    if (!m_audioOutputMain || m_no_audio_out)
        return GetVolume();
    QMutexLocker lock(&m_lock);
    m_audioOutputMain->SetCurrentVolume(newvolume);

    if(m_audioOutputSecondary)
        m_audioOutputSecondary->SetCurrentVolume(newvolume);

    return GetVolume();
}

int64_t AudioPlayer::GetAudioTime(void)
{
    if (!m_audioOutputMain || m_no_audio_out)
        return 0LL;
    internalAudioSync();
    QMutexLocker lock(&m_lock);
    return m_audioOutputMain->GetAudiotime();
}
/*
void AudioPlayer::internalAudioSync(void)
{
    int64_t preset = 0; //sync preset offset (+/- uS)
    int64_t tol = 10;      //Tolerance (+/- uS)
    int64_t timediff = m_audioOutput->GetAudiotime() - m_audioOutput2->GetAudiotime();

    static int64_t timeLast;
    int64_t period = m_audioOutput->GetAudiotime() - timeLast;

    if(period < 250)
        return;

    timeLast = m_audioOutput->GetAudiotime();
    
    //if( ((timediff - preset) > tol) || (timediff - preset) < -tol )
    {
        m_timeadj = (float) (timediff - preset) / 100.00f;
        
        float f = 1.00f;

        if(m_timeadj >= 0.0f) 
        {
            f = 1.0f + (1.00f * m_timeadj);
            //f = 1.00f / (1.0f + m_timeadj);
        }
        
        if(m_timeadj < 0.0f) 
        {
            f = -1.00f / (-1.0f + m_timeadj);
            //f = 1.0f + (-1.00f * m_timeadj);
        }

        if(f > 1.02f) f = 1.02f;
        if(f < 0.98f) f = 0.98f;

        if(m_audioOutput2)
        {
            QMutexLocker lock(&m_lock);
            m_audioOutput2->SetStretchFactor(f);
        }

        LOG(VB_AUDIO, LOG_INFO, LOC + QString("internalAudioSync(): Stretch factor1=%1").arg((f)));
    }

    LOG(VB_AUDIO, LOG_INFO, LOC + QString("internalAudioSync(%1 uS): Time diff=%2").arg(period).arg(timediff));

}
*/

void AudioPlayer::internalAudioSync(void)
{
    int64_t preset = 0; //sync preset offset (+/- uS)
    int64_t tol = 3;      //Tolerance (+/- uS)
    int64_t timediff = m_audioOutputMain->GetAudiotime() - m_audioOutputSecondary->GetAudiotime();

    static int64_t timeLast;
    int64_t period = m_audioOutputMain->GetAudiotime() - timeLast;
/*
    int delay = ( timediff > 0 ? timediff * 1 : timediff * -1 );

    if( (timediff-preset) > tol )
    {
        QMutexLocker lock(&m_lock);
        m_audioOutput->Pause(true);
        usleep(delay);
        m_audioOutput->Pause(false);
        LOG(VB_AUDIO, LOG_INFO, LOC + QString("AS(%1 uS): 1+").arg(timediff));
    }

    if( (timediff-preset) < -tol )
    {
        QMutexLocker lock(&m_lock);
        m_audioOutput2->Pause(true);
        usleep(delay);
        m_audioOutput2->Pause(false);
        LOG(VB_AUDIO, LOG_INFO, LOC + QString("AS(%1 uS): 2+").arg(timediff));
    }   
*/
    if(period > 100)
    {
        LOG(VB_AUDIO, LOG_INFO, LOC + QString("AS(%1 uS): ").arg(timediff));
        timeLast = m_audioOutputMain->GetAudiotime();
    }
    

        
}


bool AudioPlayer::IsUpmixing(void)
{
    if (!m_audioOutputMain)
        return false;
    QMutexLocker lock(&m_lock);
    return m_audioOutputMain->IsUpmixing();
}

bool AudioPlayer::EnableUpmix(bool enable, bool toggle)
{
    if (!m_audioOutputMain)
        return false;
    QMutexLocker lock(&m_lock);
    if (toggle || (enable != IsUpmixing()))
        return m_audioOutputMain->ToggleUpmix();
    return enable;
}

bool AudioPlayer::CanUpmix(void)
{
    if (!m_audioOutputMain)
        return false;
    QMutexLocker lock(&m_lock);
    return m_audioOutputMain->CanUpmix();
}

void AudioPlayer::SetStretchFactor(float factor)
{
    m_stretchfactor = factor;
    if (!m_audioOutputMain)
        return;
    QMutexLocker lock(&m_lock);
    m_audioOutputMain->SetStretchFactor(m_stretchfactor);

    if(m_audioOutputSecondary)
        m_audioOutputSecondary->SetStretchFactor(m_stretchfactor + m_timeadj);
}

// The following methods are not locked as this hinders performance.
// They are however only called from the Decoder and only the decode
// thread will trigger a deletion/recreation of the AudioOutput device, hence
// they should be safe.

inline bool TestDigitalFeature(AudioOutput *ao, DigitalFeature feature)
{
    if (!ao)
        return false;

    return ao->GetOutputSettingsUsers(true)->canFeature(feature);
}

bool AudioPlayer::CanAC3(void)
{
    return TestDigitalFeature(m_audioOutputMain, FEATURE_AC3);
}

bool AudioPlayer::CanDTS(void)
{
    return TestDigitalFeature(m_audioOutputMain, FEATURE_DTS);
}

bool AudioPlayer::CanEAC3(void)
{
    return TestDigitalFeature(m_audioOutputMain, FEATURE_EAC3);
}

bool AudioPlayer::CanTrueHD(void)
{
    return TestDigitalFeature(m_audioOutputMain, FEATURE_TRUEHD);
}

bool AudioPlayer::CanDTSHD(void)
{
    return TestDigitalFeature(m_audioOutputMain, FEATURE_DTSHD);
}

uint AudioPlayer::GetMaxChannels(void)
{
    if (!m_audioOutputMain)
        return 2;
    return m_audioOutputMain->GetOutputSettingsUsers(false)->BestSupportedChannels();
}

int AudioPlayer::GetMaxHDRate()
{
    if (!m_audioOutputMain)
        return 0;
    return m_audioOutputMain->GetOutputSettingsUsers(true)->GetMaxHDRate();
}

bool AudioPlayer::CanPassthrough(int samplerate, int channels,
                                 int codec, int profile)
{
    if (!m_audioOutputMain)
        return false;
    return m_audioOutputMain->CanPassthrough(samplerate, channels, codec, profile);
}

bool AudioPlayer::CanDownmix(void)
{
    if (!m_audioOutputMain)
        return false;
    return m_audioOutputMain->CanDownmix();
}

/*
 * if frames = -1 : let AudioOuput calculate value
 * if frames = 0 && len > 0: will calculate according to len
 */
void AudioPlayer::AddAudioData(char *buffer, int len,
                               int64_t timecode, int frames)
{
    if (!m_audioOutputMain || m_no_audio_out)
        return;

    if (m_parent->PrepareAudioSample(timecode) && !m_no_audio_out)
    {
        m_audioOutputMain->Drain();
        if(m_audioOutputSecondary)
            m_audioOutputSecondary->Drain();
    }
    int samplesize = m_audioOutputMain->GetBytesPerFrame();

    if (samplesize <= 0)
        return;

    if (frames == 0 && len > 0)
        frames = len / samplesize;

    if (!m_audioOutputMain->AddData(buffer, len, timecode, frames))
        LOG(VB_PLAYBACK, LOG_ERR, LOC + "AddAudioData(): "
                "Audio buffer overflow, audio data lost!");


    //------
    samplesize = m_audioOutputSecondary->GetBytesPerFrame();

    if (samplesize <= 0)
        return;

    if (frames == 0 && len > 0)
        frames = len / samplesize;

    if (!m_audioOutputSecondary->AddData(buffer, len, timecode+60, frames))
        LOG(VB_PLAYBACK, LOG_ERR, LOC + "AddAudioData(): "
                "Audio buffer overflow, audio data lost!");
    //------
}

bool AudioPlayer::NeedDecodingBeforePassthrough(void)
{
    if (!m_audioOutputMain)
        return true;
    else
        //#################
        return m_audioOutputMain->NeedDecodingBeforePassthrough();
}

int64_t AudioPlayer::LengthLastData(void)
{
    if (!m_audioOutputMain)
        return 0;
    else
        return m_audioOutputMain->LengthLastData();
}

bool AudioPlayer::GetBufferStatus(uint &fill, uint &total)
{
    fill = total = 0;
    if (!m_audioOutputMain || m_no_audio_out)
        return false;
    m_audioOutputMain->GetBufferStatus(fill, total);
    return true;
}

bool AudioPlayer::IsBufferAlmostFull(void)
{
    uint ofill = 0, ototal = 0, othresh = 0;
    if (GetBufferStatus(ofill, ototal))
    {
        othresh =  ((ototal>>1) + (ototal>>2));
        return ofill > othresh;
    }
    return false;
}

bool AudioPlayer::CanProcess(AudioFormat fmt)
{
    if (!m_audioOutputMain)
        return false;
    else
        return m_audioOutputMain->CanProcess(fmt);
}

uint32_t AudioPlayer::CanProcess(void)
{
    if (!m_audioOutputMain)
        return 0;
    else
        return m_audioOutputMain->CanProcess();
}

/**
 * DecodeAudio
 * Utility routine.
 * Decode an audio packet, and compact it if data is planar
 * Return negative error code if an error occurred during decoding
 * or the number of bytes consumed from the input AVPacket
 * data_size contains the size of decoded data copied into buffer
 * data decoded will be S16 samples if class instance can't handle HD audio
 * or S16 and above otherwise. No U8 PCM format can be returned
 */
int AudioPlayer::DecodeAudio(AVCodecContext *ctx,
                             uint8_t *buffer, int &data_size,
                             const AVPacket *pkt)
{
    if (!m_audioOutputMain)
    {
        data_size = 0;
        return 0;
    }
    else
        return m_audioOutputMain->DecodeAudio(ctx, buffer, data_size, pkt);
}

