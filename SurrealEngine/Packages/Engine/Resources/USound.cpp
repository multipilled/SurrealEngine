
#include "Precomp.h"
#include "USound.h"
#include "Engine.h"
#include "Audio/AudioSource.h"
#include "Audio/AudioDevice.h"
#include "Packages/Engine/Subsystems/USurrealAudioDevice.h"
#include "kissfft/kiss_fftr.h"

#include <algorithm>
#include <type_traits>

void USound::Load(ObjectStream* stream)
{
	UObject::Load(stream);

	Format = stream->ReadName();
	if (engine->LaunchInfo.IsBrotherBear())
	{
		KWInfo.Flags = stream->ReadUInt32();
		KWInfo.Duration = stream->ReadFloat();
		KWInfo.SampleCount = stream->ReadUInt32();
		KWInfo.BitsPerSample = stream->ReadUInt32();
		KWInfo.Channels = stream->ReadUInt32();
		KWInfo.SampleRate = stream->ReadUInt32();
	}
	if (stream->GetVersion() >= 63)
		stream->ReadUInt32(); // lazy array skip offset
	uint32_t size = stream->ReadIndex();
	Data.resize(size);
	stream->ReadBytes(Data.data(), size);

	if (engine->LaunchInfo.IsBrotherBear() && stream->BytesLeft() > 0)
	{
		KWTrailer.resize(stream->BytesLeft());
		stream->ReadBytes(KWTrailer.data(), (uint32_t)KWTrailer.size());
	}
}

void USound::Save(PackageStreamWriter* stream)
{
	UObject::Save(stream);

	stream->WriteName(Format);
	if (stream->GetVersion() >= 63)
		stream->BeginSkipOffset();
	stream->WriteIndex((int)Data.size());
	stream->WriteBytes(Data.data(), (uint32_t)Data.size());
	if (stream->GetVersion() >= 63)
		stream->EndSkipOffset();
}

void USound::GetSound()
{
	if (samples.size() > 0)
		return;

	std::unique_ptr<AudioSource> source;

	if (Format == "wav")
	{
		source = AudioSource::CreateWav(Data);
	}
	else if (Format == "mp3" || Format == "mp2")
	{
		source = AudioSource::CreateMp3(Data);
	}
	else if (Format == "bik")
	{
		try
		{
			source = AudioSource::CreateBink(Data);
		}
		catch (const std::exception& e)
		{
			LogMessage("Could not decode " + Name.ToString() + ": " + e.what());
		}
	}
	else if (Format == "XA" && engine->LaunchInfo.IsBrotherBear())
	{
		try
		{
			int rate = KWInfo.SampleRate ? (int)KWInfo.SampleRate : 22050;
			int numChannels = KWInfo.Channels ? (int)KWInfo.Channels : 1;
			source = AudioSource::CreateXA(Data, rate, numChannels, (int)KWInfo.SampleCount);
		}
		catch (const std::exception& e)
		{
			LogMessage("Could not decode " + Name.ToString() + ": " + e.what());
		}
	}

	if (!source && engine->LaunchInfo.IsBrotherBear())
	{
		// Unknown format or a decoding error: play silence of the right length.
		static bool warned = false;
		if (!warned)
		{
			LogMessage("Sound format " + Format.ToString() + " is not supported yet, playing silence");
			warned = true;
		}
		frequency = KWInfo.SampleRate ? (int)KWInfo.SampleRate : 22050;
		channels = 1;
		float seconds = KWInfo.Duration > 0.0f ? KWInfo.Duration : 0.1f;
		samples.resize(std::max((size_t)4, (size_t)(seconds * frequency) & ~(size_t)3), 0.0f);
		duration = samples.size() / (float)frequency;
		engine->audiodev->GetDevice()->AddSound(this);
		return;
	}
	else if (!source)
	{
		Exception::Throw("Unsupported sound format: " + Format.ToString());
	}

	// GetSamples counts sample frames, so a stereo sound holds twice as many values
	channels = source->GetChannels();

	#define ALIGN(x, a) ((x & ~(a-1)) + a)
	samples.resize(ALIGN(source->GetSamples() * channels, 4));
	samples.resize(ALIGN(source->ReadSamples(samples.data(), samples.size()), 4));

	frequency = source->GetFrequency();
	duration = samples.size() / (float)(frequency * channels);

	loopInfo.Looped = source->bIsLooped;
	loopInfo.LoopStart = source->loopStart;
	loopInfo.LoopEnd = source->loopEnd;

	engine->audiodev->GetDevice()->AddSound(this);
}

float USound::GetDuration()
{
	if (duration == 0.0f)
		GetSound();

	return duration;
}

int USound::GetChannels()
{
	if (channels == 0)
		GetSound();

	return channels;
}

const Array<uint8_t>& USound::GetLipsyncLetters()
{
	if(!lipsyncLetters.empty())
		return lipsyncLetters;

	GetSound();

	if (channels != 1)
		return lipsyncLetters;

	// guard against empty sound or decoding error
	if (samples.empty())
		return lipsyncLetters;

	static const float MIN_VOLUME = 0.02f;

	std::unique_ptr<kiss_fftr_state, void(*)(void*)> fftcfg(kiss_fftr_alloc(LIPSYNC_BLOCK_SIZE, 0, nullptr, nullptr), free);

	if (!fftcfg)
		return lipsyncLetters;

	Array<kiss_fft_cpx> freq(LIPSYNC_BLOCK_SIZE / 2 + 1);
	float binHz = frequency / (float)LIPSYNC_BLOCK_SIZE;

	for(size_t i = 0; i + LIPSYNC_BLOCK_SIZE <= samples.size(); i += LIPSYNC_BLOCK_SIZE)
	{
		float maxVolume = 0.0f;

		for(size_t j = i; j < i + LIPSYNC_BLOCK_SIZE; ++j)
			maxVolume = std::max(maxVolume, std::fabs(samples[j]));

		if (maxVolume < MIN_VOLUME)
		{
			lipsyncLetters.push_back('X');
			continue;
		}
		kiss_fftr(fftcfg.get(), samples.data() + i, freq.data());

		float bestMagn = 0.0f;
		int bestBin = 0;
		for(int b = 1; b < LIPSYNC_BLOCK_SIZE / 2 + 1; b++)
		{
			float magn = freq[b].r * freq[b].r + freq[b].i * freq[b].i;
			if (magn > bestMagn)
			{
				bestMagn = magn;
				bestBin = b;
			}
		}

		lipsyncLetters.push_back(LetterForHz(bestBin * binHz));
	}

	return lipsyncLetters;
}

uint8_t USound::LetterForHz(float hz)
{
	struct Band {
		float hzCutoff;
		uint8_t letter;
	};
	static const Band bands[] =
	{
		{100.0f, 'E'},
		{250.0f, 'M'},
		{400.0f, 'U'},
		{600.0f, 'O'},
		{1500.0f, 'A'},
		{2000.0f, 'F'},
	};
	for(const Band& b: bands)
		if(hz < b.hzCutoff)
			return b.letter;

	return 'T';
}

uint8_t USound::GetLipsyncLetterAt(float seconds)
{
	const Array<uint8_t>& letters = GetLipsyncLetters();
	if(letters.empty())
		return 0;

	int block = (int)(seconds * frequency / LIPSYNC_BLOCK_SIZE);
	return letters[std::max(0, std::min(block, (int)letters.size() - 1))];
}
