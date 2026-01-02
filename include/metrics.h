#ifndef METRICS_H
#define METRICS_H

#include <vector>
#include <cstdint>

double shannon_entropy(const std::vector<uint8_t>& byte_string);

double kolmogorov_complexity_estimate(const std::vector<uint8_t>& byte_string);

double higher_order_entropy(const std::vector<uint8_t>& byte_string);

#endif // METRICS_H
