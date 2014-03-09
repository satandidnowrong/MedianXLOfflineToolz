#include    "medianxlofflinetools.h"
#include "ui_medianxlofflinetools.h"

#include <QProcess>

#import "machelpers.h"

#import <AppKit/NSApplication.h>
#import <AppKit/NSMenu.h>
#import <AppKit/NSMenuItem.h>


void MedianXLOfflineTools::moveUpdateActionToAppleMenu()
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    QString helpMenuTitle = ui->menuHelp->title();
#if IS_QT5
    helpMenuTitle = helpMenuTitle.remove('&');
#endif

    NSMenu *mainMenu = [NSApp mainMenu], *helpMenu = [[mainMenu itemWithTitle:NSStringFromQString(helpMenuTitle)] submenu];
    NSMenuItem *checkForUpdateMenuItem = [[[helpMenu itemAtIndex:0] retain] autorelease];
    [helpMenu removeItem:checkForUpdateMenuItem];
    [[[mainMenu itemAtIndex:0] submenu] insertItem:checkForUpdateMenuItem atIndex:3];

    [pool release];
}


QByteArray MedianXLOfflineTools::getOsInfo()
{
    QProcess proc;

    // Mac OS X
    proc.start("sw_vers", QStringList() << "-productName", QProcess::ReadOnly);
    proc.waitForFinished();
    QByteArray result = proc.readAllStandardOutput().trimmed() + " ";

    // 10.8.4
    proc.start("sw_vers", QStringList() << "-productVersion", QProcess::ReadOnly);
    proc.waitForFinished();
    result += proc.readAllStandardOutput().trimmed() + " ";

    // x86_64 i386
    proc.start("uname", QStringList() << "-m" << "-p", QProcess::ReadOnly);
    proc.waitForFinished();
    result += proc.readAllStandardOutput().trimmed();

    return result;
}
