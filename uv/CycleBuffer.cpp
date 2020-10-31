/*
Copyright © 2017-2020, orcaer@yeah.net  All rights reserved.

Author: orcaer@yeah.net

Last modified: 2019-10-24

Description: https://github.com/wlgq2/uv-cpp
*/
#include "include/CycleBuffer.hpp"
#include "include/GlobalConfig.hpp"

using namespace uv;

CycleBuffer::CycleBuffer()
    :writeIndex_(0),
    readIndex_(0)
{
    curSize_ = GlobalConfig::CycleBufferSize;
    buffer_ = (uint8_t*)malloc(curSize_);// new uint8_t[curSize_];
}

CycleBuffer::~CycleBuffer()
{
    //delete[] buffer_;
    free(buffer_);
}


int CycleBuffer::append(const char* data, uint64_t size)
{
    SizeInfo info;
    usableSizeInfo(info);

    while (info.size < size)
    {
        //缓存上限?
        //if (curSize_ > GlobalConfig::CycleBufferSize * 10) {
            //return -1;
        //}
        _ASSERT(curSize_ == _msize(buffer_));
        uint64_t addSize = GlobalConfig::CycleBufferSize + GlobalConfig::CycleBufferSize * ((size - info.size) / GlobalConfig::CycleBufferSize);
        uint8_t* bufferT = (uint8_t*)realloc(buffer_, curSize_ + addSize);
        if (bufferT != nullptr) {
            buffer_ = bufferT;
            curSize_ += addSize;
            usableSizeInfo(info);
        }
        else {
            return -1;
        }
    }
    if (info.part1 >= size)
    {
        std::copy(data, data + size, buffer_ + writeIndex_);
    }
    else
    {
        std::copy(data, data + info.part1, buffer_ + writeIndex_);
        std::copy(data + info.part1, data + size, buffer_);
    }
    addWriteIndex(size);
    return 0;

}

int CycleBuffer::readBufferN(std::string& data, uint64_t N, int64_t P)
{
    SizeInfo info;
    readSizeInfo(info);
    uint64_t T = P + N;//total bytes from readIndex_
    if (T > info.size)
    {
        return -1;
    }
    uint64_t readIndex_part2 = 0;
    SizeInfo infoRead;
    infoRead.size = N;
    infoRead.part1 = std::min(T, info.part1);
    if (infoRead.part1 > P) {
        infoRead.part1 = infoRead.part1 - P;
    }
    else {
        readIndex_part2 = P - infoRead.part1;
        infoRead.part1 = 0;
    }
    infoRead.part2 = N - infoRead.part1;
    int start = (int)data.size();
    data.resize(start + N);
    //string被resize空间，所以操作指针安全
    char* out = const_cast<char*>(data.c_str());
    out += start;
    if (infoRead.part1 > 0) {
        std::copy(buffer_ + readIndex_ + P, buffer_ + readIndex_ + (P + infoRead.part1), out);
    }
    if (infoRead.part2 > 0) {
        std::copy(buffer_ + readIndex_part2, buffer_ + readIndex_part2 + infoRead.part2, out + infoRead.part1);
    }
    /*if ((P + N) <= info.part1)
    {
        std::copy(buffer_ + readIndex_ + P, buffer_ + readIndex_ + (P + N), out);
    }
    else
    {
        std::copy(buffer_ + readIndex_ + P, buffer_ + curSize_, out);
        uint64_t remain = (P + N) - info.part1;
        std::copy(buffer_, buffer_ + remain, out + info.part1);
    }*/

    return 0;
}

int CycleBuffer::clearBufferN(uint64_t N, int64_t P)
{
    N += P;
    SizeInfo info;
    readSizeInfo(info);
    if(N > info.size)
    {
        N = info.size;
    }
    addReadIndex(N);
    return 0;
}

int CycleBuffer::clear()
{
    writeIndex_ = 0;
    readIndex_ = 0;
    return 0;
}

uint64_t CycleBuffer::usableSize()
{
    uint64_t usable;
    if (writeIndex_ < readIndex_)
    {
        usable = readIndex_ - writeIndex_ - 1;
    }
    else
    {
        usable = curSize_ + readIndex_- writeIndex_-1;
    }
    return usable;
}

void CycleBuffer::usableSizeInfo(SizeInfo& info)
{
    if (writeIndex_ < readIndex_)
    {
        info.part1 = readIndex_ - writeIndex_ - 1;
        info.part2 = 0;
    }
    else
    {
        bool readIsZore = (0 == readIndex_);
        info.part1 = readIsZore ? curSize_ - writeIndex_-1: curSize_ - writeIndex_;
        info.part2 = readIsZore ? 0 : readIndex_ - 1;
    }
    info.size = info.part1 + info.part2;
}

uint64_t CycleBuffer::readSize()
{
    SizeInfo info;
    readSizeInfo(info);
    return info.size;
}

void CycleBuffer::readSizeInfo(SizeInfo& info)
{
    if (writeIndex_ >= readIndex_)
    {
        info.part1 = writeIndex_ - readIndex_;
        info.part2 = 0;
    }
    else
    {
        info.part1 = curSize_ - readIndex_;
        info.part2 = writeIndex_;
    }
    info.size = info.part1 + info.part2;
}

int CycleBuffer::addWriteIndex(uint64_t size)
{
    if (size > curSize_)
        return -1;
    writeIndex_ += size;
    if (writeIndex_ >= curSize_)
        writeIndex_ -= curSize_;
    return 0;
}

int CycleBuffer::addReadIndex(uint64_t size)
{
    if (size > curSize_)
        return -1;
    readIndex_ += size;
    if (readIndex_ >= curSize_)
        readIndex_ -= curSize_;
    return 0;
}
