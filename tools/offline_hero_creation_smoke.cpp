#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

#include "offline_hero_creation.h"
#include "login_localization_csv.h"

int main() {
    namespace fs = std::filesystem;
    namespace create = nevergone::offline_hero_creation;
    const fs::path root = fs::temp_directory_path() / "nevergone-offline-fixed-hero-smoke";
    fs::remove_all(root);
    fs::create_directories(root / "assets" / "Login");

    // Synthetic fixture: key plus five fields, English at zero-based index 4.
    {
        std::ofstream csv(root / "assets" / "Login" / "ALL_Loin.csv");
        csv << "Hero1Name,a,b,c,DAWN,d\n";
        csv << "Hero2Name,a,b,c,HIGGS,d\n";
    }
    auto candidate = create::resolve(root.string(), 1, nevergone::login_localization_csv::kEnglishColumn);
    assert(candidate.status == create::Status::kReady);
    assert(candidate.slot_id == 1 && candidate.display_name == "DAWN");

    candidate = create::resolve(root.string(), 2, nevergone::login_localization_csv::kEnglishColumn);
    assert(candidate.status == create::Status::kReady);
    assert(candidate.slot_id == 2 && candidate.display_name == "HIGGS");

    assert(create::resolve(root.string(), 0, 4).status == create::Status::kUnsupportedHero);
    assert(create::resolve(root.string(), 3, 4).status == create::Status::kUnsupportedHero);

    // Only the matching hero becomes unavailable, not the other slot.
    { std::ofstream save(root / "DMG_01.sData"); save << "existing"; }
    assert(create::resolve(root.string(), 1, 4).status == create::Status::kSlotOccupied);
    assert(create::resolve(root.string(), 2, 4).status == create::Status::kReady);
    { std::ofstream save(root / "DMG_02.sData"); save << "existing"; }
    assert(create::resolve(root.string(), 2, 4).status == create::Status::kSlotOccupied);

    // Deletion frees the identity and slot without requiring a reset/reinstall.
    fs::remove(root / "DMG_01.sData");
    assert(create::resolve(root.string(), 1, 4).status == create::Status::kReady);
    assert(create::resolve(root.string(), 2, 4).status == create::Status::kSlotOccupied);
    fs::remove(root / "DMG_02.sData");
    assert(create::resolve(root.string(), 2, 4).status == create::Status::kReady);

    // Fail closed when the original localization resource is unavailable.
    fs::remove(root / "assets" / "Login" / "ALL_Loin.csv");
    assert(create::resolve(root.string(), 1, 4).status == create::Status::kLocalizationMissing);
    fs::remove_all(root);
    return 0;
}
