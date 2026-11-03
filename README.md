# netgame-team-project

3명이 함께 개발하는 넷겜플 수업 팀 프로젝트입니다. **Solar System Adventure**는 C++와 OpenGL을 사용하는 태양계 게임입니다.

## 개발 환경

- Visual Studio 2026의 **C++를 사용한 데스크톱 개발** 워크로드
- MSVC C++ 도구 집합 **v145**, Windows 10/11 SDK
- 기본 빌드 구성: **Debug / x64**

OpenGL 의존성은 `team_project/external/`에 포함되어 있습니다. GLEW 2.2.0, freeglut 3.6.0, GLM 1.0.1을 사용하며, 별도의 전역 포함·라이브러리 경로 설정은 필요하지 않습니다. 출처와 라이선스 파일 위치는 [의존성 안내](team_project/external/README.md)를 확인하세요.

## 가져오기와 실행

1. GitHub의 저장소 URL을 복사한 뒤 각자 작업할 폴더에 복제합니다.

   ```powershell
   git clone <저장소-URL> netgame-team-project
   cd netgame-team-project
   ```

2. Visual Studio에서 `team_project/team_project.sln`을 엽니다.
3. 구성을 **Debug**, 플랫폼을 **x64**로 선택합니다.
4. 솔루션을 빌드한 뒤 **F5**로 실행합니다.

실제 실행 소스는 `team_project/team_project/team_ple.cpp`입니다. 빌드하면 `team_project/bin/x64/Debug/`에 실행 파일, 필요한 DLL, 텍스처와 GLSL 파일을 모읍니다. Visual Studio에서도 이 출력 폴더를 작업 폴더로 사용합니다.

## TCP 서버 초기 실행

솔루션의 `game_server`는 게임과 별도로 실행하는 콘솔 서버입니다. Visual Studio에서
`game_server`를 시작 프로젝트로 설정하고 **Debug / x64**로 실행하거나,
빌드 후 `team_project/bin/x64/Debug/game_server.exe`를 직접 실행합니다.

- 소스: `team_project/game_server/server.cpp`
- 초기화 순서: `WSAStartup(2.2)` → `socket` → `bind` → `listen`
- 주소: 모든 로컬 IPv4 인터페이스(`INADDR_ANY`), TCP 포트 **9000**
- 같은 PC의 접속 주소는 `127.0.0.1:9000`, 다른 PC에서는 서버 PC의 LAN IPv4 주소를 사용합니다. 외부 접속에는 방화벽의 해당 포트 허용이 필요합니다.
- Enter를 누르면 소켓을 닫고 `WSACleanup`으로 종료합니다. 실패 시 오류 코드를 출력하고 자원을 정리합니다.

임시 연결 확인 코드도 포함되어 있습니다. 서버를 먼저 켜고 게임을 실행하면
클라이언트 콘솔에 `[Client] Connected to 127.0.0.1:9000`, 서버 콘솔에
`[Server] Client connected`와 현재 연결 수가 표시됩니다. 연결 종료도 서버에 표시됩니다.
서버가 없으면 오류를 출력하고 오프라인으로 게임을 실행합니다(접속 대기 최대 2초).
다른 PC의 서버를 사용하려면 `team_ple.cpp`의 `connection.connectTo`에 있는
`127.0.0.1`을 서버의 LAN IPv4 주소로 바꿉니다. 자동 재접속이나 실시간 연결 상태 UI는 없습니다.

서버는 `select`로 최대 63개 클라이언트의 연결을 관리합니다.
`SOMAXCONN`은 접속 요청 대기열 설정이며 게임 참가 인원수가 아닙니다.
게임 데이터 송수신과 위치 동기화는 아직 구현하지 않았습니다.

## 3인 협업 방법

각자 작업할 때 `main`을 최신 상태로 가져온 뒤 기능별 브랜치를 만듭니다. 변경 내용을 커밋하고 자신의 브랜치를 푸시한 다음, GitHub에서 Pull Request를 열어 팀원이 확인한 후 `main`에 병합합니다.

```powershell
git switch main
git pull
git switch -c feature/작업이름
# 작업 후 변경 파일을 확인하고 커밋합니다.
git add <변경한-파일>
git commit -m "변경 내용 설명"
git push -u origin feature/작업이름
```

같은 소스 파일을 수정할 때는 작업 범위를 먼저 공유하세요. `.vs/`, 개인 디버거 설정, 빌드 결과와 임시 설치 파일은 저장소에서 제외됩니다. `external/`의 필요한 DLL·라이브러리·헤더는 함께 관리합니다.

## 확인한 실행 결과

2026-11-02에 x64/Win32의 Debug/Release **4개 구성 모두 오류 없이 빌드**했습니다. 기존 소스의 형변환·부호 관련 경고는 남아 있습니다. x64 Release에서 `Solar System Adventure` 창의 실행과 응답, 종료 코드 0을 확인했습니다.
