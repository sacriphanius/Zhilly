#ifndef OGG_DEMUXER_H_
#define OGG_DEMUXER_H_

#include <functional>
#include <cstdint>
#include <cstring>
#include <vector>

class OggDemuxer {
private:
    enum ParseState : int8_t {
        FIND_PAGE,
        PARSE_HEADER,
        PARSE_SEGMENTS,
        PARSE_DATA
    };

    struct Opus_t {
        bool    head_seen{false};
        bool    tags_seen{false};
        int     sample_rate{48000};
    };

    struct context_t {
        bool packet_continued{false};

        uint8_t header[27];

        uint8_t seg_table[255];

        uint8_t packet_buf[8192];

        size_t packet_len = 0;

        size_t seg_count = 0;

        size_t seg_index = 0;

        size_t data_offset = 0;

        size_t bytes_needed = 0;

        size_t seg_remaining = 0;

        size_t body_size = 0;

        size_t body_offset = 0;

    };

public:
    OggDemuxer() {
        Reset();
    }

    void Reset();

    size_t Process(const uint8_t* data, size_t size);

    void OnDemuxerFinished(std::function<void(const uint8_t* data, int sample_rate, size_t len)> on_demuxer_finished) {
        on_demuxer_finished_ = on_demuxer_finished;
    }
private:

    ParseState  state_ = ParseState::FIND_PAGE;
    context_t   ctx_;
    Opus_t      opus_info_;
    std::function<void(const uint8_t*, int, size_t)> on_demuxer_finished_;
};

#endif