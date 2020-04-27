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

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/PreWindowsApi.h"

#if PLATFORM_WINDOWS
#ifdef TEXT
#undef TEXT
#endif
#endif
#include "luasocket/tcp.h"

#include "Windows/PostWindowsApi.h"
#include "Windows/HideWindowsPlatformTypes.h"

#else

#include "luasocket/tcp.h"
#endif


namespace NS_SLUA {
    enum class HookState {
           UNHOOK=0,
           HOOKED=1,
    };

	enum class RunState {
		DISCONNECT = 0,
		CONNECTED = 1,
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
		LuaProfiler();
		LuaProfiler(const char* funcName);
		~LuaProfiler();
		
		void init(lua_State* L);
        void tick(lua_State* L);

		void takeSample(int event, int line, const char* funcname, const char* shortsrc);
       
	private:
		int sendraw(p_buffer buf, const char* data, size_t count, size_t* sent);
		int recvraw(p_buffer buf, size_t wanted, FArrayReader& messageReader);

		void buffer_skip(p_buffer buf, size_t count);
		int buffer_get(p_buffer buf, size_t* count, FArrayReader& messageReader);

		int receieveMessage(size_t wanted);
		size_t sendMessage(FArrayWriter& msg);

		bool checkSocketRead();

		void dispatchReceieveEvent(lua_State* L, int event);

        void memoryGC(lua_State* L);

        void makeProfilePackage(FArrayWriter& messageWriter,
                                int hookEvent, int64 time,
                                int lineDefined, const char* funcName,
                                const char* shortSrc);
        void makeMemoryProfilePackage(FArrayWriter& messageWriter,
                                      int hookEvent, TArray<LuaMemInfo> memInfoList);
        
        void takeMemorySample(int event, TArray<LuaMemInfo> memoryInfoList);
        size_t takeMemSnapshotSample(int event, double objSize, int memorySize, int snapshotId);

        void getMemorySnapshot(lua_State *L);
        
		TArray<LuaMemInfo> checkSnapshotDiff();

	public:
        static int changeHookState(lua_State* L);
		static int changeRunState(lua_State* L);
		static int setSocket(lua_State* L);
    
	private:
        int snapshotID = 0;
        int preSnapshotID = 0;
        int snapshotNum = 0;
        int snapshotDeleteID = 0;

		p_tcp tcpSocket = nullptr;

        LuaVar selfProfiler;   

        TMap<int, SnapshotMap> snapshotList;

	public:
		bool ignoreHook = false;

		lua_State* luaState = nullptr;
		
		HookState currentHookState = HookState::UNHOOK;
		RunState currentRunState = RunState::DISCONNECT;
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
