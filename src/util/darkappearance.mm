#include "util/darkappearance.h"
#import <AppKit/NSAppearance.h>
#import <AppKit/NSApplication.h>

bool darkAppearance() {
    if (__builtin_available(macOS 10.14, *)) {
        auto appearance =
                [NSApp.effectiveAppearance bestMatchFromAppearancesWithNames:@[
                    NSAppearanceNameAqua,
                    NSAppearanceNameDarkAqua
                ]];
        return [appearance isEqualToString:NSAppearanceNameDarkAqua];
    }
    return false;
}

void forceDarkAppearance() {
    if (__builtin_available(macOS 10.14, *)) {
        // Keep the native window chrome (title bar, menus, dialogs) dark so
        // it matches the Paramix skin even when the system is in Light mode.
        NSApp.appearance =
                [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
    }
}
