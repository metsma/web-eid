#import <AppKit/NSApplication.h>
#import <QString>

void nshideapp() {
    int windowCount = [[[NSApplication sharedApplication] windows] count];
//    printf("Window count is %d\n", windowCount);
    if (windowCount == 4) { // XXX No idea why this number
        [NSApp hide:nil];
    }
}

QString osascript(QString scpt) {
    NSAppleScript *script = [[NSAppleScript alloc] initWithSource:scpt.toNSString()];
    NSAppleEventDescriptor *result = [script executeAndReturnError:nil];
    return QString::fromNSString([result stringValue]);
}