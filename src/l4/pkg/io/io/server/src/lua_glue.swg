%module Io

%include <typemaps.i>
%include <lua_typemap.i>
%include "std_string.i"
%include "lua_typeinfo.i"

%{

#include "virt/vdevice.h"
#include "virt/vbus_factory.h"
#include "hw_device.h"
#include "gpio"

int luaopen_Io(lua_State *);
int add_vbus(Vi::Device *dev);
void dump_devs(Device *d);

%}

%ignore Resource_space;
%ignore Resource_list;
%ignore Resource_provider;
%ignore Root_resource;
%ignore Vi::Dev_feature;
%ignore Hw::Dev_feature;

%include "resource.h"

%apply SWIGTYPE *DISOWN { Hw::Device *disown };
%apply SWIGTYPE *DISOWN { Vi::Device *disown };
%apply SWIGTYPE *DISOWN { Resource *disown };

%nodefaultctor;
%rename(Vi_dev_factory) Vi::Dev_factory;
%rename(Vi_device) Vi::Device;
%rename(Hw_dev_factory) Hw::Device_factory;
%rename(Hw_device) Hw::Device;

%nodefaultdtor Vi::Dev_factory;
%nodefaultdtor Hw::Device_factory;
%nodefaultdtor Property;

class Device
{};

class Generic_device : public Device
{
public:
  void add_resource(Resource *disown);
  Property *property(std::string const &name);
};

%extend Generic_device
{
  int nresources() const
  {
    return self->resources()->size();
  }

  Resource *resource(int idx) const
  {
    return self->resources()->at(idx);
  }
};

class Property
{
public:
  int set(int, std::string const &);
  int set(int, l4_uint64_t);
  int set(int, Generic_device *);
  int set(int, Resource *);
};

namespace Vi {

class Device : public Generic_device
{
public:
  void add_child(Device *disown);
  void name(char const *);
  char const *name() const;
  Device *parent() const;
  Device *children() const;
  Device *next() const;
  int depth() const;
  int add_filter(cxx::String const &, cxx::String const &);
  int add_filter(cxx::String const &, unsigned long long);
  int add_filter(cxx::String const &, unsigned long long, unsigned long long);

  void dump(int) const;
};

class Dev_factory
{
public:
  virtual Device *vcreate() = 0;
  virtual Device *do_match(Hw::Device *f) = 0;

  static Device *create(std::string const &_class);
  static Device *create(Hw::Device *f);
};
}

%extend Vi::Device
{
  void set_name(char const *name) { self->name(name); }
};

namespace Hw {
class Device : public Generic_device
{
public:
  enum Device_flags
  {
    DF_dma_supported = 1 << 0,
  };

  Property *property(char const *name);
  void add_child(Device *disown);
  void add_cid(char const *);
  void set_name(char const *);
  char const *name() const;
  bool match_cid(char const *cid) const;
  Device *parent() const;
  Device *children() const;
  Device *next() const;
  int depth() const;
  void plugin();

  void dump(int) const;
};

class Device_factory
{
public:
  virtual Device *create() = 0;
  static Device *create(char const *name);
};

}

%extend Hw::Device
{
  Hw::Device *find_by_name(std::string const &name) const
  {
    for (Hw::Device::iterator c = self->begin(0);
         c != self->end(); ++c)
      if (name == (*c)->name())
        return *c;

    return 0;
  }

  Hw::Device *__getitem(std::string const &name) const
  {
    for (Hw::Device::iterator c = self->begin(0);
         c != self->end(); ++c)
      if (name == (*c)->name())
        return *c;

    return 0;
  }

  void __setitem(std::string const &name, Hw::Device *dev)
  {
    dev->set_name(name);
    self->add_child(dev);
    if (self->parent() || self == system_bus())
      dev->plugin();
  }
};

Hw::Device *system_bus();
void dump_devs(Device *d);
int add_vbus(Vi::Device *dev);


class Gpio_resource : public Resource
{
public:
  explicit Gpio_resource(Hw::Device *dev, unsigned start, unsigned end)
    throw(char const *);
};

%luacode {

local table = require "table"
local string = require "string"
local debug = require "debug"

local function check_device(dev, depth, err)
  if not Io.swig_instance_of(dev, "Generic_device *") then
    local e = err or "expected device, got: " .. tostring(dev);
    error(e, depth + 1);
  end
  return dev
end

Io.Res = {}

function Io.Res.io(start, _end, flags)
  local f = flags or 0
  return Io.Resource(Io.Resource_Io_res, f, start, _end or start)
end

function Io.Res.mmio(start, _end, flags)
  local f = flags or 0
  return Io.Resource(Io.Resource_Mmio_res, f, start, _end or start)
end

function Io.Res.irq(start, flags)
  local f = flags or 0
  local s, e
  if type(start) == "table" then
    s = start[1]
    e = start[2]
  else
    s = start
    e = start
  end
  return Io.Resource(Io.Resource_Irq_res, f, s, e)
end

Io.Dt = {}

function Io.Dt.add_child(parent, name, dev, idx)
  parent:add_child(dev)
  if dev.plugin and (parent:parent() or swig_equals(parent, Io.system_bus())) then
    dev:plugin()
  end
  if type(name) == "string" then
    if idx ~= nil then
      name = name .. '[' .. idx ..']'
    end
    dev:set_name(name)
  end
end

function Io.Dt.add_children(parent, bus_func)
  check_device(parent, 2);
  if type(bus_func) == "function" then
    local d = {
      Property = {},
      Resource = {},
      Child = {}
    }
    local env_upval = 0
    local name
    local my_env
    repeat
      env_upval = env_upval + 1
      name, my_env = debug.getupvalue(bus_func, env_upval)
    until name == '_ENV' or name == nil
    if name == nil then
      print("Warning: The bus function used for add_children does not have an _ENV");
      print("         Empty vbus? Use 'bus = Io.Vi.System_bus()' instead.")
      return parent;
    end

    setmetatable(d, { __index = my_env })
    debug.upvaluejoin(bus_func, env_upval, function() return d end, 1)
    bus_func();
    Io.Dt.add_device_data(parent, d)
  end
  return parent;
end

function Io.Dt.iterator(dev, max_depth)
  local max_d = (max_depth or 0) + dev:depth()
  local current = dev
  local start = dev
  return function ()
    local c = current
    if c == nil then
      return nil
    end

    local cd = c:depth()
    local cc = c:children()
    if max_d > cd and cc then
      current = cc
      return c
    elseif swig_equals(c, start) then
      current = nil
      return c
    elseif c:next() then
      current = c:next()
      return c
    else
      local p = c
      while (true) do
        p = p:parent()
        if (not p) or swig_equals(p, start) then
          current = nil
          return c
        elseif p:next() then
          current = p:next()
          return c
        end
      end
    end
  end
end

function Io.Dt.match_cids(self, ...)
  local r = {}
  for _, v in ipairs{...} do
    if self:match_cid(v) then
      return true
    end
  end
  return false
end

Io.Dt.PCI_cc =
{
  storage = "CC_01",
  network = "CC_02",
  display = "CC_03",
  media   = "CC_04",
  bridge  = "CC_06",
  com     = "CC_07",
  usb     = "CC_0c",
  wlan    = "CC_0d",
}

Io.Dt.MAX_DEPTH = 1000

function Io.Dt.Range(start, stop)
  return { range = true, start, stop }
end

function Io.Dt.match(self, ...)
  local cids = {...}
  for t,v in pairs(Io.Dt.PCI_cc) do
    for i, cid in ipairs(cids) do
      cids[i] = cid:gsub("(PCI/"..t..")", "PCI/" .. v)
    end
  end

  local devs = {}
  for d in self:devices(Io.Dt.MAX_DEPTH) do
    if d:match_cids(table.unpack(cids)) then
      devs[#devs+1] = d
    end
  end
  return devs
end

function Io.Dt.device(self, path)
  for i in string.gmatch(path, "([^%./]+)%.*") do
    self = self:find_by_name(i)
    if self == nil then
      return nil
    end
  end
  return self
end

function Io.Dt.resources(self)
  local n = self:nresources()
  local c = 0
  return function ()
    if c >= n then return nil end
    c = c + 1
    return self:resource(c - 1)
  end
end

local hwfn = Io.swig_class("Hw_device")[".fn"]
local vifn = Io.swig_class("Vi_device")[".fn"]

local dev_fns =
{
  resources = Io.Dt.resources,
  devices = Io.Dt.iterator,
  match_cids = Io.Dt.match_cids,
  match = Io.Dt.match,
  device = Io.Dt.device
}

for name, func in pairs(dev_fns) do
  hwfn[name] = func
  vifn[name] = func
end

function vifn:add_filter_val(tag, value)
  if type(value) == "table" and value.range then
    return self:add_filter(tag, value[1], value[2])
  elseif type(value) == "table" then
    for _, v in ipairs(value) do
      local res = self:add_filter_val(tag, v)
      if res < 0 then
        return res
      end
    end
    return 0
  else
    return self:add_filter(tag, value)
  end
end

local add_child = Io.Dt.add_child
local function set_device_property(dev, name, idx, val)
  -- test whether a property is registered
  local p = dev:property(name);
  if p == nil then
    print("error: no property '" .. name .. "' registered");
    return
  end
  local r = p:set(idx, val)
  if r ~= 0 then
    print("error: setting property " .. tostring(name) .. " failed")
  end
end

local function handle_device_member(dev, val, name)
  local vtype = type(val)
  if name == "compatible" then
    if vtype == "table" then
      for k, v in ipairs(val) do
        dev:add_cid(v)
      end
    elseif vtype == "string" then
      dev:add_cid(val)
    else
      error("'compatible' must be a string or table of strings (probably you assigned something else)", 2)
    end
    return
  elseif name == "Property" then
    if vtype ~= "table" then
      error("'Property' must be a table (probably you assigned something else)", 2)
    end
    for k,v in pairs(val) do
      local r
      if type(v) ~= "table" then
        set_device_property(dev, k, -1, v)
      else
        for i,j in ipairs(v) do
          set_device_property(dev, k, i, j)
        end
      end
    end
    return
  elseif name == "Resource" then
    if vtype ~= "table" then
      error("'Resource' must be a table (probably you assigned something else)", 2)
    end
    for k,v in pairs(val) do
      if Io.swig_instance_of(v, "Resource *") then
        v:set_id(k)
        dev:add_resource(v)
      else
        print("WARNING: ".. tostring(k) .. ") in 'Resource' table is not a resource.")
        print("         Ignoring.")
      end
    end
    return
  elseif name == "Child" then
    if vtype ~= "table" then
      error("'Child' must be a table (probably you assigned something else)", 2)
    end
    for k,v in pairs(val) do
      if Io.swig_instance_of(v, "Generic_device *") then
        add_child(dev, k, v)
      else
        print("WARNING: ".. tostring(k) .. ") in 'Child' table is not a device.")
        print("         Ignoring.")
      end
    end
    return
  elseif Io.swig_instance_of(val, "Resource *") then
    val:set_id(name)
    dev:add_resource(val)
    return
  elseif Io.swig_instance_of(val, "Generic_device *") then
    add_child(dev, name, val)
    return
  elseif vtype == "table" then
    for i, v in pairs(val) do
      handle_device_member(dev, v, name .. '[' .. i .. ']')
    end
    return
  else -- handle deprecated format, will be removed in a future release
    print("WARNING: deprecated property definition " .. tostring(name) ..
          "=\"".. tostring(val) .. "\"")
    print("         please use 'Property." .. tostring(name) ..
          "=\"".. tostring(val) .."\" instead. Note: you also need to use")
    print("         the '(function() ... end)' syntax to define the device")
    print("         refer to io's documentation for further details")
    local r
    if vtype ~= "table" then
      set_device_property(dev, name, -1, val)
    else
      for i,j in pairs(val) do
        set_device_property(dev, name, i, j)
      end
    end
    return
  end
  error("cannot handle device member: " .. tostring(name) .. ": " .. tostring(val), 2)
end

function Io.Dt.add_resource(dev, res)
  if not Io.swig_instance_of(dev, "Generic_device *") then
    error("expected a device got: " .. tostring(dev), 2)
  end
  if not Io.swig_instance_of(res, "Resource *") then
    error("expected a resource got: " .. tostring(res), 2)
  end
  dev:add_resource(res)
end

function Io.Dt.add_device_data(dev, data)
  local maxi = 0
  for i, v in ipairs(data) do
    handle_device_member(dev, v, i)
    maxi = i
  end
  for k, v in pairs(data) do
    if (type(k) ~= "number") or (k > maxi) then
      handle_device_member(dev, v, k)
    end
  end
end

local set_dev_data = Io.Dt.add_device_data
local add_children = Io.Dt.add_children

Io.Hw = {}

setmetatable(Io.Hw, { __index = function (self, t)
  return function (data)
    local b = check_device(Io.Hw_dev_factory_create(t), 3, "could not create device: " .. t)
    if type(data) == "function" then
      add_children(b, data)
    elseif type(data) == "table" then
      print("WARNING: deprecated device definition")
      print("         use the '(function() ... end)' syntax to define the device")
      print("         refer to io's documentation for further details")
      set_dev_data(b, data)
    end
    return b
  end
end})

function Io.hw_add_devices(data)
  local sb = Io.system_bus()
  local dtype = type(data)
  if dtype == 'function' then
    Io.Dt.add_children(sb, data)
  elseif dtype == 'table' then
    print("WARNING: deprecated method to add devices to the device tree")
    print("         use the '(function() ... end)' syntax to add devices")
    print("         refer to io's documentation for further details")
    for n, dev in pairs(data) do
      if type(n) == "string" then
        dev:set_name(n)
      end
      sb:add_child(dev)
      dev:plugin()
    end
  end
  return data
end

Io.Vi = {}

setmetatable(Io.Vi, { __index = function (self, t)
  return function (data)
    local b = Io.Vi_dev_factory_create(t)
    if type(data) == "function" then
      add_children(b, data)
    elseif type(data) == "table" then
      set_dev_data(b, data)
    end
    return b
  end
end})

function wrap(devs_, filter)
  local devs = devs_
  if type(devs_) ~= "table" then
    devs = { devs_ }
  end
  local v = {}
  for _, d in ipairs(devs) do
    local vd = Io.Vi_dev_factory_create(d)
    if vd then
      if type(filter) == "table" then
        for tag, val in pairs(filter) do
          local res = vd:add_filter_val(tag, val)
          if res < 0 then
            print("ERROR: applying filter expression: "..tag.."=...", debug.traceback(2))
          end
        end
      end
      v[#v + 1] = vd
    end
  end
  if #v == 1 then
    return v[1]
  else
    return v
  end
end

local add_vbus = Io.add_vbus

function Io.add_vbus(name, bus)
  bus:set_name(name)
  add_vbus(bus)
end

function Io.add_vbusses(busses)
  for name, bus in pairs(busses) do
    Io.add_vbus(name, bus)
  end
  return busses
end

}
