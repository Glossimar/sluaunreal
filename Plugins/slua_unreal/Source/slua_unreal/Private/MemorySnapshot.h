
// Tencent is pleased to support the open source community by making sluaunreal available.

// Copyright (C) 2018 THL A29 Limited, a Tencent company. All rights reserved.
// Licensed under the BSD 3-Clause License (the "License");
// you may not use this file except in compliance with the License. You may obtain a copy of the License at

// https://opensource.org/licenses/BSD-3-Clause

// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and limitations under the License.
#ifndef MEMORYSNAPSHOT_H_
#define MEMORYSNAPSHOT_H_

#pragma once
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "LuaMemoryProfile.h"
#include "lua/lua.hpp"

#define TABLE 0
#define FUNCTION 1
#define SOURCE 2
#define THREAD 3
#define USERDATA 4
#define OTHERS 5
#define MARKED 6

namespace NS_SLUA {
    // use lightuserdata as the map key
    typedef TMap<FString, LuaMemInfo> MemoryNodeMap;
    typedef TMap<const void*, MemoryNodeMap> MemoryTypeMap;
    
    class SnapshotMap{
    public:
        TArray<MemoryTypeMap> typeArray;
        
        void Empty();
        void initSnapShotMap(int typeSize);
        bool isMarked(const void *pointer);
        
        static void printMap(SnapshotMap shotMap);
        static int getSnapshotMemSize(SnapshotMap shotMap);
        static int getSnapshotObjSize(SnapshotMap shotMap);
        
        MemoryTypeMap* getMemoryMap(int index);
        TArray<LuaMemInfo> checkMemoryDiff(SnapshotMap map);
        TArray<LuaMemInfo> snapshotMapToArray(SnapshotMap map);
    };
    
    class MemorySnapshot{
    public:
        SnapshotMap getMemorySnapshot(lua_State *L, int typeSize);
        
    private:
        lua_State *L;
        SnapshotMap shotMap;
        
        int getLuaObjSize(int mapType, const void *pointer);
        FString getKey(int keyIndex);
        const void* readObject(const void *parent, FString description);

        void markObject(const void *parent, FString description);
        void markTable(const void *parent, FString description);
        void markThread(const void *parent, FString description);
        void markUserdata(const void *parent, FString description);
        void markFunction(const void *parent, FString description);
        void markOthers(const void *parent, FString description);
    };
}
#endif
