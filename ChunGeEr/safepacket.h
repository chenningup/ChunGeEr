#ifndef SAFEPACKET_H
#define SAFEPACKET_H

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}
#include <memory>

class SafePacket {
public:
    SafePacket() {
        pkt_ = av_packet_alloc();
    }

    explicit SafePacket(AVPacket* src) {
        pkt_ = av_packet_alloc();
        if (src)
            av_packet_ref(pkt_, src);
    }

    SafePacket(const SafePacket& other) {
        pkt_ = av_packet_alloc();
        if (other.pkt_)
            av_packet_ref(pkt_, other.pkt_);
    }

    SafePacket& operator=(const SafePacket& other) {
        if (this != &other) {
            if (!pkt_) pkt_ = av_packet_alloc();
            av_packet_unref(pkt_);
            if (other.pkt_)
                av_packet_ref(pkt_, other.pkt_);
        }
        return *this;
    }

    SafePacket(SafePacket&& other) noexcept {
        pkt_ = other.pkt_;
        other.pkt_ = nullptr;
    }

    SafePacket& operator=(SafePacket&& other) noexcept {
        if (this != &other) {
            release();
            pkt_ = other.pkt_;
            other.pkt_ = nullptr;
        }
        return *this;
    }

    ~SafePacket() {
        release();
    }

    static std::shared_ptr<SafePacket> fromRaw(AVPacket* raw) {
        auto pkt = std::make_shared<SafePacket>();
        av_packet_move_ref(pkt->pkt_, raw);  // 零拷贝接管
        return pkt;
    }

    AVPacket* get() const { return pkt_; }
    AVPacket* operator->() const { return pkt_; }
    operator AVPacket*() const { return pkt_; }

    bool valid() const { return pkt_ && pkt_->size > 0; }

private:
    void release() {
        if (pkt_) {
            av_packet_unref(pkt_);
            av_packet_free(&pkt_);
        }
    }

    AVPacket* pkt_ = nullptr;
};

#endif // SAFEPACKET_H
