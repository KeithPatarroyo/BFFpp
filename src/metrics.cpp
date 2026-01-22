#include "metrics.h"
#include <cmath>
#include <array>
#include <brotli/encode.h>

double shannon_entropy(const std::vector<uint8_t>& byte_string) {
    std::array<int, 256> counts = {0};

    for (uint8_t byte : byte_string) {
        counts[byte]++;
    }

    double entropy = 0.0;
    size_t length = byte_string.size();

    for (int i = 0; i < 256; i++) {
        if (counts[i] > 0) {
            double frequency = static_cast<double>(counts[i]) / length;
            entropy += frequency * std::log2(frequency);
        }
    }

    return -entropy;
}

double kolmogorov_complexity_estimate(const std::vector<uint8_t>& byte_string) {
    /*
     * Complexity of 8.0 means that the string is incompressible and 0.0 bits can be saved per byte
     * Complexity of 0.0 means that the string is fully compressible and 8.0 bits can be saved per byte
     */

    size_t input_size = byte_string.size();
    size_t max_compressed_size = BrotliEncoderMaxCompressedSize(input_size);
    std::vector<uint8_t> compressed(max_compressed_size);

    size_t compressed_size = max_compressed_size;
    int result = BrotliEncoderCompress(
        BROTLI_DEFAULT_QUALITY,
        BROTLI_DEFAULT_WINDOW,
        BROTLI_DEFAULT_MODE,
        input_size,
        byte_string.data(),
        &compressed_size,
        compressed.data()
    );

    if (result == BROTLI_FALSE) {
        // Compression failed, return maximum complexity
        return 8.0;
    }

    return (static_cast<double>(compressed_size) / input_size) * 8.0;
}

double compressed_size(const std::vector<uint8_t>& byte_string) {
    size_t input_size = byte_string.size();
    size_t max_compressed_size = BrotliEncoderMaxCompressedSize(input_size);
    std::vector<uint8_t> compressed(max_compressed_size);

    size_t comp_size = max_compressed_size;
    int result = BrotliEncoderCompress(
        BROTLI_DEFAULT_QUALITY,
        BROTLI_DEFAULT_WINDOW,
        BROTLI_DEFAULT_MODE,
        input_size,
        byte_string.data(),
        &comp_size,
        compressed.data()
    );

    if (result == BROTLI_FALSE) {
        // Compression failed, return input size
        return static_cast<double>(input_size);
    }

    return static_cast<double>(comp_size);
}

double higher_order_entropy(const std::vector<uint8_t>& byte_string) {
    return shannon_entropy(byte_string) - kolmogorov_complexity_estimate(byte_string);
}
