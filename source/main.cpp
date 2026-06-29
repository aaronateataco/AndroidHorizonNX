#include <switch.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <cstring>

static const char* APK_DIR = "sdmc:/BareDroidNX/apks";

struct ApkEntry {
    std::string name;  // filename only, for display
    std::string path;  // full path
};

static std::vector<ApkEntry> scanApks() {
    std::vector<ApkEntry> entries;

    DIR* dir = opendir(APK_DIR);
    if (!dir) return entries;

    struct dirent* ent;
    while ((ent = readdir(dir)) != nullptr) {
        std::string name = ent->d_name;
        if (name.size() > 4 &&
            name.compare(name.size() - 4, 4, ".apk") == 0) {
            entries.push_back({ name, std::string(APK_DIR) + "/" + name });
        }
    }
    closedir(dir);

    std::sort(entries.begin(), entries.end(),
        [](const ApkEntry& a, const ApkEntry& b) { return a.name < b.name; });

    return entries;
}

static constexpr int VISIBLE_ROWS = 22;

static void drawList(const std::vector<ApkEntry>& apks, int selected, int scroll) {
    consoleClear();
    printf("\033[1;37m BareDroidNX v0.1\033[0m\n");
    printf(" --------------------------------\n\n");

    if (apks.empty()) {
        printf(" No APKs found.\n\n");
        printf(" Place .apk files in:\n");
        printf("   sdmc:/BareDroidNX/apks/\n\n");
        printf(" \033[0;37mY: Rescan   +: Quit\033[0m\n");
        return;
    }

    int end = std::min((int)apks.size(), scroll + VISIBLE_ROWS);
    for (int i = scroll; i < end; i++) {
        if (i == selected)
            printf(" \033[1;32m> %s\033[0m\n", apks[i].name.c_str());
        else
            printf("   %s\n", apks[i].name.c_str());
    }

    printf("\n \033[0;37mA: Launch   Y: Rescan   +: Quit");
    if ((int)apks.size() > VISIBLE_ROWS)
        printf("   [%d / %d]", selected + 1, (int)apks.size());
    printf("\033[0m\n");
}

// Stub — Phase 0 will replace this with the actual loader.
static void launchApk(const ApkEntry& apk) {
    consoleClear();
    printf("\n\n \033[1;37mLaunching:\033[0m %s\n\n", apk.name.c_str());
    printf(" (loader not yet implemented)\n\n");
    printf(" Press B to return to the list.\n");

    PadState pad;
    padInitializeDefault(&pad);

    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_B) break;
        consoleUpdate(nullptr);
        svcSleepThread(16'666'666ULL);
    }
}

int main(int, char**) {
    consoleInit(nullptr);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);

    PadState pad;
    padInitializeDefault(&pad);

    // Ensure the APK directory exists.
    mkdir("sdmc:/BareDroidNX", 0777);
    mkdir(APK_DIR, 0777);

    auto apks = scanApks();
    int selected = 0;
    int scroll   = 0;

    drawList(apks, selected, scroll);
    consoleUpdate(nullptr);

    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 down = padGetButtonsDown(&pad);

        if (down & HidNpadButton_Plus) break;

        bool redraw = false;

        if (down & HidNpadButton_Y) {
            apks     = scanApks();
            selected = 0;
            scroll   = 0;
            redraw   = true;
        }

        if (!apks.empty()) {
            if (down & (HidNpadButton_Down | HidNpadButton_StickLDown)) {
                if (selected < (int)apks.size() - 1) {
                    selected++;
                    if (selected >= scroll + VISIBLE_ROWS) scroll++;
                    redraw = true;
                }
            }
            if (down & (HidNpadButton_Up | HidNpadButton_StickLUp)) {
                if (selected > 0) {
                    selected--;
                    if (selected < scroll) scroll--;
                    redraw = true;
                }
            }
            if (down & HidNpadButton_A) {
                launchApk(apks[selected]);
                redraw = true;
            }
        }

        if (redraw) {
            drawList(apks, selected, scroll);
        }

        consoleUpdate(nullptr);
        svcSleepThread(16'666'666ULL);
    }

    consoleExit(nullptr);
    return 0;
}
