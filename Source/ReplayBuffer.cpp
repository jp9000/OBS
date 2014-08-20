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

String GetOutputFilename(bool replayBuffer = false);
VideoFileStream *CreateFileStream(String strOutputFile);

namespace
{
    using packet_t = tuple<PacketType, DWORD, DWORD, vector<BYTE>>;
    using packet_list_t = list<shared_ptr<const packet_t>>;
    using packet_vec_t = deque<shared_ptr<const packet_t>>;
}

void CreateRecordingHelper(VideoFileStream *&stream, packet_list_t &packets);

static DWORD STDCALL SaveReplayBufferThread(void *arg);

struct ReplayBuffer : VideoFileStream
{
    using thread_param_t = tuple<DWORD, shared_ptr<void>, packet_vec_t, bool>;
    packet_list_t packets;
    deque<pair<DWORD, packet_list_t::iterator>> keyframes;

    vector<DWORD> save_times;
    unique_ptr<void, MutexDeleter> save_times_lock;

    int seconds;
    ReplayBuffer(int seconds) : seconds(seconds), save_times_lock(OSCreateMutex()) {}

    bool start_recording = false;

    ~ReplayBuffer()
    {
        if (save_times.size())
            StartSaveThread(save_times.back());

        if (start_recording)
            StartSaveThread(-1, true);

        for (auto &thread : threads)
            if (WaitForSingleObject(thread.second.get(), seconds * 100) != WAIT_OBJECT_0)
                OSTerminateThread(thread.first.release(), 0);
    }
    
    virtual void AddPacket(BYTE *data, UINT size, DWORD timestamp, DWORD pts, PacketType type) override
    {
        packets.emplace_back(make_shared<packet_t>(type, timestamp, pts, vector<BYTE>(data, data + size)));

        if (start_recording)
        {
            start_recording = false;
            CreateRecordingHelper(App->fileStream, packets);
        }

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
    void StartSaveThread(DWORD save_time, bool last_minute_recording=false)
    {
        shared_ptr<void> init_done;
        init_done.reset(CreateEvent(nullptr, true, false, nullptr), OSCloseEvent);
        threads.emplace_back(
            unique_ptr<void, ThreadCloser>(OSCreateThread(SaveReplayBufferThread, new thread_param_t(save_time, init_done, { begin(packets), end(packets) }, last_minute_recording))),
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

    static void SetRecording(bool recording)
    {
        App->bRecording = recording;
        App->ConfigureStreamButtons();
    }
};

static DWORD STDCALL SaveReplayBufferThread(void *arg)
{
    unique_ptr<ReplayBuffer::thread_param_t> param((ReplayBuffer::thread_param_t*)arg);

    String name = GetOutputFilename(!get<3>(*param));
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
        out->AddPacket(const_cast<BYTE*>(buf.data()), (UINT)buf.size(), get<1>(*packet), get<2>(*packet), get<0>(*packet));

        if (buf.front() == 0x17)
            signal();

        packets.pop_front();
    }
    signal();

    ReplayBuffer::SetLastFilename(name);

    return 0;
}

struct RecordingHelper : VideoFileStream
{
    packet_vec_t buffered_packets;
    unique_ptr<void, MutexDeleter> packets_mutex;

    unique_ptr<VideoFileStream> file_stream;
    unique_ptr<void, ThreadDeleter<>> save_thread;

    RecordingHelper(packet_vec_t packets) : buffered_packets(packets), packets_mutex(OSCreateMutex())
    {}

    bool StartRecording()
    {
        String name = GetOutputFilename();
        file_stream.reset(CreateFileStream(name));
        if (!file_stream)
        {
            using ::locale;
            AppWarning(L"RecordingHelper::SaveThread: Unable to create the file stream. Check the file path in Broadcast Settings.");
            OBSMessageBox(hwndMain, Str("Capture.Start.FileStream.Warning"), Str("Capture.Start.FileStream.WarningCaption"), MB_OK | MB_ICONWARNING);
            return false;
        }

        ReplayBuffer::SetRecording(true);
        save_thread.reset(OSCreateThread([](void *arg) -> DWORD { static_cast<RecordingHelper*>(arg)->SaveThread(); return 0; }, this));
        return true;
    }

    void SaveThread()
    {
        shared_ptr<const packet_t> packet;
        for (;;)
        {
            {
                ScopedLock l(packets_mutex);
                if (buffered_packets.empty())
                {
                    Log(L"RecordingHelper::SaveThread: done writing buffered packets");
                    return;
                }

                packet = buffered_packets.front();
                buffered_packets.pop_front();
            }

            auto &buf = get<3>(*packet);
            file_stream->AddPacket(const_cast<BYTE*>(buf.data()), (UINT)buf.size(), get<1>(*packet), get<2>(*packet), get<0>(*packet));
        }
    }

    void AddPacket(BYTE *data, UINT size, DWORD timestamp, DWORD pts, PacketType type) override
    {
        if (save_thread)
        {
            if (WaitForSingleObject(save_thread.get(), 0) == WAIT_OBJECT_0)
            {
                if (!buffered_packets.empty())
                    Log(L"RecordingHelper thread exited while %d buffered packets remain", buffered_packets.size());

                buffered_packets.clear();
                buffered_packets.shrink_to_fit();

                file_stream->AddPacket(data, size, timestamp, pts, type);

                decltype(save_thread) null_thread;
                swap(null_thread, save_thread);
                return;
            }

            ScopedLock l(packets_mutex);
            buffered_packets.emplace_back(make_shared<packet_t>(type, timestamp, pts, vector<BYTE>(data, data + size)));
            return;
        }

        file_stream->AddPacket(data, size, timestamp, pts, type);
    }
};

void CreateRecordingHelper(VideoFileStream *&stream, packet_list_t &packets)
{
    if (stream)
    {
        using ::locale;
        Log(L"Tried to create a recording from replay buffer but another recording is already active");
        UINT id = App->AddStreamInfo(Str("ReplayBuffer.RecordingAlreadyActive"), StreamInfoPriority_High);
        OSCloseThread(OSCreateThread([](void *arg) -> DWORD { Sleep(10000); if (App) App->RemoveStreamInfo((UINT)arg); return 0; }, (void*)id));
        return;
    }

    auto helper = make_unique<RecordingHelper>(packet_vec_t{begin(packets), end(packets)});
    if (helper->StartRecording())
        stream = helper.release();
}

pair<ReplayBuffer*, unique_ptr<VideoFileStream>> CreateReplayBuffer(int seconds)
{
    if (seconds <= 0) return {nullptr, nullptr};

    auto out = make_unique<ReplayBuffer>(seconds);
    return {out.get(), move(out)};
}

void SaveReplayBuffer(ReplayBuffer *out, DWORD timestamp)
{
    if (!out) return;
    out->SaveReplayBuffer(timestamp);
}

void StartRecordingFromReplayBuffer(ReplayBuffer *rb)
{
    if (!rb) return;
    rb->start_recording = true;
}
