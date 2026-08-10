#import <Cocoa/Cocoa.h>

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// GeneralsX @tweak 10/08/2026 Keep public launcher defaults machine-independent.
//
// A local package may still bake in an asset path with -DGX_GAME_DIRECTORY. A launcher built
// directly from public source resolves environment overrides first and otherwise uses the standard
// deploy directory under the user's home folder.
#ifndef GX_GAME_DIRECTORY
#define GX_GAME_DIRECTORY ""
#endif
#ifndef GX_GENERALS_DIRECTORY
#define GX_GENERALS_DIRECTORY ""
#endif
#ifndef GX_LIGHTING_TEST_HOME
#define GX_LIGHTING_TEST_HOME ""
#endif

static NSString *const GXGameDirectory = @GX_GAME_DIRECTORY;
static NSString *const GXGeneralsDirectory = @GX_GENERALS_DIRECTORY;
static NSString *const GXLightingTestHome = @GX_LIGHTING_TEST_HOME;
static NSString *const GXZeroHourDirectoryPreference = @"GXZeroHourDirectory";
static NSString *const GXGeneralsDirectoryPreference = @"GXGeneralsDirectory";

static void GXShowError(NSString *message);

static BOOL GXIsLightingTestBundle(void)
{
    return [NSBundle.mainBundle.bundleIdentifier hasSuffix:@".lightingtest"];
}

static NSString *GXEnvironmentPath(const char *firstName, const char *secondName)
{
    const char *path = getenv(firstName);
    if ((!path || !path[0]) && secondName)
        path = getenv(secondName);
    return path && path[0] ? [NSString stringWithUTF8String:path] : nil;
}

static BOOL GXDirectoryContainsMarker(NSString *directory, NSString *marker)
{
    BOOL isDirectory = NO;
    if (!directory ||
        ![[NSFileManager defaultManager] fileExistsAtPath:directory isDirectory:&isDirectory] ||
        !isDirectory)
        return NO;

    if ([[NSFileManager defaultManager] fileExistsAtPath:
            [directory stringByAppendingPathComponent:marker]])
        return YES;

    NSArray<NSString *> *entries = [[NSFileManager defaultManager]
        contentsOfDirectoryAtPath:directory error:nil];
    for (NSString *entry in entries)
        if ([entry caseInsensitiveCompare:marker] == NSOrderedSame)
            return YES;
    return NO;
}

// GeneralsX @feature 10/08/2026 Accept a marker file, its game directory, or a parent directory.
// This makes a native open panel forgiving: selecting the runtime root instead of its GeneralsZH
// child still works, and custom folder names are discovered one level below the selected parent.
static NSString *GXNormalizeAssetDirectory(NSString *path, NSString *marker,
                                           NSArray<NSString *> *knownChildren)
{
    if (!path.length)
        return nil;

    NSString *candidate = [[path stringByExpandingTildeInPath] stringByStandardizingPath];
    BOOL isDirectory = NO;
    if ([[NSFileManager defaultManager] fileExistsAtPath:candidate isDirectory:&isDirectory] &&
        !isDirectory)
    {
        if ([candidate.lastPathComponent caseInsensitiveCompare:marker] != NSOrderedSame)
            return nil;
        candidate = candidate.stringByDeletingLastPathComponent;
    }

    if (GXDirectoryContainsMarker(candidate, marker))
        return candidate;

    for (NSString *childName in knownChildren)
    {
        NSString *child = [candidate stringByAppendingPathComponent:childName];
        if (GXDirectoryContainsMarker(child, marker))
            return child;
    }

    NSArray<NSString *> *children = [[NSFileManager defaultManager]
        contentsOfDirectoryAtPath:candidate error:nil];
    for (NSString *childName in [children sortedArrayUsingSelector:@selector(localizedCaseInsensitiveCompare:)])
    {
        NSString *child = [candidate stringByAppendingPathComponent:childName];
        if (GXDirectoryContainsMarker(child, marker))
            return child;
    }
    return nil;
}

static NSString *GXFirstValidAssetDirectory(NSArray<NSString *> *candidates, NSString *marker,
                                             NSArray<NSString *> *knownChildren)
{
    for (NSString *candidate in candidates)
    {
        NSString *resolved = GXNormalizeAssetDirectory(candidate, marker, knownChildren);
        if (resolved)
            return resolved;
    }
    return nil;
}

static NSString *GXRuntimeChild(NSString *child)
{
    NSString *root = GXEnvironmentPath("GX_RUNTIME_ROOT", NULL);
    return root ? [root stringByAppendingPathComponent:child] : nil;
}

static NSString *GXResolvedZeroHourDirectory(void)
{
    NSMutableArray<NSString *> *candidates = [NSMutableArray array];
    for (NSString *path in @[
        GXEnvironmentPath("GX_GAME_DIRECTORY", "CNC_GENERALS_ZH_PATH") ?: @"",
        [[NSUserDefaults standardUserDefaults] stringForKey:GXZeroHourDirectoryPreference] ?: @"",
        GXGameDirectory,
        GXRuntimeChild(@"GeneralsZH") ?: @"",
        [NSHomeDirectory() stringByAppendingPathComponent:@"GeneralsX/GeneralsZH"]])
        if (path.length)
            [candidates addObject:path];

    return GXFirstValidAssetDirectory(candidates, @"INIZH.big",
        @[ @"GeneralsZH", @"Generals Zero Hour", @"Command and Conquer Generals Zero Hour" ]);
}

static NSString *GXResolvedGeneralsDirectory(NSString *zeroHourDirectory)
{
    NSMutableArray<NSString *> *candidates = [NSMutableArray array];
    NSString *sibling = [[zeroHourDirectory stringByDeletingLastPathComponent]
        stringByAppendingPathComponent:@"Generals"];
    for (NSString *path in @[
        GXEnvironmentPath("GX_GENERALS_DIRECTORY", "CNC_GENERALS_PATH") ?: @"",
        [[NSUserDefaults standardUserDefaults] stringForKey:GXGeneralsDirectoryPreference] ?: @"",
        GXGeneralsDirectory,
        sibling ?: @"",
        GXRuntimeChild(@"Generals") ?: @"",
        [NSHomeDirectory() stringByAppendingPathComponent:@"GeneralsX/Generals"]])
        if (path.length)
            [candidates addObject:path];

    return GXFirstValidAssetDirectory(candidates, @"INI.big",
        @[ @"Generals", @"Command and Conquer Generals" ]);
}

static NSString *GXChooseAssetDirectory(NSString *title, NSString *message, NSString *marker,
                                         NSArray<NSString *> *knownChildren, NSString *initialPath)
{
    while (YES)
    {
        NSOpenPanel *panel = [NSOpenPanel openPanel];
        panel.title = title;
        panel.message = message;
        panel.prompt = @"选择";
        panel.canChooseDirectories = YES;
        panel.canChooseFiles = YES;
        panel.allowsMultipleSelection = NO;
        panel.canCreateDirectories = NO;
        panel.resolvesAliases = YES;
        if (initialPath.length)
        {
            NSString *directory = initialPath;
            BOOL isDirectory = NO;
            if ([[NSFileManager defaultManager] fileExistsAtPath:directory isDirectory:&isDirectory])
            {
                if (!isDirectory)
                    directory = directory.stringByDeletingLastPathComponent;
                panel.directoryURL = [NSURL fileURLWithPath:directory isDirectory:YES];
            }
        }

        if ([panel runModal] != NSModalResponseOK)
            return nil;

        NSString *resolved = GXNormalizeAssetDirectory(panel.URL.path, marker, knownChildren);
        if (resolved)
            return resolved;

        GXShowError([NSString stringWithFormat:
            @"所选位置没有找到 %@。\n\n可以选择该文件本身、包含它的游戏文件夹，或同时包含两个游戏文件夹的上一层目录。",
            marker]);
        initialPath = panel.URL.path;
    }
}

static void GXRememberAssetDirectory(NSString *path, NSString *key)
{
    if (path.length)
        [[NSUserDefaults standardUserDefaults] setObject:path forKey:key];
}

static NSString *GXHomeDirectory(void)
{
    if (!GXIsLightingTestBundle())
        return NSHomeDirectory();
    if (GXLightingTestHome.length > 0)
        return GXLightingTestHome;
    return [NSHomeDirectory() stringByAppendingPathComponent:
        @"Library/Application Support/GeneralsX/LightingTestHome"];
}

static NSInteger GXReadIntegerOption(NSString *key, NSInteger fallback,
                                     NSInteger minimum, NSInteger maximum)
{
    NSString *path = [GXHomeDirectory() stringByAppendingPathComponent:
        @"Library/Application Support/GeneralsX/GeneralsZH/Options.ini"];
    NSString *contents = [NSString stringWithContentsOfFile:path
                                                   encoding:NSUTF8StringEncoding
                                                      error:nil];
    if (!contents)
        return fallback;

    __block NSInteger result = fallback;
    [contents enumerateLinesUsingBlock:^(NSString *line, BOOL *stop) {
        NSRange separator = [line rangeOfString:@"="];
        if (separator.location == NSNotFound)
            return;
        NSString *candidate = [[line substringToIndex:separator.location]
            stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceCharacterSet];
        if ([candidate caseInsensitiveCompare:key] != NSOrderedSame)
            return;
        NSInteger value = [[[line substringFromIndex:separator.location + 1]
            stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceCharacterSet] integerValue];
        result = MAX(minimum, MIN(maximum, value));
        *stop = YES;
    }];
    return result;
}

static void GXShowError(NSString *message)
{
    [NSApp activateIgnoringOtherApps:YES];
    NSAlert *alert = [[NSAlert alloc] init];
    alert.alertStyle = NSAlertStyleCritical;
    alert.messageText = @"将军：零点行动无法启动";
    alert.informativeText = message;
    [alert addButtonWithTitle:@"好"];
    [alert runModal];
}

static void GXSetEnvironment(NSString *gameDirectory, NSString *generalsDirectory,
                             NSString *runtimeDirectory)
{
    const char *game = gameDirectory.fileSystemRepresentation;

    // Resolve both asset roots explicitly.  Relying on registry/default probes
    // is unnecessary for this self-contained macOS package and can make launch
    // appear to hang while the archive layer searches unrelated locations.
    setenv("CNC_GENERALS_ZH_PATH", game, 1);

    // The engine loads libdxvk_d3d8.dylib via a bare dlopen() call (no @rpath/
    // or @executable_path/ prefix), which dyld never resolves against the
    // binary's LC_RPATH entries. DYLD_LIBRARY_PATH is the only mechanism that
    // makes a bare-name dlopen find the bundled Frameworks directory.
    setenv("DYLD_LIBRARY_PATH", runtimeDirectory.fileSystemRepresentation, 1);

    // SagePatch is an optional hotkey-only interposer. Keep it opt-in: loading
    // Foundation through DYLD_INSERT_LIBRARIES before SDL starts is fragile on
    // current macOS and none of the in-engine controls depend on it.
    NSString *sagePatch = [runtimeDirectory stringByAppendingPathComponent:@"libsage_patch.dylib"];
    if ([[NSFileManager defaultManager] fileExistsAtPath:sagePatch] &&
        strcmp(getenv("SAGE_PATCH_ENABLED") ?: "0", "1") == 0)
    {
        const char *oldInserted = getenv("DYLD_INSERT_LIBRARIES");
        NSString *inserted = oldInserted && oldInserted[0]
            ? [NSString stringWithFormat:@"%@:%s", sagePatch, oldInserted]
            : sagePatch;
        setenv("DYLD_INSERT_LIBRARIES", inserted.fileSystemRepresentation, 1);
    }

    setenv("DXVK_WSI_DRIVER", "SDL3", 1);
    if (!getenv("DXVK_HUD"))
        setenv("DXVK_HUD", "0", 1);

    // GeneralsX @bugfix 27/07/2026 Look for the ICD manifest in Resources first.
    //
    // It used to live in Frameworks, beside the dylib it names. codesign refuses to seal a bundle
    // whose Frameworks directory holds a file that is not code -- it reports the JSON as an unsigned
    // subcomponent and fails the whole bundle -- so the packaging script puts it in Resources and
    // points library_path back across at ../Frameworks. The Frameworks path is still checked second so
    // an older hand-built bundle keeps working.
    NSString *icd = [NSBundle.mainBundle.resourcePath
        stringByAppendingPathComponent:@"MoltenVK_icd.json"];
    if (![[NSFileManager defaultManager] fileExistsAtPath:icd])
        icd = [runtimeDirectory stringByAppendingPathComponent:@"MoltenVK_icd.json"];
    if ([[NSFileManager defaultManager] fileExistsAtPath:icd])
    {
        setenv("VK_ICD_FILENAMES", icd.fileSystemRepresentation, 1);
        setenv("VK_DRIVER_FILES", icd.fileSystemRepresentation, 1);
    }

    NSString *fontConfig = [gameDirectory stringByAppendingPathComponent:@"fontconfig/fonts.conf"];
    if ([[NSFileManager defaultManager] fileExistsAtPath:fontConfig])
    {
        NSString *fontPath = [gameDirectory stringByAppendingPathComponent:@"fontconfig"];
        setenv("FONTCONFIG_FILE", fontConfig.fileSystemRepresentation, 1);
        setenv("FONTCONFIG_PATH", fontPath.fileSystemRepresentation, 1);
    }

    // GeneralsX @bugfix 10/08/2026 Read gx-font.conf, the same file run.sh reads.
    //
    // Only run.sh used to do this, so the Chinese font a user picked applied when launching from a
    // terminal and was silently ignored when launching the app -- the engine fell through to
    // auto-detection and chose a Song face, which looks like the setting simply not working. The two
    // launch paths have to agree, because nothing on screen says which one produced the window.
    //
    // An already-exported value wins, matching run.sh, so a one-off override still works:
    //     GX_CJK_SERIF_FONT="PingFang SC" open -a "将军：零点行动"
    if (!getenv("GX_CJK_SERIF_FONT"))
    {
        NSString *fontFamilyPath = [gameDirectory stringByAppendingPathComponent:@"gx-font.conf"];
        NSString *fontFamilyFile = [NSString stringWithContentsOfFile:fontFamilyPath
                                                            encoding:NSUTF8StringEncoding
                                                               error:nil];
        if (fontFamilyFile)
        {
            __block NSString *family = nil;
            [fontFamilyFile enumerateLinesUsingBlock:^(NSString *line, BOOL *stop) {
                NSString *trimmed = [line stringByTrimmingCharactersInSet:
                    NSCharacterSet.whitespaceAndNewlineCharacterSet];
                if (trimmed.length == 0 || [trimmed hasPrefix:@"#"])
                    return;
                family = trimmed;
                *stop = YES;
            }];
            if (family.length > 0)
                setenv("GX_CJK_SERIF_FONT", family.fileSystemRepresentation, 1);
        }
    }

    if (generalsDirectory.length)
    {
        NSString *generalsWithSlash = [generalsDirectory stringByAppendingString:@"/"];
        setenv("CNC_GENERALS_PATH", generalsWithSlash.fileSystemRepresentation, 1);
        setenv("CNC_GENERALS_INSTALLPATH", generalsWithSlash.fileSystemRepresentation, 1);
    }

    NSInteger renderFPS = GXReadIntegerOption(@"GXRenderFPS", 60, 30, 240);
    NSInteger speedTenths = GXReadIntegerOption(@"GXGameSpeedTenths", 10, 5, 60);
    NSInteger logicFPS = (speedTenths * 30 + 5) / 10;
    if (!getenv("GX_RENDER_FPS")) {
        char value[16];
        snprintf(value, sizeof(value), "%ld", (long)renderFPS);
        setenv("GX_RENDER_FPS", value, 1);
    }
    if (!getenv("GX_LOGIC_FPS")) {
        char value[16];
        snprintf(value, sizeof(value), "%ld", (long)logicFPS);
        setenv("GX_LOGIC_FPS", value, 1);
    }
}

int main(int argc, const char *argv[])
{
    @autoreleasepool
    {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        BOOL checkOnly = NO;
        BOOL forceChooseDirectories =
            (NSEvent.modifierFlags & NSEventModifierFlagOption) != 0;
        for (int i = 1; i < argc; ++i)
        {
            if (strcmp(argv[i], "--check") == 0)
                checkOnly = YES;
            else if (strcmp(argv[i], "--choose-game-dir") == 0)
                forceChooseDirectories = YES;
        }

        NSString *homeDirectory = GXHomeDirectory();
        if (GXIsLightingTestBundle())
        {
            [[NSFileManager defaultManager] createDirectoryAtPath:homeDirectory
                                      withIntermediateDirectories:YES
                                                       attributes:nil
                                                            error:nil];
            setenv("HOME", homeDirectory.fileSystemRepresentation, 1);
        }

        if (!checkOnly)
        {
            NSString *logDirectory = [homeDirectory
                stringByAppendingPathComponent:@"Library/Logs/GeneralsX"];
            [[NSFileManager defaultManager] createDirectoryAtPath:logDirectory
                                      withIntermediateDirectories:YES
                                                       attributes:nil
                                                            error:nil];
            NSString *logPath = [logDirectory stringByAppendingPathComponent:@"ZeroHour.log"];
            int logFD = open(logPath.fileSystemRepresentation,
                O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (logFD >= 0)
            {
                dup2(logFD, STDOUT_FILENO);
                dup2(logFD, STDERR_FILENO);
                close(logFD);
            }
        }

        NSString *gameDirectory = forceChooseDirectories ? nil : GXResolvedZeroHourDirectory();
        if (!gameDirectory && !checkOnly)
        {
            gameDirectory = GXChooseAssetDirectory(
                @"选择《零点行动》游戏文件",
                @"请选择包含 INIZH.big 的《零点行动》文件夹。也可以选择 INIZH.big 本身或它们的上一层目录；支持从 Finder 拖入，按 ⇧⌘G 可手动输入路径。",
                @"INIZH.big",
                @[ @"GeneralsZH", @"Generals Zero Hour", @"Command and Conquer Generals Zero Hour" ],
                GXRuntimeChild(@"GeneralsZH"));
        }
        if (!gameDirectory)
        {
            NSString *message = @"没有找到《零点行动》资源目录（需要 INIZH.big）。";
            if (checkOnly)
                fprintf(stderr, "ERROR: %s\n", message.UTF8String);
            else
                GXShowError(message);
            return 1;
        }
        if (!checkOnly)
            GXRememberAssetDirectory(gameDirectory, GXZeroHourDirectoryPreference);

        NSString *generalsDirectory = forceChooseDirectories ? nil
            : GXResolvedGeneralsDirectory(gameDirectory);
        if (!generalsDirectory && !checkOnly)
        {
            generalsDirectory = GXChooseAssetDirectory(
                @"选择原版《将军》游戏文件",
                @"请选择包含 INI.big 的原版《将军》文件夹。也可以选择 INI.big 本身或同时包含《将军》和《零点行动》的上一层目录。",
                @"INI.big",
                @[ @"Generals", @"Command and Conquer Generals" ],
                gameDirectory.stringByDeletingLastPathComponent);
        }
        if (!generalsDirectory)
        {
            NSString *message = @"没有找到原版《将军》资源目录（需要 INI.big）。";
            if (checkOnly)
                fprintf(stderr, "ERROR: %s\n", message.UTF8String);
            else
                GXShowError(message);
            return 1;
        }
        if (!checkOnly)
            GXRememberAssetDirectory(generalsDirectory, GXGeneralsDirectoryPreference);

        fprintf(stderr, "\nNative app launch:\n  Zero Hour: %s\n  Generals:  %s\n",
            gameDirectory.fileSystemRepresentation,
            generalsDirectory.fileSystemRepresentation);

        BOOL isDirectory = NO;
        if (![[NSFileManager defaultManager] fileExistsAtPath:gameDirectory
                                                  isDirectory:&isDirectory] || !isDirectory)
        {
            GXShowError([NSString stringWithFormat:
                @"无法读取《零点行动》游戏文件：\n%@\n\n请设置 GX_GAME_DIRECTORY 或 GX_RUNTIME_ROOT，或者重新运行部署脚本。",
                gameDirectory]);
            return 1;
        }

        // Avoid enumerating this large directory on a removable volume.  On
        // macOS that can block launch for minutes even though direct file I/O
        // is healthy. INIZH.big is a required Zero Hour asset and is a precise
        // constant-time readiness check.
        NSString *coreAsset = [gameDirectory stringByAppendingPathComponent:@"INIZH.big"];
        if (![[NSFileManager defaultManager] fileExistsAtPath:coreAsset])
        {
            GXShowError(@"没有找到《零点行动》的核心资源 INIZH.big。");
            return 1;
        }

        NSString *runtimeDirectory = NSBundle.mainBundle.privateFrameworksPath;
        NSString *binary = [NSBundle.mainBundle.bundlePath
            stringByAppendingPathComponent:@"Contents/MacOS/GeneralsXZH"];
        if (!runtimeDirectory ||
            ![[NSFileManager defaultManager] fileExistsAtPath:runtimeDirectory
                                                  isDirectory:&isDirectory] || !isDirectory)
        {
            GXShowError(@"App 包内的 Frameworks 运行库目录缺失。请重新打包应用。");
            return 1;
        }
        if (access(binary.fileSystemRepresentation, X_OK) != 0)
        {
            GXShowError(@"App 包内没有找到可执行的原生游戏程序。请重新打包 GeneralsXZH。");
            return 1;
        }

        if (checkOnly)
        {
            fprintf(stderr, "Launcher check passed.\n");
            return 0;
        }

        GXSetEnvironment(gameDirectory, generalsDirectory, runtimeDirectory);
        if (chdir(gameDirectory.fileSystemRepresentation) != 0)
        {
            GXShowError(@"无法进入游戏文件夹。");
            return 1;
        }

        const char *windowMode = GXIsLightingTestBundle() ? "-win" : "-fullscreen";
        execl(binary.fileSystemRepresentation,
              binary.fileSystemRepresentation,
              windowMode,
              (char *)NULL);

        GXShowError(@"原生游戏程序没有成功执行。");
        return 1;
    }
}
