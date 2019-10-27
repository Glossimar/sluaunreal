// Tencent is pleased to support the open source community by making sluaunreal available.

// Copyright (C) 2018 THL A29 Limited, a Tencent company. All rights reserved.
// Licensed under the BSD 3-Clause License (the "License"); 
// you may not use this file except in compliance with the License. You may obtain a copy of the License at

// https://opensource.org/licenses/BSD-3-Clause

// Unless required by applicable law or agreed to in writing, 
// software distributed under the License is distributed on an "AS IS" BASIS, 
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
// See the License for the specific language governing permissions and limitations under the License.

#include "LuaProfiler.h"
#include "Log.h"
#include "LuaState.h"
#include "ArrayWriter.h"
#include "LuaMemoryProfile.h"
#include "Containers/Queue.h"
#include "luasocket/auxiliar.h"
#include "luasocket/buffer.h"

#if PLATFORM_WINDOWS
#ifdef TEXT
#undef TEXT
#endif
#endif
#include "luasocket/tcp.h"

#ifdef ENABLE_PROFILER
namespace NS_SLUA {

	#include "LuaProfiler.inl"

	enum class HookState {
		UNHOOK=0,
		HOOKED=1,
	};

	enum class RunState {
		DISCONNECT = 0,
		CONNECTED = 1,
	};

	namespace {

        int memInfoQueueSize = 0;
		LuaVar selfProfiler;
		bool ignoreHook = false;
		HookState currentHookState = HookState::UNHOOK;
		int64 profileTotalCost = 0;
		p_tcp tcpSocket = nullptr;
		const char* ChunkName = "[ProfilerScript]";
        TQueue<TArray<LuaMemInfo>> stayMemInfoQueue;

		void makeProfilePackage(FArrayWriter& messageWriter,
			int hookEvent, int64 time,
			int lineDefined, const char* funcName,
			const char* shortSrc)
		{
			uint32 packageSize = 0;

			FString fname = FString(funcName);
			FString fsrc = FString(shortSrc);

			messageWriter << packageSize;
			messageWriter << hookEvent;
			messageWriter << time;
			messageWriter << lineDefined;
			messageWriter << fname;
			messageWriter << fsrc;

			messageWriter.Seek(0);
			packageSize = messageWriter.TotalSize() - sizeof(uint32);
			messageWriter << packageSize;
        }

        void makeMemoryProfilePackage(FArrayWriter& messageWriter,
                                int hookEvent, TArray<LuaMemInfo> memInfoList, int index)
        {
            uint32 packageSize = 0;

            //first hookEvent used to distinguish the message belong to Memory or CPU
            messageWriter << packageSize;
            messageWriter << hookEvent;
            messageWriter << memInfoList;
            messageWriter << index;
            messageWriter.Seek(0);
            packageSize = messageWriter.TotalSize() - sizeof(uint32);
            messageWriter << packageSize;
            
        }

		// copy code from buffer.cpp in luasocket
		#define STEPSIZE 8192
		int sendraw(p_buffer buf, const char* data, size_t count, size_t * sent) {
			p_io io = buf->io;
			if (!io) return IO_CLOSED;
			p_timeout tm = buf->tm;
			size_t total = 0;
			int err = IO_DONE;
			while (total < count && err == IO_DONE) {
				size_t done = 0;
				size_t step = (count - total <= STEPSIZE) ? count - total : STEPSIZE;
				err = io->send(io->ctx, data + total, step, &done, tm);
				total += done;
			}
			*sent = total;
			buf->sent += total;
			return err;
		}

		void sendMessage(FArrayWriter& msg) {
			if (!tcpSocket) return;
			size_t sent;
			int err = sendraw(&tcpSocket->buf, (const char*)msg.GetData(), msg.Num(), &sent);
			if (err != IO_DONE) {
                Log::Log("Lua Staty : send falid %d， err : %d , total : %f , block : %f", memInfoQueueSize, err, tcpSocket->tm.total, tcpSocket->tm.block);
				selfProfiler.callField("disconnect");
			}
		}

		void takeSample(int event,int line,const char* funcname,const char* shortsrc) {
			// clear writer;
			static FArrayWriter s_messageWriter;
			s_messageWriter.Empty();
			s_messageWriter.Seek(0);
			makeProfilePackage(s_messageWriter, event, getTime(), line, funcname, shortsrc);
			sendMessage(s_messageWriter);
		}

        void takeMemorySample(int event, TArray<LuaMemInfo> memoryInfoList, int index) {
            // clear writer;
            static FArrayWriter s_memoryMessageWriter;
            s_memoryMessageWriter.Empty();
            s_memoryMessageWriter.Seek(0);
            makeMemoryProfilePackage(s_memoryMessageWriter, event, memoryInfoList, index);
            sendMessage(s_memoryMessageWriter);
        }

		void debug_hook(lua_State* L, lua_Debug* ar) {
			if (ignoreHook) return;
			
			lua_getinfo(L, "nSl", ar);

			// we don't care about LUA_HOOKLINE, LUA_HOOKCOUNT and LUA_HOOKTAILCALL
			if (ar->event > 1) 
				return;
			if (strstr(ar->short_src, ChunkName)) 
				return;

			takeSample(ar->event,ar->linedefined, ar->name ? ar->name : "", ar->short_src);
		}

		int changeHookState(lua_State* L) {
			HookState state = (HookState)lua_tointeger(L, 1);
			currentHookState = state;
			if (state == HookState::UNHOOK) {
                LuaMemoryProfile::stop();
				lua_sethook(L, nullptr, 0, 0);
			}
			else if (state == HookState::HOOKED) {
                LuaMemoryProfile::start();
				lua_sethook(L, debug_hook, LUA_MASKRET | LUA_MASKCALL, 0);
			}
			else
				luaL_error(L, "Set error value to hook state");
			return 0;
		}

		int setSocket(lua_State* L) {
			if (lua_isnil(L, 1)) {
				tcpSocket = nullptr;
				return 0;
			}
			tcpSocket = (p_tcp)auxiliar_checkclass(L, "tcp{client}", 1);
            Log::Log("Lua Staty : total : %f , block : %f", tcpSocket->tm.total, tcpSocket->tm.block);
            double tot = tcpSocket->tm.total *3;
            double block = tcpSocket->tm.block *3;
            tcpSocket->tm.total += tot;
            tcpSocket->tm.block += block;
			if (!tcpSocket) luaL_error(L, "Set invalid socket");
			return 0;
		}
	}

	void LuaProfiler::init(lua_State* L)
	{
		auto ls = LuaState::get(L);
		ensure(ls);
		selfProfiler = ls->doBuffer((const uint8*)ProfilerScript,strlen(ProfilerScript), ChunkName);
		ensure(selfProfiler.isValid());
		selfProfiler.push(L);
		lua_pushcfunction(L, changeHookState);
		lua_setfield(L, -2, "changeHookState");
		lua_pushcfunction(L, setSocket);
		lua_setfield(L, -2, "setSocket");
		// using native hook instead of lua hook for performance
		// set selfProfiler to global as slua_profiler
		lua_setglobal(L, "slua_profile");
		ensure(lua_gettop(L) == 0);
	}

	void LuaProfiler::tick()
	{
		ignoreHook = true;
		if (currentHookState == HookState::UNHOOK) {
			selfProfiler.callField("reConnect", selfProfiler);
            ignoreHook = false;
            
            TArray<LuaMemInfo> memoryInfoList;
            for(auto& memInfo : NS_SLUA::LuaMemoryProfile::memDetail()) {
                memoryInfoList.Add(memInfo.Value);
            }
            if(memInfoQueueSize > 300)
            {
                stayMemInfoQueue.Pop();
                memInfoQueueSize --;
            }
            stayMemInfoQueue.Enqueue(memoryInfoList);
            memInfoQueueSize ++;
			return;
		}
        
		RunState currentRunState = (RunState)selfProfiler.getFromTable<int>("currentRunState");
		if (currentRunState == RunState::CONNECTED) {
            TArray<LuaMemInfo> memoryInfoList;
            
            while(!stayMemInfoQueue.IsEmpty())
            {
                memoryInfoList.Empty();
                stayMemInfoQueue.Dequeue(memoryInfoList);
                memInfoQueueSize --;
                takeMemorySample(NS_SLUA::ProfilerHookEvent::PHE_MEMORY_TICK, memoryInfoList, memInfoQueueSize);
                Log::Log("Lua Staty :  send %d", memInfoQueueSize);
            }
            
            memoryInfoList.Empty();
            
            for(auto& memInfo : NS_SLUA::LuaMemoryProfile::memDetail()) {
                memoryInfoList.Add(memInfo.Value);
            }
            
            takeMemorySample(NS_SLUA::ProfilerHookEvent::PHE_MEMORY_TICK, memoryInfoList, -1);
			takeSample(NS_SLUA::ProfilerHookEvent::PHE_TICK, -1, "", "");
		}
		ignoreHook = false;
	}

	LuaProfiler::LuaProfiler(const char* funcName)
	{
        takeSample(ProfilerHookEvent::PHE_CALL, 0, funcName, "");
	}

	LuaProfiler::~LuaProfiler()
	{
		takeSample(ProfilerHookEvent::PHE_RETURN, 0, "", "");
	}

}
#endif
