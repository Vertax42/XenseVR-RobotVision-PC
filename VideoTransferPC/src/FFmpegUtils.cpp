//FFmpeg utility functions for error handling and common operations
extern "C" {
#include <libavutil/error.h>
}


// Error string buffer
static char av_error[AV_ERROR_MAX_STRING_SIZE];

const char* av_err2str_cpp(int errnum) {
    av_make_error_string(av_error, AV_ERROR_MAX_STRING_SIZE, errnum);
    return av_error;
}
