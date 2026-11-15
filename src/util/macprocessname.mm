#include "util/macprocessname.h"

#import <Foundation/Foundation.h>

namespace mixxx {

void setMacProcessName(const QString& name) {
    @autoreleasepool {
        [NSProcessInfo processInfo].processName =
                [NSString stringWithUTF8String:name.toUtf8().constData()];
    }
}

} // namespace mixxx
