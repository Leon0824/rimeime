
#import <Cocoa/Cocoa.h>
#import <InputMethodKit/InputMethodKit.h>
#import <rime_api.h>

// Each input method needs a unique connection name.
// Note that periods and spaces are not allowed in the connection name.
const NSString *kConnectionName = @"Squirrel_1_Connection";

//let this be a global so our application controller delegate can access it easily
IMKServer* g_server;

int main(int argc, char *argv[])
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  
  // find the bundle identifier and then initialize the input method server
  g_server = [[IMKServer alloc] initWithName: (NSString *) kConnectionName
                            bundleIdentifier: [[NSBundle mainBundle] bundleIdentifier]];
  
  // load the bundle explicitly because in this case the input method is a
  // background only application
  [NSBundle loadNibNamed: @"MainMenu" owner: [NSApplication sharedApplication]];
  
  // opencc will be configured with relative dictionary paths
  [[NSFileManager defaultManager] changeCurrentDirectoryPath:[[NSBundle mainBundle] sharedSupportPath]];
  
  // start Rime
  RimeTraits squirrel_traits;
  squirrel_traits.shared_data_dir = [[[NSBundle mainBundle] sharedSupportPath] UTF8String];
  squirrel_traits.user_data_dir = [[@"~/Library/Rime" stringByStandardizingPath] UTF8String];
  squirrel_traits.distribution_code_name = "Squirrel";
  squirrel_traits.distribution_name = "鼠鬚管";
  squirrel_traits.distribution_version = "0.9";
  NSLog(@"Initializing la rime...");
  RimeInitialize(&squirrel_traits);
  if (RimeStartMaintenanceOnWorkspaceChange()) {
    // TODO: notification...
  }
  NSLog(@"Squirrel reporting!");
  
  // finally run everything
  [[NSApplication sharedApplication] run];
  
  NSLog(@"Squirrel quitting...");
  RimeFinalize();
  
  [g_server release];
  [pool release];
  return 0;
}

