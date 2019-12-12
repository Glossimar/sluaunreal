
// Tencent is pleased to support the open source community by making sluaunreal available.

// Copyright (C) 2018 THL A29 Limited, a Tencent company. All rights reserved.
// Licensed under the BSD 3-Clause License (the "License");
// you may not use this file except in compliance with the License. You may obtain a copy of the License at

// https://opensource.org/licenses/BSD-3-Clause

// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and limitations under the License.

#include "MemorySnapshot.h"
#include "LuaState.h"
#include "Log.h"

#define convert2str(data) FString::Printf(TEXT(data))

namespace NS_SLUA {
    FString chooseMemoryUnit(float memoryByteSize) {
        if(memoryByteSize < 1024.0f) {
            return FString::Printf(TEXT("%d Bytes"), (int)memoryByteSize);
        }
        // form byte to KB
        memoryByteSize /= 1024.0f;
        
        if (memoryByteSize < 1024) {
            return FString::Printf(TEXT("%.3f KB"), memoryByteSize);
        } else if (memoryByteSize < 1024 * 1024) {
            return FString::Printf(TEXT("%.3f MB"), (memoryByteSize /= 1024.0f));
        } else if (memoryByteSize >= 1024 * 1024) {
            return FString::Printf(TEXT("%.3f GB"), (memoryByteSize /= (1024.0f * 1024.0f)));
        }
        
        return FString::Printf(TEXT("%.3f"), memoryByteSize);
    }
    
    void SnapshotMap::SnapshotMap::Empty() {
        typeArray.Empty();
    }
    
    void SnapshotMap::initSnapShotMap(int typeSize){
        for(int i = 0; i < typeSize; i++) {
            MemoryTypeMap map;
            typeArray.Add(map);
        }
    }
    
    bool SnapshotMap::isMarked(const void *pointer){
        if(typeArray[MARKED].Contains(pointer))
           return true;
           
       typeArray[MARKED].Add(pointer);
       return false;
    }
    
    MemoryTypeMap* SnapshotMap::getMemoryMap(int index){
        return &typeArray[index];
    }
    
    /* check memory difference between two SnapshotMap */
    SnapshotMap SnapshotMap::checkMemoryDiff(SnapshotMap previousMap) {
        SnapshotMap diffMap;
        diffMap.initSnapShotMap(MARKED);
        
        for(int i = 0; i < MARKED; i++){
            MemoryTypeMap typeMap = typeArray[i];
            MemoryTypeMap compareTypeMap = *previousMap.getMemoryMap(i);
            
            for(auto &parent : typeArray[i]) {
                if(compareTypeMap.Contains(parent.Key)) {
                    compareTypeMap.Remove(parent.Key);
                } else {
                    diffMap.getMemoryMap(i)->Add(parent.Key, parent.Value);
                }
            }
            
            for(auto &parent : compareTypeMap) {
                // if not contains the key, the key-value is a light c function
                if(parent.Value.Contains(parent.Key)) {
                    int size = -(parent.Value.FindRef(parent.Key).size);
                    parent.Value.Find(parent.Key)->size = size;
                }
                diffMap.getMemoryMap(i)->Add(parent.Key, parent.Value);
            }
        }
        
        return diffMap;
    }
    
    SnapshotMap MemorySnapshot::getMemorySnapshot(lua_State *cL, int typeSize){
        L = cL;
        shotMap.Empty();
        shotMap.initSnapShotMap(typeSize);

        // mark lua registry table
        lua_pushvalue(L, LUA_REGISTRYINDEX);
        markTable(NULL, convert2str("registry table"));

        // mark lua _G root
//        lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
//        markTable(NULL, convert2str("_G root"));
        
        // free pointer L;
        L = nullptr;
        delete L;
        return shotMap;
    }
    
    FString MemorySnapshot::chooseMemoryUnit(float memoryByteSize) {
        // form byte to KB
        memoryByteSize /= 1024.0f;

        if (memoryByteSize < 1024) {
            return FString::Printf(TEXT("%.3f KB"), memoryByteSize);
        } else if (memoryByteSize < 1024 * 1024) {
            return FString::Printf(TEXT("%.3f MB"), (memoryByteSize /= 1024.0f));
        } else if (memoryByteSize >= 1024 * 1024) {
            return FString::Printf(TEXT("%.3f GB"), (memoryByteSize /= (1024.0f * 1024.0f)));
        }
        
        return FString::Printf(TEXT("%.3f"), memoryByteSize);
    }
    
    /* get the value's key in one pair */
    FString MemorySnapshot::getKey(int keyIndex){
        int type = lua_type(L, keyIndex);
        FString keyStr = convert2str("");
        switch (type) {
            case LUA_TSTRING:
                keyStr = FString::Printf(TEXT("%s"), UTF8_TO_TCHAR(lua_tostring(L, keyIndex)));
                break;
            case LUA_TNIL:
                keyStr = convert2str("nil");
                break;
            case LUA_TBOOLEAN:
                keyStr = lua_toboolean(L, keyIndex) ? convert2str("true") : convert2str("false");
                break;
            case LUA_TNUMBER:
                keyStr = FString::Printf(TEXT("%f"), lua_tonumber(L, keyIndex));
                break;
            default:
                keyStr = FString::Printf(TEXT("other type -> %s : %p"), UTF8_TO_TCHAR(lua_typename(L, type)), UTF8_TO_TCHAR(lua_topointer(L, keyIndex)));
                break;
        }
        
        return keyStr;
    }

    /* read the top item in the lua regirstry */
    const void* MemorySnapshot::readObject(const void *parent, FString description){
        int type = lua_type(L, -1);
        int mapType = 0;
        
        switch (type) {
            case LUA_TTABLE:
                mapType = TABLE;
                break;
            case LUA_TFUNCTION:
                mapType = FUNCTION;
                break;
            case LUA_TTHREAD:
                mapType = THREAD;
                break;
            case LUA_TUSERDATA:
                mapType = USERDATA;
                break;
            default:{
                if(description.Equals("table's key", ESearchCase::CaseSensitive))
                    return NULL;
                else
                    mapType = OTHERS;
                break;
            }
        }
        
        const void *pointer = lua_topointer(L, -1);
        if(pointer == NULL) {
            lua_pop(L,1);
            return NULL;
        }
        // check if the item have already recorded
        if(shotMap.isMarked(pointer)){
            // put the item's description and its parent address as child item in its map
            if(shotMap.getMemoryMap(mapType)->Contains(pointer) ||
               shotMap.getMemoryMap(mapType)->Find(pointer) != NULL){
                MemoryNodeMap *childMap = shotMap.getMemoryMap(mapType)->Find(pointer);
                LuaMemInfo memInfo;
                memInfo.hint = description;
                memInfo.ptr = (void *)pointer;
                childMap->Add(parent, memInfo);
            }
            lua_pop(L,1);
            return NULL;
        }

        MemoryNodeMap childMap;
        LuaMemInfo memInfo;
        memInfo.hint = description;
        memInfo.ptr = (void *)parent;
        memInfo.size = 0;
        childMap.Add(pointer, memInfo);
        shotMap.getMemoryMap(mapType)->Add(pointer, childMap);
        return pointer;
    }
    
    void MemorySnapshot::markObject(const void *parent, FString description){
        int type = lua_type(L, -1);
        switch (type) {
            case LUA_TTABLE:
                markTable(parent, description);
                break;
            case LUA_TTHREAD:
                markThread(parent, description);
                break;
            case LUA_TFUNCTION:
                markFunction(parent, description);
                break;
            case LUA_TUSERDATA:
                markUserdata(parent, description);
                break;
            default:{
                if(description.Equals("table's key", ESearchCase::CaseSensitive))
                    lua_pop(L, 1);
                else
                    markOthers(parent, description);
            }
        }
    }
    
    void MemorySnapshot::markTable(const void *parent, FString description){
        int size = 0;
        const void *pointer = readObject(parent, description);
        
        if(pointer == NULL) return;

        // check the mode of table's key and value
        bool weakKey = false;
        bool weakValue = false;
        if(lua_getmetatable(L, -1)) {
            // push the key in top and get the key's value
            lua_pushliteral(L, "__mode");
            lua_rawget(L, -2);
            if(lua_isstring(L, -1)) {
                const char *mode = lua_tostring(L, -1);
                
                if(strchr(mode, 'k'))
                        weakKey = true;
                if(strchr(mode, 'v'))
                        weakValue = true;
            }
            lua_pop(L, 1);
            markTable(pointer, convert2str("metatable"));
        }
        
        
            weakKey = false;
            weakValue = false;
        
        
        //traverce table - regirstry table or the table's value(if value is a table -1:nil, -2:value, -3:key)
        lua_pushnil(L);
        while(lua_next(L, -2)) {
            // traverce statck top item : value(L : -1)
            if(weakValue) {
                lua_pop(L, 1);
            } else {
                FString childDescription = getKey(-2);
                markObject(pointer, childDescription);
            }
            
            if(!weakKey) {
                // copy key and put it onto top to mark top value of stack and keep the stack balance
                lua_pushvalue(L, -1);
                FString tableKey = convert2str("table's key");
                markObject(pointer, tableKey);
            }
        }
        
        // record table size
        Table *h = (Table *)pointer;
        size = sizeof(Table) + sizeof(TValue) * h->sizearray +
                               sizeof(Node) * (h->lastfree == NULL ? 0 : sizenode(h));

        shotMap.getMemoryMap(TABLE)->Find(pointer)->Find(pointer)->size = size;
        lua_pop(L, 1);
    }
    
    void MemorySnapshot::markThread(const void *parent, FString description){
        int size = 0;
        const void *pointer = readObject(parent, description);
        if(pointer == NULL) return;
        
        int level = 0;
        lua_Debug ar;
        FString threadInfo;
        lua_State *tL = lua_tothread(L, -1);
        if(tL == L) level = 1;
        
        // record the thread stack
        while(lua_getstack(tL, level, &ar) && lua_getinfo(tL, "nSl", &ar)){
            threadInfo = FString::Printf(TEXT("%s"), UTF8_TO_TCHAR(ar.source));
            
            if(ar.currentline >= 0)
                threadInfo += FString::Printf(TEXT(":%d"), ar.currentline);
            
            // record the local variable in thread
            for(int i = 1;; i++) {
                const char *variableName = lua_getlocal(L, &ar, i);
                if(variableName == NULL) break;
                
                FString localVariableInfo = FString::Printf(TEXT("local variable name -> %s :%s:%d"),
                                                            UTF8_TO_TCHAR(variableName), UTF8_TO_TCHAR(ar.source), ar.currentline);
                markObject(parent, localVariableInfo);
            }
            level++;
        }
        
        //record thread size
        size = sizeof(lua_State) + sizeof(TValue) * tL->stacksize +
                                   sizeof(CallInfo) * tL->nci;

        MemoryNodeMap map;
        LuaMemInfo memInfo;
        memInfo.hint = threadInfo;
        memInfo.ptr = (void *)pointer;
        memInfo.size = size;
        map.Add(pointer, memInfo);
        shotMap.getMemoryMap(SOURCE)->Add(pointer, map);
        
        lua_pop(L, 1);
    }
    
    void MemorySnapshot::markUserdata(const void *parent, FString description){
        int size = 0;
        bool isTexture = false;
        TArray<FString> strArray;
        
//        // get userdata type info
//        lua_getglobal(L, "tostring");
//        lua_pushvalue(L, -1);   // tostring function to be called
//        lua_pushvalue(L, -3);   // push userdata copy into stack
//        lua_call(L, 1, 1);      // call tostring function to get type info
//
//        const char *name = lua_tostring(L, -1);    // get result
//        FString nameStr = FString::Printf(TEXT("%s"), UTF8_TO_TCHAR(name));
//        int32 arrayNum = nameStr.ParseIntoArray(strArray, TEXT(" ")) - 1;
//        // drop the userdata's address
//        nameStr = TEXT("");
//        for (int i = 0; i < arrayNum; i++) {
//            nameStr += (strArray[i] + TEXT(" "));
//        }
//        description = nameStr + description;
//        lua_pop(L, 2);
        
        // get userdata type info
        UObject* obj = LuaObject::checkUD<UObject>(L, -1, false);
        if (obj) {
            FString clsName = obj->GetClass()->GetFName().ToString();
            FString objName = obj->GetFName().ToString();
            description = FString::Printf(TEXT("%s : %s %s"), *clsName, *objName, *description);
        } else {
            int tt = luaL_getmetafield(L, -1, "__name");
            if(lua_type(L,-1)==LUA_TSTRING) {
                const char *metaname = lua_tostring(L,-1);
                
                description = FString::Printf(TEXT("%s : %s"), *FString(metaname), *description);
            }
            
            if(tt != LUA_TNIL)lua_pop(L, 1);
        }
        
        const void *pointer = readObject(parent, description);
        
        if(pointer == NULL) return;
        
        if(obj) {
            // calculate the UObject size, plus 512 used to output the greatest integer
            size = obj->GetResourceSizeBytes(EResourceSizeMode::Exclusive);
//            Log::Log("%d -> %f", size, ((float)size / 1024.0f));
        }
        
        // if the UObject's size is 0, use lua_rawlen to get size
        if(size == 0) {
            size = lua_rawlen(L, -1);
//            Log::Log("rawlen : %d -> %f", size, ((float)size / 1024.0f));
        }
        shotMap.getMemoryMap(USERDATA)->Find(pointer)->Find(pointer)->size = size;

        // record userdata's metatable
        if(lua_getmetatable(L, -1)) markObject(parent, convert2str("userdata metatable"));
        
        // reocrd userdata's uservalue (used to be called as enviroment)
        lua_getuservalue(L, -1);
        if(lua_istable(L, -1))
            markTable(parent, convert2str("uservalue"));
        else
            lua_pop(L, 1);
        
        lua_pop(L, 1);
    }
    
    void MemorySnapshot::markFunction(const void *parent, FString description){
        int size = 0;
        const void *pointer = readObject(parent, description);
        
        if(pointer == NULL)return;
        
        // TODO : record enviroment
       
        // record function upvalue;
        int upvalueIndex = 1;
        while(true) {
            const char *upvalueName = lua_getupvalue(L, -1, upvalueIndex);

            if(upvalueName == NULL) break;

            upvalueIndex++;
            markObject(pointer, FString::Printf(TEXT("upvalue : %s"),
                                                *upvalueName == '\0' ?
                                                *convert2str("c function upvalue") :
                                                UTF8_TO_TCHAR(upvalueName)
                                                ));
        }
        
        // record c function or lua closure
        Closure *cl = (Closure *)pointer;
        if(lua_iscfunction(L, -1)){
            // the c function have no upvalue means that it is a light c function
            if(upvalueIndex == 1) {
                // NULL value use to judge whether the function map item is light c function
                shotMap.getMemoryMap(FUNCTION)->FindAndRemoveChecked(pointer);
                shotMap.getMemoryMap(FUNCTION)->Add(pointer);
            } else {
                // reocrd c function size
                size = sizeof(CClosure) + sizeof(TValue) * (cl->c.nupvalues - 1);
                shotMap.getMemoryMap(FUNCTION)->Find(pointer)->Find(pointer)->size = size;
            }
            lua_pop(L, 1);
        } else {
            // get lua closure debug info
            lua_Debug ar;

            lua_getinfo(L, ">Sln", &ar);
            FString info = FString::Printf(TEXT("%s:%d"), UTF8_TO_TCHAR(ar.short_src), ar.linedefined);
            size = sizeof(LClosure) + sizeof(TValue *) * (cl->l.nupvalues - 1);
            
            MemoryNodeMap map;
            LuaMemInfo memInfo;
            memInfo.hint = info;
            memInfo.size = size;
            memInfo.ptr = (void *)pointer;
            map.Add(pointer, memInfo);
            shotMap.getMemoryMap(SOURCE)->Add(pointer, map);
        }
    }
    
    /* mark lua string, number, boolean, nil types */
    void MemorySnapshot::markOthers(const void *parent, FString description){
        int size = 0;
        const void *pointer = readObject(parent, description);
        
        if(pointer == NULL) return;
        
        lua_pop(L, 1);
    }
    
    /* print the map recording lua memory*/
    void SnapshotMap::printMap(SnapshotMap shotMap) {
        for(int i = 0; i < MARKED; i++){
            if(i == SOURCE) continue;
            
            FString mapType = convert2str("");
            switch (i) {
                case TABLE:
                    mapType = convert2str("Table");
                    break;
                case FUNCTION:
                    mapType = convert2str("Function");
                    break;
                case THREAD:
                    mapType = convert2str("Thread");
                    break;
                case USERDATA:
                    mapType = convert2str("Userdata");
                    break;
                default:
                    mapType = convert2str("OtherType");
                    break;
            }
            
            MemoryTypeMap map = *shotMap.getMemoryMap(i);
            for(auto &parent : map) {
                FString memInfo = convert2str("");
                memInfo = mapType + FString::Printf(TEXT(" -> Item Address : %p "), parent.Key);
                
                if(i == FUNCTION) {
                    MemoryNodeMap *sourceMap = shotMap.getMemoryMap(SOURCE)->Find(parent.Key);
                    FString source;
                    
                    if(sourceMap == NULL) {
                        source = convert2str("Light c function");
                        memInfo += source;
                        continue;
                    } else {
                        FString funcInfo =  sourceMap->Contains(parent.Key) ?
                                            sourceMap->FindRef(parent.Key).hint :
                                            convert2str("");
                        FString funcName =  parent.Value.Contains(parent.Key) ?
                                            parent.Value.FindRef(parent.Key).hint:
                                            convert2str("");
                        FString funcSize =  parent.Value.Contains(parent.Key) ?
                                            chooseMemoryUnit(sourceMap->FindRef(parent.Key).size) :
                                            convert2str("");
                        
                        memInfo += FString::Printf(TEXT("%s, name : %s, size : %s "), *funcInfo, *funcName, *funcSize);
                        Log::Log("%s", TCHAR_TO_UTF8(*memInfo));
                        continue;
                    }
                } else if(i == THREAD) {
                    MemoryNodeMap *sourceMap =  shotMap.getMemoryMap(SOURCE)->Find(parent.Key);
                    FString threadInfo = sourceMap->Contains(parent.Key) ?
                                                sourceMap->FindRef(parent.Key).hint :
                                                convert2str("");
                    FString threadSize = parent.Value.Contains(parent.Key) ?
                                        chooseMemoryUnit(sourceMap->FindRef(parent.Key).size) :
                                        convert2str("");
                    
                    memInfo += FString::Printf(TEXT("%s , size : %s "),  *threadInfo, *threadSize);
                } else {
                    FString info =  parent.Value.Contains(parent.Key) ?
                                    parent.Value.FindRef(parent.Key).hint :
                                    convert2str("");
                    FString size =  parent.Value.Contains(parent.Key) ?
                                    chooseMemoryUnit(parent.Value.FindRef(parent.Key).size) :
                                    convert2str("");
                    memInfo += FString::Printf(TEXT(" %s , size : %s "), *info, *size);
                }
                
                for(auto &child : parent.Value) {
                    if(child.Key == parent.Key) continue;
                    FString size = chooseMemoryUnit(child.Value.size);
//                    memInfo = FString::Printf(TEXT("Child : address :%p\tvalue : %s"), child.Key, *child.Value);
                    memInfo += FString::Printf(TEXT("\nChild value : %s , parent : %p, address : %p , size : %s"), *child.Value.hint, child.Key, child.Value.ptr, *chooseMemoryUnit(child.Value.size));
                }
                Log::Log("%s", TCHAR_TO_UTF8(*memInfo));
            }
        }
    }
}
