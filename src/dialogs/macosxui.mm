#import <AppKit/NSApplication.h>

void nshideapp() {
    printf("hiding\n");
    [NSApp hide:nil];
}
