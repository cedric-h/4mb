@echo off

set application_name=app
set build_options=-DBUILD_WIN32=1
set smol=no

IF "%smol%" EQU "yes" (
    set compile_flags=/GS- /Gs9999999 /Gm- /EHa- /GF /Gy /GA /GR- /O1 /Os /Fe:app.exe /DYNAMICBASE:NO
    set link_flags=/subsystem:windows /opt:ref /opt:icf /stack:0x100000,0x100000
) ELSE (
    set compile_flags=-nologo /DYNAMICBASE:NO  /Zi /FC /TC /I "../"
    set link_flags=/subsystem:windows /opt:ref /incremental:no
)

IF "%_called_vcvars_%" EQU "" (
    set _called_vcvars_=yes
    pushd "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\"
    call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
    popd
)

if not exist build mkdir build
pushd build

:: build shaders
fxc.exe /nologo /T vs_4_0 /E vs /O3 /WX /Zpc /Ges /Fh d3d11_vshader.h /Vn d3d11_vshader /Qstrip_reflect /Qstrip_debug /Qstrip_priv ../shader.hlsl
fxc.exe /nologo /T ps_4_0 /E ps /O3 /WX /Zpc /Ges /Fh d3d11_pshader.h /Vn d3d11_pshader /Qstrip_reflect /Qstrip_debug /Qstrip_priv ../shader.hlsl

:: build bootleg C runtime
lib.exe -nologo /MACHINE:X64 /out:ced_crt.lib /def:../ced_crt.def

:: actually compile the C code, finally
start /b /wait "" "cl.exe"  %build_options% %compile_flags% ../main.c /link %link_flags% /out:%application_name%.exe
IF "%smol%" EQU "yes" (
    "../../upx" app.exe --ultra-brute
) ELSE (
    call :saysize app.exe
)
popd

goto :eof
:saysize
echo app.exe is %~z1 bytes
