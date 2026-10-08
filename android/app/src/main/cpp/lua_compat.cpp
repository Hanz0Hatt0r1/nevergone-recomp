#include "lua_compat.h"

#if defined(NEVERGONE_HAS_LUA)
extern "C" {
#include <lauxlib.h>
#include <lua.h>
}
#endif

namespace nevergone::lua_runtime {

void install_lua51_compat(lua_State* state) {
#if defined(NEVERGONE_HAS_LUA)
    // Never Gone ships Lua 5.2.3 but recovered scripts still use a few legacy
    // globals and embedded libraries registered by the original native client.
    // Install only the compatibility surface referenced by recovered scripts.
    constexpr const char* kCompatScript = R"LUA(
if unpack == nil and table ~= nil then
    unpack = table.unpack
end

if bit == nil and bit32 ~= nil then
    bit = bit32
end

if cjson == nil then
    local json_null = {}

    local function utf8_from_codepoint(codepoint)
        if codepoint <= 0x7f then
            return string.char(codepoint)
        elseif codepoint <= 0x7ff then
            return string.char(
                0xc0 + math.floor(codepoint / 0x40),
                0x80 + (codepoint % 0x40))
        elseif codepoint <= 0xffff then
            return string.char(
                0xe0 + math.floor(codepoint / 0x1000),
                0x80 + (math.floor(codepoint / 0x40) % 0x40),
                0x80 + (codepoint % 0x40))
        elseif codepoint <= 0x10ffff then
            return string.char(
                0xf0 + math.floor(codepoint / 0x40000),
                0x80 + (math.floor(codepoint / 0x1000) % 0x40),
                0x80 + (math.floor(codepoint / 0x40) % 0x40),
                0x80 + (codepoint % 0x40))
        end
        error("invalid Unicode codepoint")
    end

    local escape_map = {
        ['"'] = '\\"',
        ['\\'] = '\\\\',
        ['\b'] = '\\b',
        ['\f'] = '\\f',
        ['\n'] = '\\n',
        ['\r'] = '\\r',
        ['\t'] = '\\t',
    }

    local function encode_string(value)
        return '"' .. value:gsub('[%z\1-\31\\"]', function(character)
            local escaped = escape_map[character]
            if escaped ~= nil then
                return escaped
            end
            return string.format('\\u%04x', string.byte(character))
        end) .. '"'
    end

    local encode_value
    encode_value = function(value, seen)
        local value_type = type(value)
        if value == json_null or value_type == 'nil' then
            return 'null'
        elseif value_type == 'boolean' then
            return value and 'true' or 'false'
        elseif value_type == 'number' then
            if value ~= value or value == math.huge or value == -math.huge then
                error('cannot encode non-finite number')
            end
            return tostring(value)
        elseif value_type == 'string' then
            return encode_string(value)
        elseif value_type ~= 'table' then
            error('cannot encode Lua type ' .. value_type)
        end

        if seen[value] then
            error('cannot encode recursive table')
        end
        seen[value] = true

        local count = 0
        local max_index = 0
        local array_candidate = true
        for key in pairs(value) do
            count = count + 1
            if type(key) ~= 'number' or key < 1 or key % 1 ~= 0 then
                array_candidate = false
            elseif key > max_index then
                max_index = key
            end
        end

        local output = {}
        if count > 0 and array_candidate and max_index == count then
            for index = 1, max_index do
                output[#output + 1] = encode_value(value[index], seen)
            end
            seen[value] = nil
            return '[' .. table.concat(output, ',') .. ']'
        end

        for key, item in pairs(value) do
            local key_type = type(key)
            if key_type ~= 'string' and key_type ~= 'number' then
                seen[value] = nil
                error('JSON object keys must be strings or numbers')
            end
            output[#output + 1] = encode_string(tostring(key)) .. ':' .. encode_value(item, seen)
        end
        seen[value] = nil
        return '{' .. table.concat(output, ',') .. '}'
    end

    local function decode_json(text)
        assert(type(text) == 'string', 'JSON string expected')
        local position = 1
        local length = #text

        local function fail(message)
            error(message .. ' at character ' .. position, 0)
        end

        local function skip_space()
            while position <= length do
                local byte = string.byte(text, position)
                if byte == 0x20 or byte == 0x09 or byte == 0x0a or byte == 0x0d then
                    position = position + 1
                else
                    break
                end
            end
        end

        local function parse_hex4(offset)
            local hex = text:sub(offset, offset + 3)
            if #hex ~= 4 or not hex:match('^%x%x%x%x$') then
                fail('invalid Unicode escape')
            end
            return tonumber(hex, 16)
        end

        local function parse_string()
            if text:sub(position, position) ~= '"' then
                fail('expected string')
            end
            position = position + 1
            local chunks = {}
            local start = position

            while position <= length do
                local byte = string.byte(text, position)
                if byte == 0x22 then
                    chunks[#chunks + 1] = text:sub(start, position - 1)
                    position = position + 1
                    return table.concat(chunks)
                elseif byte == 0x5c then
                    chunks[#chunks + 1] = text:sub(start, position - 1)
                    position = position + 1
                    if position > length then
                        fail('unterminated escape')
                    end
                    local escape = text:sub(position, position)
                    local simple = {
                        ['"'] = '"',
                        ['\\'] = '\\',
                        ['/'] = '/',
                        ['b'] = '\b',
                        ['f'] = '\f',
                        ['n'] = '\n',
                        ['r'] = '\r',
                        ['t'] = '\t',
                    }
                    if simple[escape] ~= nil then
                        chunks[#chunks + 1] = simple[escape]
                        position = position + 1
                    elseif escape == 'u' then
                        local codepoint = parse_hex4(position + 1)
                        position = position + 5
                        if codepoint >= 0xd800 and codepoint <= 0xdbff then
                            if text:sub(position, position + 1) ~= '\\u' then
                                fail('missing low surrogate')
                            end
                            local low = parse_hex4(position + 2)
                            if low < 0xdc00 or low > 0xdfff then
                                fail('invalid low surrogate')
                            end
                            codepoint = 0x10000 + (codepoint - 0xd800) * 0x400 + (low - 0xdc00)
                            position = position + 6
                        elseif codepoint >= 0xdc00 and codepoint <= 0xdfff then
                            fail('unexpected low surrogate')
                        end
                        chunks[#chunks + 1] = utf8_from_codepoint(codepoint)
                    else
                        fail('invalid escape')
                    end
                    start = position
                elseif byte < 0x20 then
                    fail('control character in string')
                else
                    position = position + 1
                end
            end
            fail('unterminated string')
        end

        local parse_value

        local function parse_number()
            local start = position
            if text:sub(position, position) == '-' then
                position = position + 1
            end

            local first = text:sub(position, position)
            if first == '0' then
                position = position + 1
            elseif first:match('[1-9]') then
                repeat
                    position = position + 1
                until position > length or not text:sub(position, position):match('%d')
            else
                fail('invalid number')
            end

            if text:sub(position, position) == '.' then
                position = position + 1
                if not text:sub(position, position):match('%d') then
                    fail('invalid number fraction')
                end
                repeat
                    position = position + 1
                until position > length or not text:sub(position, position):match('%d')
            end

            local exponent = text:sub(position, position)
            if exponent == 'e' or exponent == 'E' then
                position = position + 1
                local sign = text:sub(position, position)
                if sign == '+' or sign == '-' then
                    position = position + 1
                end
                if not text:sub(position, position):match('%d') then
                    fail('invalid number exponent')
                end
                repeat
                    position = position + 1
                until position > length or not text:sub(position, position):match('%d')
            end

            local number = tonumber(text:sub(start, position - 1))
            if number == nil then
                fail('invalid number')
            end
            return number
        end

        local function parse_array()
            position = position + 1
            skip_space()
            local result = {}
            if text:sub(position, position) == ']' then
                position = position + 1
                return result
            end

            local index = 1
            while true do
                result[index] = parse_value()
                index = index + 1
                skip_space()
                local delimiter = text:sub(position, position)
                if delimiter == ']' then
                    position = position + 1
                    return result
                elseif delimiter ~= ',' then
                    fail('expected comma or closing bracket')
                end
                position = position + 1
                skip_space()
            end
        end

        local function parse_object()
            position = position + 1
            skip_space()
            local result = {}
            if text:sub(position, position) == '}' then
                position = position + 1
                return result
            end

            while true do
                local key = parse_string()
                skip_space()
                if text:sub(position, position) ~= ':' then
                    fail('expected colon')
                end
                position = position + 1
                skip_space()
                result[key] = parse_value()
                skip_space()
                local delimiter = text:sub(position, position)
                if delimiter == '}' then
                    position = position + 1
                    return result
                elseif delimiter ~= ',' then
                    fail('expected comma or closing brace')
                end
                position = position + 1
                skip_space()
            end
        end

        parse_value = function()
            skip_space()
            local current = text:sub(position, position)
            if current == '"' then
                return parse_string()
            elseif current == '{' then
                return parse_object()
            elseif current == '[' then
                return parse_array()
            elseif current == '-' or current:match('%d') then
                return parse_number()
            elseif text:sub(position, position + 3) == 'true' then
                position = position + 4
                return true
            elseif text:sub(position, position + 4) == 'false' then
                position = position + 5
                return false
            elseif text:sub(position, position + 3) == 'null' then
                position = position + 4
                return json_null
            end
            fail('unexpected JSON token')
        end

        local result = parse_value()
        skip_space()
        if position <= length then
            fail('trailing JSON data')
        end
        return result
    end

    cjson = {
        null = json_null,
        encode = function(value)
            return encode_value(value, {})
        end,
        decode = decode_json,
    }
    package.loaded.cjson = cjson
end

if module == nil then
    function module(name)
        assert(type(name) == "string" and name ~= "", "module name expected")

        local target = package.loaded[name]
        if type(target) ~= "table" then
            target = rawget(_G, name)
        end
        if type(target) ~= "table" then
            target = {}
        end

        package.loaded[name] = target
        rawset(_G, name, target)
        target._M = target
        target._NAME = name
        target._PACKAGE = string.match(name, "^(.*%.)") or ""

        local caller = debug.getinfo(2, "f")
        if caller ~= nil and caller.func ~= nil then
            local index = 1
            while true do
                local upvalue = debug.getupvalue(caller.func, index)
                if upvalue == nil then
                    break
                end
                if upvalue == "_ENV" then
                    -- Lua 5.2 closures can share the same _ENV upvalue cell.
                    -- A plain setupvalue() would therefore also mutate the
                    -- caller's siblings/parent chunk. Detach this function's
                    -- environment first to emulate Lua 5.1 setfenv(module).
                    local isolated_environment = target
                    local function environment_holder()
                        return isolated_environment
                    end
                    debug.upvaluejoin(caller.func, index, environment_holder, 1)
                    break
                end
                index = index + 1
            end
        end

        return target
    end
end
)LUA";

    if (luaL_loadstring(state, kCompatScript) != 0) {
        lua_pop(state, 1);
        return;
    }
    if (lua_pcall(state, 0, 0, 0) != 0) {
        lua_pop(state, 1);
    }
#else
    (void)state;
#endif
}

}  // namespace nevergone::lua_runtime
