#pragma once
#include <atomic>
#include <cstdint>
#include <vector>

namespace DSP {

struct NoteEvent {
    enum Type : uint32_t {
        NoteOn = 0,
        NoteOff = 1
    };

    Type type;
    uint8_t note;
    uint8_t velocity;
    uint32_t sampleOffset; // ブロック内のサンプルオフセット
};

// SPSC (Single-Producer Single-Consumer) Lock-Free / Wait-Free Queue
class MidiQueue {
public:
    explicit MidiQueue(uint32_t capacity = 1024) {
        // 容量は2のべき乗に制限
        mCapacity = 1;
        while (mCapacity < capacity) {
            mCapacity <<= 1;
        }
        mMask = mCapacity - 1;
        mBuffer.resize(mCapacity);
        
        mWriteIndex.store(0, std::memory_order_relaxed);
        mReadIndex.store(0, std::memory_order_relaxed);
    }

    ~MidiQueue() = default;

    // プロデューサー側 (MIDI入力スレッドなど) からプッシュ
    bool tryPush(const NoteEvent& event) {
        const uint32_t currentWrite = mWriteIndex.load(std::memory_order_relaxed);
        const uint32_t currentRead = mReadIndex.load(std::memory_order_acquire);

        // バッファがいっぱいかどうかチェック
        if (((currentWrite + 1) & mMask) == currentRead) {
            return false;
        }

        mBuffer[currentWrite] = event;
        mWriteIndex.store((currentWrite + 1) & mMask, std::memory_order_release);
        return true;
    }

    // コンシューマー側 (DSPスレッド) からポップ
    bool tryPop(NoteEvent& event) {
        const uint32_t currentRead = mReadIndex.load(std::memory_order_relaxed);
        const uint32_t currentWrite = mWriteIndex.load(std::memory_order_acquire);

        // バッファが空かどうかチェック
        if (currentRead == currentWrite) {
            return false;
        }

        event = mBuffer[currentRead];
        mReadIndex.store((currentRead + 1) & mMask, std::memory_order_release);
        return true;
    }

    void clear() {
        mWriteIndex.store(0, std::memory_order_relaxed);
        mReadIndex.store(0, std::memory_order_relaxed);
    }

private:
    uint32_t mCapacity;
    uint32_t mMask;
    std::vector<NoteEvent> mBuffer;

    // 偽共有 (False Sharing) を防ぐため、読取ポインタと書込ポインタを別々のキャッシュライン (64バイト) に配置
    alignas(64) std::atomic<uint32_t> mWriteIndex;
    alignas(64) std::atomic<uint32_t> mReadIndex;
};

} // namespace DSP
