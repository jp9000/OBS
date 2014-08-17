/********************************************************************************
Copyright (C) 2014 Ruwen Hahn <palana@stunned.de>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
********************************************************************************/


#include "Main.h"

#include <deque>
#include <list>
#include <memory>
#include <vector>

using namespace std;

String GetOutputFilename();
VideoFileStream *CreateFileStream(String strOutputFile);

static DWORD STDCALL SaveReplayBufferThread(void *arg);

struct ReplayBuffer : VideoFileStream
{
    using packet_t = tuple<PacketType, DWORD, DWORD, vector<BYTE>>;
    using packet_list_t = list<shared_ptr<packet_t>>;
    using packet_vec_t = deque<shared_ptr<packet_t>>;
    using thread_param_t = tuple<DWORD, shared_ptr<void>, packet_vec_t>;
    packet_list_t packets;
    deque<pair<DWORD, packet_list_t::iterator>> keyframes;

    vector<DWORD> save_times;
    unique_ptr<void, MutexDeleter> save_times_lock;

    int seconds;
    ReplayBuffer(int seconds) : seconds(seconds), save_times_lock(OSCreateMutex()) {}

    ~ReplayBuffer()
    {
        if (save_times.size())
            StartSaveThread(save_times.back());

        for (auto &thread : threads)
            if (WaitForSingleObject(thread.second.get(), seconds * 100) != WAIT_OBJECT_0)
                OSTerminateThread(thread.first.release(), 0);
    }
    
    virtual void AddPacket(BYTE *data, UINT size, DWORD timestamp, DWORD pts, PacketType type) override
    {
        packets.emplace_back(make_shared<packet_t>(type, timestamp, pts, vector<BYTE>(data, data + size)));

        if (data[0] != 0x17)
            return;

        HandleSaveTimes(pts);

        keyframes.emplace_back(timestamp, --end(packets));

        while (keyframes.size() > 2)
        {
            if (((long long)timestamp - keyframes[0].first) < (seconds * 1000) || ((long long)timestamp - keyframes[1].first) < (seconds * 1000))
                break;

            packets.erase(begin(packets), keyframes[1].second);
            keyframes.erase(begin(keyframes));
        }
    }

    vector<pair<unique_ptr<void, ThreadCloser>, shared_ptr<void>>> threads;
    void StartSaveThread(DWORD save_time)
    {
        shared_ptr<void> init_done;
        init_done.reset(CreateEvent(nullptr, true, false, nullptr), OSCloseEvent);
        threads.emplace_back(
            unique_ptr<void, ThreadCloser>(OSCreateThread(SaveReplayBufferThread, new thread_param_t(save_time, init_done, { begin(packets), end(packets) }))),
            init_done);
    }

    void HandleSaveTimes(DWORD timestamp)
    {
        DWORD save_time = 0;
        {
            bool save = false;

            ScopedLock st(save_times_lock);
            auto iter = cbegin(save_times);
            for (; iter != cend(save_times); iter++)
            {
                if (*iter > timestamp)
                    break;

                save = true;
                save_time = *iter;
            }

            if (!save)
                return;

            save_times.erase(begin(save_times), iter);
        }

        StartSaveThread(save_time);
    }

    void SaveReplayBuffer(DWORD timestamp)
    {
        ScopedLock st(save_times_lock);
        save_times.emplace_back(timestamp);
    }

    static void SetLastFilename(String name)
    {
        App->lastOutputFile = name;
    }
};

static DWORD STDCALL SaveReplayBufferThread(void *arg)
{
    unique_ptr<ReplayBuffer::thread_param_t> param((ReplayBuffer::thread_param_t*)arg);

    String name = GetOutputFilename();
    unique_ptr<VideoFileStream> out(CreateFileStream(name));
    if (!out)
    {
        Log(L"ReplayBuffer: Failed to create file stream for file name '%s'", name.Array());
        return 1;
    }

    auto &packets = get<2>(*param);
    DWORD target_ts = get<0>(*param);

    DWORD stop_ts = -1;
    for (auto it = rbegin(packets); it != rend(packets); it++)
    {
        if (get<0>(**it) == PacketType_Audio)
            continue;

        DWORD ts = get<2>(**it);
        if (ts <= target_ts)
            break;

        stop_ts = ts;
    }

    bool signalled = false;
    auto signal = [&]()
    {
        if (signalled)
            return;

        SetEvent(get<1>(*param).get());
        signalled = true;
    };

    while (packets.size())
    {
        auto &packet = packets.front();
        if (get<2>(*packet) == stop_ts)
            break;

        auto &buf = get<3>(*packet);
        out->AddPacket(buf.data(), (UINT)buf.size(), get<1>(*packet), get<2>(*packet), get<0>(*packet));

        if (buf.front() == 0x17)
            signal();

        packets.pop_front();
    }
    signal();

    ReplayBuffer::SetLastFilename(name);

    return 0;
}

pair<ReplayBuffer*, VideoFileStream*> CreateReplayBuffer(int seconds)
{
    if (seconds <= 0) return {nullptr, nullptr};

    ReplayBuffer *out = new ReplayBuffer(seconds);
    return {out, out};
}

void SaveReplayBuffer(ReplayBuffer *out, DWORD timestamp)
{
    if (!out) return;
    out->SaveReplayBuffer(timestamp);
}