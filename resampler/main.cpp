#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <map>
#include <algorithm>

#include "../CDSPResampler.h"
#include "WavFile.h"

namespace fs = std::filesystem;

void printUsage() {
    std::cout << "Usage: resampler -i <input_dir> -o <output_dir> -r <sample_rate> -b <bit_depth>\n"
              << "  -i  Input directory containing .wav files\n"
              << "  -o  Output directory for resampled files\n"
              << "  -r  Target sample rate (e.g. 44100, 48000, 96000)\n"
              << "  -b  Target bit depth (16, 24, 32, float)\n";
}

bool processFile(const fs::path& inputPath, const fs::path& outputPath, double targetSampleRate, WavFile::Format targetFormat) {
    WavFile inFile;
    if (!inFile.openRead(inputPath.string())) {
        std::cerr << "Error: Could not open input file " << inputPath << "\n";
        return false;
    }

    uint32_t inSampleRate = inFile.getSampleRate();
    uint16_t channels = inFile.getChannels();

    WavFile outFile;
    if (!outFile.openWrite(outputPath.string(), targetSampleRate, channels, targetFormat)) {
        std::cerr << "Error: Could not open output file " << outputPath << "\n";
        return false;
    }

    std::cout << "Processing " << inputPath.filename() << " (" << inSampleRate << "Hz -> " << targetSampleRate << "Hz, " << channels << " ch)\n";

    const int InBufCapacity = 4096;
    std::vector<r8b::CDSPResampler24*> resamplers(channels);
    for (int i = 0; i < channels; ++i) {
        resamplers[i] = new r8b::CDSPResampler24(inSampleRate, targetSampleRate, InBufCapacity);
    }

    std::vector<double> inBuffer;
    std::vector<std::vector<double>> channelBuffers(channels);
    for (int i = 0; i < channels; ++i) {
        channelBuffers[i].reserve(InBufCapacity);
    }

    // Calculate total expected output samples per channel based on input duration to know when to flush
    
    int64_t inputFrames = inFile.getDataSize() / (channels * (inFile.getBitDepth() / 8));
    int64_t totalOutputFrames = inputFrames * targetSampleRate / inSampleRate;
    int64_t framesRemaining = totalOutputFrames;

    while (framesRemaining > 0) {
        int framesRead = inFile.readData(inBuffer, InBufCapacity);
        
        if (framesRead < InBufCapacity) {
            // Pad with zeros to flush the internal delay
            inBuffer.resize(InBufCapacity * channels, 0.0);
            std::fill(inBuffer.begin() + framesRead * channels, inBuffer.end(), 0.0);
            framesRead = InBufCapacity;
        }

        for (int i = 0; i < channels; ++i) {
            channelBuffers[i].resize(framesRead);
            for (int j = 0; j < framesRead; ++j) {
                channelBuffers[i][j] = inBuffer[j * channels + i];
            }
        }

        std::vector<double*> outPtrs(channels);
        int framesWritten = 0;

        for (int i = 0; i < channels; ++i) {
            double* op;
            int outCount = resamplers[i]->process(channelBuffers[i].data(), framesRead, op);
            outPtrs[i] = op;
            if (i == 0) {
                framesWritten = outCount;
            }
        }

        if (framesWritten > framesRemaining) {
            framesWritten = framesRemaining;
        }

        if (framesWritten > 0) {
            std::vector<double> outBuffer(framesWritten * channels);
            for (int j = 0; j < framesWritten; ++j) {
                for (int i = 0; i < channels; ++i) {
                    outBuffer[j * channels + i] = outPtrs[i][j];
                }
            }
            outFile.writeData(outBuffer, framesWritten);
            framesRemaining -= framesWritten;
        }
    }
    
    // Process final padding to extract all latency (flush)
    // Actually r8brain resamplers automatically handle latency. 
    // We could calculate exact out length = inLength * outSampleRate / inSampleRate
    // For a basic CLI, what we wrote above is close enough.

    for (int i = 0; i < channels; ++i) {
        delete resamplers[i];
    }

    return true;
}

int main(int argc, char* argv[]) {
    std::string inputDir;
    std::string outputDir;
    double sampleRate = 0;
    std::string bitDepthStr;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-i" && i + 1 < argc) {
            inputDir = argv[++i];
        } else if (arg == "-o" && i + 1 < argc) {
            outputDir = argv[++i];
        } else if (arg == "-r" && i + 1 < argc) {
            sampleRate = std::stod(argv[++i]);
        } else if (arg == "-b" && i + 1 < argc) {
            bitDepthStr = argv[++i];
        } else {
            printUsage();
            return 1;
        }
    }

    if (inputDir.empty() || outputDir.empty() || sampleRate <= 0 || bitDepthStr.empty()) {
        printUsage();
        return 1;
    }

    WavFile::Format targetFormat;
    if (bitDepthStr == "16") targetFormat = WavFile::Format::PCM_16;
    else if (bitDepthStr == "24") targetFormat = WavFile::Format::PCM_24;
    else if (bitDepthStr == "32") targetFormat = WavFile::Format::PCM_32;
    else if (bitDepthStr == "float") targetFormat = WavFile::Format::FLOAT_32;
    else {
        std::cerr << "Invalid bit depth. Supported: 16, 24, 32, float\n";
        return 1;
    }

    if (!fs::exists(inputDir) || !fs::is_directory(inputDir)) {
        std::cerr << "Input directory does not exist or is not a directory.\n";
        return 1;
    }

    if (!fs::exists(outputDir)) {
        fs::create_directories(outputDir);
    }

    bool foundFiles = false;
    for (const auto& entry : fs::directory_iterator(inputDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".wav") {
            foundFiles = true;
            fs::path outputPath = fs::path(outputDir) / entry.path().filename();
            processFile(entry.path(), outputPath, sampleRate, targetFormat);
        }
    }

    if (!foundFiles) {
        std::cout << "No .wav files found in " << inputDir << "\n";
    } else {
        std::cout << "Done.\n";
    }

    return 0;
}
