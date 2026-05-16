#ifndef WAVFILE_H
#define WAVFILE_H

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <stdexcept>
#include <cstring>

class WavFile {
public:
    enum class Format {
        PCM_16 = 16,
        PCM_24 = 24,
        PCM_32 = 32,
        FLOAT_32 = 323 // 32 for bits, 3 for IEEE float
    };

    WavFile() : file(), isReading(false), channels(0), sampleRate(0), bitDepth(0), dataSize(0), dataOffset(0), isFloat(false) {}

    ~WavFile() {
        if (file.is_open()) {
            if (!isReading) {
                finalizeWrite();
            }
            file.close();
        }
    }

    bool openRead(const std::string& filename) {
        file.open(filename, std::ios::binary | std::ios::in);
        if (!file.is_open()) return false;

        isReading = true;
        char buffer[4];

        // RIFF header
        file.read(buffer, 4);
        if (std::strncmp(buffer, "RIFF", 4) != 0) return false;
        file.ignore(4); // ChunkSize
        file.read(buffer, 4);
        if (std::strncmp(buffer, "WAVE", 4) != 0) return false;

        bool foundFmt = false;
        bool foundData = false;

        while (!file.eof() && (!foundFmt || !foundData)) {
            file.read(buffer, 4);
            if (file.gcount() != 4) break;

            uint32_t chunkSize;
            file.read(reinterpret_cast<char*>(&chunkSize), 4);

            if (std::strncmp(buffer, "fmt ", 4) == 0) {
                uint16_t audioFormat;
                file.read(reinterpret_cast<char*>(&audioFormat), 2);
                file.read(reinterpret_cast<char*>(&channels), 2);
                file.read(reinterpret_cast<char*>(&sampleRate), 4);
                file.ignore(6); // ByteRate, BlockAlign
                file.read(reinterpret_cast<char*>(&bitDepth), 2);
                
                isFloat = (audioFormat == 3);
                if (audioFormat != 1 && audioFormat != 3) {
                    throw std::runtime_error("Unsupported audio format (only PCM and Float supported)");
                }

                if (chunkSize > 16) {
                    file.ignore(chunkSize - 16);
                }
                foundFmt = true;
            } else if (std::strncmp(buffer, "data", 4) == 0) {
                dataSize = chunkSize;
                dataOffset = file.tellg();
                foundData = true;
            } else {
                file.ignore(chunkSize);
            }
        }

        return foundFmt && foundData;
    }

    bool openWrite(const std::string& filename, uint32_t sampleRate, uint16_t channels, Format format) {
        file.open(filename, std::ios::binary | std::ios::out);
        if (!file.is_open()) return false;

        isReading = false;
        this->sampleRate = sampleRate;
        this->channels = channels;
        this->bitDepth = (format == Format::FLOAT_32) ? 32 : static_cast<uint16_t>(format);
        this->isFloat = (format == Format::FLOAT_32);
        this->dataSize = 0;

        writeHeader(0); // Write dummy header, will update on close
        return true;
    }

    int readData(std::vector<double>& buffer, int maxFrames) {
        if (!isReading || !file.is_open()) return 0;

        int bytesPerSample = bitDepth / 8;
        int frameSize = channels * bytesPerSample;
        int framesToRead = std::min(maxFrames, static_cast<int>((dataSize - (static_cast<uint32_t>(file.tellg()) - dataOffset)) / frameSize));
        
        if (framesToRead <= 0) return 0;

        std::vector<char> rawBytes(framesToRead * frameSize);
        file.read(rawBytes.data(), rawBytes.size());

        buffer.resize(framesToRead * channels);
        int rawIdx = 0;

        for (int i = 0; i < framesToRead * channels; ++i) {
            if (isFloat && bitDepth == 32) {
                float val;
                std::memcpy(&val, &rawBytes[rawIdx], 4);
                buffer[i] = static_cast<double>(val);
            } else if (bitDepth == 16) {
                int16_t val;
                std::memcpy(&val, &rawBytes[rawIdx], 2);
                buffer[i] = static_cast<double>(val) / 32768.0;
            } else if (bitDepth == 24) {
                int32_t val = 0;
                // Little endian read
                val = (static_cast<unsigned char>(rawBytes[rawIdx]) << 8) |
                      (static_cast<unsigned char>(rawBytes[rawIdx + 1]) << 16) |
                      (static_cast<signed char>(rawBytes[rawIdx + 2]) << 24);
                val >>= 8; // Sign extend
                buffer[i] = static_cast<double>(val) / 8388608.0;
            } else if (bitDepth == 32 && !isFloat) {
                int32_t val;
                std::memcpy(&val, &rawBytes[rawIdx], 4);
                buffer[i] = static_cast<double>(val) / 2147483648.0;
            }
            rawIdx += bytesPerSample;
        }

        return framesToRead;
    }

    void writeData(const std::vector<double>& buffer, int frames) {
        if (isReading || !file.is_open()) return;

        int bytesPerSample = bitDepth / 8;
        std::vector<char> rawBytes(frames * channels * bytesPerSample);
        int rawIdx = 0;

        for (int i = 0; i < frames * channels; ++i) {
            double val = buffer[i];
            // Clip
            if (val > 1.0) val = 1.0;
            if (val < -1.0) val = -1.0;

            if (isFloat && bitDepth == 32) {
                float fval = static_cast<float>(val);
                std::memcpy(&rawBytes[rawIdx], &fval, 4);
            } else if (bitDepth == 16) {
                int16_t ival = static_cast<int16_t>(val * 32767.0);
                std::memcpy(&rawBytes[rawIdx], &ival, 2);
            } else if (bitDepth == 24) {
                int32_t ival = static_cast<int32_t>(val * 8388607.0);
                rawBytes[rawIdx] = ival & 0xFF;
                rawBytes[rawIdx + 1] = (ival >> 8) & 0xFF;
                rawBytes[rawIdx + 2] = (ival >> 16) & 0xFF;
            } else if (bitDepth == 32 && !isFloat) {
                int32_t ival = static_cast<int32_t>(val * 2147483647.0);
                std::memcpy(&rawBytes[rawIdx], &ival, 4);
            }
            rawIdx += bytesPerSample;
        }

        file.write(rawBytes.data(), rawBytes.size());
        dataSize += rawBytes.size();
    }

    uint32_t getSampleRate() const { return sampleRate; }
    uint16_t getChannels() const { return channels; }
    uint16_t getBitDepth() const { return bitDepth; }
    uint32_t getDataSize() const { return dataSize; }

private:
    std::fstream file;
    bool isReading;
    uint16_t channels;
    uint32_t sampleRate;
    uint16_t bitDepth;
    uint32_t dataSize;
    uint32_t dataOffset;
    bool isFloat;

    void writeHeader(uint32_t dataBytes) {
        file.seekp(0);
        file.write("RIFF", 4);
        uint32_t subchunk1Size = isFloat ? 18 : 16;
        uint32_t chunkSize = 20 + subchunk1Size + dataBytes;
        file.write(reinterpret_cast<char*>(&chunkSize), 4);
        file.write("WAVE", 4);
        file.write("fmt ", 4);
        file.write(reinterpret_cast<char*>(&subchunk1Size), 4);
        uint16_t audioFormat = isFloat ? 3 : 1;
        file.write(reinterpret_cast<char*>(&audioFormat), 2);
        file.write(reinterpret_cast<char*>(&channels), 2);
        file.write(reinterpret_cast<char*>(&sampleRate), 4);
        uint32_t byteRate = sampleRate * channels * (bitDepth / 8);
        file.write(reinterpret_cast<char*>(&byteRate), 4);
        uint16_t blockAlign = channels * (bitDepth / 8);
        file.write(reinterpret_cast<char*>(&blockAlign), 2);
        file.write(reinterpret_cast<char*>(&bitDepth), 2);
        if (isFloat) {
            uint16_t extraSize = 0;
            file.write(reinterpret_cast<char*>(&extraSize), 2);
        }
        file.write("data", 4);
        file.write(reinterpret_cast<char*>(&dataBytes), 4);
    }

    void finalizeWrite() {
        if (file.is_open() && !isReading) {
            writeHeader(dataSize);
        }
    }
};

#endif // WAVFILE_H
