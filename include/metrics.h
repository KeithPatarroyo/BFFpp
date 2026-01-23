#ifndef METRICS_H
#define METRICS_H

#include <vector>
#include <cstdint>
#include <string>

double shannon_entropy(const std::vector<uint8_t>& byte_string);

double kolmogorov_complexity_estimate(const std::vector<uint8_t>& byte_string);

double compressed_size(const std::vector<uint8_t>& byte_string);

double normalized_edit_distance(const std::string& s1, const std::string& s2);

double higher_order_entropy(const std::vector<uint8_t>& byte_string);

#endif // METRICS_H
