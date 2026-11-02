# OpenGL dependencies

The project uses local relative paths; no global include/library configuration is required.

| Component | Version | Official source |
| --- | --- | --- |
| GLEW | 2.2.0 | https://github.com/nigels-com/glew/releases/tag/glew-2.2.0 |
| freeglut | 3.6.0 | https://github.com/freeglut/freeglut/releases/tag/v3.6.0 |
| GLM | 1.0.1 | https://github.com/g-truc/glm/releases/tag/1.0.1 |

GLEW uses the official Windows binaries. GLM is header-only. freeglut was built from its official tagged source using Visual Studio 2026 v145 for x64 and Win32, Debug and Release, with shared libraries enabled and demos/static libraries disabled. Debug uses freeglutd.lib/freeglutd.dll; Release uses freeglut.lib/freeglut.dll. The original license files are included with each component.

Downloaded archive SHA256 values:

- glew-2.2.0-win32.zip: EA6B14A1C6C968D0034E61FF6CB242CFF2CE0EDE79267A0F2B47B1B0B652C164
- freeglut-3.6.0.zip: A0021F0CD0FEBA3B2CCDB566F40430A318FDA8D5A42CA249C571284801186AF3
- glm-1.0.1.zip: 09C5716296787E1F7FCB87B1CBDBF26814EC1288ED6259CCD30D5D9795809FA5
