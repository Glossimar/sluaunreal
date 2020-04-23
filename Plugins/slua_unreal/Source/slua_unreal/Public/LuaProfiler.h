// Tencent is pleased to support the open source community by making sluaunreal available.

// Copyright (C) 2018 THL A29 Limited, a Tencent company. All rights reserved.
// Licensed under the BSD 3-Clause License (the "License"); 
// you may not use this file except in compliance with the License. You may obtain a copy of the License at

// https://opensource.org/licenses/BSD-3-Clause

// Unless required by applicable law or agreed to in writing, 
// software distributed under the License is distributed on an "AS IS" BASIS, 
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
// See the License for the specific language governing permissions and limitations under the License.
#ifndef LUAPROFILER_H_
#define LUAPROFILER_H_

#pragma once
#include "CoreMinimal.h"
#include "lua/lua.hpp"
#include "LuaVar.h"
#include "slua_unreal/Private/MemorySnapshot.h"
#include "slua_unreal/Private/LuaMemoryProfile.h"
#include "luasocket/tcp.h"

namespace NS_SLUA {
    enum class HookState {
           UNHOOK=0,
           HOOKED=1,
    };

	enum ProfilerHookEvent
	{
        PHE_MEMORY_TICK = -2,
		PHE_TICK = -1,
		PHE_CALL = 0,
		PHE_RETURN = 1,
		PHE_LINE = 2,
		PHE_TAILRET = 4,
        PHE_MEMORY_GC = 5,
        PHE_MEMORY_SNAPSHOT = 6,
        PHE_SNAPSHOT_COMPARE = 7,
        PHE_SNAPSHOT_DELETE = 8,
        PHE_SNAPSHOT_DELETE_ALL = 9
	};

	class SLUA_UNREAL_API LuaProfiler
	{
	public:
        LuaProfiler(){}
		LuaProfiler(const char* funcName);
		~LuaProfiler();
		
		void init(lua_State* L);
        void tick(lua_State* L);
        
        int receieveMessage(size_t wanted);
        void memoryGC(lua_State* L);
        bool checkSocketRead();
        void makeProfilePackage(FArrayWriter& messageWriter,
                                int hookEvent, int64 time,
                                int lineDefined, const char* funcName,
                                const char* shortSrc);
        void makeMemoryProfilePackage(FArrayWriter& messageWriter,
                                      int hookEvent, TArray<LuaMemInfo> memInfoList);
        size_t sendMessage(FArrayWriter& msg);
        void takeSample(int event,int line,const char* funcname,const char* shortsrc);
        void takeMemorySample(int event, TArray<LuaMemInfo> memoryInfoList);
        size_t takeMemSnapshotSample(int event, double objSize, int memorySize, int snapshotId);
        void getMemorySnapshot(lua_State *L);
        TArray<LuaMemInfo> checkSnapshotDiff();
        void dispatchReceieveEvent(lua_State *L, int event);
//        static void debug_hook(lua_State* L, lua_Debug* ar);
//        void dumpLastSnapshotInfo();
        
        static int changeHookState(lua_State* L);
        static int setSocket(lua_State* L);
        
        int snapshotID = 0;
        int preSnapshotID = 0;
        int snapshotNum = 0;
        int snapshotDeleteID = 0;
        LuaVar selfProfiler;
        bool ignoreHook = false;
        HookState currentHookState = HookState::UNHOOK;
//        int64 profileTotalCost = 0;
        p_tcp tcpSocket = nullptr;
//        const char* ChunkName = "[ProfilerScript]";
        TMap<int, SnapshotMap> snapshotList;
        
	};

#ifdef ENABLE_PROFILER
	// for native function
#define PROFILER_WATCHER(x)  NS_SLUA::LuaProfiler x(__FUNCTION__);
#define PROFILER_WATCHER_X(x,name)  NS_SLUA::LuaProfiler x(name);
#else
#define PROFILER_WATCHER(x) 
#define PROFILER_WATCHER_X(x,name)
#endif

}
#endif
