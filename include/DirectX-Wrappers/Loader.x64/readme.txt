It's possible to load another MDX DLL with adding `.Chained.dll` (e.g dinput8.Chained.dll, d3d9.Chained.dll).
It's possible to load other DLLs with adding `.ChainLoad1.dll` (e.g myHook.ChainLoad1.dll, otherHook.ChainLoad2.dll).

Additional options are available for dinput8.dll in GtoMnK.ini and are enabled by default:

[dinput8]
NoJoy=1
CooperativeLevelUnlocker=1