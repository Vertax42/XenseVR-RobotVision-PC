// H264 NALU parser class for analyzing H264 video stream structure
#pragma once

#include <cstddef>
#include <cstdint>

class H264NALUParser {
public:
  // NALU type enumeration
  enum NAL_TYPE {
    NAL_SLICE = 1,
    NAL_DPA = 2,
    NAL_DPB = 3,
    NAL_DPC = 4,
    NAL_IDR_SLICE = 5,
    NAL_SEI = 6,
    NAL_SPS = 7,
    NAL_PPS = 8,
    NAL_AUD = 9,
    NAL_END_SEQUENCE = 10,
    NAL_END_STREAM = 11,
    NAL_FILLER_DATA = 12,
    NAL_SPS_EXT = 13,
    NAL_PREFIX = 14,
    NAL_SUB_SPS = 15,
    NAL_AUX_SLICE = 19,
    NAL_EXTEN_SLICE = 20
  };

private:
  // State machine states
  enum StartCodeState {
    INIT,   // Initial state
    ZERO_1, // Found first zero
    ZERO_2, // Found second zero
    ZERO_3, // Found third zero (possible 4-byte start code case)
    FOUND   // Found complete start code
  };

public:
  // Get string description of NALU type
  static const char *getNALUTypeStr(int nal_type);

  // Find start code
  static const uint8_t *findStartCode(const uint8_t *p, const uint8_t *end);

  // Analyze NALU
  static void analyzeNALUs(const uint8_t *data, size_t size);
};
