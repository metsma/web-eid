#import <AppKit/NSApplication.h>

void nshideapp() {
    int windowCount = [[[NSApplication sharedApplication] windows] count];
//    printf("Window count is %d\n", windowCount);
    if (windowCount == 4) { // XXX No idea why this number
        [NSApp hide:nil];
    }
}
