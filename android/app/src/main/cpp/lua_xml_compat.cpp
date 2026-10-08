#include "lua_xml_compat.h"

#if defined(NEVERGONE_HAS_LUA)
extern "C" {
#include <lauxlib.h>
#include <lua.h>
}
#endif

namespace nevergone::lua_runtime {

void install_luaxml_compat(lua_State* state) {
#if defined(NEVERGONE_HAS_LUA)
    constexpr const char* kLuaXmlCompat = R"LUA(
if xml == nil then xml = {} end
package.loaded.xml = xml

local function decode_entities(value)
    return (value:gsub('&(#?x?[%w]+);', function(entity)
        if entity == 'lt' then return '<' end
        if entity == 'gt' then return '>' end
        if entity == 'amp' then return '&' end
        if entity == 'quot' then return '"' end
        if entity == 'apos' then return "'" end
        local number
        if entity:sub(1, 2) == '#x' or entity:sub(1, 2) == '#X' then
            number = tonumber(entity:sub(3), 16)
        elseif entity:sub(1, 1) == '#' then
            number = tonumber(entity:sub(2), 10)
        end
        if number ~= nil and number >= 0 and number <= 255 then
            return string.char(number)
        end
        return '&' .. entity .. ';'
    end))
end

function xml.encode(value)
    value = tostring(value or '')
    return (value:gsub('[&<>"\']', {
        ['&'] = '&amp;', ['<'] = '&lt;', ['>'] = '&gt;',
        ['"'] = '&quot;', ["'"] = '&apos;'
    }))
end

local function parse_attributes(source, node)
    local position = 1
    while true do
        local start_pos, end_pos, key, quote, value = source:find("^%s*([%w_:%-%.]+)%s*=%s*(['\"])(.-)%2", position)
        if not start_pos then
            if source:sub(position):match('^%s*$') then return end
            error('invalid XML attribute list')
        end
        node[key] = decode_entities(value)
        position = end_pos + 1
    end
end

local function parse_xml(text)
    assert(type(text) == 'string', 'XML string expected')
    local position = 1
    local stack = {}
    local root = nil

    local function append(value)
        local parent = stack[#stack]
        if parent == nil then
            if type(value) == 'table' then
                if root ~= nil then error('multiple XML roots') end
                root = value
            elseif value:match('%S') then
                error('text outside XML root')
            end
            return
        end
        if type(value) == 'string' and value == '' then return end
        parent[#parent + 1] = value
    end

    while position <= #text do
        local lt = text:find('<', position, true)
        if not lt then
            append(decode_entities(text:sub(position)))
            position = #text + 1
            break
        end
        if lt > position then
            append(decode_entities(text:sub(position, lt - 1)))
        end

        if text:sub(lt, lt + 3) == '<!--' then
            local close = text:find('-->', lt + 4, true)
            if not close then error('unterminated XML comment') end
            position = close + 3
        elseif text:sub(lt, lt + 8) == '<![CDATA[' then
            local close = text:find(']]>', lt + 9, true)
            if not close then error('unterminated CDATA') end
            append(text:sub(lt + 9, close - 1))
            position = close + 3
        elseif text:sub(lt, lt + 1) == '<?' then
            local close = text:find('?>', lt + 2, true)
            if not close then error('unterminated XML declaration') end
            position = close + 2
        elseif text:sub(lt, lt + 1) == '</' then
            local close = text:find('>', lt + 2, true)
            if not close then error('unterminated closing tag') end
            local name = text:sub(lt + 2, close - 1):match('^%s*([%w_:%-%.]+)%s*$')
            if not name then error('invalid closing tag') end
            local node = stack[#stack]
            if node == nil or node[0] ~= name then error('mismatched closing tag: ' .. name) end
            stack[#stack] = nil
            position = close + 1
        elseif text:sub(lt, lt + 1) == '<!' then
            local close = text:find('>', lt + 2, true)
            if not close then error('unterminated XML declaration') end
            position = close + 1
        else
            local close = text:find('>', lt + 1, true)
            if not close then error('unterminated opening tag') end
            local body = text:sub(lt + 1, close - 1)
            local self_closing = body:match('/%s*$') ~= nil
            if self_closing then body = body:gsub('/%s*$', '') end
            local name, attrs = body:match('^%s*([%w_:%-%.]+)(.-)%s*$')
            if not name then error('invalid opening tag') end
            local node = {[0] = name}
            parse_attributes(attrs or '', node)
            append(node)
            if not self_closing then stack[#stack + 1] = node end
            position = close + 1
        end
    end

    if #stack ~= 0 then error('unclosed XML tag: ' .. tostring(stack[#stack][0])) end
    if root == nil then error('XML root not found') end
    return root
end

local function serialize_node(node)
    assert(type(node) == 'table' and type(node[0]) == 'string', 'XML node expected')
    local attrs = {}
    for key, value in pairs(node) do
        if type(key) == 'string' and key ~= '_M' and key ~= '_NAME' and key ~= '_PACKAGE' then
            attrs[#attrs + 1] = ' ' .. key .. '="' .. xml.encode(value) .. '"'
        end
    end
    table.sort(attrs)
    local children = {}
    for index = 1, #node do
        local child = node[index]
        if type(child) == 'table' then
            children[#children + 1] = serialize_node(child)
        elseif child ~= nil then
            children[#children + 1] = xml.encode(child)
        end
    end
    local open = '<' .. node[0] .. table.concat(attrs)
    if #children == 0 then return open .. '/>' end
    return open .. '>' .. table.concat(children) .. '</' .. node[0] .. '>'
end

function xml.loadString(text)
    return parse_xml(text)
end
xml.eval = xml.loadString

function xml.load(path)
    local handle, reason = io.open(path, 'rb')
    if not handle then return nil, reason end
    local text = handle:read('*a')
    handle:close()
    local ok, result = pcall(parse_xml, text)
    if not ok then return nil, result end
    return result
end

function xml.str(value)
    if type(value) == 'string' then return value end
    return serialize_node(value)
end

function xml._save(value, path)
    local handle, reason = io.open(path, 'wb')
    if not handle then return false, reason end
    local payload = type(value) == 'string' and value or serialize_node(value)
    local ok, write_reason = handle:write(payload)
    handle:close()
    if not ok then return false, write_reason end
    return true
end
)LUA";

    if (luaL_loadstring(state, kLuaXmlCompat) != 0) {
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
