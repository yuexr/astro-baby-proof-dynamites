local script = assert(arg[1])
local scripts = assert(script:match('^(.*)[/\\]main.lua$'))
local config = scripts .. '/../config.ini'
local status = scripts .. '/BabyProofDynamites-status.txt'
local handler, enabled, vehicles, active = nil, true, false, true
local messages = {}
local function write(path, text) local f=assert(io.open(path,'w')); assert(f:write(text)); assert(f:close()) end
local function read(path) local f=assert(io.open(path,'r'));local s=f:read('*a');f:close();return s end
local function updateStatus()
    local text = active and ('ACTIVE: Safe explosions '..(enabled and 'ON.' or 'OFF.')..
        ' Attached vehicle destruction '..(vehicles and 'ON.' or 'OFF.')) or 'INACTIVE: unsupported build'
    write(status, text)
end
local function loadSettings()
    local text=read(config):match('%[Explosions%]([^%[]*)') or ''
    local protection=text:match('ProtectVehicles=(%w+)')
    vehicles=protection and protection=='false' or
        (not protection and text:find('AllowAttachedVehicleDestruction=true',1,true)~=nil)
    updateStatus()
end
write(config,'; keep this comment\n[Explosions]\nEnabled=false\nAllowAttachedVehicleDestruction=false\nOtherSetting=123\n[Other]\nEnabled=leave-this-alone\nKeep=true\n')
package.loadlib=function(_,name)
    if name=='luaopen_babyproofdynamites' then return function() enabled=true;loadSettings() end end
    if name=='luaopen_babyproofdynamites_refresh' then return loadSettings end
    if name=='luaopen_babyproofdynamites_on' then return function() if active then enabled=true end;updateStatus() end end
    if name=='luaopen_babyproofdynamites_off' then return function() if active then enabled=false end;updateStatus() end end
    error('Unknown export: '..name)
end
RegisterConsoleCommandHandler=function(name,fn) assert(name=='babyproof');handler=fn end
local output={Log=function(_,text) messages[#messages+1]=text end}
local statusMissing = false
local function expectedSummary()
    if not active then
        return 'Babyproof: INACTIVE\nEquipment: NOT PROTECTED BY THIS MOD\nVehicles: NOT PROTECTED BY THIS MOD\nunsupported build\n'
    end
    if not enabled then
        return 'Babyproof: OFF (vanilla explosions)\nEquipment: NOT PROTECTED\nVehicles: NOT PROTECTED\n'
    end
    return 'Babyproof: ON (tiny explosions)\nEquipment: PROTECTED\nVehicles: '..
        (vehicles and 'BREAKABLE (direct attachment required)' or 'PROTECTED')..'\n'
end
local function command(...)
    local count=#messages
    assert(handler('',{...},output))
    assert(#messages==count+1)
    local result=messages[#messages]
    if statusMissing then
        assert(result:find('Babyproof: STATUS UNAVAILABLE',1,true))
        assert(result:find('Protection could not be confirmed.',1,true))
    else
        local expected=expectedSummary()
        assert(result:sub(-#expected)==expected,result)
    end
    return result
end

dofile(script)
assert(enabled and not vehicles)
local before=read(config)
local help=command()
assert(not help:find('babyproof toggle',1,true) and not help:find('babyproof vehicle status',1,true))
assert(read(config)==before)
assert(command('status')==expectedSummary())
command('v','off');assert(vehicles and enabled)
assert(read(config):find('ProtectVehicles=false',1,true))
local beforeMainCommand=read(config)
command('off');assert(read(config)==beforeMainCommand and not enabled and vehicles)
assert(command('status')==expectedSummary())
assert(command('vehicle','on'):find('Babyproof turned ON.',1,true))
assert(enabled and not vehicles)
command('vehicle','off');assert(enabled and vehicles)
command('on');assert(enabled and not vehicles)
assert(read(config):find('ProtectVehicles=true',1,true))
assert(not read(config):find('AllowAttachedVehicleDestruction',1,true))
command('OFF');assert(not enabled and not vehicles)
command('vehicle','off');assert(not enabled and vehicles)
command('vehicles','on');assert(enabled and not vehicles)
command('V','OFF');assert(enabled and vehicles)
command('off');assert(not enabled and vehicles)
loadSettings();assert(not enabled and vehicles)
enabled=true;loadSettings();assert(enabled and vehicles)
command('ON');assert(enabled and not vehicles)
local saved=read(config)
assert(saved:find('; keep this comment',1,true) and saved:find('OtherSetting=123',1,true))
assert(saved:find('[Other]\nEnabled=leave-this-alone\nKeep=true',1,true))
assert(saved:find('Enabled=false',1,true))
for _,params in ipairs({{'bogus'},{'toggle'},{'toggle','on'},{'v','bogus'},{'vehicle','status'},{'vehicles','status'},{'v','status'}}) do
    assert(command(table.unpack(params)):find('Unknown command.',1,true))
    assert(read(config)==saved and enabled and not vehicles)
end
active=false;updateStatus()
assert(command('status')==expectedSummary())
assert(command():find('babyproof on',1,true))
assert(command('off'):find('Command not applied.',1,true))
command('vehicle','on')
assert(read(config)==saved and enabled and not vehicles)
active=true;updateStatus()
command('off');assert(not enabled)
assert(os.remove(config))
assert(command('on'):find('Command failed:',1,true))
assert(not enabled)
write(config,saved)
assert(os.remove(status));statusMissing=true
command('status');command();command('on')
assert(not enabled and read(config)==saved)
statusMissing=false;updateStatus()
command('on');assert(enabled and not vehicles)
print('PASS: every command/help/error reports current protections; all three states; singular/plural/v ON/OFF; redundant vehicle status removed; inactive/missing status never claims protection; config error preserves runtime state.')
