// H264 NALU parser implementation for analyzing video stream structure
#include "H264NALUParser.h"
#include <cstddef>
#include <stdio.h>

const char *H264NALUParser::getNALUTypeStr(int nal_type) {
  switch (nal_type) {
  case NAL_SLICE:
    return "SLICE";
  case NAL_DPA:
    return "DPA";
  case NAL_DPB:
    return "DPB";
  case NAL_DPC:
    return "DPC";
  case NAL_IDR_SLICE:
    return "IDR";
  case NAL_SEI:
    return "SEI";
  case NAL_SPS:
    return "SPS";
  case NAL_PPS:
    return "PPS";
  case NAL_AUD:
    return "AUD";
  case NAL_END_SEQUENCE:
    return "END_SEQUENCE";
  case NAL_END_STREAM:
    return "END_STREAM";
  case NAL_FILLER_DATA:
    return "FILLER_DATA";
  case NAL_SPS_EXT:
    return "SPS_EXT";
  case NAL_PREFIX:
    return "PREFIX_NAL";
  case NAL_SUB_SPS:
    return "SUBSET_SPS";
  case NAL_AUX_SLICE:
    return "AUX_SLICE";
  case NAL_EXTEN_SLICE:
    return "SLICE_EXT";
  default:
    return "UNKNOWN";
  }
}

const uint8_t *H264NALUParser::findStartCode(const uint8_t *p,
                                             const uint8_t *end) {
  StartCodeState state = INIT;

  while (p < end) {
    switch (state) {
    case INIT:
      if (*p == 0) {
        state = ZERO_1;
      }
      break;

    case ZERO_1:
      if (*p == 0) {
        state = ZERO_2;
      } else {
        state = INIT;
      }
      break;

    case ZERO_2:
      if (*p == 0) {
        state = ZERO_3;
      } else if (*p == 1) {
        // Find 3-byte start code
        return p - 2;
      } else {
        state = INIT;
      }
      break;

    case ZERO_3:
      if (*p == 1) {
        // Find 4-byte start code
        return p - 3;
      } else if (*p != 0) {
        state = INIT;
      }
      // If 0, maintain ZERO_3 state
      break;
    }

    p++;
  }

  // If reached end without finding, return end
  return end;
}

void H264NALUParser::analyzeNALUs(const uint8_t *data, size_t size) {
  const uint8_t *end = data + size;
  int count = 0;

  // Find start code
  const uint8_t *nal_start = findStartCode(data, end);
  while (nal_start < end) {
    // Check if it's 3-byte or 4-byte start code
    if (*(nal_start + 2) == 0) { // If the third byte is 0
      nal_start += 4;            // It's a 4-byte start code, skip 4 bytes
    } else {
      nal_start += 3; // Otherwise it's a 3-byte start code, skip 3 bytes
    }

    // Get NALU type
    uint8_t nal_type = nal_start[0] & 0x1F;

    // Find next start code to calculate NALU size
    const uint8_t *next_nal = findStartCode(nal_start, end);
    size_t nal_size = next_nal - nal_start;

    printf("NALU %d: Type=0x%02X (%d, %s), Size=%zu bytes\n", count++,
           nal_start[0], nal_type, getNALUTypeStr(nal_type), nal_size);

    nal_start = next_nal;
  }

  printf("Total NALUs: %d\n", count);
}
