// High-confidence iOS integration reconstruction.
// This is evidence-based pseudocode, not original Objective-C++ source.
// Keychain (Security framework) calls removed per project requirements.

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <objc/runtime.h>

namespace delta_reconstructed {

// Native range 0x3a1d60..0x3a3870 builds this class at runtime.  The class
// name and Objective-C type encoding are plaintext evidence; the protected
// selector bytes are intentionally left address-labelled until fully decoded.
Class register_app_lifecycle_observer(SEL handler_selector, IMP handler) {
    Class existing = objc_getClass("AppLifecycleObserver");
    if (existing)
        return existing;
    Class cls = objc_allocateClassPair([NSObject class],
                                       "AppLifecycleObserver", 0);
    if (!cls)
        return Nil;
    if (!class_addMethod(cls, handler_selector, handler, "v@:@"))
        return Nil;
    objc_registerClassPair(cls);
    return cls;
}

void observe_application_activation(id observer, SEL selector) {
    [[NSNotificationCenter defaultCenter]
        addObserver:observer
           selector:selector
               name:UIApplicationDidBecomeActiveNotification
             object:nil];
}

} // namespace delta_reconstructed
