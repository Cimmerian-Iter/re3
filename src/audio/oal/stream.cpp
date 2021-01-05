#include "common.h"

#ifdef AUDIO_OAL
#include "stream.h"
#include "sampman.h"

#ifdef AUDIO_OPUS
#include <opusfile.h>
#else
#ifdef _WIN32
#pragma comment( lib, "libsndfile-1.lib" )
#pragma comment( lib, "libmpg123-0.lib" )
#endif
#if !defined(PSP2)
#include <sndfile.h>
#else
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"
#endif
#include <mpg123.h>
#endif

#ifndef _WIN32
#include "crossplatform.h"
#endif

/*
As we ran onto an issue of having different volume levels for mono streams
and stereo streams we are now handling all the stereo panning ourselves.
Each stream now has two sources - one panned to the left and one to the right,
and uses two separate buffers to store data for each individual channel.
For that we also have to reshuffle all decoded PCM stereo data from LRLRLRLR to
LLLLRRRR (handled by CSortStereoBuffer).
*/

class CSortStereoBuffer
{
	uint16* PcmBuf;
	size_t BufSize;
public:
	CSortStereoBuffer() : PcmBuf(nil), BufSize(0) {}
	~CSortStereoBuffer()
	{
		if (PcmBuf)
			free(PcmBuf);
	}

	uint16* GetBuffer(size_t size)
	{
		if (size == 0) return nil;
		if (!PcmBuf)
		{
			BufSize = size;
			PcmBuf = (uint16*)malloc(BufSize);
		}
		else if (BufSize < size)
		{
			BufSize = size;
			PcmBuf = (uint16*)realloc(PcmBuf, size);
		}
		return PcmBuf;
	}

	void SortStereo(void* buf, size_t size)
	{
		uint16* InBuf = (uint16*)buf;
		uint16* OutBuf = GetBuffer(size);

		if (!OutBuf) return;

		size_t rightStart = size / 4;
		for (size_t i = 0; i < size / 4; i++)
		{
			OutBuf[i] = InBuf[i*2];
			OutBuf[i+rightStart] = InBuf[i*2+1];
		}

		memcpy(InBuf, OutBuf, size);
	}

};

CSortStereoBuffer SortStereoBuffer;

#ifndef AUDIO_OPUS
#if !defined(PSP2)
class CSndFile : public IDecoder
{
	SNDFILE *m_pfSound;
	SF_INFO m_soundInfo;
public:
	CSndFile(const char *path) :
		m_pfSound(nil)
	{
		memset(&m_soundInfo, 0, sizeof(m_soundInfo));
		m_pfSound = sf_open(path, SFM_READ, &m_soundInfo);
	}
	
	~CSndFile()
	{
		if ( m_pfSound )
		{
			sf_close(m_pfSound);
			m_pfSound = nil;
		}
	}
	
	bool IsOpened()
	{
		return m_pfSound != nil;
	}
	
	uint32 GetSampleSize()
	{
		return sizeof(uint16);
	}
	
	uint32 GetSampleCount()
	{
		return m_soundInfo.frames;
	}
	
	uint32 GetSampleRate()
	{
		return m_soundInfo.samplerate;
	}
	
	uint32 GetChannels()
	{
		return m_soundInfo.channels;
	}
	
	void Seek(uint32 milliseconds)
	{
		if ( !IsOpened() ) return;
		sf_seek(m_pfSound, ms2samples(milliseconds), SF_SEEK_SET);
	}
	
	uint32 Tell()
	{
		if ( !IsOpened() ) return 0;
		return samples2ms(sf_seek(m_pfSound, 0, SF_SEEK_CUR));
	}
	
	uint32 Decode(void *buffer)
	{
		if ( !IsOpened() ) return 0;

		size_t size = sf_read_short(m_pfSound, (short*)buffer, GetBufferSamples()) * GetSampleSize();
		if (GetChannels()==2)
			SortStereoBuffer.SortStereo(buffer, size);
		return size;
	}
};
#else
class CDrWav : public IDecoder
{
	drwav m_drWav;
	bool m_bIsLoaded;
public:
	CDrWav(const char *path) :
		m_bIsLoaded(false)
	{
		memset(&m_drWav, 0, sizeof(m_drWav));
		if( !drwav_init_file(&m_drWav, path, NULL) ) {
			return;
		}

		m_bIsLoaded = true;
	}

	~CDrWav()
	{
		if ( m_bIsLoaded )
		{
			drwav_uninit(&m_drWav);
			m_bIsLoaded = false;
		}
	}

	bool IsOpened()
	{
		return m_bIsLoaded;
	}

	uint32 GetSampleSize()
	{
		return drwav_get_bytes_per_pcm_frame(&m_drWav);
	}
	
	uint32 GetSampleCount()
	{
		return m_drWav.totalPCMFrameCount;
	}
	
	uint32 GetSampleRate()
	{
		return m_drWav.sampleRate;
	}
	
	uint32 GetChannels()
	{
		return m_drWav.channels;
	}
	
	void Seek(uint32 milliseconds)
	{
		if ( !IsOpened() ) return;
		drwav_seek_to_pcm_frame(&m_drWav, ms2samples(milliseconds));
	}
	
	uint32 Tell()
	{
		if ( !IsOpened() ) return 0;

		if (drwav__is_compressed_format_tag(m_drWav.translatedFormatTag)) {
			return samples2ms(m_drWav.compressed.iCurrentPCMFrame);
		} else {
			uint32 bytes_per_frame = GetSampleSize();
			return samples2ms(
				((m_drWav.totalPCMFrameCount * bytes_per_frame) - m_drWav.bytesRemaining) / bytes_per_frame
			);
		}
	}
	
	uint32 Decode(void *buffer)
	{
		if ( !IsOpened() ) return 0;
		size_t size = drwav_read_raw(&m_drWav, GetBufferSize(), buffer);
		if (GetChannels()==2)
			SortStereoBuffer.SortStereo(buffer, size);
		return size;
	}
};
#endif

#ifdef _WIN32
// fuzzy seek eliminates stutter when playing ADF but spams errors a lot (nothing breaks though)
#define MP3_USE_FUZZY_SEEK
#endif // _WIN32


class CMP3File : public IDecoder
{
	mpg123_handle *m_pMH;
	bool m_bOpened;
	uint32 m_nRate;
	uint32 m_nChannels;
public:
	CMP3File(const char *path) :
		m_pMH(nil),
		m_bOpened(false),
		m_nRate(0),
		m_nChannels(0)
	{
		m_pMH = mpg123_new(nil, nil);
		if ( m_pMH )
		{
#ifdef MP3_USE_FUZZY_SEEK
			mpg123_param(m_pMH, MPG123_FLAGS, MPG123_FUZZY | MPG123_SEEKBUFFER | MPG123_GAPLESS, 0.0);
#endif
			long rate = 0;
			int channels = 0;
			int encoding = 0;
			
			m_bOpened = mpg123_open(m_pMH, path) == MPG123_OK
				&& mpg123_getformat(m_pMH, &rate, &channels, &encoding) == MPG123_OK;
			m_nRate = rate;
			m_nChannels = channels;
			
			if ( IsOpened() )
			{
				mpg123_format_none(m_pMH);
				mpg123_format(m_pMH, rate, channels, encoding);
			}
		}
	}
	
	~CMP3File()
	{
		if ( m_pMH )
		{
			mpg123_close(m_pMH);
			mpg123_delete(m_pMH);
			m_pMH = nil;
		}
	}
	
	bool IsOpened()
	{
		return m_bOpened;
	}
	
	uint32 GetSampleSize()
	{
		return sizeof(uint16);
	}
	
	uint32 GetSampleCount()
	{
		if ( !IsOpened() ) return 0;
		return mpg123_length(m_pMH);
	}
	
	uint32 GetSampleRate()
	{
		return m_nRate;
	}
	
	uint32 GetChannels()
	{
		return m_nChannels;
	}
	
	void Seek(uint32 milliseconds)
	{
		if ( !IsOpened() ) return;
		mpg123_seek(m_pMH, ms2samples(milliseconds), SEEK_SET);
	}
	
	uint32 Tell()
	{
		if ( !IsOpened() ) return 0;
		return samples2ms(mpg123_tell(m_pMH));
	}
	
	uint32 Decode(void *buffer)
	{
		if ( !IsOpened() ) return 0;
		
		size_t size;
		int err = mpg123_read(m_pMH, (unsigned char *)buffer, GetBufferSize(), &size);
#if defined(__LP64__) || defined(_WIN64)
		assert("We can't handle audio files more then 2 GB yet :shrug:" && (size < UINT32_MAX));
#endif
		if (err != MPG123_OK && err != MPG123_DONE) return 0;
		if (GetChannels() == 2)
			SortStereoBuffer.SortStereo(buffer, size);
		return (uint32)size;
	}
};

#define VAG_LINE_SIZE (0x10)
#define VAG_SAMPLES_IN_LINE (28)

class CVagDecoder
{
	const double f[5][2] = { { 0.0, 0.0 },
					{  60.0 / 64.0,  0.0 },
					{  115.0 / 64.0, -52.0 / 64.0 },
					{  98.0 / 64.0, -55.0 / 64.0 },
					{  122.0 / 64.0, -60.0 / 64.0 } };

	double s_1;
	double s_2;
public:
	CVagDecoder()
	{
		ResetState();
	}

	void ResetState()
	{
		s_1 = s_2 = 0.0;
	}

	static short quantize(double sample)
	{
		int a = int(sample + 0.5);
		return short(clamp(int(sample + 0.5), -32768, 32767));
	}

	void Decode(void* _inbuf, int16* _outbuf, size_t size)
	{
		uint8* inbuf = (uint8*)_inbuf;
		int16* outbuf = _outbuf;
		size &= ~(VAG_LINE_SIZE - 1);

		while (size > 0) {
			double samples[VAG_SAMPLES_IN_LINE];

			int predict_nr, shift_factor, flags;
			predict_nr = *(inbuf++);
			shift_factor = predict_nr & 0xf;
			predict_nr >>= 4;
			flags = *(inbuf++);
			if (flags == 7) // TODO: ignore?
				break;
			for (int i = 0; i < VAG_SAMPLES_IN_LINE; i += 2) {
				int d = *(inbuf++);
				int16 s = int16((d & 0xf) << 12);
				samples[i] = (double)(s >> shift_factor);
				s = int16((d & 0xf0) << 8);
				samples[i + 1] = (double)(s >> shift_factor);
			}

			for (int i = 0; i < VAG_SAMPLES_IN_LINE; i++) {
				samples[i] = samples[i] + s_1 * f[predict_nr][0] + s_2 * f[predict_nr][1];
				s_2 = s_1;
				s_1 = samples[i];
				*(outbuf++) = quantize(samples[i] + 0.5);
			}
			size -= VAG_LINE_SIZE;
		}
	}
};

#define VB_BLOCK_SIZE (0x2000)
#define NUM_VAG_LINES_IN_BLOCK (VB_BLOCK_SIZE / VAG_LINE_SIZE)
#define NUM_VAG_SAMPLES_IN_BLOCK (NUM_VAG_LINES_IN_BLOCK * VAG_SAMPLES_IN_LINE)

class CVbFile : public IDecoder
{
	FILE* pFile;
	size_t m_FileSize;
	size_t m_nNumberOfBlocks;
	CVagDecoder* decoders;

	uint32 m_nSampleRate;
	uint8 m_nChannels;
	bool m_bBlockRead;
	uint16 m_LineInBlock;
	size_t m_CurrentBlock;

	uint8** ppTempBuffers;

	void ReadBlock(int32 block = -1)
	{
		// just read next block if -1
		if (block != -1)
			fseek(pFile, block * m_nChannels * VB_BLOCK_SIZE, SEEK_SET);

		for (int i = 0; i < m_nChannels; i++)
			fread(ppTempBuffers[i], VB_BLOCK_SIZE, 1, pFile);
		m_bBlockRead = true;
	}

public:
	CVbFile(const char* path, uint32 nSampleRate = 32000, uint8 nChannels = 2) : m_nSampleRate(nSampleRate), m_nChannels(nChannels)
	{
		pFile = fopen(path, "rb");
		if (pFile) {
			fseek(pFile, 0, SEEK_END);
			m_FileSize = ftell(pFile);
			fseek(pFile, 0, SEEK_SET);
			m_nNumberOfBlocks = m_FileSize / (nChannels * VB_BLOCK_SIZE);
			decoders = new CVagDecoder[nChannels];
			m_CurrentBlock = 0;
			m_LineInBlock = 0;
			m_bBlockRead = false;
			ppTempBuffers = new uint8 * [nChannels];
			for (uint8 i = 0; i < nChannels; i++)
				ppTempBuffers[i] = new uint8[VB_BLOCK_SIZE];
		}
	}

	~CVbFile()
	{
		if (pFile)
		{
			fclose(pFile);
			delete decoders;
			for (int i = 0; i < m_nChannels; i++)
				delete ppTempBuffers[i];
			delete ppTempBuffers;
		}
	}

	bool IsOpened()
	{
		return pFile != nil;
	}

	uint32 GetSampleSize()
	{
		return sizeof(uint16);
	}

	uint32 GetSampleCount()
	{
		if (!IsOpened()) return 0;
		return m_nNumberOfBlocks * NUM_VAG_LINES_IN_BLOCK * VAG_SAMPLES_IN_LINE;
	}

	uint32 GetSampleRate()
	{
		return m_nSampleRate;
	}

	uint32 GetChannels()
	{
		return m_nChannels;
	}

	void Seek(uint32 milliseconds)
	{
		if (!IsOpened()) return;
		uint32 samples = ms2samples(milliseconds);
		int32 block = samples / NUM_VAG_SAMPLES_IN_BLOCK;
		if (block > m_nNumberOfBlocks)
		{
			samples = 0;
			block = 0;
		}
		if (block != m_CurrentBlock)
			ReadBlock(block);

		uint32 remainingSamples = samples - block * NUM_VAG_SAMPLES_IN_BLOCK;
		uint32 newLine = remainingSamples / VAG_SAMPLES_IN_LINE / VAG_LINE_SIZE;

		if (m_CurrentBlock != block || m_LineInBlock != newLine)
		{
			m_CurrentBlock = block;
			m_LineInBlock = newLine;
			for (int i = 0; i < GetChannels(); i++)
				decoders[i].ResetState();
		}

	}

	uint32 Tell()
	{
		if (!IsOpened()) return 0;
		uint32 pos = (m_CurrentBlock * NUM_VAG_LINES_IN_BLOCK + m_LineInBlock) * VAG_SAMPLES_IN_LINE;
		return samples2ms(pos);
	}

	uint32 Decode(void* buffer)
	{
		if (!IsOpened()) return 0;

		if (!m_bBlockRead)
			ReadBlock(m_CurrentBlock);

		if (m_CurrentBlock == m_nNumberOfBlocks) return 0;
		int size = 0;

		int numberOfRequiredLines = GetBufferSamples() / GetChannels() / VAG_SAMPLES_IN_LINE;
		int numberOfRemainingLines = (m_nNumberOfBlocks - m_CurrentBlock) * NUM_VAG_LINES_IN_BLOCK - m_LineInBlock;
		int bufSizePerChannel = Min(numberOfRequiredLines, numberOfRemainingLines) * VAG_SAMPLES_IN_LINE * GetSampleSize();

		if (numberOfRequiredLines > numberOfRemainingLines)
			numberOfRemainingLines = numberOfRemainingLines;

		int16* buffers[2] = { (int16*)buffer, &((int16*)buffer)[bufSizePerChannel / GetSampleSize()] };

		while (size < bufSizePerChannel)
		{
			for (int i = 0; i < GetChannels(); i++)
			{
				decoders[i].Decode(ppTempBuffers[i] + m_LineInBlock * VAG_LINE_SIZE, buffers[i], VAG_LINE_SIZE);
				buffers[i] += VAG_SAMPLES_IN_LINE;
			}
			size += VAG_SAMPLES_IN_LINE * GetSampleSize();
			m_LineInBlock++;
			if (m_LineInBlock >= NUM_VAG_LINES_IN_BLOCK)
			{
				m_CurrentBlock++;
				if (m_CurrentBlock >= m_nNumberOfBlocks)
					break;
				m_LineInBlock = 0;
				ReadBlock();
			}
		}

		return bufSizePerChannel * GetChannels();
	}
};
#else
class COpusFile : public IDecoder
{
	OggOpusFile *m_FileH;
	bool m_bOpened;
	uint32 m_nRate;
	uint32 m_nChannels;
public:
	COpusFile(const char *path) : m_FileH(nil),
		m_bOpened(false),
		m_nRate(0),
		m_nChannels(0)
	{
		int ret;
		m_FileH = op_open_file(path, &ret);

		if (m_FileH) {
			m_nChannels = op_head(m_FileH, 0)->channel_count;
			m_nRate = 48000;
			const OpusTags *tags = op_tags(m_FileH, 0);
			for (int i = 0; i < tags->comments; i++) {
				if (strncmp(tags->user_comments[i], "SAMPLERATE", sizeof("SAMPLERATE")-1) == 0)
				{
					sscanf(tags->user_comments[i], "SAMPLERATE=%i", &m_nRate);
					break;
				}
			}
			
			m_bOpened = true;
		}
	}
	
	~COpusFile()
	{
		if (m_FileH)
		{
			op_free(m_FileH);
			m_FileH = nil;
		}
	}
	
	bool IsOpened()
	{
		return m_bOpened;
	}
	
	uint32 GetSampleSize()
	{
		return sizeof(uint16);
	}
	
	uint32 GetSampleCount()
	{
		if ( !IsOpened() ) return 0;
		return op_pcm_total(m_FileH, 0);
	}
	
	uint32 GetSampleRate()
	{
		return m_nRate;
	}
	
	uint32 GetChannels()
	{
		return m_nChannels;
	}
	
	void Seek(uint32 milliseconds)
	{
		if ( !IsOpened() ) return;
		op_pcm_seek(m_FileH, ms2samples(milliseconds) / GetChannels());
	}
	
	uint32 Tell()
	{
		if ( !IsOpened() ) return 0;
		return samples2ms(op_pcm_tell(m_FileH) * GetChannels());
	}
	
	uint32 Decode(void *buffer)
	{
		if ( !IsOpened() ) return 0;

		int size = op_read(m_FileH, (opus_int16 *)buffer, GetBufferSamples(), NULL);

		if (size < 0)
			return 0;

		if (GetChannels() == 2)
			SortStereoBuffer.SortStereo(buffer, size * m_nChannels * GetSampleSize());

		return size * m_nChannels * GetSampleSize();
	}
};
#endif

void CStream::Initialise()
{
#ifndef AUDIO_OPUS
	mpg123_init();
#endif
}

void CStream::Terminate()
{
#ifndef AUDIO_OPUS
	mpg123_exit();
#endif
}

CStream::CStream(char *filename, ALuint *sources, ALuint (&buffers)[NUM_STREAMBUFFERS], uint32 overrideSampleRate) :
	m_pAlSources(sources),
	m_alBuffers(buffers),
	m_pBuffer(nil),
	m_bPaused(false),
	m_bActive(false),
	m_pSoundFile(nil),
	m_bReset(false),
	m_nVolume(0),
	m_nPan(0),
	m_nPosBeforeReset(0)
	
{
// Be case-insensitive on linux (from https://github.com/OneSadCookie/fcaseopen/)
#if !defined(_WIN32)
	char *real = casepath(filename);
	if (real) {
		strcpy(m_aFilename, real);
		free(real);
	} else {
#else
	{
#endif
		strcpy(m_aFilename, filename);
	}
		
	DEV("Stream %s\n", m_aFilename);

#ifndef AUDIO_OPUS
	if (!strcasecmp(&m_aFilename[strlen(m_aFilename) - strlen(".mp3")], ".mp3"))
		m_pSoundFile = new CMP3File(m_aFilename);
	else if (!strcasecmp(&m_aFilename[strlen(m_aFilename) - strlen(".wav")], ".wav"))
#if !defined(PSP2)
		m_pSoundFile = new CSndFile(m_aFilename);
	else if (!strcasecmp(&m_aFilename[strlen(m_aFilename) - strlen(".vb")], ".VB"))
		m_pSoundFile = new CVbFile(m_aFilename, overrideSampleRate);
#else
		m_pSoundFile = new CDrWav(m_aFilename);
#endif
#else
	if (!strcasecmp(&m_aFilename[strlen(m_aFilename) - strlen(".opus")], ".opus"))
		m_pSoundFile = new COpusFile(m_aFilename);
#endif
	else 
		m_pSoundFile = nil;

	if ( IsOpened() )
	{
		m_pBuffer            = malloc(m_pSoundFile->GetBufferSize());
		ASSERT(m_pBuffer!=nil);
		
		DEV("AvgSamplesPerSec: %d\n", m_pSoundFile->GetAvgSamplesPerSec());
		DEV("SampleCount: %d\n",      m_pSoundFile->GetSampleCount());
		DEV("SampleRate: %d\n",       m_pSoundFile->GetSampleRate());
		DEV("Channels: %d\n",         m_pSoundFile->GetChannels());
		DEV("Buffer Samples: %d\n",   m_pSoundFile->GetBufferSamples());
		DEV("Buffer sec: %f\n",       (float(m_pSoundFile->GetBufferSamples()) / float(m_pSoundFile->GetChannels())/ float(m_pSoundFile->GetSampleRate())));
		DEV("Length MS: %02d:%02d\n", (m_pSoundFile->GetLength() / 1000) / 60, (m_pSoundFile->GetLength() / 1000) % 60);
		
		return;
	}
}

CStream::~CStream()
{
	Delete();
}

void CStream::Delete()
{
	Stop();
	ClearBuffers();
	
	if ( m_pSoundFile )
	{
		delete m_pSoundFile;
		m_pSoundFile = nil;
	}
	
	if ( m_pBuffer )
	{
		free(m_pBuffer);
		m_pBuffer = nil;
	}
}

bool CStream::HasSource()
{
	return (m_pAlSources[0] != AL_NONE) && (m_pAlSources[1] != AL_NONE);
}

bool CStream::IsOpened()
{
	return m_pSoundFile && m_pSoundFile->IsOpened();
}

bool CStream::IsPlaying()
{
	if ( !HasSource() || !IsOpened() ) return false;
	
	if ( !m_bPaused )
	{
		ALint sourceState[2];
		alGetSourcei(m_pAlSources[0], AL_SOURCE_STATE, &sourceState[0]);
		alGetSourcei(m_pAlSources[1], AL_SOURCE_STATE, &sourceState[1]);
		if ( m_bActive || sourceState[0] == AL_PLAYING || sourceState[1] == AL_PLAYING)
			return true;
	}
	
	return false;
}

void CStream::Pause()
{
	if ( !HasSource() ) return;
	ALint sourceState = AL_PAUSED;
	alGetSourcei(m_pAlSources[0], AL_SOURCE_STATE, &sourceState);
	if (sourceState != AL_PAUSED)
		alSourcePause(m_pAlSources[0]);
	alGetSourcei(m_pAlSources[1], AL_SOURCE_STATE, &sourceState);
	if (sourceState != AL_PAUSED)
		alSourcePause(m_pAlSources[1]);
}

void CStream::SetPause(bool bPause)
{
	if ( !HasSource() ) return;
	if ( bPause )
	{
		Pause();
		m_bPaused = true;
	}
	else
	{
		if (m_bPaused)
			SetPlay(true);
		m_bPaused = false;
	}
}

void CStream::SetPitch(float pitch)
{
	if ( !HasSource() ) return;
	alSourcef(m_pAlSources[0], AL_PITCH, pitch);
	alSourcef(m_pAlSources[1], AL_PITCH, pitch);
}

void CStream::SetGain(float gain)
{
	if ( !HasSource() ) return;
	alSourcef(m_pAlSources[0], AL_GAIN, gain);
	alSourcef(m_pAlSources[1], AL_GAIN, gain);
}

void CStream::SetPosition(int i, float x, float y, float z)
{
	if ( !HasSource() ) return;
	alSource3f(m_pAlSources[i], AL_POSITION, x, y, z);
}

void CStream::SetVolume(uint32 nVol)
{
	m_nVolume = nVol;
	SetGain(ALfloat(nVol) / MAX_VOLUME);
}

void CStream::SetPan(uint8 nPan)
{
	m_nPan = clamp((int8)nPan - 63, 0, 63);
	SetPosition(0, (m_nPan - 63) / 64.0f, 0.0f, Sqrt(1.0f - SQR((m_nPan - 63) / 64.0f)));

	m_nPan = clamp((int8)nPan + 64, 64, 127);
	SetPosition(1, (m_nPan - 63) / 64.0f, 0.0f, Sqrt(1.0f - SQR((m_nPan - 63) / 64.0f)));

	m_nPan = nPan;
}

void CStream::SetPosMS(uint32 nPos)
{
	if ( !IsOpened() ) return;
	m_pSoundFile->Seek(nPos);
	ClearBuffers();
}

uint32 CStream::GetPosMS()
{
	if ( !HasSource() ) return 0;
	if ( !IsOpened() ) return 0;
	
	ALint offset;
	//alGetSourcei(m_alSource, AL_SAMPLE_OFFSET, &offset);
	alGetSourcei(m_pAlSources[0], AL_BYTE_OFFSET, &offset);

	return m_pSoundFile->Tell()
		- m_pSoundFile->samples2ms(m_pSoundFile->GetBufferSamples() * (NUM_STREAMBUFFERS/2-1)) / m_pSoundFile->GetChannels()
		+ m_pSoundFile->samples2ms(offset/m_pSoundFile->GetSampleSize()) / m_pSoundFile->GetChannels();
}

uint32 CStream::GetLengthMS()
{
	if ( !IsOpened() ) return 0;
	return m_pSoundFile->GetLength();
}

bool CStream::FillBuffer(ALuint *alBuffer)
{
	if ( !HasSource() )
		return false;
	if ( !IsOpened() )
		return false;
	if ( !(alBuffer[0] != AL_NONE && alIsBuffer(alBuffer[0])) )
		return false;
	if ( !(alBuffer[1] != AL_NONE && alIsBuffer(alBuffer[1])) )
		return false;
	
	uint32 size = m_pSoundFile->Decode(m_pBuffer);
	if( size == 0 )
		return false;

	uint32 channelSize = size / m_pSoundFile->GetChannels();
	
	alBufferData(alBuffer[0], AL_FORMAT_MONO16, m_pBuffer, channelSize, m_pSoundFile->GetSampleRate());
	// TODO: use just one buffer if we play mono
	if (m_pSoundFile->GetChannels() == 1)
		alBufferData(alBuffer[1], AL_FORMAT_MONO16, m_pBuffer, channelSize, m_pSoundFile->GetSampleRate());
	else
		alBufferData(alBuffer[1], AL_FORMAT_MONO16, (uint8*)m_pBuffer + channelSize, channelSize, m_pSoundFile->GetSampleRate());
	return true;
}

int32 CStream::FillBuffers()
{
	int32 i = 0;
	for ( i = 0; i < NUM_STREAMBUFFERS/2; i++ )
	{
		if ( !FillBuffer(&m_alBuffers[i*2]) )
			break;
		alSourceQueueBuffers(m_pAlSources[0], 1, &m_alBuffers[i*2]);
		alSourceQueueBuffers(m_pAlSources[1], 1, &m_alBuffers[i*2+1]);
	}
	
	return i;
}

void CStream::ClearBuffers()
{
	if ( !HasSource() ) return;

	ALint buffersQueued[2];
	alGetSourcei(m_pAlSources[0], AL_BUFFERS_QUEUED, &buffersQueued[0]);
	alGetSourcei(m_pAlSources[1], AL_BUFFERS_QUEUED, &buffersQueued[1]);

	ALuint value;
	while (buffersQueued[0]--)
		alSourceUnqueueBuffers(m_pAlSources[0], 1, &value);
	while (buffersQueued[1]--)
		alSourceUnqueueBuffers(m_pAlSources[1], 1, &value);
}

bool CStream::Setup()
{
	if ( IsOpened() )
	{
		m_pSoundFile->Seek(0);
		//SetPosition(0.0f, 0.0f, 0.0f);
		SetPitch(1.0f);
		//SetPan(m_nPan);
		//SetVolume(100);
	}
	
	return IsOpened();
}

void CStream::SetPlay(bool state)
{
	if ( !HasSource() ) return;
	if ( state )
	{
		ALint sourceState = AL_PLAYING;
		alGetSourcei(m_pAlSources[0], AL_SOURCE_STATE, &sourceState);
		if (sourceState != AL_PLAYING )
			alSourcePlay(m_pAlSources[0]);

		sourceState = AL_PLAYING;
		alGetSourcei(m_pAlSources[1], AL_SOURCE_STATE, &sourceState);
		if (sourceState != AL_PLAYING)
			alSourcePlay(m_pAlSources[1]);

		m_bActive = true;
	}
	else
	{
		ALint sourceState = AL_STOPPED;
		alGetSourcei(m_pAlSources[0], AL_SOURCE_STATE, &sourceState);
		if (sourceState != AL_STOPPED)
			alSourceStop(m_pAlSources[0]);

		sourceState = AL_STOPPED;
		alGetSourcei(m_pAlSources[1], AL_SOURCE_STATE, &sourceState);
		if (sourceState != AL_STOPPED)
			alSourceStop(m_pAlSources[1]);

		m_bActive = false;
	}
}

void CStream::Start()
{
	if ( !HasSource() ) return;
	if ( FillBuffers() != 0 )
		SetPlay(true);
}

void CStream::Stop()
{
	if ( !HasSource() ) return;
	SetPlay(false);
}

void CStream::Update()
{
	if ( !IsOpened() )
		return;
	
	if ( !HasSource() )
		return;
	
	if ( m_bReset )
		return;
	
	if ( !m_bPaused )
	{
		ALint sourceState[2];
		ALint buffersProcessed[2] = { 0, 0 };
		
		// Relying a lot on left buffer states in here

		//alSourcef(m_pAlSources[0], AL_ROLLOFF_FACTOR, 0.0f);
		alGetSourcei(m_pAlSources[0], AL_SOURCE_STATE, &sourceState[0]);
		alGetSourcei(m_pAlSources[0], AL_BUFFERS_PROCESSED, &buffersProcessed[0]);
		//alSourcef(m_pAlSources[1], AL_ROLLOFF_FACTOR, 0.0f);
		alGetSourcei(m_pAlSources[1], AL_SOURCE_STATE, &sourceState[1]);
		alGetSourcei(m_pAlSources[1], AL_BUFFERS_PROCESSED, &buffersProcessed[1]);
		
		ALint looping = AL_FALSE;
		alGetSourcei(m_pAlSources[0], AL_LOOPING, &looping);
		
		if ( looping == AL_TRUE )
		{
			TRACE("stream set looping");
			alSourcei(m_pAlSources[0], AL_LOOPING, AL_TRUE);
			alSourcei(m_pAlSources[1], AL_LOOPING, AL_TRUE);
		}

		assert(buffersProcessed[0] == buffersProcessed[1]);
		
		while( buffersProcessed[0]-- )
		{
			ALuint buffer[2];
			
			alSourceUnqueueBuffers(m_pAlSources[0], 1, &buffer[0]);
			alSourceUnqueueBuffers(m_pAlSources[1], 1, &buffer[1]);
			
			if (m_bActive && FillBuffer(buffer))
			{
				alSourceQueueBuffers(m_pAlSources[0], 1, &buffer[0]);
				alSourceQueueBuffers(m_pAlSources[1], 1, &buffer[1]);
			}
		}
		
		if ( sourceState[0] != AL_PLAYING )
		{
			alGetSourcei(m_pAlSources[0], AL_BUFFERS_PROCESSED, &buffersProcessed[0]);
			SetPlay(buffersProcessed[0]!=0);
		}
	}
}

void CStream::ProviderInit()
{
	if ( m_bReset )
	{
		if ( Setup() )
		{
			SetPan(m_nPan);
			SetVolume(m_nVolume);
			SetPosMS(m_nPosBeforeReset);
			if (m_bActive)
				FillBuffers();
			SetPlay(m_bActive);
			if ( m_bPaused )
				Pause();
		}
	
		m_bReset = false;
	}
}

void CStream::ProviderTerm()
{
	m_bReset = true;
	m_nPosBeforeReset = GetPosMS();
	
	ClearBuffers();
}
	
#endif
