#include "Precomp.h"
#include "AudioSource.h"
#include "Utils/Exception.h"
#include "kissfft/kiss_fftr.h"
#include <cmath>
#include <cstring>
#include <memory>

// Decodes the first audio track of a Bink 1 video file. Brother Bear stores almost all of its sounds this way, as Bink files
// with a tiny dummy video track. Only the DCT flavour of Bink audio is implemented, as that is the only one the game uses.
//
// Each frame of the file starts with the audio packets. An audio packet holds the decoded size in bytes, then a bit stream
// (read from the least significant bit up) of transform blocks, each padded to 32 bits. A block holds the frequency
// coefficients of every channel: two floats, a quantizer per critical band, then runs of coefficients of a given bit width.
// An inverse DCT turns them into samples, and the start of each block is cross-faded with the end of the previous one.

namespace
{
	class BinkBitReader
	{
	public:
		BinkBitReader(const uint8_t* data, size_t size) : data(data), size(size) {}

		uint32_t Get(int count)
		{
			uint32_t value = 0;
			int bit = 0;
			while (bit < count)
			{
				size_t byteIndex = pos >> 3;
				int shift = (int)(pos & 7);
				int take = std::min(8 - shift, count - bit);
				uint32_t byte = byteIndex < size ? data[byteIndex] : 0;
				value |= ((byte >> shift) & ((1u << take) - 1)) << bit;
				bit += take;
				pos += take;
			}
			return value;
		}

		float GetFloat()
		{
			int power = (int)Get(5);
			float value = std::ldexp((float)Get(23), power - 23);
			return Get(1) ? -value : value;
		}

		void AlignTo32() { pos = (pos + 31) & ~(size_t)31; }
		bool AtEnd() const { return pos >= size * 8; }

	private:
		const uint8_t* data = nullptr;
		size_t size = 0;
		size_t pos = 0;
	};

	class BinkAudioDecoder
	{
	public:
		BinkAudioDecoder(int sampleRate, int channels) : channels(channels)
		{
			int frameBits = sampleRate < 22050 ? 9 : sampleRate < 44100 ? 10 : 11;
			frameLength = 1 << frameBits;
			overlapLength = frameLength / 16;

			float root = frameLength / (std::sqrt((float)frameLength) * 32768.0f);
			for (int i = 0; i < 96; i++)
				quantTable[i] = std::exp(i * 0.15289164787221953823f) * root;
			coefficientScale = root;

			static const int criticalFreqs[25] = { 100, 200, 300, 400, 510, 630, 770, 920, 1080, 1270, 1480, 1720, 2000, 2320, 2700, 3150, 3700, 4400, 5300, 6400, 7700, 9500, 12000, 15500, 24500 };
			int halfRate = (sampleRate + 1) / 2;
			numBands = 1;
			while (numBands < 25 && halfRate > criticalFreqs[numBands - 1])
				numBands++;
			bands[0] = 2;
			for (int i = 1; i < numBands; i++)
				bands[i] = (int)(((int64_t)criticalFreqs[i - 1] * frameLength / halfRate) & ~1);
			bands[numBands] = frameLength;

			coefficients.resize(channels * frameLength);
			blockOutput.resize(channels * frameLength);
			previous.resize(channels * overlapLength);
			spectrum.resize(frameLength / 2 + 1);
			reordered.resize(frameLength);
			twiddles.resize(frameLength / 2 + 1);
			for (int k = 0; k <= frameLength / 2; k++)
			{
				double angle = 3.14159265358979323846 * k / (2.0 * frameLength);
				twiddles[k] = { (float)std::cos(angle), (float)std::sin(angle) };
			}
			fft.reset(kiss_fftr_alloc(frameLength, 1, nullptr, nullptr));
		}

		// Decodes one packet and appends its interleaved samples to output
		void DecodePacket(const uint8_t* data, size_t size, Array<float>& output)
		{
			BinkBitReader bits(data, size);
			bits.Get(32); // Decoded size in bytes
			while (!bits.AtEnd())
			{
				DecodeBlock(bits);
				int blockSamples = frameLength - overlapLength;
				size_t start = output.size();
				output.resize(start + blockSamples * channels);
				for (int i = 0; i < blockSamples; i++)
				{
					for (int ch = 0; ch < channels; ch++)
						output[start + i * channels + ch] = blockOutput[ch * frameLength + i];
				}
				bits.AlignTo32();
			}
		}

	private:
		void DecodeBlock(BinkBitReader& bits)
		{
			bits.Get(2);

			for (int ch = 0; ch < channels; ch++)
			{
				float* coeffs = coefficients.data() + ch * frameLength;
				std::memset(coeffs, 0, frameLength * sizeof(float));

				coeffs[0] = bits.GetFloat() * coefficientScale;
				coeffs[1] = bits.GetFloat() * coefficientScale;

				float quant[25];
				for (int i = 0; i < numBands; i++)
					quant[i] = quantTable[std::min(bits.Get(8), 95u)];

				static const int runLengths[16] = { 2, 3, 4, 5, 6, 8, 9, 10, 11, 12, 13, 14, 15, 16, 32, 64 };
				int band = 0;
				float q = quant[0];
				int i = 2;
				while (i < frameLength)
				{
					int end = i + (bits.Get(1) ? runLengths[bits.Get(4)] * 8 : 8);
					end = std::min(end, frameLength);
					int width = (int)bits.Get(4);
					if (width == 0)
					{
						i = end;
						while (bands[band] < i)
							q = quant[band++];
					}
					else
					{
						while (i < end)
						{
							if (bands[band] == i)
								q = quant[band++];
							uint32_t value = bits.Get(width);
							if (value)
								coeffs[i] = bits.Get(1) ? -q * value : q * value;
							i++;
						}
					}
				}

				coeffs[0] *= 2.0f;
				InverseDCT(coeffs, blockOutput.data() + ch * frameLength);
			}

			for (int ch = 0; ch < channels; ch++)
			{
				float* out = blockOutput.data() + ch * frameLength;
				float* prev = previous.data() + ch * overlapLength;
				if (!first)
				{
					float count = (float)(overlapLength * channels);
					for (int i = 0; i < overlapLength; i++)
					{
						float weight = (i * channels + ch) / count;
						out[i] = prev[i] * (1.0f - weight) + out[i] * weight;
					}
				}
				std::memcpy(prev, out + frameLength - overlapLength, overlapLength * sizeof(float));
			}
			first = false;
		}

		// x[n] = (X[0] / 2 + sum X[k] * cos(pi * k * (2n + 1) / 2N)) * 2 / N, computed with one real inverse FFT
		void InverseDCT(const float* input, float* output)
		{
			int n = frameLength;
			for (int k = 0; k <= n / 2; k++)
			{
				float re = input[k];
				float im = k > 0 ? -input[n - k] : 0.0f;
				spectrum[k].r = re * twiddles[k].r - im * twiddles[k].i;
				spectrum[k].i = re * twiddles[k].i + im * twiddles[k].r;
			}
			kiss_fftri(fft.get(), spectrum.data(), reordered.data());
			float scale = 1.0f / n;
			for (int m = 0; m < n / 2; m++)
			{
				output[2 * m] = reordered[m] * scale;
				output[2 * m + 1] = reordered[n - 1 - m] * scale;
			}
		}

		int channels = 1;
		int frameLength = 0;
		int overlapLength = 0;
		int numBands = 0;
		int bands[26] = {};
		float quantTable[96] = {};
		float coefficientScale = 1.0f;
		bool first = true;

		Array<float> coefficients;
		Array<float> blockOutput;
		Array<float> previous;
		Array<kiss_fft_cpx> spectrum;
		Array<float> reordered;
		Array<kiss_fft_cpx> twiddles;
		std::unique_ptr<kiss_fftr_state, void(*)(void*)> fft = { nullptr, free };
	};

	uint32_t ReadUInt32(const Array<uint8_t>& data, size_t offset)
	{
		if (offset + 4 > data.size())
			Exception::Throw("Unexpected end of Bink file");
		return data[offset] | (data[offset + 1] << 8) | (data[offset + 2] << 16) | ((uint32_t)data[offset + 3] << 24);
	}
}

class BinkAudioSource : public AudioSource
{
public:
	BinkAudioSource(const Array<uint8_t>& filedata)
	{
		if (filedata.size() < 44 || std::memcmp(filedata.data(), "BIK", 3) != 0)
			Exception::Throw("Not a Bink file");
		if (filedata[3] == 'b')
			Exception::Throw("Bink revision b audio is not supported");

		uint32_t numFrames = ReadUInt32(filedata, 8);
		uint32_t numTracks = ReadUInt32(filedata, 40);
		if (numTracks == 0)
			Exception::Throw("Bink file has no audio");

		size_t trackInfo = 44 + 4 * (size_t)numTracks;
		uint32_t rateAndFlags = ReadUInt32(filedata, trackInfo);
		frequency = rateAndFlags & 0xffff;
		uint32_t flags = rateAndFlags >> 16;
		channels = (flags & 0x2000) ? 2 : 1;
		if ((flags & 0x1000) == 0)
			Exception::Throw("Bink RDFT audio is not supported");

		BinkAudioDecoder decoder(frequency, channels);

		// After the track infos and track IDs comes the frame offset table. The lowest bit marks keyframes.
		size_t frameTable = trackInfo + 8 * (size_t)numTracks;
		for (uint32_t frame = 0; frame < numFrames; frame++)
		{
			size_t frameStart = ReadUInt32(filedata, frameTable + frame * 4) & ~1u;
			size_t frameEnd = ReadUInt32(filedata, frameTable + frame * 4 + 4) & ~1u;
			uint32_t packetSize = ReadUInt32(filedata, frameStart);
			if (frameEnd > filedata.size() || frameStart + 4 + packetSize > frameEnd)
				Exception::Throw("Invalid Bink frame");
			if (packetSize >= 4)
				decoder.DecodePacket(filedata.data() + frameStart + 4, packetSize, samples);
		}
	}

	int GetFrequency() override { return frequency; }
	int GetChannels() override { return channels; }
	int GetSamples() override { return (int)(samples.size() / channels); }

	void SeekToSample(uint64_t position) override
	{
		readPos = std::min((size_t)position * channels, samples.size());
	}

	size_t ReadSamples(float* output, size_t count) override
	{
		count = std::min(count, samples.size() - readPos);
		std::memcpy(output, samples.data() + readPos, count * sizeof(float));
		readPos += count;
		return count;
	}

private:
	int frequency = 22050;
	int channels = 1;
	Array<float> samples;
	size_t readPos = 0;
};

std::unique_ptr<AudioSource> AudioSource::CreateBink(Array<uint8_t> filedata)
{
	return std::make_unique<BinkAudioSource>(filedata);
}
