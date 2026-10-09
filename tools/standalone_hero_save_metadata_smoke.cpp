#include <cassert>
#include <cstdint>
#include <string>

#include "login_localization_csv.h"
#include "standalone_hero_save_metadata.h"

int main() {
    using nevergone::standalone_hero_save_metadata::Metadata;
    using nevergone::standalone_hero_save_metadata::parse;

    const std::string valid =
        "447389477 device-uuid 10003 2 old-a old-b "
        "11 22 33 44 55 17 77 88 99 100 12 34 103 ";
    Metadata metadata;
    assert(parse(valid, 1u, &metadata));
    assert(metadata.slot_id == 1u);
    assert(metadata.version_code == 10003);
    assert(metadata.level == 17);
    assert(metadata.game_hours == 12);
    assert(metadata.game_minutes == 34);
    assert(metadata.name_key == "Hero1Name");

    Metadata second;
    assert(parse(valid + std::string(1, '\0'), 2u, &second));
    assert(second.name_key == "Hero2Name");

    assert(!parse(
        "123 device-uuid 10003 0 1 2 3 4 5 6 7 8 9 10 11 12 13 ",
        1u,
        &metadata));
    assert(!parse(
        "447389477 device-uuid 10001 0 1 2 3 4 5 6 7 8 9 10 11 12 13 ",
        1u,
        &metadata));
    assert(!parse(
        "447389477 device-uuid 10003 6 a b c d e f 1 2 3 4 5 6 7 8 9 10 11 12 13 ",
        1u,
        &metadata));
    assert(!parse(
        "447389477 device-uuid 10003 0 1 2 3 4 5 6 ",
        1u,
        &metadata));
    assert(!parse(valid, 3u, &metadata));

    const std::string csv =
        "\xEF\xBB\xBFkeyword,note,CN,CNT,EN,KR,FR,GM,JP,end\r\n"
        "Hero1Name,,瞳恩,瞳恩,DAWN,아더,DAWN,DAWN,ダウン,NULL\r\n"
        "Hero2Name,,茜格斯,希格斯,HIGGS,마티나,HIGGS,HIGGS,ヒグス,NULL\r\n"
        "GdUI08,LV,等级,等級,Lv.,Lv.,Lv.,Lv.,Lv.,NULL\r\n"
        "Quoted,,测试,測試,\"D,A,W,N\",x,x,x,x,NULL\r\n";

    std::string value;
    using namespace nevergone::login_localization_csv;
    assert(resolve_key(csv, "Hero1Name", kEnglishColumn, &value));
    assert(value == "DAWN");
    assert(resolve_key(csv, "Hero2Name", kSimplifiedChineseColumn, &value));
    assert(value == "茜格斯");
    assert(resolve_key(csv, "GdUI08", kEnglishColumn, &value));
    assert(value == "Lv.");
    assert(resolve_key(csv, "Quoted", kEnglishColumn, &value));
    assert(value == "D,A,W,N");
    assert(!resolve_key(csv, "Missing", kEnglishColumn, &value));
    assert(!resolve_key(csv, "Hero1Name", 20u, &value));

    return 0;
}
