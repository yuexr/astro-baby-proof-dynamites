local script = assert(arg[1])
local scripts = script:match('^(.*)[/\\]main.lua$')
local config = scripts .. '/../config.ini'
local status = scripts .. '/BabyProofDynamites-status.txt'
local handler, live, active = nil, false, true
local function write(path, text) local f=assert(io.open(path,'w')); assert(f:write(text)); assert(f:close()) end
local function read(path) local f=assert(io.open(path,'r'));local s=f:read('*a');f:close();return s end
local function updateStatus()
    write(status, active and ('ACTIVE: Attached vehicle destruction '..(live and 'ON.' or 'OFF.')) or 'INACTIVE: unsupported build')
end
write(config,'; keep this comment\n[Explosions]\nAllowAttachedVehicleDestruction=false\nOtherSetting=123\n[Other]\nKeep=true\n')
package.loadlib=function(_, name)
    if name=='luaopen_babyproofdynamites' then return updateStatus end
    assert(name=='luaopen_babyproofdynamites_refresh')
    return function() live=read(config):find('AllowAttachedVehicleDestruction=true',1,true)~=nil;updateStatus() end
end
RegisterConsoleCommandHandler=function(name, fn) assert(name=='babyproof');handler=fn end
local messages={};local output={Log=function(_,text) messages[#messages+1]=text end}
dofile(script)
assert(not live)
assert(handler('',{'v','on'},output));assert(live and read(config):find('AllowAttachedVehicleDestruction=true',1,true))
assert(read(config):find('; keep this comment',1,true) and read(config):find('OtherSetting=123',1,true) and read(config):find('[Other]\nKeep=true',1,true))
handler('',{'vehicles','status'},output);assert(messages[#messages]:find('ON',1,true))
handler('',{'vehicles','off'},output);assert(not live)
handler('',{'V','ON'},output);assert(live)
local before=read(config)
handler('',{'v','bogus'},output);assert(read(config)==before and live)
active=false;updateStatus();handler('',{'v','off'},output);assert(read(config)==before and live)
print('PASS: v/vehicles aliases, case handling, saved on/off, status, preserved unrelated settings, invalid/inactive rejection')