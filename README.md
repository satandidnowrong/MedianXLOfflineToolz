# Median XL Offline Tools

Character editor (to some extent) and item manager for Diablo 2 - Median XL mod. Written on C++ using [Qt framework](https://qt.io/).

## Median XL

- the latest version: https://www.median-xl.com/
- the classic one by BrotherLaz: https://modsbylaz.vn.cz/welcome.html

## Code

**DISCLAIMER**: this is far from a great example of writing code and application architecture!

- current code should be compatible with C++03 standard
- release binaries are built against Qt 4.8.7 at the moment
- the latest code is compatible only with the latest mod version

## Building

It can be built basically for any platform. You will need:

1. C++ compiler
2. Qt 4 or 5

To build from GUI:

- any OS: open `MedianXLOfflineTools.pro` in Qt Creator
- Windows + Qt 4 only: open `MedianXLOfflineTools.sln` in Microsoft Visual Studio 2019, you will also need to install _Qt VS Tools_ extension

For command-line builds you will be using `qmake` and `make` / `jom` / `nmake`, please refer to qmake manual for details.

## EZPZ INSTALL RELEASEY (WIND0ze)

Download and install [Visual Studio 2022 Community](https://visualstudio.microsoft.com/thank-you-downloading-visual-studio/?sku=Community&channel=Release&version=VS2022&source=VSLandingPage&cid=2030&passive=false)

Download [QT 5.15.x Source Code](https://download.qt.io/official_releases/qt/5.15/5.15.2/single/qt-everywhere-src-5.15.2.zip) (it's not that scary dw)

Following [this guide](https://www.qtcentre.org/threads/72011-Qt5-and-Visual-Studion-2022?p=310993#post310993):
 - Extract Source
 - Create Build directory outside of Source directory
 - Open a Visual Studio command prompt window.
     - (Start Menu: Programs -> Visual Studio 2022 -> Visual Studio Tools -> Developer Command Prompt)
 - CD to Source directory
 - Run "..\src\configure -opensource"
     - Follow prompts
 - Run "nmake" (Can be three hours)
 - Run "nmake install"
 - Open Visual Studio
 - Install C++ workload
     - Tools -> Get Tools and Features -> Workloads -> Desktop development with C++
 - Install extension [LEGACY Qt Visual Studio Tools](https://marketplace.visualstudio.com/items?itemName=TheQtCompany.LEGACYQtVisualStudioTools2019)
     - Extensions -> Manage Extensions -> Online
 - Configure Qt
     - Extensions -> Qt VS Tools -> Qt Versions
     - Path to your Qt directory
         - Default: "C:\Qt\Qt-5.15.x"
 - Reopen Visual Studio
 - Clone a repository
 - Either [og](https://github.com/kambala-decapitator/MedianXLOfflineTools.git) or [hacked](https://github.com/satandidnowrong/MedianXLOfflineToolz.git)
 - Configure Qt some more
     - Extensions -> Qt VS Tools -> Qt Project Settings -> Properties -> Version -> $(DefaultQtVersion)
 As of writing, project currently needs:
     - Extensions -> Qt VS Tools -> Qt Project Settings -> Qt Modules: Core, Gui, Network, Widgets
 Your Visual Studio is now set up with Qt 5.15.x
 Here is where it gets stupid and I probably did this wrong but it works and I can't yet get vs to build the pro so shrug
 - Download and install [Qt 5.12.x Offline Installer](https://download.qt.io/archive/qt/5.12/5.12.12/qt-opensource-windows-x86-5.12.12.exe)
 - Download and install [Qt Creator 12.0.x](https://download.qt.io/official_releases/qtcreator/12.0/12.0.2/qt-creator-opensource-windows-x86_64-12.0.2.exe)

 I write and edit in VS and build and compile in Qt Creator shrug shrug geges

## Right click MXLOLT -> Properties -> Compatibility -> Change high DPI settings -> override -> System / System (Enhanced)
