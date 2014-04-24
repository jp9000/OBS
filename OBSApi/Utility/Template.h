/********************************************************************************
 Copyright (C) 2001-2012 Hugh Bailey <obs.jim@gmail.com>

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


#pragma once

template<typename T> class List
{
private:
    List(List const&) = delete;
    List(List&&) = delete;
    List &operator=(List const&) = delete;
    List &operator=(List&&) = delete;
protected:
    T *array;
    unsigned int num;

public:

    inline List() : array(NULL), num(0) {}
    inline ~List()
    {
        Clear();
    }

    inline T* Array() const             {return array;}
    inline unsigned int Num() const     {return num;}

    inline unsigned int Add(const T& val)
    {
        array = (T*)ReAllocate(array, sizeof(T)*++num);
        mcpy(&array[(num-1)], (void*)&val, sizeof(T));
        return num-1;
    }

    inline unsigned int SafeAdd(const T& val)
    {
        UINT i;
        for(i=0; i<num; i++)
        {
            if(array[i] == val)
                break;
        }

        return (i != num) ? i : Add(val);
    }

    inline void Insert(unsigned int index, const T& val)
    {
        assert(index <= num);
        if(index > num) return;

        if(!num && !index)
        {
            Add(val);
            return;
        }

        //this makes it safe to insert an item already in the list
        T *temp = (T*)Allocate(sizeof(T));
        mcpy(temp, &val, sizeof(T));

        UINT moveCount = num-index;
        array = (T*)ReAllocate(array, sizeof(T)*++num);
        if(moveCount)
            mcpyrev(array+(index+1), array+index, moveCount*sizeof(T));
        mcpy(&array[index], temp, sizeof(T));

        Free(temp);
    }

    inline void Remove(unsigned int index)
    {
        assert(index < num);
        if(index >= num) return;

        if(!--num) {Free(array); array=NULL; return;}

        mcpy(&array[index], &array[index+1], sizeof(T)*(num-index));

        array = (T*)ReAllocate(array, sizeof(T)*num);
    }

    inline void RemoveItem(const T& obj)
    {
        for(UINT i=0; i<num; i++)
        {
            if(array[i] == obj)
            {
                Remove(i);
                break;
            }
        }
    }

    inline void RemoveRange(UINT start, UINT end)
    {
        if(start > num || end > num || end <= start)
        {
            AppWarning(TEXT("List::RemoveRange:  Invalid range specified."));
            return;
        }

        UINT count = end-start;
        if(count == 1)
        {
            Remove(start);
            return;
        }
        else if(count == num)
        {
            Clear();
            return;
        }

        num -= count;

        UINT cutoffCount = num-start;
        if(cutoffCount)
            mcpy(array+start, array+end, cutoffCount*sizeof(T));
        array = (T*)ReAllocate(array, num*sizeof(T));
    }

    inline void CopyArray(const T *new_array, unsigned int n)
    {
        if(!new_array && n)
        {
            AppWarning(TEXT("List::CopyArray:  NULL array with count above zero"));
            return;
        }

        SetSize(n);

        if(!num) {array=NULL; return;}

        mcpy(array, (void*)new_array, sizeof(T)*num);
    }

    inline void InsertArray(unsigned int index, const T *new_array, unsigned int n)
    {
        assert(index <= num);

        if(index > num)
            return;

        if(!new_array && n)
        {
            AppWarning(TEXT("List::InsertArray:  NULL array with count above zero"));
            return;
        }

        if(!n)
            return;

        int oldnum = num;

        SetSize(n+num);

        assert(num);

        if(!num) {array=NULL; return;}

        mcpyrev(array+index+n, array+index, sizeof(T)*(oldnum-index));
        mcpy(array+index, new_array, sizeof(T)*n);
    }

    inline void AppendArray(const T *new_array, unsigned int n)
    {
        if(!new_array && n)
        {
            AppWarning(TEXT("List::AppendArray:  NULL array with count above zero"));
            return;
        }

        if(!n)
            return;

        int oldnum = num;

        SetSize(n+num);

        assert(num);

        if(!num) {array=NULL; return;}

        mcpy(&array[oldnum], (void*)new_array, sizeof(T)*n);
    }

    inline BOOL SetSize(unsigned int n)
    {
        if(num == n)
            return FALSE;
        else if(!n)
        {
            Clear();
            return TRUE;
        }

        BOOL bClear=(n>num);
        UINT oldNum=num;

        num = n;
        array = (T*)ReAllocate(array, sizeof(T)*num);

        if(bClear)
            zero(&array[oldNum], sizeof(T)*(num-oldNum));

        return TRUE;
    }

    inline void MoveItem(int id, int newID)
    {
        register int last = num-1;

        assert(id <= last);
        if(newID == id || id > last || newID > last)
            return;

        T val;
        mcpy(&val, array+id, sizeof(T));
        if(newID < id)
            mcpyrev(array+newID+1, array+newID, (id-newID)*sizeof(T));
        else if(id < newID)
            mcpy(array+id, array+id+1, (newID-id)*sizeof(T));
        mcpy(array+newID, &val, sizeof(T));
        zero(&val, sizeof(T));
    }

    inline void MoveToFront(int id)
    {
        register int last = num-1;

        assert(id <= last);
        if(id > last || id == 0)
            return;

        T val;
        mcpy(&val, array+id, sizeof(T));
        mcpyrev(array+1, array, id*sizeof(T));
        mcpy(array, &val, sizeof(T));
        zero(&val, sizeof(T));
    }

    inline void MoveToEnd(int id)
    {
        register int last = num-1;

        assert(id <= last);
        if(id >= last)
            return;

        T val;
        mcpy(&val, array+id, sizeof(T));
        mcpy(array+id, array+(id+1), (last-id)*sizeof(T)); //last-id = num-(id+1)
        mcpy(array+last, &val, sizeof(T));
        zero(&val, sizeof(T));
    }

    inline void RemoveDupes()
    {
        for(UINT i=0; i<num; i++)
        {
            T val1 = array[i];
            for(UINT j=i+1; j<num; j++)
            {
                T val2 = array[j];
                if(val1 == val2)
                {
                    Remove(j);
                    --j;
                }
            }
        }
    }

    inline void CopyList(const List<T>& list)
    {
        CopyArray(list.Array(), list.Num());
    }

    inline void InsertList(unsigned int index, const List<T>& list)
    {
        InsertArray(index, list.Array(), list.Num());
    }

    inline void AppendList(const List<T>& list)
    {
        AppendArray(list.Array(), list.Num());
    }

    inline void TransferFrom(List<T>& list)
    {
        if(array) Clear();
        array = list.Array();
        num   = list.Num();
        zero(&list, sizeof(List<T>));
    }

    inline void TransferFrom(T *arrayIn, UINT numIn)
    {
        if(array) Clear();
        array = arrayIn;
        num   = numIn;
    }

    inline void TransferTo(List<T>& list)
    {
        list.TransferFrom(*this);
    }

    inline void TransferTo(T *&arrayIn, UINT &numIn)
    {
        arrayIn = array;
        numIn = num;
        zero(this, sizeof(List<T>));
    }

    inline void Clear()
    {
        if(array)
        {
            /*if(IsBadWritePtr(array, sizeof(T)*num))
                CrashError(TEXT("what the.."));*/
            Free(array);
            array = NULL;
            num = 0;
        }
    }

    inline T* CreateNew()
    {
        SetSize(num+1);

        T *value = &array[num-1];

        return value;
    }

    inline T* InsertNew(int index)
    {
        T ins;

        zero(&ins, sizeof(T));

        Insert(index, ins);

        return &array[index];
    }

    inline List<T>& operator<<(const T& val)
    {
        Add(val);
        return *this;
    }

    inline T& GetElement(unsigned int index)
    {
        assert(index < num);
        if (index >= num) { DumpError(TEXT("Out of range!  List<%S>::operator[](%d)"), typeid(T).name(), index); return array[0]; }
        return array[index];
    }

    inline T& operator[](unsigned int index)
    {
        assert(index < num);
        if(index >= num) {DumpError(TEXT("Out of range!  List<%S>::operator[](%d)"), typeid(T).name(), index); return array[0];}
        return array[index];
    }

    inline T& operator[](unsigned int index) const
    {
        assert(index < num);
        if (index >= num) { DumpError(TEXT("Out of range!  List<%S>::operator[](%d)"), typeid(T).name(), index); return array[0]; }
        return array[index];
    }

    inline T* operator+(unsigned int index)
    {
        assert(index < num);
        if (index >= num) { DumpError(TEXT("Out of range!  List<%S>::operator[](%d)"), typeid(T).name(), index); return NULL; }
        return array+index;
    }

    inline T* operator+(unsigned int index) const
    {
        assert(index < num);
        if (index >= num) { DumpError(TEXT("Out of range!  List<%S>::operator[](%d)"), typeid(T).name(), index); return NULL; }
        return array+index;
    }

    inline T* operator-(unsigned int index)
    {
        assert(index < num);
        if (index >= num) { DumpError(TEXT("Out of range!  List<%S>::operator[](%d)"), typeid(T).name(), index); return NULL; }
        return array-index;
    }

    inline BOOL HasValue(const T& val) const
    {
        for(UINT i=0; i<num; i++)
            if(array[i] == val) return 1;

        return 0;
    }

    inline UINT FindValueIndex(const T& val) const
    {
        for(UINT i=0; i<num; i++)
            if(array[i] == val) return i;

        return INVALID;
    }

    inline UINT FindNextValueIndex(const T& val, UINT lastIndex=INVALID) const
    {
        for(UINT i=lastIndex+1; i<num; i++)
            if(array[i] == val) return i;

        return INVALID;
    }

    inline void SwapValues(UINT valA, UINT valB)
    {
        assert(valA < num && valB < num);
        if(valA == valB || valA >= num || valB >= num)
            return;
        mswap(array+valA, array+valB, sizeof(T));
    }

    inline T& Last() const
    {
        assert(num > 0);

        return array[num-1];
    }

    inline friend Serializer& operator<<(Serializer &s, List<T> &list)
    {
        if(s.IsLoading())
        {
            //FIXME: ???
            UINT num;
            s << num;
            list.SetSize(num);
            if(num)
                s.Serialize(list.array, sizeof(T)*list.num);
        }
        else
        {
            s << list.num;
            if(list.num)
                s.Serialize(list.array, sizeof(T)*list.num);
        }

        return s;
    }
};


class BitList
{
    List<UINT> Data;
    UINT bitSize;

public:
    inline BitList() : bitSize(0) {}

    inline void Clear()
    {
        Data.Clear();
        bitSize = 0;
    }

    inline void SetSize(int size)
    {
        if(size == bitSize)
            return;
        else if(size == 0)
        {
            Clear();
            return;
        }

        int adjSize = ((size+31)/32);
        Data.SetSize(adjSize);

        bitSize = size;
    }

    inline void SetVal(unsigned int index, BOOL bVal)
    {
        if(bVal)
            Set(index);
        else
            Clear(index);
    }

    unsigned int Add(BOOL bVal)
    {
        int index = bitSize;
        SetSize(bitSize+1);

        if(bVal) Set(index);
        return index;
    }

    inline void TransferFrom(BitList &from)
    {
        Data.TransferFrom(from.Data);
        bitSize = from.bitSize;
        from.bitSize = 0;
    }

    inline void ClearAll()
    {
        int UINTOffset = ((bitSize+31)/32);

        while(UINTOffset--)
            Data[UINTOffset] = 0;
    }

    inline void SetAll()
    {
        int UINTOffset = (bitSize/32);
        int bitOffset = (bitSize%32);

        while(bitOffset--)
            Data[UINTOffset] |= (1<<bitOffset);
        while(UINTOffset--)
            Data[UINTOffset] = 0xFFFFFFFF;
    }

    inline BitList& operator<<(const BOOL val)
    {
        Add(val);
        return *this;
    }

    inline UINT Num() {return bitSize;}

    inline BOOL operator[](unsigned int index) const
    {
        if (index >= bitSize) DumpError(TEXT("Out of range!  BitList::operator[](%d)"), index);

        int UINTOffset = (index/32);
        int bitOffset = (index%32);

        return Data[UINTOffset]&(1<<bitOffset);
    }

    inline void Set(unsigned int index)
    {
        if(index >= bitSize)
        {
            DumpError(TEXT("Out of range!  BitList::Set(%d)"), index);
            return;
        }

        int UINTOffset = (index/32);
        int bitOffset = (index%32);

        Data[UINTOffset] |= (1<<bitOffset);
    }

    inline void Clear(unsigned int index)
    {
        if(index >= bitSize)
        {
            DumpError(TEXT("Out of range!  BitList::Clear(%d)"), index);
            return;
        }

        int UINTOffset = (index/32);
        int bitOffset = (index%32);

        Data[UINTOffset] &= ~(1<<bitOffset);
    }

    inline BOOL Get(unsigned int index)
    {
        if (index >= bitSize) DumpError(TEXT("Out of range!  BitList::Get(%d)"), index);

        int UINTOffset = (index/32);
        int bitOffset = (index%32);

        return Data[UINTOffset]&(1<<bitOffset);
    }

    inline void CopyList(const BitList& list)
    {
        Clear();

        bitSize = list.bitSize;
        Data.CopyList(list.Data);
    }

    inline friend Serializer& operator<<(Serializer &s, BitList &list)
    {
        return s << list.Data << list.bitSize;
    }
};


//===================================================================
// SafeList
//   list that is index safe
//===================================================================

template<typename T> class SafeList : public List<T>
{
    List<UINT> AvailableItems;

    inline BOOL CheckAndCleanAvail()
    {
        UINT i;
        for(i=0; i<AvailableItems.Num(); i++)

        {
            if(AvailableItems[i] == (num-1))
            {
                AvailableItems.Remove(i);
                --num;
                return 1;
            }
        }
        return 0;
    }

public:
    inline unsigned int Add(const T& val)
    {
        if(AvailableItems.Num())
        {
            UINT avail = *AvailableItems.Array();
            AvailableItems.Remove(0);
            array[avail] = val;
            return avail;
        }
        else
            return List<T>::Add(val);
    }

    inline BOOL ItemExists(unsigned int id)
    {
        if(id >= num)
            return FALSE;
        for(int i=0; i<AvailableItems.Num(); i++)
        {
            if(AvailableItems[i] == id)
                return FALSE;
        }

        return TRUE;
    }

    inline void Insert(unsigned int index, const T& val) { DumpError(TEXT("Illegal Insert into SafeList")); }

    inline void Remove(unsigned int index)
    {
        assert(index < num);
        if(index >= num) return;

        if((index+1) == num)
        {
            --num;
            while(CheckAndCleanAvail());

            if(!num)
                {Free(array); array = NULL;}
            else
                array = (T*)ReAllocate(array, sizeof(T)*num);
        }
        else
        {
            AvailableItems << index;
            zero(&array[index], sizeof(T));
        }
    }

    inline void CopyList(const SafeList<T>& safelist)
    {
        CopyArray(list.Array(), list.Num());
        AvailableItems.CopyList(list.AvailableItems);
    }

    inline void AppendList(const SafeList<T>& safelist)
    {
        AppendArray(list.Array(), list.Num());
        AvailableItems.AppendList(list.AvailableItems);
    }

    inline void operator=(const SafeList<T>& list)
    {
        array = list.Array();
        num   = list.Num();
        AvailableItems = list.AvailableItems;
    }

    inline T* CreateNew(UINT *pID=NULL)
    {
        T *value;
        if(AvailableItems.Num())
        {
            UINT avail = *AvailableItems.Array();
            if(pID) *pID = avail;

            AvailableItems.Remove(0);
            value = array+avail;
        }
        else
        {
            if(pID) *pID = num;

            SetSize(num+1);

            value = &array[num-1];
        }

        return value;
    }

    inline void Clear()
    {
        AvailableItems.Clear();
        List<T>::Clear();
    }

    inline int operator<<(const T& val)
    {
        return Add(val);
    }

    inline friend Serializer& operator<<(Serializer &s, SafeList<T> &list)
    {
        s << (List<T>&)list;
        s << list.AvailableItems;
        return s;
    }
};

//===================================================================
// CircularList
//   a list designed around minimizing allocation and data movement.
//   NOTE: to maximize speed, does not resize downward.
//   apologies, this was surprisingly complicated to write
//
//   code annoyance rating: hide yo kids, hide yo wives, hide yo husbands
//===================================================================

template<typename T> class CircularList : protected List<T>
{
    unsigned int startID, endID;
    unsigned int storedNum;

    inline unsigned int GetRealIndex(unsigned int index)
    {
        if (startID == 0) {
            return index;
        } else {
            unsigned int newIndex = startID + index;
            if (newIndex >= num)
                newIndex -= num;

            return newIndex;
        }
    }

public:
    inline CircularList() : startID(0), endID(0), storedNum(0) {}

    inline unsigned int Add(const T& val)
    {
        if (storedNum == num) {
            if (startID == 0) {
                List::Add(val);

                if (storedNum != 0)
                    ++endID;
            } else {
                List::SetSize(storedNum+1);
                mcpyrev(array+startID+1, array+startID, (storedNum-startID)*sizeof(T));
                mcpy(array+startID, &val, sizeof(T));
                ++startID;
                ++endID;
            }
        } else {
            if (storedNum > 0)
                endID = (endID == num-1) ? 0 : endID+1;
            mcpy(array+endID, &val, sizeof(T));
        }

        return storedNum++;
    }

    inline void Insert(unsigned int index, const T& val)
    {
        unsigned int realID = GetRealIndex(index);

        if (storedNum == num) {
            List::Insert(realID, val);

            if (realID == (num-1)) {
                endID = realID;
            } else if (index == (num-1)) {
                ++startID;
                ++endID;
            } else {
                if (startID > realID)
                    ++startID;
                if (endID >= realID)
                    ++endID;
            }
        } else {
            if (realID == endID+1) {
                ++endID;
                mcpy(array+endID, &val, sizeof(T));
            } else if (realID == startID) {
                if (storedNum > 0)
                    startID = (startID == 0) ? num-1 : startID-1;
                mcpy(array+startID, &val, sizeof(T));
            } else if (realID <= endID && endID < (num-1)) {
                unsigned int count = endID-realID+1;

                mcpyrev(array+realID+1, array+realID, count*sizeof(T));
                mcpy(array+realID, &val, sizeof(T));
                ++endID;
            } else if (index == storedNum && realID == 0) {

                endID = realID;
                mcpy(array, &val, sizeof(T));
            } else {
                unsigned int count = realID-startID;

                mcpy(array+startID-1, array+startID, count*sizeof(T));
                --realID;
                mcpy(array+realID, &val, sizeof(T));
                --startID;
            }
        }

        ++storedNum;
    }

    inline void Remove(unsigned int index)
    {
        unsigned int realID = GetRealIndex(index);

        if (realID == endID) {
            endID = (endID == 0) ? num-1 : endID-1;
        } else if (realID == startID) {
            startID = (startID == num-1) ? 0 : startID+1;
        } else if (realID < endID) {
            unsigned int count = endID-realID;
            mcpy(array+realID, array+realID+1, count*sizeof(T));
            --endID;
        } else if (realID > startID) {
            unsigned int count = realID-startID;
            mcpyrev(array+startID+1, array+startID, count*sizeof(T));
            ++startID;
        }

        --storedNum;

        if (!storedNum)
            startID = endID = 0;
    }

    inline void RemoveItem(const T& obj)
    {
        for (unsigned int i=0; i<storedNum; i++) {
            if (GetElement(i) == obj) {
                Remove(i);
                return;
            }
        }
    }

    inline void SetBaseSize(unsigned int newSize)
    {
        if (newSize > num) {
            unsigned int endPoint = num;

            List::SetSize(newSize);

            if (endID < startID) {
                unsigned int count = (num-endPoint);
                mcpyrev(array+startID+count, array+startID, count*sizeof(T));
            }
        }
    }

    inline unsigned int Num() const
    {
        return storedNum;
    }

    inline void Clear()
    {
        storedNum = startID = endID = 0;
        List::Clear();
    }

    inline T* CreateNew()
    {
        T addVal;
        zero(&addVal, sizeof(T));
        Add(addVal);

        return array+endID;
    }

    inline T* InsertNew(int index)
    {
        T ins;
        zero(&ins, sizeof(T));
        Insert(index, ins);

        return array+GetRealIndex(index);
    }

    inline CircularList<T>& operator<<(const T& val)
    {
        Add(val);
        return *this;
    }

    inline T& GetElement(unsigned int index)
    {
        if (index >= storedNum) DumpError(TEXT("Out of range!  CircularList::GetElement(%d)"), index);
        return array[GetRealIndex(index)];
    }

    inline T& operator[](unsigned int index)
    {
        if (index >= storedNum) DumpError(TEXT("Out of range!  CircularList::operator[](%d)"), index);
        return array[GetRealIndex(index)];
    }

    inline T& operator[](unsigned int index) const
    {
        if (index >= storedNum) DumpError(TEXT("Out of range!  CircularList::operator[](%d)"), index);
        return array[GetRealIndex(index)];
    }

    inline void SwapValues(UINT valA, UINT valB)
    {
        List::SwapValues(GetRealID(valA), GetRealID(valB));
    }

    inline void MoveItem(UINT valFrom, UINT valTo)
    {
        if(valFrom >= storedNum || valTo >= storedNum) {
            AppWarning(TEXT("CircularList::MoveItem:  invalid values"));
            return;
        }

        T val;
        mcpy(&val, array+GetRealIndex(valFrom), sizeof(T));
        Remove(valFrom);
        Insert(valTo, val);

        zero(&val, sizeof(T));
    }

    inline T& Last() const
    {
        assert(num);
        return array[endID];
    }
};

//===================================================================

class BASE_EXPORT BufferInputSerializer : public Serializer
{
public:
    BufferInputSerializer(const List<BYTE> &InBuffer) : buffer(InBuffer.Array()), bufferSize(InBuffer.Num()), position(0) {}
    BufferInputSerializer(const void *pBuffer, DWORD dwSize) : buffer((const LPBYTE)pBuffer), bufferSize(dwSize), position(0) {}

    BOOL IsLoading() {return TRUE;}

    void Serialize(LPVOID lpData, DWORD length)
    {
        assert(lpData);
        assert(length <= bufferSize-position);

        if(!lpData)
            return;

        if(length > (bufferSize-position))
            return;

        mcpy(lpData, buffer+position, length);
        position += length;
    }

    BOOL DataPending()
    {
        return position < bufferSize;
    }

    UINT64 Seek(INT64 offset, DWORD seekType=SERIALIZE_SEEK_START)
    {
        long newPos = 0;

        if(seekType == SERIALIZE_SEEK_START)
            newPos = (long)offset;
        else
            newPos = ((seekType == SERIALIZE_SEEK_CURRENT) ? (long)position : (long)bufferSize) + (long)offset;

        if(newPos > (long)bufferSize)
        {
            offset -= newPos-bufferSize;
            newPos = (long)bufferSize;
        }
        else if(newPos < 0)
        {
            offset -= newPos;
            newPos = 0;
        }

        position = (DWORD)newPos;

        return abs((long)offset);
    }

    UINT64 GetPos() const
    {
        return UINT64(position);
    }

private:
    const LPBYTE buffer;
    DWORD bufferSize;

    DWORD position;
};

class BASE_EXPORT BufferOutputSerializer : public Serializer
{
public:
    BufferOutputSerializer(List<BYTE> &InBuffer, BOOL bAppend=TRUE)
    : Buffer(InBuffer)
    {
        if(!bAppend)
        {
            Buffer.Clear();
            position = 0;
        }
        else
            position = Buffer.Num();
    }

    BOOL IsLoading() {return FALSE;}

    void Serialize(LPCVOID lpData, DWORD length)
    {
        assert(lpData);
        assert(length);

        if(!lpData || !length)
            return;

        register DWORD potentialSize = (position+length);

        if(potentialSize > Buffer.Num())
            Buffer.SetSize(potentialSize);
        mcpy(Buffer+position, lpData, length);
        position += length;
    }

    UINT64 Seek(INT64 offset, DWORD seekType=SERIALIZE_SEEK_START)
    {
        long newPos = 0;
        if(seekType == SERIALIZE_SEEK_START)
            newPos = (long)offset;
        else 
            newPos = ((seekType == SERIALIZE_SEEK_CURRENT) ? (long)position : (long)Buffer.Num()) + (long)offset;

        if(newPos < 0)
        {
            offset -= newPos;
            newPos = 0;
        }

        position = (DWORD)newPos;

        return abs((long)offset);
    }

    UINT64 GetPos() const
    {
        return UINT64(position);
    }

private:
    List<BYTE> &Buffer;
    DWORD position;
};


template<typename T> T Lerp(const T &p1, const T &p2, float t)
{
    return p1 + (p2-p1) * t;
}


template<typename T> void Swap(const T &p1, const T &p2)
{
    T temp = p1;
    p1 = p2;
    p2 = temp;
}
