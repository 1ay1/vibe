-- libvibe — LuaJIT binding using the built-in FFI (no extra modules).
--
--   local vibe = require("vibe")
--   local doc = vibe.parse(io.open("config.vibe", "rb"):read("*a"))
--   print(doc:get_string("server.host"))
--
-- Requires LuaJIT (for the `ffi` library). Plain PUC Lua has no FFI.

local ffi = require("ffi")

ffi.cdef([[
  typedef struct { bool has_error; int code; char* message; int line; int column; } VibeError;
  const char* vibe_version(void);
  void*       vibe_parse(const char*, size_t, VibeError*);
  const char* vibe_get_string(void*, const char*);
  int64_t     vibe_get_int(void*, const char*);
  double      vibe_get_float(void*, const char*);
  bool        vibe_get_bool(void*, const char*);
  void*       vibe_get_array(void*, const char*);
  size_t      vibe_array_size(void*);
  char*       vibe_emit(void*);
  void        vibe_free(void*);
  void        vibe_value_free(void*);
  const char* vibe_error_code_string(int);
]])

local L = ffi.load(os.getenv("VIBE_LIB") or (arg and arg[0]:gsub("[^/]*$", "") or "") .. "../../libvibe.so")

local vibe = {}

function vibe.version()
  return ffi.string(L.vibe_version())
end

local Doc = {}
Doc.__index = Doc

function Doc:get_string(path)
  local p = L.vibe_get_string(self.ptr, path)
  return p ~= nil and ffi.string(p) or nil
end

function Doc:get_int(path)   return tonumber(L.vibe_get_int(self.ptr, path)) end
function Doc:get_float(path) return L.vibe_get_float(self.ptr, path) end
function Doc:get_bool(path)  return L.vibe_get_bool(self.ptr, path) end

function Doc:array_size(path)
  local arr = L.vibe_get_array(self.ptr, path)
  return arr ~= nil and tonumber(L.vibe_array_size(arr)) or 0
end

function Doc:emit()
  local raw = L.vibe_emit(self.ptr)
  if raw == nil then return "" end
  local s = ffi.string(raw)
  L.vibe_free(raw)
  return s
end

function Doc:free()
  if self.ptr ~= nil then L.vibe_value_free(self.ptr); self.ptr = nil end
end

function vibe.parse(data)
  local err = ffi.new("VibeError")
  local ptr = L.vibe_parse(data, #data, err)
  if ptr == nil then
    local code = ffi.string(L.vibe_error_code_string(err.code))
    local msg = err.message ~= nil and ffi.string(err.message) or "parse error"
    error(("VibeError: %s (%s at %d:%d)"):format(msg, code, err.line, err.column), 0)
  end
  return setmetatable({ ptr = ptr }, Doc)
end

-- self-test ----------------------------------------------------------------
if arg and (arg[0] or ""):match("vibe%.lua$") then
  local sample = os.getenv("VIBE_SAMPLE") or (arg[0]:gsub("[^/]*$", "") .. "../sample.vibe")
  local f = assert(io.open(sample, "rb"))
  local doc = vibe.parse(f:read("*a"))
  f:close()
  local ok = true
  local function check(name, got, want)
    if got ~= want then ok = false end
    print(("  [%s] %s = %s"):format(got == want and "ok " or "BAD", name, tostring(got)))
  end
  check("version", vibe.version(), "1.2.0")
  check("name", doc:get_string("name"), "libvibe")
  check("answer", doc:get_int("answer"), 42)
  check("pi", tonumber(string.format("%.5f", doc:get_float("pi"))), 3.14159)
  check("enabled", doc:get_bool("enabled"), true)
  check("server.host", doc:get_string("server.host"), "localhost")
  check("server.port", doc:get_int("server.port"), 8080)
  check("len(ports)", doc:array_size("ports"), 3)
  if doc:emit():find("libvibe", 1, true) then
    print("  [ok ] emit() round-trips")
  else ok = false; print("  [BAD] emit() did not round-trip") end
  local caught = pcall(function() vibe.parse("name {") end)
  if caught then ok = false; print("  [BAD] malformed input did not raise")
  else print("  [ok ] rejects malformed input") end
  print(ok and "ALL OK (luajit)" or "FAILED (luajit)")
  os.exit(ok and 0 or 1)
end

return vibe
