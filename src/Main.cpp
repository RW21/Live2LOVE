/**
 * Copyright (c) 2040 Dark Energy Processor Corporation
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 **/

// Lua
extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

// Live2LOVE
#include "Live2LOVE.h"
using namespace live2love;

// JSON
#include "picojson.h"

// RefData
#include "RefData.h"

#define L2L_TRYWRAP(expr) {try { expr } catch(std::exception &x) { lua_settop(L, 0); luaL_error(L, x.what()); }}

// idx must be positive
inline const void *getLoveData(lua_State *L, int idx, size_t &size)
{
	// Get size
	lua_getfield(L, idx, "getSize");
	lua_pushvalue(L, idx);
	lua_call(L, 1, 1);
	size = lua_tointeger(L, -1);
	lua_pop(L, 1);
	// Get pointer
	lua_getfield(L, idx, "getPointer");
	lua_pushvalue(L, idx);
	lua_call(L, 1, 1);
	const void *ptr = lua_topointer(L, -1);
	lua_pop(L, 1);
	return ptr;
}

// Push data to stack
const void *argToData(lua_State *L, int idx, size_t &size)
{
	idx = idx < 0 ? (lua_gettop(L) + 1 - idx) : idx;
	int ltype = lua_type(L, idx);

	if (ltype == LUA_TSTRING)
	{
		size_t length;
		const char *data = lua_tolstring(L, idx, &length);

		// length is higher than 512 is considered file contents
		if (length >= 512)
		{
			size = length;
			lua_pushvalue(L, idx);
			return data;
		}
		
		for (size_t i = 0; i < length; i++)
		{
			// if line contains 0x1A or 0x0A then it's assumed to be
			// file contents too
			if (data[i] == 26 || data[i] == 10)
			{
				size = length;
				lua_pushvalue(L, idx);
				return data;
			}
		}

		RefData::getRef(L, "love.filesystem.read");
		lua_pushstring(L, "data");
		lua_pushvalue(L, idx);
		lua_call(L, 2, 2);

		if (lua_isnil(L, -2))
		{
			lua_remove(L, -2);
			lua_error(L);
		}

		// FileData is now at top
		lua_pop(L, 1);
		return getLoveData(L, lua_gettop(L), size);
	}
	else if (ltype == LUA_TUSERDATA)
	{
		lua_getfield(L, idx, "typeOf");

		if (lua_isnil(L, -1))
		{
			// Okay it's probably Lua FILE* object
			lua_pop(L, 1);
			lua_pushvalue(L, idx);
			const char *strp = lua_tostring(L, -1);

			if (strstr(strp, "file (") == strp)
			{
				// Okay it's FILE* handle
				lua_pop(L, 1);
				lua_getfield(L, idx, "read");
				lua_pushvalue(L, idx);
				lua_pushstring(L, "*a");
				lua_call(L, 2, 2);

				if (lua_isnil(L, -2))
				{
					lua_remove(L, -2);
					lua_error(L);
				}

				lua_pop(L, 1);
				return lua_tolstring(L, -1, &size);
			}
		}

		// Apparently it's LOVE object
		lua_pushvalue(L, -2);
		lua_pushstring(L, "File");
		lua_pushvalue(L, -3);
		lua_pushvalue(L, -3);
		lua_pushstring(L, "Data");
		lua_call(L, 3, 1);
		bool isData = lua_toboolean(L, -1) != 0;
		lua_pop(L, 1);
		lua_call(L, 3, 1);
		bool isFile = lua_toboolean(L, -1) != 0;
		lua_pop(L, 1);

		if (isData)
		{
			lua_pushvalue(L, idx);
			return getLoveData(L, idx, size);
		}
		else if (isFile)
		{
			lua_getfield(L, idx, "read");
			lua_pushvalue(L, idx);
			lua_pushstring(L, "data");
			lua_call(L, 2, 1);
			return getLoveData(L, lua_gettop(L), size);
		}
		else
			luaL_argerror(L, idx, "Data or File expected");
	}
	else
		luaL_argerror(L, idx, "Data, File, or string expected");

	return nullptr;
}

int Live2LOVE_setTexture(lua_State *L)
{
	// Get udata
	Live2LOVE *l2l = *(Live2LOVE**)luaL_checkudata(L, 1, "Live2LOVE");
	// Check live2d tex number
	lua_Integer texno = luaL_checkinteger(L, 2);
	// If it's userdata, then assume it's LOVE object
	luaL_checktype(L, 3, LUA_TUSERDATA);
	// Call
	L2L_TRYWRAP(l2l->setTexture(texno, 3););

	return 0;
}

int Live2LOVE_setParamValue(lua_State *L)
{
	// Get udata
	Live2LOVE *l2l = *(Live2LOVE**)luaL_checkudata(L, 1, "Live2LOVE");
	// Get param name
	size_t nameLen; const char *name = luaL_checklstring(L, 2, &nameLen);
	// Get value
	double value = luaL_checknumber(L, 3);
	// Get weight
	double weight = luaL_optnumber(L, 4, 1.0);
	// Call
	L2L_TRYWRAP(l2l->setParamValue(std::string(name, nameLen), value, weight););

	return 0;
}

int Live2LOVE_setParamValuePost(lua_State *L)
{
	// Get udata
	Live2LOVE *l2l = *(Live2LOVE**)luaL_checkudata(L, 1, "Live2LOVE");
	// Get param name
	size_t nameLen; const char *name = luaL_checklstring(L, 2, &nameLen);
	// Get value
	double value = luaL_checknumber(L, 3);
	// Get weight
	double weight = luaL_optnumber(L, 4, 1.0);
	// Call
	L2L_TRYWRAP(l2l->setParamValuePost(std::string(name, nameLen), value, weight););

	return 0;
}

// Copypaste from Live2LOVE_setParamValue
int Live2LOVE_addParamValue(lua_State *L)
{
	// Get udata
	Live2LOVE *l2l = *(Live2LOVE**)luaL_checkudata(L, 1, "Live2LOVE");
	// Get param name
	size_t nameLen; const char *name = luaL_checklstring(L, 2, &nameLen);
	// Get value
	double value = luaL_checknumber(L, 3);
	// Get weight
	double weight = luaL_optnumber(L, 4, 1.0);
	// Call
	L2L_TRYWRAP(l2l->addParamValue(std::string(name, nameLen), value, weight););

	return 0;
}

// Copypaste from Live2LOVE_setParamValue
int Live2LOVE_mulParamValue(lua_State *L)
{
	// Get udata
	Live2LOVE *l2l = *(Live2LOVE**)luaL_checkudata(L, 1, "Live2LOVE");
	// Get param name
	size_t nameLen; const char *name = luaL_checklstring(L, 2, &nameLen);
	// Get value
	double value = luaL_checknumber(L, 3);
	// Get weight
	double weight = luaL_optnumber(L, 4, 1.0);
	// Call
	L2L_TRYWRAP(l2l->mulParamValue(std::string(name, nameLen), value, weight););

	return 0;
}

int Live2LOVE_getParamValue(lua_State *L)
{
	// Get udata
	Live2LOVE *l2l = *(Live2LOVE**)luaL_checkudata(L, 1, "Live2LOVE");
	// Get param name
	size_t nameLen; const char *name = luaL_checklstring(L, 2, &nameLen);
	double value;
	// Call
	L2L_TRYWRAP(value = l2l->getParamValue(std::string(name, nameLen)););

	// Push value
	lua_pushnumber(L, value);
	return 1;
}

int Live2LOVE_getParamInfoList(lua_State *L)
{
	// Get udata
	Live2LOVE *l2l = *(Live2LOVE**)luaL_checkudata(L, 1, "Live2LOVE");
	// Get list
	auto x = l2l->getParamInfoList();

	lua_createtable(L, x->size(), 0);
	int i = 0;
	for (auto& y: *x)
	{
		lua_createtable(L, 0, 4);
		lua_pushstring(L, "name");
		lua_pushstring(L, y->getParamID()->toChar());
		lua_rawset(L, -3);
		lua_pushstring(L, "min");
		lua_pushnumber(L, y->getMinValue());
		lua_rawset(L, -3);
		lua_pushstring(L, "max");
		lua_pushnumber(L, y->getMaxValue());
		lua_rawset(L, -3);
		lua_pushstring(L, "default");
		lua_pushnumber(L, y->getDefaultValue());
		lua_rawset(L, -3);
		lua_rawseti(L, -2, ++i);
	}

	return 1;
}

/*
static std::map<std::string, int> motionMapMode = {
	{"normal", 0},
	{"loop", 1},
	{"preserve", 2}
};
*/

static std::vector<std::string> motionStringMode = {"normal", "loop", "preserve"};

int Live2LOVE_setMotion(lua_State *L)
{
	size_t motionNameLen;
	// Get udata
	Live2LOVE *l2l = *(Live2LOVE**)luaL_checkudata(L, 1, "Live2LOVE");
	// If gettop is 0, that means stop all motions
	if (lua_gettop(L) == 1)
		L2L_TRYWRAP(l2l->setMotion();)
	else
	{
		// Get string
		const char *motionName = luaL_checklstring(L, 2, &motionNameLen);
		// Get motion mode
		live2love::MotionModeID mode = MOTION_MAX_ENUM;
		if (lua_isnoneornil(L, 3)) mode = MOTION_NORMAL;
		else if (lua_isnumber(L, 3)) mode = (MotionModeID) lua_tointeger(L, 3);
		else if (lua_isstring(L, 3))
		{
			std::string modeStr = std::string(lua_tostring(L, 3));

			for (int i = 0; i < MOTION_MAX_ENUM; i++)
			{
				if (motionStringMode[i] == modeStr)
				{
					mode = (MotionModeID) i;
					break;
				}
			}

			if (mode == MOTION_MAX_ENUM)
				luaL_argerror(L, 3, "invalid mode");
		}
		else luaL_typerror(L, 3, "string or number");
		if (mode < 0 || mode > 2) luaL_argerror(L, 3, "invalid mode");
		// Call
		L2L_TRYWRAP(l2l->setMotion(std::string(motionName, motionNameLen), mode););
	}

	return 0;
}

int Live2LOVE_setAnimationMovement(lua_State *L)
{
	Live2LOVE *l2l = *(Live2LOVE**)luaL_checkudata(L, 1, "Live2LOVE");
	luaL_checktype(L, 2, LUA_TBOOLEAN);
	l2l->setAnimationMovement(lua_toboolean(L, 2) != 0);
	return 0;
}

int Live2LOVE_setEyeBlinkMovement(lua_State *L)
{
	Live2LOVE *l2l = *(Live2LOVE**)luaL_checkudata(L, 1, "Live2LOVE");
	luaL_checktype(L, 2, LUA_TBOOLEAN);
	l2l->setEyeBlinkMovement(lua_toboolean(L, 2) != 0);
	return 0;
}

int Live2LOVE_loadMotion(lua_State *L)
{
	size_t motionNameLen, size;
	// Get udata
	Live2LOVE *l2l = *(Live2LOVE**)luaL_checkudata(L, 1, "Live2LOVE");
	// Get name and path
	const char *motionName = luaL_checklstring(L, 2, &motionNameLen);
	double fadeIn = luaL_checknumber(L, 3);
	double fadeOut = luaL_checknumber(L, 4);
	const void *buf = argToData(L, 5, size);

	L2L_TRYWRAP(l2l->loadMotion(
		std::string(motionName, motionNameLen),
		std::pair<double, double>(fadeIn, fadeOut),
		buf, size
	););

	lua_pop(L, 1);
	return 0;
}

// Just copypaste from Live2LOVE_setMotion
int Live2LOVE_setExpression(lua_State *L)
{
	size_t motionNameLen;
	// Get udata
	Live2LOVE *l2l = *(Live2LOVE**)luaL_checkudata(L, 1, "Live2LOVE");
	// Get string
	const char *motionName = luaL_checklstring(L, 2, &motionNameLen);
	// Call
	L2L_TRYWRAP(l2l->setExpression(std::string(motionName, motionNameLen)););

	return 0;
}

// Just copypaste from Live2LOVE_loadMotion
int Live2LOVE_loadExpression(lua_State *L)
{
	size_t motionNameLen, size;
	// Get udata
	Live2LOVE *l2l = *(Live2LOVE**)luaL_checkudata(L, 1, "Live2LOVE");
	// Get name and path
	const char *motionName = luaL_checklstring(L, 2, &motionNameLen);
	const void *buf = argToData(L, 3, size);
	L2L_TRYWRAP(l2l->loadExpression(std::string(motionName, motionNameLen), buf, size););

	return 0;
}

int Live2LOVE_loadPhysics(lua_State *L)
{
	size_t physLen;
	// Get udata
	Live2LOVE *l2l = *(Live2LOVE**)luaL_checkudata(L, 1, "Live2LOVE");
	// Get name and path
	const void *phys = argToData(L, 2, physLen);
	L2L_TRYWRAP(l2l->loadPhysics(phys, physLen););

	return 0;
}

int Live2LOVE_update(lua_State *L)
{
	Live2LOVE *l2l = *(Live2LOVE**)luaL_checkudata(L, 1, "Live2LOVE");
	double dT = luaL_optnumber(L, 2, 0);
	L2L_TRYWRAP(l2l->update(dT););
	return 0;
}

int Live2LOVE_draw(lua_State *L)
{
	Live2LOVE *l2l = *(Live2LOVE**)luaL_checkudata(L, 1, "Live2LOVE");
	double x = luaL_optnumber(L, 2, 0);
	double y = luaL_optnumber(L, 3, 0);
	double r = luaL_optnumber(L, 4, 0);
	double sx = luaL_optnumber(L, 5, 1);
	double sy = luaL_optnumber(L, 6, 1);
	double ox = luaL_optnumber(L, 7, 0);
	double oy = luaL_optnumber(L, 8, 0);
	double kx = luaL_optnumber(L, 9, 0);
	double ky = luaL_optnumber(L, 10, 0);
	L2L_TRYWRAP(l2l->draw(x, y, r, sx, sy, ox, oy, kx, ky););
	return 0;
}

int Live2LOVE_getMeshCount(lua_State *L)
{
	Live2LOVE *l2l = *(Live2LOVE**)luaL_checkudata(L, 1, "Live2LOVE");
	lua_pushinteger(L, l2l->meshData.size());
	return 1;
}

int Live2LOVE_getMesh(lua_State *L)
{
	Live2LOVE *l2l = *(Live2LOVE**)luaL_checkudata(L, 1, "Live2LOVE");
	size_t meshLen = l2l->meshData.size();
	if (lua_isnumber(L, 2))
	{
		// Individual mesh
		int index = luaL_checkinteger(L, 2);
		if (index <= 0 || index > meshLen) luaL_argerror(L, 2, "index out of range");

		RefData::getRef(L, l2l->meshData[index - 1]->meshRefID);
	}
	else
	{
		// All mesh in a table
		int i = 0;
		lua_createtable(L, meshLen, 0);
		for (auto x: l2l->meshData)
		{
			lua_pushinteger(L, ++i);
			RefData::getRef(L, x->meshRefID);
			lua_rawset(L, -3);
		}
	}
	return 1;
}

int Live2LOVE_getDimensions(lua_State *L)
{
	Live2LOVE *l2l = *(Live2LOVE**)luaL_checkudata(L, 1, "Live2LOVE");
	std::pair<float, float> dim = l2l->getDimensions();
	lua_pushnumber(L, dim.first);
	lua_pushnumber(L, dim.second);
	return 2;
}

int Live2LOVE_getWidth(lua_State *L)
{
	Live2LOVE *l2l = *(Live2LOVE**)luaL_checkudata(L, 1, "Live2LOVE");
	lua_pushnumber(L, l2l->getDimensions().first);
	return 1;
}

int Live2LOVE_getHeight(lua_State *L)
{
	Live2LOVE *l2l = *(Live2LOVE**)luaL_checkudata(L, 1, "Live2LOVE");
	lua_pushnumber(L, l2l->getDimensions().second);
	return 1;
}

int Live2LOVE_getExpressionList(lua_State *L)
{
	Live2LOVE *l2l = *(Live2LOVE**)luaL_checkudata(L, 1, "Live2LOVE");
	lua_createtable(L, l2l->expressionList.size(), 0);
	
	int i = 0;
	for (auto& x: l2l->getExpressionList())
	{
		lua_pushlstring(L, x->c_str(), x->length());
		lua_rawseti(L, -2, ++i);
	}
	return 1;
}

int Live2LOVE_getMotionList(lua_State *L)
{
	Live2LOVE *l2l = *(Live2LOVE**)luaL_checkudata(L, 1, "Live2LOVE");
	lua_createtable(L, l2l->motionList.size(), 0);
	
	int i = 0;
	for (auto& x: l2l->getMotionList())
	{
		lua_pushlstring(L, x->c_str(), x->length());
		lua_rawseti(L, -2, ++i);
	}
	return 1;
}

int Live2LOVE_isAnimationMovementEnabled(lua_State *L)
{
	Live2LOVE *l2l = *(Live2LOVE**)luaL_checkudata(L, 1, "Live2LOVE");
	lua_pushboolean(L, l2l->isAnimationMovementEnabled());
	return 1;
}

int Live2LOVE_isEyeBlinkEnabled(lua_State *L)
{
	Live2LOVE *l2l = *(Live2LOVE**)luaL_checkudata(L, 1, "Live2LOVE");
	lua_pushboolean(L, l2l->isEyeBlinkEnabled());
	return 1;
}

int Live2LOVE___gc(lua_State *L)
{
	Live2LOVE **x = (Live2LOVE**)luaL_checkudata(L, 1, "Live2LOVE");
	delete *x;
	*x = nullptr;
	return 0;
}

int Live2LOVE___tostring(lua_State *L)
{
	Live2LOVE *x = *(Live2LOVE**)luaL_checkudata(L, 1, "Live2LOVE");
	lua_pushfstring(L, "Live2LOVE Model: %p", x);
	return 1;
}

static luaL_Reg Live2LOVE_methods[] = {
	{"setTexture", Live2LOVE_setTexture},
	{"setAnimationMovement", Live2LOVE_setAnimationMovement},
	{"setEyeBlinkMovement", Live2LOVE_setEyeBlinkMovement},
	{"setParamValue", Live2LOVE_setParamValue},
	{"setParamValuePost", Live2LOVE_setParamValuePost},
	{"addParamValue", Live2LOVE_addParamValue},
	{"mulParamValue", Live2LOVE_mulParamValue},
	{"setMotion", Live2LOVE_setMotion},
	{"setExpression", Live2LOVE_setExpression},
	{"loadMotion", Live2LOVE_loadMotion},
	{"loadExpression", Live2LOVE_loadExpression},
	{"loadPhysics", Live2LOVE_loadPhysics},
	{"getParamValue", Live2LOVE_getParamValue},
	{"getParamInfoList", Live2LOVE_getParamInfoList},
	{"getMesh", Live2LOVE_getMesh},
	{"getMeshCount", Live2LOVE_getMeshCount},
	{"getExpressionList", Live2LOVE_getExpressionList},
	{"getMotionList", Live2LOVE_getMotionList},
	{"getWidth", Live2LOVE_getWidth},
	{"getHeight", Live2LOVE_getHeight},
	{"getDimensions", Live2LOVE_getDimensions},
	{"isAnimationMovementEnabled", Live2LOVE_isAnimationMovementEnabled},
	{"isEyeBlinkEnabled", Live2LOVE_isEyeBlinkEnabled},
	{"update", Live2LOVE_update},
	{"draw", Live2LOVE_draw}
};

// Load model file (basic)
int Live2LOVE_Live2LOVE(lua_State *L)
{
	size_t mocSize;
	Live2LOVE *l2l = nullptr;
	// Get path
	const void *mocBuf = argToData(L, 1, mocSize);
	// Call constructor
	L2L_TRYWRAP(l2l = new Live2LOVE(L, mocBuf, mocSize););
	// Create new user data
	Live2LOVE **obj = (Live2LOVE**)lua_newuserdata(L, sizeof(Live2LOVE*));
	*obj = l2l;
	// Set userdata metatable
	luaL_getmetatable(L, "Live2LOVE");
	lua_setmetatable(L, -2);
	// Return Live2LOVE object
	return 1;
}

const void *loadFileData(lua_State *L, const std::string& path, size_t &fileSize)
{
	// New file data
	RefData::getRef(L, "love.filesystem.newFileData");
	lua_pushlstring(L, path.c_str(), path.length());
	lua_call(L, 1, 2);
	if (lua_isnil(L, -2))
		lua_error(L);

	// We now have FileData at -2 index, so pop 1 value
	lua_pop(L, 1);
	// Now FileData in -1
	return getLoveData(L, lua_gettop(L), fileSize);
}

// Live2D JSON parser seems to append "garbage"
// (many "/"), so this code is used to fix it.
const char *fixJsonPaths(const char *src)
{
	if (strstr(src, "//") == src)
	{}
	return nullptr;
}

// Load model file (full)
int Live2LOVE_Live2LOVE_full(lua_State *L)
{
	// Checkstack
	luaL_checkstack(L, lua_gettop(L) + 24, "Internal error: cannot grow Lua stack size");
	size_t fileLen, dataSize;
	const char *file = luaL_checklstring(L, 1, &fileLen);
	std::string filename = std::string(file, fileLen);
	const char *data;
	L2L_TRYWRAP(data = (const char *) loadFileData(L, filename, dataSize););

	// Parse JSON
	picojson::value json;
	std::string parseErr;
	picojson::parse(json, data, data + dataSize, &parseErr);

	if (!parseErr.empty())
		luaL_error(L, parseErr.c_str());

	if (!json.is<picojson::object>())
		luaL_error(L, "Root is not an object");

	auto &root = json.get<picojson::object>();

	// Get dir path
	std::string dir = "";
	{
		size_t last = filename.rfind('/');
		if (last != std::string::npos)
			dir = filename.substr(0, last + 1);
	}

	// Load model file
	if (root.count("model") == 0)
		luaL_error(L, "\"model\" field is missing");

	auto &model = root["model"];
	if (!model.is<std::string>())
		luaL_error(L, "\"model\" field is not a string");

	Live2LOVE *l2l = nullptr;
	size_t modelSize;
	const void *modelData = loadFileData(L, dir + model.get<std::string>(), modelSize);
	L2L_TRYWRAP(l2l = new Live2LOVE(L, modelData, modelSize););

	// Textures
	if (root.count("textures") != 0)
	{
		auto &textures = root["textures"];
		if (!textures.is<picojson::array>())
		{
			delete l2l;
			luaL_error(L, "\"textures\" is not array");
		}

		// Check love.graphics.newImage settings
		if (!lua_istable(L, 2))
		{
			// Not have one. Make new one (mipmaps is true by default)
			lua_createtable(L, 0, 1);
			lua_pushboolean(L, 1);
			lua_setfield(L, -2, "mipmaps");
		}
		else
			lua_pushvalue(L, 2);

		// New image function
		RefData::getRef(L, "love.graphics.newImage");
		// Loop
		auto &tex = textures.get<picojson::array>();
		for (int i = 0; i < tex.size(); i++)
		{
			auto &strpathval = tex[i];
			if (!strpathval.is<std::string>())
			{
				delete l2l;
				luaL_error(L, "\"textures\"[%d] is not a string", i);
			}

			lua_pushvalue(L, -1);
			std::string texPath = dir + strpathval.get<std::string>();

			// If no extension, provide one
			if (texPath.substr(texPath.length() - 4, 4) != ".png")
				texPath += ".png";
			lua_pushlstring(L, texPath.c_str(), texPath.length());
			lua_pushvalue(L, -4);

			// Call love.graphics.newImage(texPath, {mipmaps = true})
			lua_call(L, 2, 1);
			l2l->setTexture(i + 1, lua_gettop(L));
			lua_pop(L, 1);
		}
		lua_pop(L, 2);
	}

	// Must be in try-catch block
	try
	{
		// Expressions
		if (root.count("expressions"))
		{
			auto &expressionList = root["expressions"].get<picojson::array>();
			std::string defaultExpr = "";

			// Loop expressions
			for (int i = 0; i < expressionList.size(); i++)
			{
				auto &v = expressionList[i].get<picojson::object>();
				std::string exprName = v["name"].get<std::string>();

				if (defaultExpr.length() == 0 && exprName.find("default") != std::string::npos)
					defaultExpr = exprName;

				// Load
				size_t exprSize;
				const void *expr = loadFileData(L, dir + v["file"].get<std::string>(), exprSize);
				l2l->loadExpression(exprName, expr, exprSize);
				lua_pop(L, 1);
			}

			// Set as default
			if (defaultExpr.length() > 0)
				l2l->setExpression(defaultExpr);
		}

		// Motion
		if (root.count("motions"))
		{
			std::string idleMotion;
			for (auto& x: root["motions"].get<picojson::object>())
			{
				std::string name = x.first;
				auto& motionObject = x.second.get<picojson::array>();

				if (idleMotion.length() == 0 && name.find("idle") == 0)
					idleMotion = name;

				if (motionObject.size() > 1)
				{
					// Load multiple motion
					for (int j = 1; j <= motionObject.size(); j++)
					{
						auto &motionInfo = motionObject[j - 1].get<picojson::object>();
						std::string mName = name + ":" + std::to_string(j);

						if (idleMotion.length() == 0 && mName.find("idle") == 0)
							idleMotion = mName;

						double fadeIn = 1000;
						if (motionInfo.count("fade_in"))
						{
							auto &fadeInVal = motionInfo["fade_in"];
							if (fadeInVal.is<double>())
								fadeIn = fadeInVal.get<double>();
						}

						double fadeOut = 1000;
						if (motionInfo.count("fade_out"))
						{
							auto &fadeOutVal = motionInfo["fade_out"];
							if (fadeOutVal.is<double>())
								fadeOut = fadeOutVal.get<double>();
						}

						size_t motionSize;
						const void *motionData = loadFileData(L, dir + motionInfo["file"].get<std::string>(), motionSize);
						l2l->loadMotion(
							mName,
							std::pair<double, double>(fadeIn, fadeOut),
							motionData, motionSize
						);
						lua_pop(L, 1);
					}
				}
				else
				{
					auto &motionInfo = motionObject[0].get<picojson::object>();

					if (idleMotion.length() == 0 && name.find("idle") == 0)
						idleMotion = name;

					double fadeIn = 1000;
					if (motionInfo.count("fade_in"))
					{
						auto &fadeInVal = motionInfo["fade_in"];
						if (fadeInVal.is<double>())
							fadeIn = fadeInVal.get<double>();
					}

					double fadeOut = 1000;
					if (motionInfo.count("fade_out"))
					{
						auto &fadeOutVal = motionInfo["fade_out"];
						if (fadeOutVal.is<double>())
							fadeOut = fadeOutVal.get<double>();
					}

					size_t motionSize;
					const void *motionData = loadFileData(L, dir + motionInfo["file"].get<std::string>(), motionSize);

					// Load single motion.
					l2l->loadMotion(
						name,
						std::pair<double, double>(fadeIn, fadeOut),
						motionData, motionSize
					);
					lua_pop(L, 1);
				}
			}

			// Set default motion
			if (idleMotion.length() > 0)
				l2l->setMotion(idleMotion, MOTION_LOOP);
		}

		// Physics
		if (root.count("physics"))
		{
			auto &physicsInfo = root["physics"];
			if (physicsInfo.is<std::string>())
			{
				// Load physics
				size_t physicsSize;
				const void *physics = loadFileData(L, dir + physicsInfo.get<std::string>(), physicsSize);
				l2l->loadPhysics(physics, physicsSize);
				lua_pop(L, 1);
			}
		}
	}
	catch (std::exception &e)
	{
		delete l2l;
		luaL_error(L, e.what());
	}

	// New user data
	Live2LOVE **ptr = (Live2LOVE**)lua_newuserdata(L, sizeof(Live2LOVE*));
	l2l->model->saveParam();
	*ptr = l2l;
	luaL_getmetatable(L, "Live2LOVE");
	lua_setmetatable(L, -2);

	// Return new object
	return 1;
}

extern "C" int LUALIB_API luaopen_Live2LOVE(lua_State *L)
{
	// Initialize Live2D
	{
		unsigned int err; live2d::Live2D::init();
		if ((err = live2d::Live2D::getError()) != live2d::Live2D::L2D_NO_ERROR)
			luaL_error(L, "Live2D initialize error: %u",err);
	}

	// Create new Live2LOVE metatable
	luaL_newmetatable(L, "Live2LOVE");
	// Setup function methods
	lua_pushstring(L, "__gc");
	lua_pushcfunction(L, Live2LOVE___gc);
	lua_rawset(L, -3);
	lua_pushstring(L, "__tostring");
	lua_pushcfunction(L, Live2LOVE___tostring);
	lua_rawset(L, -3);
	lua_pushstring(L, "__index");
	lua_createtable(L, 0, 0);
	// Export methods
	for (auto& x: Live2LOVE_methods)
	{
		lua_pushstring(L, x.name);
		lua_pushcfunction(L, x.func);
		lua_rawset(L, -3);
	}
	lua_rawset(L, -3); // For the Live2LOVE metatable. set __index to table
	lua_pop(L, 1); // Remove the metatable from stack for now.

	// Setup needed LOVE functions
	lua_getfield(L, LUA_GLOBALSINDEX, "package");
	lua_getfield(L, -1, "loaded");
	lua_remove(L, -2); // remove package
	lua_getfield(L, -1, "love");
	lua_remove(L, -2); // remove loaded

	// Check if LOVE module is loaded. If it's not then throw error.
	if (lua_isnil(L, -1))
		luaL_error(L, "love module is not loaded!");

	// Check love version
	lua_getfield(L, -1, "_version_major");
	if (lua_tointeger(L, -1) < 11)
		luaL_error(L, "Live2LOVE requires at least LOVE 11.0");
	lua_pop(L, 1); // pop _version_major

	lua_getfield(L, -1, "graphics");
	// Check if love.graphics module is loaded.
	if (lua_isnil(L, -1))
		luaL_error(L, "love.graphics module is not loaded!");

	// Get love.graphics.draw
	lua_getfield(L, -1, "draw");
	RefData::setRef(L, "love.graphics.draw", -1);
	lua_pop(L, 1); // pop the function
	// Get needed function
	lua_getfield(L, -1, "newMesh");
	RefData::setRef(L, "love.graphics.newMesh", -1);
	lua_pop(L, 1);
	lua_getfield(L, -1, "setBlendMode");
	RefData::setRef(L, "love.graphics.setBlendMode", -1);
	lua_pop(L, 1);
	lua_getfield(L, -1, "getBlendMode");
	RefData::setRef(L, "love.graphics.getBlendMode", -1);
	lua_pop(L, 1);
	lua_getfield(L, -1, "newImage");
	RefData::setRef(L, "love.graphics.newImage", -1);
	lua_pop(L, 1);
	lua_getfield(L, -1, "setStencilTest");
	RefData::setRef(L, "love.graphics.setStencilTest", -1);
	lua_pop(L, 1);
	lua_getfield(L, -1, "stencil");
	RefData::setRef(L, "love.graphics.stencil", -1);
	lua_pop(L, 1);
	lua_getfield(L, -1, "newShader");
	RefData::setRef(L, "love.graphics.newShader", -1);
	lua_pop(L, 1);
	lua_getfield(L, -1, "setShader");
	RefData::setRef(L, "love.graphics.setShader", -1);
	lua_pop(L, 1);
	lua_getfield(L, -1, "getShader");
	RefData::setRef(L, "love.graphics.getShader", -1);
	lua_pop(L, 2); // pop the function and the graphics table

	// Setup newFileData
	lua_getfield(L, -1, "filesystem"); // assume it's always available
	lua_getfield(L, -1, "newFileData");
	RefData::setRef(L, "love.filesystem.newFileData", -1);
	lua_pop(L, 1);
	lua_getfield(L, -1, "read");
	RefData::setRef(L, "love.filesystem.read", -1);
	lua_pop(L, 2); // pop the function and "filesystem" table.

	// Setup newByteData
	lua_getfield(L, -1, "data");
	if (lua_isnil(L, -1))
	{
		// Hmm, it's not loaded by user. Use "require"
		lua_pop(L, 1);
		lua_getglobal(L, "require");
		lua_pushstring(L, "love.data");
		lua_call(L, 1, 1);
	}
	lua_getfield(L, -1, "newByteData");
	RefData::setRef(L, "love.data.newByteData", -1);
	lua_pop(L, 3); // pop newByteData, love.data, and love table itself

	// Export table
	lua_createtable(L, 0, 0);
	lua_pushstring(L, "loadMocFile");
	lua_pushcfunction(L, Live2LOVE_Live2LOVE);
	lua_rawset(L, -3);
	lua_pushstring(L, "loadModel");
	lua_pushcfunction(L, Live2LOVE_Live2LOVE_full);
	lua_rawset(L, -3);
	lua_pushstring(L, "_VERSION");
	lua_pushstring(L, "0.5.0");
	lua_rawset(L, -3);
	lua_pushstring(L, "Live2DVersion");
	lua_pushstring(L, live2d::Live2D::getVersionStr());
	lua_rawset(L, -3);
	return 1;
}
